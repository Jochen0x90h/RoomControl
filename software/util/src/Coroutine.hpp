#pragma once

#include "assert.hpp"

#ifdef __clang__

#include <experimental/coroutine>

//#define COROUTINE_DEBUG_PRINT
#ifdef COROUTINE_DEBUG_PRINT
#include <iostream>
#endif


namespace std {
	using namespace std::experimental::coroutines_v1;
}

#else

#include <coroutine>

#endif


struct WaitlistElement;

// head of the linked list used by Waitlist
struct WaitlistHead {
	WaitlistElement *next;
	WaitlistElement *prev;
};

// element of the linked list used by Waitlist
struct WaitlistElement : public WaitlistHead {
	std::coroutine_handle<> handle;


	// default constructor
	WaitlistElement() noexcept {
		// set to "not in list"
		this->next = nullptr;
	}
	
	// delete copy constructor
	WaitlistElement(WaitlistElement const &) = delete;

	// move constructor
	WaitlistElement(WaitlistElement &&element) noexcept {
		replace(element);
	}

	// add this element to a list
	void add(WaitlistHead &head) {
		this->next = static_cast<WaitlistElement*>(&head);
		this->prev = head.prev;
		head.prev->next = this;
		head.prev = this;
	}

	// replace other element by this element
	void replace(WaitlistElement &element) {
		// replace other element by this element if it is "in list"
		if (element.next != nullptr) {
			this->prev = element.prev;
			this->next = element.next;
			this->prev->next = this;
			this->next->prev = this;
			
			// set other element to "not in list"
			element.next = nullptr;
		} else {
			// set to "not in list"
			this->next = nullptr;
		}
		
		this->handle = element.handle;
	}

	// remove this element from the list
	void remove() noexcept {
		this->next->prev = this->prev;
		this->prev->next = this->next;

		// set to "not in list"
		this->next = nullptr;
	}
};


/**
 * Check if a type is derived from a given class
 * @tparam T type to check if it derives from base class B
 * @tparam B base class
 */
template <typename T, class B>
class IsDerivedFrom {
	class No {};
	class Yes {No no[3];};
	
	static Yes test(B *);
	static No test(...);
public:
	enum {RESULT = sizeof(test(static_cast<T*>(0))) == sizeof(Yes)};
};

// specialization for T being a reference which is not derived from B
template <typename T, class B>
class IsDerivedFrom<T &, B> {
public:
	enum {RESULT = 0};
};


template <typename T, int>
struct WaitlistElementSelector {
	// element type that is not derived from WaitListElement
	struct Element : public WaitlistElement {
		T value;
		
		// default constructor
		template <typename ...Args>
		Element(Args &&...args) : value{std::forward<Args>(args)...} {}

		// move constructor
		Element(Element &&element) noexcept : WaitlistElement(std::move(element)), value(std::move(element.value)) {}
	};
	
	static T &getValue(Element *e) {return e->value;}
};

template <typename T>
struct WaitlistElementSelector<T, 1> {
	// element type that is derived from WaitListElement
	using Element = T;
	
	static T &getValue(Element *e) {return *e;}
};


// forward declare Awaitable
template <typename T = WaitlistElement>
struct Awaitable;


/**
 * List of coroutines that wait to be resumed
 * @tparam T list element type or structure that automatically gets extended to a list element
 */
template <typename T = WaitlistElement>
class Waitlist {
public:

	// select element type
	using Selector = WaitlistElementSelector<T, IsDerivedFrom<T, WaitlistElement>::RESULT>;
	using Element = typename Selector::Element;


	/**
	 * Default constructor
	 */
	Waitlist() {
		this->head.next = static_cast<Element *>(&this->head);
		this->head.prev = static_cast<Element *>(&this->head);
	}
	
	// delete copy constructor
	Waitlist(Waitlist const &) = delete;
	
	// delete assignment operator
	Waitlist &operator =(Waitlist const &) = delete;
	
	/**
	 * Check if list is empty
	 * @return true when empty
	 */
	bool isEmpty() {return this->head.next == &this->head;}

	/**
	 * Resume first waiting coroutine
	 * @return true when a coroutine was resumed, false when the list was empty
	 */
	bool resumeFirst() {
		Element *head = static_cast<Element*>(&this->head);
		Element *current = head->next;

		// check if list is empty
		if (current == head)
			return false;
		
		// remove element from list (special implementation of remove() may lock interrupts to avoid race condition)
		current->remove();
		//head->next = current->next;
		//current->next->prev = head;

		// indicate that element is ready (await_ready() returns true on subsequent co_await)
		current->next = nullptr;

		// resume coroutine if it is waiting
		if (current->handle)
			current->handle.resume();

		return true;
	}
	
	/**
	 * Resume all waiting coroutines
	 */
	void resumeAll() {
		Element *head = static_cast<Element*>(&this->head);
		Element *current = head;
		Element *next = static_cast<Element*>(current->next);
		Element *last = static_cast<Element*>(head->prev);

		// iterate over all elements that were in the list at this point, omit any new elements added during resume()
		while (current != last) {
			current = next;
			next = static_cast<Element*>(current->next);

			// remove element from list (special implementation of remove() may lock interrupts to avoid race condition)
			current->remove();
			//current->next->prev = current->prev;
			//current->prev->next = current->next;

			// indicate that element is ready (await_ready() returns true on subsequent co_await)
			current->next = nullptr;

			// resume coroutine if it is waiting
			if (current->handle)
				current->handle.resume();
		}
/*
		Element *head = static_cast<Element*>(&this->head);
		Element *current = head->next;

		// clear list
		head->prev = head->next = head;
	
		// resume all waiting coroutines
		while (current != head) {
			Element *next = current->next;

			// indicate that element is ready (await_ready() returns true on subsequent co_await)
			current->next = nullptr;

			// resume coroutine if it is waiting
			if (current->handle)
				current->handle.resume();

			current = next;
		}
*/
	}
	
	/**
	 * Resume first waiting coroutine when the filter returns true
	 * @param filter filter returns true to resume and false to leave the coroutine in the list
	 * @return true when list was not empty
	 */
	template <typename V>
	bool resumeFirst(V const &filter) {
		Element *head = static_cast<Element*>(&this->head);
		Element *current = static_cast<Element*>(head->next);

		// check if list is empty
		if (current == head)
			return false;

		if (filter(Selector::getValue(current))) {
			// remove element from list (special implementation of remove() may lock interrupts to avoid race condition)
			current->remove();
			
			// resume coroutine if it is waiting
			if (current->handle)
				current->handle.resume();
		}
		
		return true;
	}

	/**
	 * Resume one waiting coroutines for which the filter returns true
	 * @param filter filter returns true to resume and false to leave the coroutine in the list
	 */
	template <typename V>
	void resumeOne(V const &filter) {
		Element *head = static_cast<Element*>(&this->head);
		Element *current = head;
		Element *next = static_cast<Element*>(current->next);
		Element *last = static_cast<Element*>(head->prev);

		// iterate over all elements that were in the list at this point, omit any new elements added during resume()
		while (current != last) {
			current = next;
			next = static_cast<Element*>(current->next);

			if (filter(Selector::getValue(current))) {
				// remove element from list (special implementation of remove() may lock interrupts to avoid race condition)
				current->remove();
				//current->next->prev = current->prev;
				//current->prev->next = current->next;

				// indicate that element is ready (await_ready() returns true on subsequent co_await)
				current->next = nullptr;

				// resume coroutine if it is waiting
				if (current->handle)
					current->handle.resume();
				return;
			}
		}
	}

	/**
	 * Resume all waiting coroutines for which the filter returns true
	 * @param filter filter returns true to resume and false to leave the coroutine in the list
	 */
	template <typename V>
	void resumeAll(V const &filter) {
		Element *head = static_cast<Element*>(&this->head);
		Element *current = head;
		Element *next = static_cast<Element*>(current->next);
		Element *last = static_cast<Element*>(head->prev);

		// iterate over all elements that were in the list at this point, omit any new elements added during resume()
		while (current != last) {
			current = next;
			next = static_cast<Element*>(current->next);

			if (filter(Selector::getValue(current))) {
				// remove element from list (special implementation of remove() may lock interrupts to avoid race condition)
				current->remove();
				//current->next->prev = current->prev;
				//current->prev->next = current->next;

				// indicate that element is ready (await_ready() returns true on subsequent co_await)
				current->next = nullptr;

				// resume coroutine if it is waiting
				if (current->handle)
					current->handle.resume();
			}
		}
	}
	
	/**
	 * Find the first element for wich the predicate returns true
	 * @param predicate boolean predicate for the list elements
	 */
	template <typename V>
	bool find(V const &predicate) {
		Element *head = static_cast<Element*>(&this->head);
		Element *current = static_cast<Element*>(head->next);

		while (current != head) {
			if (predicate(Selector::getValue(current)))
				return true;
			current = static_cast<Element*>(current->next);
		}
		return false;
	}

	/**
	 * Wait until resumeFirst() or resumeAll() get called
	 */
	template <typename ...Args>
	Awaitable<T> wait(Args &&...args) {
		return {*this, std::forward<Args>(args)...};
	}

	// list of waiting coroutines
	WaitlistHead head;
};


/**
 * This type is returned from functions/methods that can be awaited on using co_await. It behaves like an unique_ptr
 * to a resource and therefore can only be moved, but not copied.
 */
template <typename T>
struct Awaitable {
	typename Waitlist<T>::Element element;

	Awaitable() = default;

	template <typename ...Args>
	Awaitable(Waitlist<T> &list, Args &&...args) : element(std::forward<Args>(args)...) {
		this->element.add(list.head);
		#ifdef COROUTINE_DEBUG_PRINT
			std::cout << "Awaitable add" << std::endl;
		#endif
	}

	Awaitable(Awaitable const &) = delete;

	Awaitable(Awaitable &&a) noexcept : element(std::move(a.element)) {
		#ifdef COROUTINE_DEBUG_PRINT
			std::cout << "Awaitable move" << std::endl;
		#endif
	}

	~Awaitable() {
		cancel();
	}

	Awaitable &operator =(Awaitable &&a) noexcept {
		cancel();
		this->element.replace(a.element);
		return *this;
	}

	void cancel() {
		if (this->element.next != nullptr) {
			#ifdef COROUTINE_DEBUG_PRINT
				std::cout << "Awaitable remove" << std::endl;
			#endif
			this->element.remove();
		}
	}

	bool await_ready() const noexcept {
		// is ready when the element is "not in list"
		return this->element.next == nullptr;
	}

	void await_suspend(std::coroutine_handle<> handle) noexcept {
		#ifdef COROUTINE_DEBUG_PRINT
			std::cout << "Awaitable await_suspend" << std::endl;
		#endif

		// set the coroutine handle
		this->element.handle = handle;
	}

	bool await_resume() noexcept {
		#ifdef COROUTINE_DEBUG_PRINT
			std::cout << "Awaitable await_resume" << std::endl;
		#endif

		// clear the coroutine handle
		this->element.handle = nullptr;

		// return true when the element is "not in list", i.e. it is ready
		return this->element.next == nullptr;
	}
};


/**
 * Return value for a sub-coroutine with no return value.
 *
 * Use like this:
 * AwaitableCoroutine bar() {
 * }
 * Coroutine foo() {
 *   co_await bar();
 * }
 */
struct AwaitableCoroutine {
	// an awaitable function or method can also be a coroutine
	struct promise_type {
		Waitlist<> list;

		~promise_type() {
			#ifdef COROUTINE_DEBUG_PRINT
				std::cout << "AwaitableCoroutine ~promise_type" << std::endl;
			#endif

			// the coroutine exits normally or gets destroyed
			this->list.resumeAll();
		}
		
		AwaitableCoroutine get_return_object() {
			return *this;
		}

		std::suspend_never initial_suspend() noexcept {
			return {};
		}

		std::suspend_never final_suspend() noexcept {
			return {};
		}
		
		void unhandled_exception() {
		}
		
		void return_void() {
			#ifdef COROUTINE_DEBUG_PRINT
				std::cout << "AwaitableCoroutine return_void" << std::endl;
			#endif

			// the coroutine exits normally
			//this->list.resumeAll();
		}
	};


	std::coroutine_handle<> handle;
	typename Waitlist<>::Element element;


	AwaitableCoroutine() = default;
	
	AwaitableCoroutine(promise_type &promise) : handle(std::coroutine_handle<promise_type>::from_promise(promise)) {
		this->element.add(promise.list.head);
		#ifdef COROUTINE_DEBUG_PRINT
			std::cout << "AwaitableCoroutine add" << std::endl;
		#endif
	}

	AwaitableCoroutine(AwaitableCoroutine const &) = delete;

	AwaitableCoroutine(AwaitableCoroutine &&a) noexcept : handle(a.handle), element(std::move(a.element)) {
		#ifdef COROUTINE_DEBUG_PRINT
			std::cout << "AwaitableCoroutine move" << std::endl;
		#endif
	}

	~AwaitableCoroutine() {
		cancel();
	}

	AwaitableCoroutine &operator =(AwaitableCoroutine &&a) noexcept {
		cancel();
		this->handle = a.handle;
		this->element.replace(a.element);
		return *this;
	}

	/**
	 * Cancel the coroutine if it is still running
	 */
	void cancel() {
		if (this->element.next != nullptr) {
			#ifdef COROUTINE_DEBUG_PRINT
				std::cout << "AwaitableCoroutine destroy" << std::endl;
			#endif
			this->element.remove();
			this->handle.destroy();
		}
	}

	/**
	 * Determine if a coroutine is still alive
	 * @return true when alive (running or stopped), false when finished
	 */
	bool isAlive() const noexcept {
		return this->element.next != nullptr;
	}


	bool await_ready() const noexcept {
		// is ready when the element is not part of a list
		return this->element.next == nullptr;
	}

	void await_suspend(std::coroutine_handle<> handle) noexcept {
		// set the coroutine handle
		this->element.handle = handle;
	}

	bool await_resume() noexcept {
		// clear the coroutine handle
		this->element.handle = nullptr;

		// return true when the element is not part of a list, i.e. it is ready
		return this->element.next == nullptr;
	}
};


/**
 * Simple detached coroutine for asynchronous processing. If the return value is kept, it can be destroyed later unless
 * it already has destroyed itself.
 *
 * Use like this:
 * Coroutine foo() {
 *   co_await bar();
 * }
 * void main() {
 *   Coroutine c = foo();
 *   c.destroy();
 * }
 */
struct Coroutine {
	struct promise_type {
		Coroutine get_return_object() {
			#ifdef COROUTINE_DEBUG_PRINT
				std::cout << "Coroutine get_return_object" << std::endl;
			#endif
			return {std::coroutine_handle<promise_type>::from_promise(*this)};
		}
		std::suspend_never initial_suspend() {
			#ifdef COROUTINE_DEBUG_PRINT
				std::cout << "Coroutine initial_suspend" << std::endl;
			#endif
			return {};
		}
		std::suspend_never final_suspend() noexcept {return {};}
		void unhandled_exception() {}
		void return_void() {}
	};
	
	std::coroutine_handle<> handle;
	
	/**
	 * Destroy the coroutine if it is still alive and suspended (using co_await).
	 */
	void destroy() {
		this->handle.destroy();
	}
};


/**
 * Helper class to obtain the handle of a coroutine
 *//*
struct CoroutineHandle {
	std::coroutine_handle<> handle;

	bool await_ready() noexcept {
		return false;
	}

	void await_suspend(std::coroutine_handle<> handle) noexcept {
		this->handle = handle;
		handle.resume();
	}

	void await_resume() noexcept {
	}
	
	void destroy() {
		if (this->handle) {
			this->handle.destroy();
			this->handle = nullptr;
		}
	}
};*/



// helper struct
template <typename A1, typename A2>
struct Awaitable2 {
	A1 const &a1;
	A2 const &a2;


	bool await_ready() noexcept {
		return this->a1.await_ready() || this->a2.await_ready();
	}

	void await_suspend(std::coroutine_handle<> handle) noexcept {
		const_cast<A1 &>(this->a1).await_suspend(handle);
		const_cast<A2 &>(this->a2).await_suspend(handle);
	}

	int await_resume() noexcept {
		int result = 0;
		if (const_cast<A2 &>(this->a2).await_resume())
			result = 2;
		if (const_cast<A1 &>(this->a1).await_resume())
			result = 1;
		return result;
	}
};

/**
 * Wait on two awaitables and return 1 if the first is ready and 2 if the second is ready, e.g.
 * switch (co_await select(read(data, length), delay(1s))) {
 * case 1:
 *   // read is ready
 *   break;
 * case 2:
 *   // timeout
 *   break;
 * }
 */
template <typename A1, typename A2>
[[nodiscard]] inline Awaitable2<A1, A2> select(A1 const& a1, A2 const& a2) {
	//assert((void *)a1.list != (void *)a2.list);
	return {a1, a2};
}


// helper struct
template <typename A1, typename A2, typename A3>
struct Awaitable3 {
	A1 const &a1;
	A2 const &a2;
	A2 const &a3;


	bool await_ready() noexcept {
		return this->a1.await_ready() || this->a2.await_ready() || this->a3.await_ready();
	}

	void await_suspend(std::coroutine_handle<> handle) noexcept {
		const_cast<A1 &>(this->a1).await_suspend(handle);
		const_cast<A2 &>(this->a2).await_suspend(handle);
		const_cast<A3 &>(this->a3).await_suspend(handle);
	}

	int await_resume() noexcept {
		int result = 0;
		if (const_cast<A3 &>(this->a3).await_resume())
			result = 3;
		if (const_cast<A2 &>(this->a2).await_resume())
			result = 2;
		if (const_cast<A1 &>(this->a1).await_resume())
			result = 1;
		return result;
	}
};

template <typename A1, typename A2, typename A3>
[[nodiscard]] inline Awaitable3<A1, A2, A3> select(A1 const& a1, A2 const& a2, A3 const &a3) {
	return {a1, a2, a3};
}


template <typename T = WaitlistElement>
using Barrier = Waitlist<T>;

/**
 * Auto reset event
 */
class Event {
public:
	/**
	 * Construct an event in cleared state
	 */
	Event() = default;

	/**
	 * Set the event
	 */
	void set() {
		//this->state = this->waitlist.isEmpty();
		//this->waitlist.resumeFirst();
		this->state = true;
		this->waitlist.resumeAll();
	}

	void clear() {
		this->state = false;
	}

	/**
	 * Wait until the event is set
	 */
	[[nodiscard]] Awaitable<> wait() {
		if (this->state) {
			//this->state = false;
			return {};
		}
		return {this->waitlist};
	}


	// list of waiting coroutines
	Waitlist<> waitlist;
	bool state = false;
};

class Semaphore {
public:

	/**
	 * Construct a semaphore with a given number of initial tokens that can be handed out
	 * @param n number of initial tokens
	 */
	explicit Semaphore(int n) : n(n) {}
	
	/**
	 * Post a token and resume the next coroutine waiting for a token
	 */
	void post() {
		this->n += 1 - int(this->waitlist.resumeFirst());
	}

	/**
	 * Wait for a token to become available
	 */
	[[nodiscard]] Awaitable<> wait() {
		// check if tokens are available
		if (this->n > 0) {
			--this->n;
			return {};
		}

		// wait until token is available
		return {this->waitlist};
	}


	// number of tokens
	int n;
	
	// list of waiting coroutines
	Waitlist<> waitlist;
};
