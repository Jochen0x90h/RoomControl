#pragma once

#include "assert.hpp"
#include "IsSubclass.hpp"
#include <utility>

#ifdef __clang__
#include <experimental/coroutine>
namespace std {
	using namespace std::experimental::coroutines_v1;
}
#else
#include <coroutine>
#endif


//#define COROUTINE_DEBUG_PRINT
#ifdef COROUTINE_DEBUG_PRINT
#include <iostream>
#endif


/**
 * Node for Waitlist and and list elements representing waiting coroutines.
 * These methods need to be implemented by list elements:
 *
 * append(WaitlistNode &list): Append the node to the given list
 * cancel(): Cancel the operation or awaitable coroutine (e.g. to stop a DMA transfer or destroy the awaitable coroutine).
 * take(WaitlistNode &node): Optional for move assignment, take the waiting coroutine from the given node
 *
 * The cancel() method only gets called if the element is still part of a waitlist and also has to remove the element
 * from the waitlist and maybe needs to be protected the list against concurrent modifications. A destructor also needs
 * to be implemented to call cancel() when the node is still part of a waitlist (if (isInList()) cancel();)
 * @tparam T
 */
class WaitlistNode {
public:
	// default constructor with no initialization
	WaitlistNode() = default;

	// delete copy constructor
	WaitlistNode(WaitlistNode const &) = delete;

	/**
	 * Destructor. It is an error to destroy a waitlist node that is still part of a list
	 */
	~WaitlistNode() {
		assert(!isInList());
	}

	/**
	 * Set the node to "not in list" state
	 */
	void setNotInList() {
		this->next = nullptr;
	}

	/**
	 * Add the node to a list
	 * @param list list to add to
	 */
	void add(WaitlistNode &list) noexcept {
		list.prev = this->prev;
		list.next = this;
		this->prev->next = &list;
		this->prev = &list;
	}

	/**
	 * Check if a node is part of a list. Can only be called after either setNotInList() or add() has been called
	 */
	bool isInList() const {
		return this->next != nullptr;
	}

	/**
	 * Pass waiting coroutine to another node and set this node to "not in list". Can only be called if this node is in
	 * a list
	 * @param node node to pass the waiting coroutine to
	 */
	void pass(WaitlistNode &node) noexcept {
		this->prev->next = &node;
		this->next->prev = &node;
		node.prev = this->prev;
		node.next = this->next;

		// set to "not in list"
		this->next = nullptr;
	}

	/**
	 * Remove the node from the list and set to "not in list". Can only be called if the node is in a list
	 */
	void remove() noexcept {
		this->next->prev = this->prev;
		this->prev->next = this->next;

		// set to "not in list"
		this->next = nullptr;
	}

	WaitlistNode *next;
	WaitlistNode *prev;
};

/**
 * Waitlist element with default implementation of append(), cancel() and take().
 */
class WaitlistElement : public WaitlistNode {
public:
	void append(WaitlistNode &list) noexcept {list.add(*this);}

	void cancel() noexcept {remove();}

	void take(WaitlistElement &element) {
		element.pass(*this);
	}

	// handle of waiting coroutine
	std::coroutine_handle<> handle = nullptr;
};


template <typename T>
class WaitlistParameters : public WaitlistElement {
public:
	template <typename ...Args>
	explicit WaitlistParameters(Args &&...args) : parameters{std::forward<Args>(args)...} {}

	T parameters;
};


/**
 * Default waitlist element with no parameters
 */
using DefaultWaitlistElement = WaitlistElement;


template <typename T, int>
struct WaitlistElementSelector {
	using Element = WaitlistParameters<T>;
	static T &get(Element *e) {return e->parameters;}
};

// specialization for T being derived from WaitlistElement
template <typename T>
struct WaitlistElementSelector<T, 1> {
	using Element = T;
	static T &get(Element *e) {return *e;}
};


/**
 * List for waiting coroutines
 * @tparam T list element
 */
template <typename T = DefaultWaitlistElement>
class Waitlist {
public:

	// select element type
	using Selector = WaitlistElementSelector<T, IsSubclass<T, WaitlistNode>::RESULT>;
	using Element = typename Selector::Element;

	Waitlist() {
		this->head.next = this->head.prev = &this->head;
	}

	/**
	 * Destructor, list should be empty when the destructor is called
	 */
	~Waitlist() {
		this->head.remove();
	}

	/**
	 * Check if the list is empty
	 * @return true if empty
	 */
	bool isEmpty() {
		return this->head.next == &this->head;
	}

	/**
	 * Check if the list contains an element for which the predicate is true
	 * @param predicate boolean predicate function for the list elements
	 * @return true if an element is contained
	 */
	template <typename P>
	bool contains(P const &predicate) {
		auto *current = this->head.next;
		auto *end = &this->head;
		while (current != end) {
			if (predicate(Selector::get(static_cast<Element*>(current))))
				return true;
			current = current->next;
		}
		return false;
	}

	/**
	 * Get first element. List must not be empty
	 * @return first element
	 */
	auto &getFirst() {
		assert(!isEmpty());
		auto first = this->head.next;
		return Selector::get(static_cast<Element*>(first));
	}

	/**
	 * Visit the first element
	 * @tparam V visitor type, e.g. a lambda function
	 * @param visitor visitor
	 * @param true if a first element exists
	 */
	template <typename V>
	bool visitFirst(V const &visitor) {
		auto first = this->head.next;
		auto end = &this->head;
		if (first != end) {
			visitor(Selector::get(static_cast<Element*>(first)));
			return true;
		}
		return false;
	}

	/**
	 * Visit the second element. Practical for starting the next pending operation before dealing with the current
	 * operation that has just finished.
	 * @tparam V visitor type, e.g. a lambda function
	 * @param visitor visitor
	 * @param true if a second element exists
	 */
	template <typename V>
	bool visitSecond(V const &visitor) {
		auto first = this->head.next;
		auto end = &this->head;
		if (first != end) {
			auto second = first->next;
			if (second != end) {
				visitor(Selector::get(static_cast<Element*>(second)));
				return true;
			}
		}
		return false;
	}

	/**
	 * Remove and resume first waiting coroutine
	 * @return true when a coroutine was resumed, false when the list was empty
	 */
	bool resumeFirst() {
		auto first = static_cast<Element*>(this->head.next);
		auto end = &this->head;
		if (first != end) {
			// remove element from list (special implementation of remove() may lock interrupts to avoid race condition)
			first->remove();

			// resume coroutine if it is waiting
			if (first->handle)
				first->handle.resume();
			return true;
		}
		return false;
	}

	/**
	 * Resume all waiting coroutines that were waiting when resumeAll() was called
	 */
	void resumeAll() {
		// add end marker node
		WaitlistNode end;
		this->head.add(end);

		// iterate over elements
		while (this->head.next != &end) {
			auto *current = static_cast<Element*>(this->head.next);

			// remove element from list (special implementation of remove() may lock interrupts to avoid race condition)
			current->remove();

			// resume coroutine if it is waiting
			if (current->handle)
				current->handle.resume();
		}

		// remove temporary nodes
		end.remove();
	}

	/**
	 * Remove and resume first waiting coroutine when the predicate is true
	 * @param predicate boolean predicate function for the first list element
	 * @return true when list was not empty
	 */
	template <typename P>
	void resumeFirst(P const &predicate) {
		auto first = static_cast<Element*>(this->head.next);
		auto end = &this->head;
		if (first != end) {
			if (predicate(Selector::get(first))) {
				// remove element from list (special implementation of remove() may lock interrupts to avoid race condition)
				first->remove();

				// resume coroutine if it is waiting
				if (first->handle)
					first->handle.resume();
			}
		}
	}

	/**
	 * Resume one waiting coroutine for which the predicate is true (and remove it from the list)
	 * @param predicate boolean predicate function for the list elements
	 */
	template <typename P>
	void resumeOne(P const &predicate) {
		auto current = static_cast<Element*>(this->head.next);
		auto end = &this->head;
		while (current != end) {
			if (predicate(Selector::get(current))) {
				// remove element from list (special implementation of remove() may lock interrupts to avoid race condition)
				current->remove();

				// resume coroutine if it is waiting
				if (current->handle)
					current->handle.resume();
				return;
			}
			current = static_cast<Element*>(current->next);
		}
	}

	/**
	 * Resume all waiting coroutines for which the predicate is true (and remove them from the list)
	 * @param predicate boolean predicate function for the list elements
	 */
	template <typename P>
	void resumeAll(P const &predicate) {
		// add iterator node at the beginning
		WaitlistNode it;
		this->head.next->add(it);

		// add end marker node
		WaitlistNode end;
		this->head.add(end);

		// iterate over elements
		while (it.next != &end) {
			auto current = static_cast<Element*>(it.next);

			if (predicate(Selector::get(current))) {
				// remove element from list (special implementation of remove() may lock interrupts to avoid race condition)
				current->remove();

				// resume coroutine if it is waiting
				if (current->handle)
					current->handle.resume();
			} else {
				// advance iterator node
				it.remove();
				current->next->add(it);
			}
		}

		// remove temporary nodes
		it.remove();
		end.remove();
	}

	WaitlistNode head;
};



/**
 * This type is returned from functions/methods that can be awaited on using co_await. It behaves like an unique_ptr
 * to a resource and therefore can only be moved, but not copied.
 */
template <typename T = DefaultWaitlistElement>
struct Awaitable {
	typename Waitlist<T>::Element element;

	Awaitable() {
		this->element.setNotInList();
	}

	template <typename ...Args>
	Awaitable(Waitlist<T> &list, Args &&...args) noexcept : element(std::forward<Args>(args)...) {
		this->element.append(list.head);
#ifdef COROUTINE_DEBUG_PRINT
		std::cout << "Awaitable add" << std::endl;
#endif
	}

	Awaitable(Awaitable const &) = delete;

	/**
	 * Move constructor, only supported when the element supports it
	 */
	Awaitable(Awaitable &&a) noexcept {
#ifdef COROUTINE_DEBUG_PRINT
		std::cout << "Awaitable move" << std::endl;
#endif
		// take waiting coroutine from other element
		if (a.element.isInList())
			this->element.take(a.element);
	}

	~Awaitable() {
#ifdef COROUTINE_DEBUG_PRINT
		std::cout << "Awaitable destructor" << std::endl;
#endif
		if (this->element.isInList()) {
#ifdef COROUTINE_DEBUG_PRINT
			std::cout << "Awaitable cancel" << std::endl;
#endif
			this->element.cancel();
		}
	}

	/**
	 * Move assignment, only supported when the element implements take()
	 */
	Awaitable &operator =(Awaitable &&a) noexcept {
		// cancel existing job
		if (this->element.isInList())
			this->element.cancel();

		// take waiting coroutine from other element
		if (a.element.isInList())
			this->element.take(a.element);
		return *this;
	}

	bool isAlive() const noexcept {
		return this->element.isInList();
	}

	/**
	 * Determine if the operation or coroutine has finished
	 * @return true when finished, false when still in progress (coroutine: running or co_awaiting)
	 */
	bool hasFinished() const noexcept {
		return !this->element.isInList();
	}

	/**
	 * Cancel the operation or coroutine
	 */
	void cancel() {
#ifdef COROUTINE_DEBUG_PRINT
		std::cout << "Awaitable cancel" << std::endl;
#endif
		if (this->element.isInList())
			this->element.cancel();
	}

	/**
	 * Used by co_await to determine if the operation has finished (ready to continue)
	 * @return true when finished
	 */
	bool await_ready() const noexcept {
		// is ready when the element is "not in list"
		return !this->element.isInList();
	}

	/**
	 * Used by co_await to store the handle of the calling coroutine before suspending
	 */
	void await_suspend(std::coroutine_handle<> handle) noexcept {
#ifdef COROUTINE_DEBUG_PRINT
		std::cout << "Awaitable await_suspend" << std::endl;
#endif
		// set the coroutine handle
		this->element.handle = handle;
	}

	/**
	 * Used by co_await to determine the return value of co_await
	 */
	void await_resume() noexcept {
#ifdef COROUTINE_DEBUG_PRINT
		std::cout << "Awaitable await_resume" << std::endl;
#endif
	}


	/**
	 * An awaitable function or method can also be a coroutine, therefore define a promise_type
	 */
	struct promise_type {
		// the waitlist is part of the coroutine promise
		Waitlist<T> list;

		~promise_type() {
#ifdef COROUTINE_DEBUG_PRINT
			std::cout << "AwaitableCoroutine ~promise_type" << std::endl;
#endif
			// the coroutine exits normally or gets destroyed
			this->list.resumeAll();
		}

		Awaitable get_return_object() {
			auto handle = std::coroutine_handle<promise_type>::from_promise(*this);
			return {this->list, handle};
		}

		std::suspend_never initial_suspend() noexcept {
			return {};
		}

		std::suspend_never final_suspend() noexcept {
			return {};
		}

		void unhandled_exception() {
		}

		// the coroutine exits normally
		void return_void() {
#ifdef COROUTINE_DEBUG_PRINT
			std::cout << "AwaitableCoroutine return_void" << std::endl;
#endif
		}
	};
};


class AwaitableCoroutineElement : public WaitlistNode {
public:

	AwaitableCoroutineElement() = default;
	explicit AwaitableCoroutineElement(std::coroutine_handle<> context) : context(context) {}

	void append(WaitlistNode &list) {
		list.add(*this);
	}

	void take(AwaitableCoroutineElement &element) {
		element.pass(*this);
		this->context = element.context;
	}

	void cancel() {
		remove();
		this->context.destroy();
	}


	// handle of waiting coroutine
	std::coroutine_handle<> handle = nullptr;

	// handle of awaitable coroutine
	std::coroutine_handle<> context = nullptr;
};

using AwaitableCoroutine = Awaitable<AwaitableCoroutineElement>;



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
 *
 *   // we are sure that the coroutine is still alive
 *   c.destroy();
 * }
 */
struct Coroutine {
	struct promise_type {
		Coroutine get_return_object() noexcept {
#ifdef COROUTINE_DEBUG_PRINT
			std::cout << "Coroutine get_return_object" << std::endl;
#endif
			return {std::coroutine_handle<promise_type>::from_promise(*this)};
		}
		std::suspend_never initial_suspend() noexcept {
#ifdef COROUTINE_DEBUG_PRINT
			std::cout << "Coroutine initial_suspend" << std::endl;
#endif
			return {};
		}
		std::suspend_never final_suspend() noexcept {return {};}
		void unhandled_exception() noexcept {}
		void return_void() noexcept {}
	};
	
	std::coroutine_handle<> handle;
	
	/**
	 * Destroy the coroutine if it is still alive and suspended (coroutine has called co_await).
	 * Call only once and when it is sure that the coroutine is still alive, e.g. when it contains an infinite loop.
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
	A1 &a1;
	A2 &a2;

	bool await_ready() noexcept {
		return this->a1.await_ready() || this->a2.await_ready();
	}

	void await_suspend(std::coroutine_handle<> handle) noexcept {
		this->a1.await_suspend(handle);
		this->a2.await_suspend(handle);
	}

	int await_resume() noexcept {
		int result = 0;
		if (this->a2.await_ready())
			result = 2;
		if (this->a1.await_ready())
			result = 1;
		await_suspend(nullptr);
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
[[nodiscard]] inline Awaitable2<A1, A2> select(A1 &&a1, A2 &&a2) {
	return {a1, a2};
}


// helper struct
template <typename A1, typename A2, typename A3>
struct Awaitable3 {
	A1 &a1;
	A2 &a2;
	A3 &a3;

	bool await_ready() noexcept {
		return this->a1.await_ready() || this->a2.await_ready() || this->a3.await_ready();
	}

	void await_suspend(std::coroutine_handle<> handle) noexcept {
		this->a1.await_suspend(handle);
		this->a2.await_suspend(handle);
		this->a3.await_suspend(handle);
	}

	int await_resume() noexcept {
		int result = 0;
		if (this->a3.await_ready())
			result = 3;
		if (this->a2.await_ready())
			result = 2;
		if (this->a1.await_ready())
			result = 1;
		await_suspend(nullptr);
		return result;
	}
};

/**
 * Wait on three awaitables and return 1 if the first is ready, 2 if the second is ready and 3 if the third is ready
 */
template <typename A1, typename A2, typename A3>
[[nodiscard]] inline Awaitable3<A1, A2, A3> select(A1 &&a1, A2 &&a2, A3 &&a3) {
	return {a1, a2, a3};
}


// helper struct
template <typename A1, typename A2, typename A3, typename A4>
struct Awaitable4 {
	A1 &a1;
	A2 &a2;
	A3 &a3;
	A4 &a4;

	bool await_ready() noexcept {
		return this->a1.await_ready() || this->a2.await_ready() || this->a3.await_ready() || this->a4.await_ready();
	}

	void await_suspend(std::coroutine_handle<> handle) noexcept {
		this->a1.await_suspend(handle);
		this->a2.await_suspend(handle);
		this->a3.await_suspend(handle);
		this->a4.await_suspend(handle);
	}

	int await_resume() noexcept {
		int result = 0;
		if (this->a4.await_ready())
			result = 4;
		if (this->a3.await_ready())
			result = 3;
		if (this->a2.await_ready())
			result = 2;
		if (this->a1.await_ready())
			result = 1;
		await_suspend(nullptr);
		return result;
	}
};

/**
 * Wait on four awaitables and return index 1 to 4 of the awaitable that is ready
 */
template <typename A1, typename A2, typename A3, typename A4>
[[nodiscard]] inline Awaitable4<A1, A2, A3, A4> select(A1 &&a1, A2 &&a2, A3 &&a3, A4 &&a4) {
	return {a1, a2, a3, a4};
}



/**
 * Simple barrier on which a data consumer coroutine can wait until it gets resumed by a data producer.
 * If a resume method gets called by a data producer while no consumer is waiting, the event/data gets lost.
 * @tparam T
 */
template <typename T = DefaultWaitlistElement>
class Barrier : public Waitlist<T> {
public:

	/**
	 * Wait until a data producer passes data (using e.g. resumeFirst() or resumeAll()).
	 * Call this as a data consumer
	 * @return use co_await on return value to wait for data
	 */
	template <typename ...Args>
	[[nodiscard]] Awaitable<T> wait(Args &&...args) {
		return {*this, std::forward<Args>(args)...};
	}
};


/**
 * Manual reset event
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
