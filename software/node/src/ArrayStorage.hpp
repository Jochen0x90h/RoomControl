#pragma once

#include "Flash.hpp"
#include <assert.hpp>
#include <defines.hpp>
#include <Pointer.hpp>
#include <util.hpp>

/**
 * Flash based storage of multiple arrays of elements with variable size
 */
class ArrayStorage {
public:

	// maximum number of elements for all arrays
	static constexpr int MAX_ELEMENT_COUNT = 512;
	//static constexpr int RAM_SIZE = 16384;


	// delete copy constructor
	ArrayStorage(ArrayStorage const &) = delete;

	/**
	 * Constructor
	 * @param pageStart first flash page to use
	 * @param pageCount number of flash pages to use, at least 2
	 * @param arrays arrays that use this flash storage
	 */
	template <typename ...Arrays>
	ArrayStorage(uint8_t pageStart, uint8_t pageCount, Arrays &...arrays) {
		assert(pageStart >= 0 && pageStart + pageCount <= FLASH_PAGE_COUNT);
		assert(pageCount >= 2);
		this->pageStart = pageStart;

		// we only use half of the pages and copy to the other half if they are full
		this->pageCount = pageCount / 2;

		add(arrays...);

		init();
	}

protected:

	template <typename T>
	void add(T &array) {
		array.data.storage = this;
		array.data.next = this->first;
		this->first = &array.data;
		array.data.index = this->arrayCount++;
		array.data.count = 0;
		array.data.sizeElement = &sizeElement<typename T::FlashType>;
		array.data.allocateElement = &allocateElement<typename T::FlashType>;
		array.data.elements = this->elements;
	}

	template <typename T, typename... Arrays>
	void add(T &array, Arrays &...arrays) {
		add(array);
		add(arrays...);
	}

	/**
	 * Initialize Storage
	 */
	void init();

public:

	/**
	 * Return true if there is space available for the requested size
	 * @param elementCount
	 * @param byteSize
	 */
	//bool hasSpace(int elementCount, int byteSize) const;


	static constexpr int flashAlign(int size) {
		// use at least 4 so that 32 bit types such as int are 4 byte aligned
		int const align = FLASH_WRITE_ALIGN < 4 ? 4 : FLASH_WRITE_ALIGN;
		return (size + align - 1) & ~(align - 1);
	}

	static constexpr int ramAlign(int size) {
		return (size + 3) >> 2;
	}

	class ElementInternal {
	public:
		void const *flash;
	};

	template <typename T>
	class Element : public ElementInternal {
	public:
		using FlashType = T;

		Element(FlashType const &flash) {this->flash = &flash;}
		Element(Element const &) = delete;

		/**
		 * Dereference operator. Don't store the pointer or keep it during co_await because it may change
		 */
		T const *operator ->() {return reinterpret_cast<T const *>(this->flash);}

		/**
		 * Indirection operator. Don't store the reference or keep it during co_await because it may change
		 */
		T const &operator *() const {return *reinterpret_cast<T const *>(this->flash);}
	};

protected:

	enum class Op : uint8_t {
		// todo: first flash header contains version
		VERSION = 0x55,

		// overwrite elements
		OVERWRITE = 0x56,

		// erase elements
		ERASE = 0x59,

		// move an element to a different index
		MOVE = 0x5a,
	};

	struct FlashHeader {
		// index of array to modify
		uint8_t arrayIndex;

		// element index to modify
		uint8_t index;

		// operation dependent value
		// OVERWRITE, ERASE: element count
		// MOVE: destination index
		uint8_t value;

		// operation (is last so that it can be used to check if the header was fully written to flash)
		Op op;
	};

	// size of FlashHeader should be 4
	static_assert(sizeof(FlashHeader) == 4);

	struct ArrayData {
		ArrayStorage *storage;

		// next array in a linked list
		ArrayData *next;

		// index of array
		uint8_t index;

		// number of elements in array
		uint8_t count;

		// function to determine the flash size of an element
		int (*sizeElement)(void const *flashElement);

		// function to allocate state in ram
		ElementInternal *(*allocateElement)(void const *flashElement);

		// elements
		//void const **flashElements;
		ElementInternal **elements;

		void enlarge(int count);
		void shrink(int index, int count);
		bool hasSpace(void const *flashElement);
		ElementInternal *write(int index, ElementInternal *element);
		ElementInternal *erase(int index);
		void move(int oldIndex, int newIndex);
	};

	// get flash size of element
	template <typename T>
	static int sizeElement(void const *flashElement) {
		return reinterpret_cast<const T*>(flashElement)->size();
	}

	// allocate element in ram
	template <typename T>
	static ElementInternal *allocateElement(void const *flashElement) {
		return reinterpret_cast<const T*>(flashElement)->allocate();
	}

public:
	template <typename T>
	struct Iterator {
		ElementInternal **e;
		void operator ++() {++this->e;}
		T &operator *() const {return *static_cast<T *>(*this->e);}
		bool operator !=(Iterator it) {return it.e != this->e;}
	};

	/**
	 * Array of elements that is managed by the Storage class
	 * @tparam F element part that is stored in flash, needs getFlashSize() and allocate() methods
	 * @tparam R element part that is stored in ram (state)
	 */
	template <typename T>
	class Array {
	public:
		// element part that is stored in flash (e.g. permanent configuration)
		using FlashType = typename T::FlashType;

		// element part that is stored in ram (e.g. current state)
		using RamType = T;

		static_assert(sizeof(FlashType) <= FLASH_PAGE_SIZE - flashAlign(sizeof(FlashHeader)));


		Array() = default;
		Array(const Array &) = delete;

		/**
		 * Returns number of elements in the array
		 * @return number of elements
		 */
		int count() const {
			return this->data.count;
		}

		/**
		 * Returns true when empty
		 * @return true when empty
		 */
		bool isEmpty() const {
			return this->data.count == 0;
		}

		/**
		 * Index operator
		 * @param index element index
		 * @return element at given index
		 */
		T &operator[](int index) const {
			assert(index >= 0 && index < this->data.count);
			return *static_cast<RamType*>(this->data.elements[index]);
		}

		/**
		 * Returns true if there is enough space for a new element
		 * @param flashElement element to test
		 * @return true if space is available
		 */
		bool hasSpace(FlashType const *flashElement) {
			return this->data.hasSpace(flashElement);
		}

		/**
		 * Overwrite the flash storage of an element at the given index.
		 * @param index of element, must be in the range [0, size())
		 * @param flashElement element to store in flash (gets copied and other elements may be moved in flash)
		 */
		void write(int index, FlashType const &flashElement) {
			assert(index >= 0 && index < this->data.count);
			ElementInternal *element = this->data.elements[index];
			assert(element != nullptr);
			element->flash = &flashElement;
			this->data.write(index, element);
		}

		/**
		 * Overwrite or append an element at the given index
		 * @param index of element, must be in the range [0, size()]
		 * @param element element to store, element->flash is stored in flash (gets copied and other elements may be moved in flash)
		 */
		void write(int index, RamType *element) {
			assert(index >= 0 && index <= this->data.count);
			auto old = static_cast<RamType *>(this->data.write(index, element));
			delete old;
		}

		/**
		 * Overwrite or append an element at the given index and take ownership from the pointer.
		 * @param index of element, must be in the range [0, size()]
		 * @param element element to store, element->flash is stored in flash (gets copied and other elements may be moved in flash)
		 */
		void write(int index, Pointer<RamType> &&element) {
			assert(index >= 0 && index <= this->data.count);
			auto old = static_cast<RamType *>(this->data.write(index, element.ptr));
			delete old;

			// remove ownership from given element
			element.ptr = nullptr;
		}

		/**
		 * Erase the element at the given index
		 */
		void erase(int index) {
			assert(index >= 0 && index < this->data.count);
			auto old = static_cast<RamType *>(this->data.erase(index));
			delete old;
		}

		/**
		 * Move an element from an old index to a new index and move all elements in between by one index
		 * @param index index of element to move
		 * @param newIndex new index of the element
		 */
		void move(int index, int newIndex) {
			assert(index >= 0 && index < this->data.count);
			assert(newIndex >= 0 && newIndex < this->data.count);
			this->data.move(index, newIndex);
		}

		Iterator<RamType> begin() const {
			return {this->data.elements};
		}

		Iterator<RamType> end() const {
			return {this->data.elements + this->data.count};
		}

		ArrayData data;
	};

protected:

	void switchFlashRegions();

	// configuration
	uint8_t pageStart;
	uint8_t pageCount;
	uint8_t arrayCount = 0;

	// linked list of arrays
	ArrayData *first = nullptr;

	// total number of elements in all arrays
	int elementCount = 0;

	// flash pointers
	uint8_t const *it;
	uint8_t const *end;

	// accumulated size of all flash elements bytes
	int flashElementsSize = 0;

	// space for array elements of all arrays
	ElementInternal *elements[MAX_ELEMENT_COUNT];
};
