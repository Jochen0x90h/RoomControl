#pragma once

#include "String.hpp"


/**
 * Hash table for strings
 * @tparam N size of hash table
 * @tparam M maximum number of elements in hash table (set to e.g. 3/4 of N)
 * @tparam B size of buffer for string data of all keys, maximum is 262144
 * @tparam V value type
 */
template <int N, int M, int B, typename V>
class StringHash {
	static constexpr int OFFSET_SHIFT = 14;
	static constexpr int LENGTH_MASK = ~(0xffffffff << OFFSET_SHIFT);

    static_assert(B <= 1 << (32 - OFFSET_SHIFT));

public:

	struct Element {
		// key (string length and offset of string data)
		uint32_t key;

		// value
		V value;
	};

	struct KeyValue {
		String const key;
		V &value;

		KeyValue *operator ->() {return this;}
	};

	/**
	 * Iterator of the string hash table. Use it->key and it->value to access key and value or for (auto keyValue : hash) {...}
	 */
	struct Iterator {
		Element *element;
		char *data;

		KeyValue operator *() {
			auto key = this->element->key;
			return {{int(key & LENGTH_MASK), this->data + (key >> OFFSET_SHIFT)}, this->element->value};
		}
		KeyValue operator ->() {
			auto key = this->element->key;
			return {{int(key & LENGTH_MASK), this->data + (key >> OFFSET_SHIFT)}, this->element->value};
		}

		bool operator ==(Iterator const &b) const {
			return this->element == b.element;
		}

		Iterator& operator ++() {
			auto element = this->element;
			do {
				++element;
			} while (element->key == 0xffffffff);
			this->element = element;
			return *this;
		}

		//int operator -(Iterator const &b) const {
		//	return this->element - b.element;
		//}
	};


	StringHash() {clear();}

	bool isEmpty() const {return this->elementCount <= 0;}

	int count() const {return this->elementCount;}

	void clear() {
		this->elementCount = 0;
		for (Element &element : this->hashTable) {
			element.key = 0xffffffff;
		}
		this->dataSize = 0;
	}

	/**
	 * Find a value by key string
	 * @param key key string
	 * @return iterator to the element or end() if not found
	 */
	Iterator find(String const &key) {
		int index = key.hash() % N;

		// search for the key in the hash table
		Element *element;
		while ((element = &this->hashTable[index])->key != 0xffffffff) {
			auto k = element->key;
			String str(k & LENGTH_MASK, this->data + (k >> OFFSET_SHIFT));
			if (str == key) {
				// found element
				return {element, this->data};
			}
			++index;
			if (index == N)
				index = 0;
		}
		return end();
	}

	/**
	 * Locate a key string in the hash table
	 * @param key key string
	 * @return location in hash table or -1 if not found
	 */
	int locate(String const &key) {
		int index = key.hash() % N;

		// search for the key in the hash table
		Element *element;
		while ((element = &this->hashTable[index])->key != 0xffffffff) {
			auto k = element->key;
			String str(k & LENGTH_MASK, this->data + (k >> OFFSET_SHIFT));
			if (str == key) {
				// found element
				return index;//{element, this->data};
			}
			++index;
			if (index == N)
				index = 0;
		}
		return -1;//end();
	}

	/**
	 * Gat the value for a key string or put it if not found
	 * @param key key string
	 * @param defaultValue function that obtains the default value if a new key was inserted
	 * @return location in hash table or -1 if not found and no new element could be added
	 */
	template <typename F>
	int getOrPut(String const &key, F const &defaultValue) {
		int index = key.hash() % N;

		// search for the key in the hash table
		Element *element;
		while ((element = &this->hashTable[index])->key != 0xffffffff) {
			auto k = element->key;
			String str(k & LENGTH_MASK, this->data + (k >> OFFSET_SHIFT));
			if (str == key) {
				// found element
				return index;//{element, this->data};
			}
			++index;
			if (index == N)
				index = 0;
		}

		// not found: check if new key will fit
		if (this->elementCount >= M || this->dataSize + key.count() > B)
			return -1;//end();
		++this->elementCount;

		// set key
		element->key = key.count() + (this->dataSize << OFFSET_SHIFT);

		// add key string to data
		array::copy(key.count(), this->data + this->dataSize, key.data);
		this->dataSize += key.count();

		element->value = defaultValue();//(index);

		return index;//{element, this->data};
	}

	bool isValid(int index) {
		return uint32_t(index) < N && this->hashTable[index].key != 0xffffffff;
	}

	V &operator [](int index) {
		assert(uint32_t(index) < N);
		return this->hashTable[index].value;
	}

	Iterator get(int index) {
		return {this->hashTable + index, this->data};
	}

	Iterator begin() {
		auto element = &this->hashTable[0];
		while (element->key == 0xffffffff) {
			++element;
		}
		return {element, this->data};
	}
	Iterator end() {return {&this->hashTable[N], this->data};}

protected:

	// list of elements
    int elementCount;
    Element hashTable[N];
	uint32_t endKey = 0;

    // string data
    int dataSize;
	char data[B];
};
