#pragma once

#include "String.hpp"


/**
 * Alphabetically sorted set of strings
 * @tparam N maximum number of elements
 * @tparam M size of buffer for string data of all elements, maximum is 65535
 */
template <int N, int M>
class StringSet {
    static_assert(M < 65536);

public:
    
    StringSet() = default;

	bool isEmpty() const {return this->elementCount <= 0;}
	
	int count() const {return this->elementCount;}

	void clear() {
		this->elementCount = 0;
		this->dataSize = 0;
	}
	
	/**
	 * Add an element to the set
	 * @param element to add
	 * @return true if there was enough space to add the element and the element didn't exist already
	 */
	bool add(String element) {
		// check if data has enough space for new element
		if (this->dataSize + element.count() > M)
			return false;

		// find insert position
		int index = binaryLowerBound(element);
		
		// check if element is already in the set
		if (index < this->elementCount && (*this)[index] == element)
			return false;
		
        // enlarge by one element and check if new element fits
        int elementCount = this->elementCount;
        if (elementCount >= N)
            return false;

		// move elements behind insert position to mace space for new element
		for (int i = elementCount; i > index; --i) {
			this->elements[i] = this->elements[i - 1];
		}
        this->elementCount = elementCount + 1;
        
		// add new element
		this->elements[index] = {uint16_t(element.count()), uint16_t(this->dataSize)};
		char *data = this->data + this->dataSize;
		array::copy(data, data + element.count(), element.data);
		this->dataSize += element.length;

		return true;
	}

	/**
	 * Determine the lower bound for the given element using binary search
	 * @param element element for which the lower bound is calculated
	 * @return index of the first element that is greater or equal to the given element. Returns count() if the element is greater than the last element in the set
	 */
	int binaryLowerBound(String element) {
		int l = 0;
		int h = this->elementCount;
		while (l < h) {
			int mid = l + (h - l) / 2;
			if ((*this)[mid] < element) {
				l = mid + 1;
			} else {
				h = mid;
			}
		}
		return l;
	}

	/**
	 * Index operator
	 */
	String const operator [](int index) const {
		assert(index >= 0 && index < this->elementCount);
		Element const &element = this->elements[index];
		return {element.length, this->data + element.offset};
	}

	struct Element {
		uint16_t length;
		uint16_t offset;
	};

	struct Iterator {
		Element const *element;
		char const *data;
		
		void operator ++() {++this->element;}
		String operator *() const {return {element->length, this->data + element->offset};}
		bool operator !=(Iterator it) {return it.element != this->element;}
	};

	Iterator begin() const {return {this->elements, this->data};}
	Iterator end() const {return {this->elements + this->elementCount, this->data};}

protected:

	// list of elements
    int elementCount = 0;
    Element elements[N];
	
    // string data
    int dataSize = 0;
	char data[M];
};
