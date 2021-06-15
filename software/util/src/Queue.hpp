#pragma once

#include "util.hpp"
#include "assert.hpp"


/**
 * Queue with fixed size where elements can be added at the head and taken out at the tail
 *
 * Example when count() is 3:
 *  get()     getHead()
 *   v           v
 * _____________....
 * | 0 | 1 | 2 | 3 : -> add(), increment()
 * -------------....
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
	int size() const {return this->c;}
	int size() const volatile {return this->c;}

	/**
	 * Get the element at the tail of the queue
	 * @return element at tail
	 */
	Element &get() {
		return this->elements[this->tail];
	}
	Element &get() volatile {
		return const_cast<Element &>(this->elements[this->tail]);
	}
	
	/**
	 * Get the element at the head of the queue
	 * @return head element of the queue
	 */
	Element &getHead() {
		assert(this->c < N);
		
		int head = this->tail + this->c;
		return this->elements[head >= N ? head - N : head];
	}
   Element &getHead() volatile {
		assert(this->c < N);

		int head = this->tail + this->c;
		return const_cast<Element &>(this->elements[head >= N ? head - N : head]);
   }
   
	/**
	 * Increment the head of the queue (use after head was written using getHead())
	*/
	void increment() {
	   assert(this->c < N);
	   ++this->c;
	}
	void increment() volatile {
	   assert(this->c < N);
	   ++this->c;
	}

	/**
	 * Add a new element to the head of the queue and return previous head.
	 * Equivalent to head = queue.getHead(); queue.increment();
	 * @return previous head element of the queue
	*/
	Element &add() {
		assert(this->c < N);
		int head = this->tail + this->c++;
		return this->elements[head >= N ? head - N : head];
	}
	Element &add() volatile {
		assert(this->c < N);
		int head = this->tail + this->c++;
		return const_cast<Element &>(this->elements[head >= N ? head - N : head]);
	}

	/**
	 * Remove the element at tail and make the next element the current element
	 * A pointer to the tail element stays valid until the head is obtained and written to using getHead() or add()
	 */
	void remove() {
		assert(this->c > 0);
		int tail = this->tail + 1;
		this->tail = tail == N ? 0 : tail;
		--this->c;
	}
	void remove() volatile {
		assert(this->c > 0);
		int tail = this->tail + 1;
		this->tail = tail == N ? 0 : tail;
		--this->c;
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
	typename UInt<N>::Type tail = 0;
	
	// number of elements in queue
	typename UInt<N + 1>::Type c = 0;
};
