#pragma once

#include "assert.hpp"

/**
 * Array list with given maximum size
 * @tparam T array element
 * @tparam N maximum size
 */
template <typename T, int N>
struct ArrayList {
		
	ArrayList() : length(0) {}

	bool empty() {return this->length <= 0;}
	int size() {return this->length;}

	void clear() {
		this->length = 0;
	}

	/**
	 * Add an element to the list
	 * @param element to add
	 * @return true if there was enough space to add the element
	 */
	bool add(T const& element) {
		if (this->length >= N)
			return false;
		this->elements[this->length++] = element;
		return true;
	}

	/**
	 * Insert a new element
	 * @param index insert position
	 * @param element element to insert
	 * @return true if successful, false if index out of range or an element was shifted out at the end
	 */
	bool insert(int index, T const &element) {
		// enlarge by one element if possible
		int length = this->length;
		bool result = length < N;
		if (result)
			++length;
		if (index >= length)
			return false;
		this->length = length;

		// move elements
		for (int i = length - 1; i > index; --i)
			this->elements[i] = this->elements[i - 1];
		
		// set new element
		this->elements[index] = element;

		return result;
	}

	/**
	 * Determine the lower bound for the given element using binary search
	 * @param element element for which the lower bound is calculated
	 * @return index of lower bound, can be size() if the element is greater than the last element in the list
	 */
	int binaryLowerBound(T const &element) {
		int l = 0;
		int h = this->length;
		while (l < h) {
			int mid = l + (h - l) / 2;
			if (this->elements[mid] < element) {
				l = mid + 1;
			} else {
				h = mid;
			}
		}
		return l;
	}

	constexpr T &operator [](int index) {
		assert(index >= 0 && index < this->length);
		return this->elements[index];
	}
	constexpr T const &operator [](int index) const {
		assert(index >= 0 && index < this->length);
		return this->elements[index];
	}

	const T *begin() const {return this->elements;}
	const T *end() const {return this->elements + this->length;}

protected:

	T elements[N];
	int length;
};
