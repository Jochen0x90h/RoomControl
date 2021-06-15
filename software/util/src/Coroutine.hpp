#pragma once

#include "assert.hpp"

#ifdef __clang__

#include <experimental/coroutine>

namespace std {
	using namespace std::experimental::coroutines_v1;
}

#else

#include <coroutine>

#endif


template <typename T = void>
class CoList {
	public:

	struct Element;
	
	struct ElementBase {
		Element *next;
		Element *prev;

		ElementBase() = default;
		constexpr ElementBase(Element *head) : next(head), prev(head) {}
	};
	
	struct Element : public ElementBase {
		std::coroutine_handle<> handle;
		T value;
	
		explicit Element(T value) : value(value) {}
	
		/**
		 * Remove this element from the list
		 */
		void remove() {
			this->next->prev = this->prev;
			this->prev->next = this->next;
		}
		
		/**
		 * Remove this element from the list and resume it
		 */
		void resume() {
			remove();
			this->next = nullptr;
			this->handle.resume();
		}
	};

	constexpr CoList() : head((Element*)&this->head) {}
	
	CoList(CoList const &) = delete;
	
	CoList & operator=(CoList const &) = delete;
	
	bool isEmpty() {return this->head.next == &this->head;}
	
	void add(Element &element) {
		Element *head = reinterpret_cast<Element*>(&this->head);

		element.next = head;
		element.prev = head->prev;
		head->prev->next = &element;
		head->prev = &element;
	}

	/**
	 * Return the first value. List must not be empty
	 */
	/*const T &getFirstValue() {
		assert(!isEmpty());
		return this->head.next->value;
	}*/

	/**
	 * Resume first waiting coroutine when the filter returns true
	 * @param filter filter returns true to resume and false to leave the coroutine in the list
	 * @return true when list was not empty
	 */
	template <typename V>
	bool resumeFirst(V const &filter) {
		Element *head = reinterpret_cast<Element*>(&this->head);
		Element *current = head->next;

		// check if list is empty
		if (current == head)
			return false;

		if (filter(current->value)) {
			// remove element from list
			head->next = current->next;
			current->next->prev = head;
			
			// mark element as handled
			current->next = nullptr;
			
			// resume coroutine
			current->handle.resume();
		}
		
		return true;
	}

	/**
	 * Resume all waiting coroutines for which the filter returns true
	 * @param filter filter returns true to resume and false to leave the coroutine in the list
	 */
	template <typename V>
	void resumeAll(V const &filter) {
		Element *head = reinterpret_cast<Element*>(&this->head);
		Element *current = head->next;

		while (current != head) {
			Element *next = current->next;

			if (filter(current->value)) {
				// remove element from list
				current->remove();

				// mark element as handled
				current->next = nullptr;
				
				// resume coroutine
				current->handle.resume();
			}

			current = next;
		}
	}
	
	//Element *begin() {return this->head.next;}
	//Element *end() {return reinterpret_cast<Element*>(&this->head);}

	ElementBase head;
};


template <>
class CoList<void> {
public:

	struct Element;
	
	struct ElementBase {
		ElementBase() = default;
		constexpr ElementBase(Element *head) : next(head), prev(head) {}
		
		Element *next;
		Element *prev;
	};
	
	struct Element : public ElementBase {
		std::coroutine_handle<> handle;
	
		/**
		 * Remove this element from the list
		 */
		void remove() {
			this->next->prev = this->prev;
			this->prev->next = this->next;
		}
	};

	constexpr CoList() : head((Element*)&this->head) {}
	
	CoList(CoList const &) = delete;
	
	CoList & operator=(CoList const &) = delete;
	
	bool isEmpty() {return this->head.next == &this->head;}
	
	void add(Element &element) {
		Element *head = reinterpret_cast<Element*>(&this->head);

		element.next = head;
		element.prev = head->prev;
		head->prev->next = &element;
		head->prev = &element;
	}

	/**
	 * Resume first waiting coroutine
	 * @return true when a coroutine was resumed, false when the list was empty
	 */
	bool resumeFirst() {
		Element *head = reinterpret_cast<Element*>(&this->head);
		Element *current = head->next;

		// check if list is empty
		if (current == head)
			return false;
			
		// remove element from list
		head->next = current->next;
		current->next->prev = head;
		
		// mark element as handled
		current->next = nullptr;
		
		// resume coroutine
		current->handle.resume();

		return true;
	}
	
	/**
	 * Resume all waiting coroutines
	 */
	void resumeAll() {
		Element *head = reinterpret_cast<Element*>(&this->head);
		Element *current = head->next;

		// clear list
		head->prev = head->next = head;
	
		// resume all waiting coroutines
		while (current != head) {
			Element *next = current->next;

			// mark element as handled
			current->next = nullptr;

			// resume coroutine
			current->handle.resume();

			current = next;
		}
	}

	struct Awaitable {
		Awaitable(CoList<void> &list) : list(list) {}

		bool await_ready() {return false;}

		void await_suspend(std::coroutine_handle<> handle) noexcept {
			this->element.handle = handle;
			this->list.add(this->element);
		}

		void await_resume() noexcept {}

		CoList<void> &list;
		Element element;
	};

	Awaitable wait() {
		return *this;
	}

	ElementBase head;
};

/**
 * Simple awaitable function with no return value which can either be a normal function/method or a coroutine.
 * A normal function should return a pointer to a coroutine_handle that is initialized to nullptr and that gets set
 * when a coroutine awaits it. The function has to make sure that eventually resume() gets called on the handle.
 */
template <typename T = void>
struct Awaitable {
	Awaitable(CoList<T> &list, T value) : list(list), element(value) {}

	bool await_ready() {return false;}

	void await_suspend(std::coroutine_handle<> handle) noexcept {
		this->element.handle = handle;
		this->list.add(this->element);
	}

	void await_resume() noexcept {}

	CoList<T> &list;
	typename CoList<T>::Element element;
};



template <>
struct Awaitable<void> {
	Awaitable(CoList<void> &list) : list(list) {}

	bool await_ready() {return false;}

	void await_suspend(std::coroutine_handle<> handle) noexcept {
		this->element.handle = handle;
		this->list.add(this->element);
	}

	void await_resume() noexcept {}

	CoList<void> &list;
	typename CoList<void>::Element element;

	// an awaitable function or method can also be a coroutine
	struct promise_type {
		Awaitable<void> get_return_object() {return this->list;}
		std::suspend_never initial_suspend() {return {};}
		std::suspend_never final_suspend() noexcept {return {};}
		void unhandled_exception() {}
		void return_void() {
			this->list.resumeAll();
		}

		CoList<void> list;
	};
};
 



/**
 * Return value for a simple coroutine for asynchronous processing with no return value.
 *
 * Use like this:
 * Coroutine foo() {
 *   co_await bar();
 * }
 */
struct Coroutine {
	struct promise_type {
		Coroutine get_return_object() {return {};}
		std::suspend_never initial_suspend() {return {};}
		std::suspend_never final_suspend() noexcept {return {};}
		void unhandled_exception() {}
		void return_void() {}
	};
};


// helper struct
template <typename T1, typename T2>
struct Selector2 {
	bool await_ready() {return false;}

	//Selector2(CoList<T1> *list1, CoList<T2> *list2) : list1(list1), list2(list2) {}

	void await_suspend(std::coroutine_handle<> handle) noexcept {
		/*this->element1.handle = handle;
		this->list1->add(this->element1);
		this->element2.handle = handle;
		this->list2->add(this->element2);*/
	/*
		this->a1.element.handle = handle;
		this->a1.list->add(this->a1.element);
		this->a2.element.handle = handle;
		this->a2.list->add(this->a2.element);
	*/
		const_cast<Awaitable<T1> &>(this->a1).await_suspend(handle);
		const_cast<Awaitable<T2> &>(this->a2).await_suspend(handle);
	}

	int await_resume() noexcept {
		int result = 0;
/*
		if (this->element1.next == nullptr)
			result = 1;
		else
			this->element1.remove();
		if (this->element2.next == nullptr)
			result = 2;
		else
			this->element2.remove();
*/
		if (this->a1.element.next == nullptr)
			result = 1;
		else
			const_cast<Awaitable<T1> &>(this->a1).element.remove();
		if (this->a2.element.next == nullptr)
			result = 2;
		else
			const_cast<Awaitable<T2> &>(this->a2).element.remove();

		return result;
	}
/*
	CoList *list1;
	CoList *list2;
	CoList::Element element1;
	CoList::Element element2;
*/
	Awaitable<T1> const& a1;
	Awaitable<T2> const& a2;
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
template <typename T1, typename T2>
inline Selector2<T1, T2> select(Awaitable<T1> const& a1, Awaitable<T2> const& a2) {
	//return {a1.list, a2.list};
	assert((void *)&a1.list != (void *)&a2.list);
	return {a1, a2};
}

/*
// helper struct
struct Selector3 {
	bool await_ready() {return false;}

	void await_suspend(std::coroutine_handle<> handle) noexcept {
		*this->handle1 = handle;
		*this->handle2 = handle;
	}

	int await_resume() noexcept {
		int result = 0;
		if (*this->handle1 == nullptr)
			result = 1;
		else
			*this->handle1 = nullptr;
		if (*this->handle2 == nullptr)
			result = 2;
		else
			*this->handle2 = nullptr;
		if (*this->handle3 == nullptr)
			result = 3;
		else
			*this->handle3 = nullptr;
		return result;
	}

	std::coroutine_handle<> *handle1;
	std::coroutine_handle<> *handle2;
	std::coroutine_handle<> *handle3;
};

/ **
 * Wait on three awaitables and return 1 if the first is ready, 2 if the second is ready and 3 if the third is ready
 * /
constexpr Selector3 select(Awaitable a1, Awaitable a2, Awaitable a3) {
	return {a1.handle, a2.handle, a3.handle};
}
*/


class Semaphore {
public:

	struct Awaitable {
	   Awaitable(CoList<void> *list) : list(list) {}

	   bool await_ready() {return this->list == nullptr;}

	   void await_suspend(std::coroutine_handle<> handle) noexcept {
		   this->element.handle = handle;
		   this->list->add(this->element);
	   }

	   void await_resume() noexcept {}

	   CoList<void> *list;
	   CoList<void>::Element element;
	};

	/**
	 * Construct a semaphore with a given number of initial tokens that can be handed out
	 * @param n number of initial tokens
	 */
	explicit Semaphore(int n) : n(n) {}
	
	/**
	 * Post a token and resume the next coroutine waiting for a token
	 */
	void post() {
		this->n += 1 - int(this->waitingList.resumeFirst());
	}

	/**
	 * Wait for a token to become available
	 */
	Awaitable wait() {
		// check if tokens are available
		if (this->n > 0) {
			--this->n;
			return nullptr;
		}

		// wait until token is available
		return &waitingList;
	}


	// number of tokens
	int n;
	
	// list of waiting coroutines
	CoList<void> waitingList;
};
