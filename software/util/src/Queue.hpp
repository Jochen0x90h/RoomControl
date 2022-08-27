#pragma once

#include "util.hpp"
#include "assert.hpp"


/**
 * Queue with fixed size where elements can be added at the back and taken out at the front
 *
 * Example when count() is 6:
 * getFront()     getBack() getNextBack()
 *   v                   v   v
 * _________________________....
 * | 0 | 1 | 2 | 3 | 4 | 5 | 6 : -> addBack()
 * -------------------------....
 */
template <typename Element, int N>
class Queue {
public:
	/**
	 * Check if the queue is empty
	 * @return true if queue is empty
	 */
	bool isEmpty() const {return this->c == 0;}
	bool isEmpty() const volatile {return this->c == 0;}

	/**
	 * Check if the queue is full
	 * @return true if queue is full
	 */
	bool isFull() const {return this->c >= N;}
	bool isFull() const volatile {return this->c >= N;}

	/**
	 * Get number of elements in queue
	 * @return number of elements
	 */
	int count() const {return this->c;}
	int count() const volatile {return this->c;}

	/**
	 * Get the element at the front of the queue. The queue must not be empty
	 * @return element at front
	 */
	Element &getFront() {
		assert(this->c > 0);
		return this->elements[this->front];
	}
	Element &getFront() volatile {
		assert(this->c > 0);
		return const_cast<Element &>(this->elements[this->front]);
	}

	/**
	 * Get the element at the back of the queue. The queue must not be empty
	 * @return element at back
	 */
	Element &getBack() {
		assert(this->c > 0);
		return this->elements[(this->front + this->c - 1) % N];
	}
	Element &getBack() volatile {
		assert(this->c > 0);
		return const_cast<Element &>(this->elements[(this->front + this->c - 1) % N]);
	}
	
	/**
	 * Get the next element behind the back of the queue. The queue must not be full
	 * @return element behind the back
	 */
	Element &getNextBack() {
		assert(this->c < N);
		return this->elements[(this->front + this->c) % N];
	}
	Element &getNextBack() volatile {
		assert(this->c < N);
		return const_cast<Element &>(this->elements[(this->front + this->c) % N]);
	}

	/**
	 * Get the element at given index
	 */
	Element &operator [](int index) {
		unsigned int i = index;
		assert(i < this->c);
		return this->elements[(this->front + i) % N];
	}
	Element &operator [](unsigned int index) volatile {
		unsigned int i = index;
		assert(i < this->c);
		return const_cast<Element &>(this->elements[(this->front + i) % N]);
	}

	/**
	 * Add a new uninitialized element to the back of the queue. If the queue is full, the front element gets removed.
	 * If the element was already written using getNext(), it is remains untuched and is now part of the queue.
	 */
	void addBack() {
		auto c = this->c;
		if (c == N) {
			this->front = (this->front + 1) % N;
		} else {
			this->c = c + 1;
		}
	}
	void addBack() volatile {
		auto c = this->c;
		if (c == N) {
			this->front = (this->front + 1) % N;
		} else {
			this->c = c + 1;
		}
	}

	/**
	 * Add a new uninitialized element to the back of the queue. If the queue is full, the front element gets removed.
	 * If the element was already written using getNext(), it is remains untuched and is now part of the queue.
	 */
	void addBack(Element const &element) {
		this->elements[(this->front + this->c) % N] = element;
		auto c = this->c;
		if (c == N) {
			this->front = (this->front + 1) % N;
		} else {
			this->c = c + 1;
		}
	}
	void addBack(Element const &element) volatile {
		const_cast<Element &>(this->elements[(this->front + this->c) % N]) = element;
		auto c = this->c;
		if (c == N) {
			this->front = (this->front + 1) % N;
		} else {
			this->c = c + 1;
		}
	}

	/**
	 * Remove the element at front and make the next element the front element.
	 * A pointer to the old front element stays valid until the queue is modified again
	 */
	void removeFront() {
		auto c = this->c;
		if (c > 0) {
			this->c = c - 1;
			this->front = (this->front + 1) % N;
		}
	}
	void removeFront() volatile {
		auto c = this->c;
		if (c > 0) {
			this->c = c - 1;
			this->front = (this->front + 1) % N;
		}
	}

	/**
	 * Remove the element at the back
	 */
	void removeBack() {
		auto c = this->c;
		if (c > 0) {
			this->c = c - 1;
		}
	}
	void removeBack() volatile {
		auto c = this->c;
		if (c > 0) {
			this->c = c - 1;
		}
	}

	/**
	 * Clear the queue
	 */
	void clear() {this->c = 0;}
	void clear() volatile {this->c = 0;}

protected:

	// queue elements
	Element elements[N];
	
	// queue tail where elements are taken out
	typename UInt<N>::Type front = 0;
	
	// number of elements in queue
	typename UInt<N + 1>::Type c = 0;
};
