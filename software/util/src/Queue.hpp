#pragma once

#include <assert.hpp>
#include <util.hpp>


/**
 * Queue for elements with data
 * @tparam T custom info for each element
 * @tparam N maximum number of queue elements
 * @tparam M size of data buffer
 */
template <typename T, int N, int M>
struct Queue {
public:

	struct Element {
		T &info;
		uint8_t *data;
		int length;
	};

	/**
	 * Check if the queue has space for a new element with given data length
	 * @param length data length
	 */
	bool hasSpace(int length) {
		// check if elements array is full
		if (this->entryCount >= N)
			return false;
			
		// check if there is space in the data buffer
		if (this->entryCount > 0) {
			int head = this->bufferHead;
			int tail = this->entries[this->entryTail].offset;
			
			if (head < tail) {
				// head behind tail
				return tail - head > length;
			} else {
				// head ahead of tail
				return M - head >= length + (tail == 0) || tail > length;
			}
		}
		return length < M;
	}
	
	Element add(T const &info, int length) {
		assert(hasSpace(length));
		
		// set head to next element
		this->entryHead = this->entryHead == N - 1 ? 0 : this->entryHead + 1;
		++this->entryCount;

		Entry &entry = this->entries[this->entryHead];
		entry.info = info;
		
		if (this->bufferHead + length > M)
			this->bufferHead = 0;

		entry.offset = this->bufferHead;
		entry.length = length;

		this->bufferHead += length;
				
		return {entry.info, &this->buffer[entry.offset], entry.length};
	}

	/**
	 * Remove last element
	 */
	void remove() {
		assert(this->entryCount > 0);
		
		Entry &entry = this->entries[this->entryTail];
		
		// set tail to next element
		this->entryTail = this->entryTail == N - 1 ? 0 : this->entryTail + 1;
		--this->entryCount;
	}

	bool empty() const {return this->entryCount == 0;}

	Element front() {
		Entry &entry = this->entries[this->entryHead];
		return {entry.info, &this->buffer[entry.offset], entry.length};
	}
	Element back() {
		Entry &entry = this->entries[this->entryTail];
		return {entry.info, &this->buffer[entry.offset], entry.length};
	}

	// get data buffer, can be used as temp buffer when the queue is not in use
	uint8_t *data() {return this->buffer;}

protected:

	struct Entry {
		typename UInt<M>::Type offset;
		typename UInt<M>::Type length;
		T info;
	};

	// entries
	Entry entries[N];
	typename UInt<N>::Type entryHead = N - 1;
	typename UInt<N>::Type entryTail = 0;
	typename UInt<N>::Type entryCount = 0;

	// data buffer
	uint8_t buffer[M];
	typename UInt<M>::Type bufferHead = 0;
};
