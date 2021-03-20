#pragma once

#include "flash.hpp"
#include <assert.hpp>
#include "defines.hpp"
#include "util.hpp"

/**
 * Flash based storage of multiple arrays of elements with variable size
 */
class Storage {
public:

	static constexpr int MAX_ELEMENT_COUNT = 256;
	static constexpr int RAM_SIZE = 16384;


	Storage(Storage const &) = delete;

	/**
	 * Constructor. First call this constructor, then constructors of arrays and then init()
	 * @param pageStart first flash page to use
	 * @param pageCount number of flash pages to use
	 */
	Storage(uint8_t pageStart, uint8_t pageCount) {
		pageCount >>= 1;
		this->pageStart = pageStart;
		this->pageCount = pageCount;
	}

	/**
	 * Initialize Storage
	 */
	void init();

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

protected:

	enum class Op : uint8_t {
		OVERWRITE = 0,
		ERASE = 1,
		MOVE = 2,
		INVALID = 0xff
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
		Storage *storage;

		// next array in a linked list
		ArrayData *next;
		
		// index of array
		uint8_t index;
		
		// element size divided by 4
		//uint8_t elementSize;
		
		// number of elements in array
		uint8_t count;
		
		// function to determine the size of an element
		int (*flashSize)(void const *flashElement);
		int (*ramSize)(void const *flashElement);

		// elements
		void const **flashElements;
		uint32_t **ramElements;

		void enlarge(int count);
		void shrink(int index, int count);
		bool hasSpace(void const *flashElement);
		bool write(int index, void const *flashElement, void const *ramElement);
		void erase(int index);
		void move(int oldIndex, int newIndex);
	};

	// get flash element size
	template <typename F>
	static int flashSize(const void *flashElement) {
		return reinterpret_cast<const F*>(flashElement)->getFlashSize();
	}

	// get ram element size
	template <typename F>
	static int ramSize(const void *flashElement) {
		return reinterpret_cast<const F*>(flashElement)->getRamSize();
	}

public:

	template <typename F, typename R>
	struct Element {
		F const *flash;
		R *ram;
	};

	template <typename F, typename R>
	struct Iterator {
		void const *const *f;
		uint32_t **r;
		void operator ++() {++this->f; ++this->r;}
		Element<F, R> operator *() const {return {reinterpret_cast<F const *>(*this->f), reinterpret_cast<R *>(*this->r)};}
		bool operator !=(Iterator it) {return it.f != this->f;}
	};

	/**
	 * Array of elements that is managed by the Storage class
	 * @tparam F structure that is stored in flash, needs a size() const method returning size in bytes
	 * @tparam R structure that is stored in ram, needs a size(F const &) const method returning size in bytes
	 */
	template <typename F, typename R>
	class Array {
	public:
		// type that is stored in flash (e.g. permanent configuration)
		using FlashType = F;
		
		// type that is stored in ram (e.g. current state)
		using RamType = R;


		Array(const Array &) = delete;

		/**
		 * Constructor. Needs a storage object that manages this array in flash and ram memory
		 * @param storage storage object
		 */
		Array(Storage &storage) {
			static_assert(sizeof(FlashType) <= FLASH_PAGE_SIZE - flashAlign(sizeof(FlashHeader)));
			
			this->data.storage = &storage;
			this->data.next = storage.first;
			storage.first = &this->data;
			this->data.index = storage.arrayCount++;
			this->data.count = 0;
			this->data.flashSize = &flashSize<FlashType>;
			this->data.ramSize = &ramSize<FlashType>;
			this->data.flashElements = storage.flashElements;
			this->data.ramElements = storage.ramElements;
		}

		/**
		 * Returns number of elements in the array
		 * @return number of elements
		 */
		int size() const {
			return this->data.count;
		}
				
		Element<FlashType, RamType> operator[](int index) const {
			assert(index >= 0 && index < this->data.count);
			FlashType const *f = reinterpret_cast<const FlashType*>(this->data.flashElements[index]);
			RamType *r = reinterpret_cast<RamType*>(this->data.ramElements[index]);
			return {f, r};
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
		 * Overwrite or append an element at the given index.
		 * The ram element is only resized and preserved if it is nullptr
		 * @param index of element, must be in the range [0, size()]
		 * @param flashElement element to store in flash
		 * @param ramElement element to store in ram, can be nullptr
		 * @return true if successful, false if out of memory
		 */
		bool write(int index, FlashType const *flashElement, RamType *ramElement = nullptr) {
			assert(index >= 0 && index <= this->data.count);
			return this->data.write(index, flashElement, ramElement);
		}

		/**
		 * Erase the element at the given index
		 */
		void erase(int index) {
			assert(index >= 0 && index < this->data.count);
			this->data.erase(index);
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
		
		Iterator<FlashType, RamType> begin() const {
			return {this->data.flashElements, this->data.ramElements};
			
		}
		Iterator<FlashType, RamType> end() const {
			return {this->data.flashElements + this->data.count, this->data.ramElements + this->data.count};
		}
/*
		// only for emulator to set initial configuration
		template <int N>
		void assign(const ELEMENT (&elements)[N]) {
			assert(this->data.count == 0);
			Storage *storage = this->data.storage;

			// enlarge array
			this->data.enlarge(N);
			
			// write header
			FlashHeader header = {this->data.index, uint8_t(0), uint8_t(N), Op::OVERWRITE};
			Flash::write(storage->it, &header, sizeof(Header));
			storage->it += align(sizeof(Header));
			
			// write elements to flash
			for (int i = 0; i < N; ++i) {
				const void *element = &elements[i];
				
				// update used memory size
				int elementSize = this->data.size(element);
				storage->elementsSize += elementSize;

				// set element in flash
				this->data.elements[i] = storage->it;

				// write element to flash
				Flash::write(storage->it, element, elementSize);
				storage->it += align(elementSize);
			}
		}
*/
		ArrayData data;
	};
		
protected:
	
	void switchFlashRegions();

	void ramInsert(uint32_t **ramElement, int sizeChange);
	
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
	
	// space for array elements of all arrays
	void const *flashElements[MAX_ELEMENT_COUNT];
	
	
	// accumulated size of all flash elements bytes
	int flashElementsSize = 0;


	// space for array elements of all arrays followed by an "end"-pointer
	uint32_t *ramElements[MAX_ELEMENT_COUNT + 1];

	// space for ram elements
	uint32_t ram[RAM_SIZE];
};
