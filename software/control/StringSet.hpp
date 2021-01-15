#pragma once


/**
 * Alphabetically sorted set of strings
 * @tparam N maximum size
 * @tparam M array element
 */
template <int N, int M>
struct StringSet {
		
	StringSet() : length(0), dataLength(0) {}

	bool empty() {return this->length <= 0;}
	int size() {return this->length;}

	void clear() {
		this->length = 0;
		this->dataLength = 0;
	}
	
	/**
	 * Add an element to the set
	 * @param element to add
	 * @return true if there was enough space to add the element and the element didn't exist already
	 */
	bool add(String element) {
		// check if data has enough space for new element
		if (M - this->dataLength < element.length)
			return false;

		// find insert position
		int index = binaryLowerBound(element);
		
		// check if element is already in the set
		if (index < this->length && (*this)[index] == element)
			return false;
		
		// enlarge by one element if possible
		length = this->length;
		bool result = length < N;
		if (result)
			++length;
		if (index >= length)
			return false;
		this->length = length;

		// move elements
		for (int i = length - 1; i > index; --i) {
			this->elements[i] = this->elements[i - 1];
		}
		
		// set new element
		this->elements[index] = {uint16_t(this->dataLength), uint16_t(element.length)};
		memcpy(this->data + this->dataLength, element.data, element.length);
		this->dataLength += element.length;

		return result;
	}

	/**
	 * Determine the lower bound for the given element using binary search
	 * @param element element for which the lower bound is calculated
	 * @return index of lower bound, can be size() if the element is greater than the last element in the list
	 */
	int binaryLowerBound(String element) {
		int l = 0;
		int h = this->length;
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

	String const operator [](int index) const {
		assert(index >= 0 && index < this->length);
		Element const &element = this->elements[index];
		return {this->data + element.offset, element.length};
	}

	struct Element {
		uint16_t offset;
		uint16_t length;
	};

	struct Iterator {
		Element const *element;
		char const *data;
		
		void operator ++() {++this->element;}
		String operator *() const {return {this->data + element->offset, element->length};}
		bool operator !=(Iterator it) {return it.element != this->element;}
	};

	Iterator begin() const {return {this->elements, this->data};}
	Iterator end() const {return {this->elements + this->length, this->data};}

protected:

	Element elements[N];
	int length;
	char data[M];
	int dataLength;
};
