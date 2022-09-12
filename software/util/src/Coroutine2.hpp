#pragma once

#include "assert.hpp"
#include "LinkedListNode.hpp"
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


template <typename T>
struct WaitlistElement2 : public LinkedListNode<T> {
	// handle of waiting coroutine
	std::coroutine_handle<> handle = nullptr;

	/**
	 * Cancel the operation or coroutine. Needs to be overloaded e.g. to stop a DMA transfer or destroy a coroutine.
	 * Only gets called if the element is still part of a list
	 */
	void cancel() {LinkedListNode<T>::remove();}
};

struct DefaultWaitlistElement : public WaitlistElement2<DefaultWaitlistElement> {
};



/**
 * List for waiting coroutines
 * @tparam T list element
 */
template <typename T = DefaultWaitlistElement>
class Waitlist2 {
public:

	// element type
	using Element = T;


	bool isEmpty() {
		return this->head.isEmpty();
	}

	void add(T &node) {
		this->head.add(node);
	}

	/**
	 * Check if the list contains an element for which the predicate is true
	 * @param predicate boolean predicate function for the list elements
	 * @return true if an element is contained
	 */
	template <typename P>
	bool contains(P const &predicate) {
		auto *current = this->head.next;
		auto *end = static_cast<Element*>(&this->head);
		while (current != end) {
			if (predicate(*current))
				return true;
			current = current->next;
		}
		return false;
	}

	/**
	 * Visit the second element. Practical for starting the next pending operation before dealing with the current
	 * operation that has just finished.
	 * @tparam V visitor type, e.g. a lambda function
	 * @param visitor visitor
	 */
	template <typename V>
	void visitSecond(V const &visitor) {
		auto *current = this->head.next;
		auto *end = static_cast<Element*>(&this->head);
		if (current != end) {
			current = current->next;
			if (current != end) {
				visitor(*current);
			}
		}
	}

	/**
	 * Remove and resume first waiting coroutine
	 * @return true when a coroutine was resumed, false when the list was empty
	 */
	void resumeFirst() {
		auto *current = this->head.next;
		auto *end = static_cast<Element*>(&this->head);
		if (current != end) {
			// remove element from list (special implementation of remove() may lock interrupts to avoid race condition)
			current->remove();

			// resume coroutine if it is waiting
			if (current->handle)
				current->handle.resume();
		}
	}

	/**
	 * Resume all waiting coroutines that were waiting when resumeAll() was called
	 */
	void resumeAll() {
		// add end marker node
		LinkedListNode<Element> end;
		this->head.add(*static_cast<Element *>(&end));

		// iterate over elements
		while (this->head.next != &end) {
			auto *current = this->head.next;

			// remove element from list (special implementation of remove() may lock interrupts to avoid race condition)
			current->remove();

			// resume coroutine if it is waiting
			if (current->handle)
				current->handle.resume();
		}

		// end removes itself automatically in the destructor
	}

	/**
	 * Remove and resume first waiting coroutine when the predicate is true
	 * @param predicate boolean predicate function for the first list element
	 * @return true when list was not empty
	 */
	template <typename P>
	void resumeFirst(P const &predicate) {
		auto *current = this->head.next;
		auto *end = static_cast<Element*>(&this->head);
		if (current != end) {
			if (predicate(*current)) {
				// remove element from list (special implementation of remove() may lock interrupts to avoid race condition)
				current->remove();

				// resume coroutine if it is waiting
				if (current->handle)
					current->handle.resume();
			}
		}
	}

	/**
	 * Resume one waiting coroutine for which the predicate is true (and remove it from the list)
	 * @param predicate boolean predicate function for the list elements
	 */
	template <typename P>
	void resumeOne(P const &predicate) {
		auto *current = this->head.next;
		auto *end = static_cast<Element*>(&this->head);
		while (current != end) {
			if (predicate(*current)) {
				// remove element from list (special implementation of remove() may lock interrupts to avoid race condition)
				current->remove();

				// resume coroutine if it is waiting
				if (current->handle)
					current->handle.resume();
				return;
			}
			current = current->next;
		}
	}

	/**
	 * Resume all waiting coroutines for which the predicate is true (and remove them from the list)
	 * @param predicate boolean predicate function for the list elements
	 */
	template <typename P>
	void resumeAll(P const &predicate) {
		// add iterator node at the beginning
		LinkedListNode<Element> it;
		this->head.next->add(*static_cast<Element *>(&it));

		// add end marker node
		LinkedListNode<Element> end;
		this->head.add(*static_cast<Element *>(&end));

		// iterate over elements
		while (it.next != &end) {
			auto *current = it.next;

			if (predicate(*current)) {
				// remove element from list (special implementation of remove() may lock interrupts to avoid race condition)
				current->remove();

				// resume coroutine if it is waiting
				if (current->handle)
					current->handle.resume();
			} else {
				// advance iterator node
				it.remove();
				current->next->add(*static_cast<Element *>(&it));
			}
		}

		// it and end remove themselves automatically in the destructor
	}

	LinkedListNode<Element> head;
};



/**
 * This type is returned from functions/methods that can be awaited on using co_await. It behaves like an unique_ptr
 * to a resource and therefore can only be moved, but not copied.
 */
template <typename T = DefaultWaitlistElement>
struct Awaitable12 {
	typename Waitlist2<T>::Element element;

	Awaitable12() = default;

	template <typename ...Args>
	Awaitable12(Waitlist2<T> &list, Args &&...args) noexcept : element(std::forward<Args>(args)...) {
		list.add(this->element);
#ifdef COROUTINE_DEBUG_PRINT
		std::cout << "Awaitable add" << std::endl;
#endif
	}

	Awaitable12(Awaitable12 const &) = delete;

	/**
	 * Move constructor, only supported when the element supports it
	 */
	Awaitable12(Awaitable12 &&a) noexcept : element(std::move(a.element)) {
#ifdef COROUTINE_DEBUG_PRINT
		std::cout << "Awaitable move" << std::endl;
#endif
	}

	~Awaitable12() {
		if (this->element.isInList())
			this->element.cancel();
	}

	/**
	 * Move assignment, only supported when the element supports it
	 */
	Awaitable12 &operator =(Awaitable12 &&a) noexcept {
		this->element = std::move(a.element);
		return *this;
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
		// clear the coroutine handle
		//this->element.handle = nullptr;
	}


	/**
	 * An awaitable function or method can also be a coroutine, therefore define a promise_type
	 */
	struct promise_type {
		// the waitlist is part of the coroutine promise
		Waitlist2<T> list;

		~promise_type() {
#ifdef COROUTINE_DEBUG_PRINT
			std::cout << "AwaitableCoroutine ~promise_type" << std::endl;
#endif
			// the coroutine exits normally or gets destroyed
			this->list.resumeAll();
		}

		Awaitable12 get_return_object() {
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


struct AwaitableCoroutineElement : public WaitlistElement2<AwaitableCoroutineElement> {
	// handle of awaitable coroutine
	std::coroutine_handle<> context = nullptr;

	AwaitableCoroutineElement() = default;
	explicit AwaitableCoroutineElement(std::coroutine_handle<> context) : context(context) {}

	// move assignment
	AwaitableCoroutineElement &operator =(AwaitableCoroutineElement &&a) noexcept {
		remove();
		add(a);
		a.remove();
		this->context = a.context;
		return *this;
	}

	void cancel() {
		LinkedListNode<AwaitableCoroutineElement>::remove();
		this->context.destroy();
	}
};

using AwaitableCoroutine2 = Awaitable12<AwaitableCoroutineElement>;



/**
 * Synchronizer on which a data consumer coroutine can wait until it gets resumed by a data producer.
 * To prevent data loss like with Barrier, call waitForConsumer() before calling a resume method.
 * @tparam T
 */
template <typename T>
class Synchronizer : public Waitlist2<T> {
public:

	/**
	 * Wait until a data producer passes data (using e.g. resumeFirst() or resumeAll()).
	 * Call this as a data consumer
	 * @return use co_await on return value to wait for data
	 */
	template <typename ...Args>
	[[nodiscard]] Awaitable12<T> wait(Args &&...args) {
		bool empty = this->dataWaitlist.isEmpty();
		Awaitable<T> a{*this, std::forward<Args>(args)...};
		if (empty)
			emptyWaitlist.resumeFirst();
		return a;
	}

	/**
	 * Wait until a data consumer waits for data
	 * Call this as a data producer before resuming the consumer (using e.g. resumeFirst() or resumeAll()).
	 * @return use co_await on return value to wait for a consumer
	 */
	[[nodiscard]] Awaitable12<> waitForConsumer() {
		// check if a consumer is already waiting
		if (!Waitlist2<T>::isEmpty())
			return {};
		return {this->emptyWaitlist};
	}

	Waitlist2<> emptyWaitlist;
};
