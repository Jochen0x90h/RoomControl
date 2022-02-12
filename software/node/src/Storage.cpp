#include "util.hpp"
#include "Storage.hpp"


// Storage::ArrayData

void Storage::ArrayData::enlarge(int count) {
	// update number of elements of this array and of storage
	this->count += count;
	this->storage->elementCount += count;

	// move elements
	{
		// find range of ram elements of following arrays to move
		ElementInternal **begin = this->elements + this->count;
		ElementInternal **it = begin;
		ArrayData *arrayData = this;
		while ((arrayData = arrayData->next) != nullptr) {
			arrayData->elements = it;
			it += arrayData->count;
		}

		// move ram elements of following arrays
		while (it != begin) {
			--it;
			it[0] = it[-count];
		}
	}
}

void Storage::ArrayData::shrink(int index, int count) {
	ElementInternal **elements = this->elements;

	// update number of elements of this array and of storage
	this->count -= count;
	this->storage->elementCount -= count;

	// erase from this array
	for (int i = index; i < this->count; ++i) {
		elements[i] = elements[i + count];
	}

	// move elements
	{
		// find range of flash elements of following arrays to move
		ElementInternal **end = elements + this->count;
		ElementInternal **it = end + count;
		ArrayData *arrayData = this;
		while ((arrayData = arrayData->next) != nullptr) {
			arrayData->elements = end;
			end += arrayData->count;
		}

		// move elements of following arrays
		while (it < end) {
			it[-count] = it[0];
			++it;
		}
	}
}

bool Storage::ArrayData::hasSpace(void const *flashElement) {
	auto storage = this->storage;
	if (storage->elementCount >= MAX_ELEMENT_COUNT)
		return false;
	int flashSize = storage->flashElementsSize + storage->elementCount * 4
		+ this->sizeElement(flashElement);
	if (flashSize > storage->pageCount * (FLASH_PAGE_SIZE * 2 / 3))
		return false;
	//int ramSize = &storage->ram[RAM_SIZE] - storage->ramElements[storage->elementCount]
	//	+ this->ramSize(flashElement);
	//if (ramSize > RAM_SIZE)
	//	return false;
	return true;
}

void *Storage::ArrayData::write(int index, ElementInternal *element) {
	auto storage = this->storage;
	void const *flashElement = element->flash;

	// element sizes
	int flashElementSize = this->sizeElement(flashElement);
	int flashSizeChange = flashElementSize;

	ElementInternal *oldElement = nullptr;

	// automatically enlarge array by one element if write at end
	if (index != this->count) {
		// subtract size of old element
		oldElement = this->elements[index];
		flashSizeChange -= this->sizeElement(oldElement->flash);
	}

	// check if new element will fit
	int flashSize = storage->flashElementsSize + storage->elementCount * 4 + flashSizeChange;
	if (flashSize > storage->pageCount * (FLASH_PAGE_SIZE * 2 / 3)) {
		// no: don't modify array and return new element which will be deleted
		return element;
	}

	// automatically enlarge array by one element if write at end
	if (index == this->count) {
		// check if new element will fit
		if (storage->elementCount >= MAX_ELEMENT_COUNT) {
			// no: don't modify array and return ram element which will be deleted
			return element;
		}
		enlarge(1);
	}
	storage->flashElementsSize += flashSizeChange;

	// set ram element
	this->elements[index] = element;

	// check if there is enough flash space in current page set
	int alignedFlashHeaderSize = flashAlign(sizeof(FlashHeader));
	int alignedFlashElementSize = flashAlign(flashElementSize);
	if (storage->it + alignedFlashHeaderSize + alignedFlashElementSize <= storage->end) {
		// yes: write header to flash
		FlashHeader flashHeader = {this->index, uint8_t(index), uint8_t(1), Op::OVERWRITE};
		flash::write(storage->it, sizeof(FlashHeader), &flashHeader);
		storage->it += alignedFlashHeaderSize;

		// set element in flash
		element->flash = storage->it;
		//this->elements[index]->flash = storage->it;

		// write element to flash
		flash::write(storage->it, flashElementSize, flashElement);
		storage->it += alignedFlashElementSize;
	} else {
		// no: set flash element, gets copied to flash in switchFlashRegions()
		//this->elements[index]->flash = flashElement;

		// no: defragment flash, element->flash also gets copied to flash
		storage->switchFlashRegions();
	}

	// todo: report out of memory
	return oldElement;
}

void *Storage::ArrayData::erase(int index) {
	auto storage = this->storage;

	ElementInternal *oldElement = this->elements[index];

	// element sizes
	int alignedFlashHeaderSize = flashAlign(sizeof(FlashHeader));
	storage->flashElementsSize -= this->sizeElement(oldElement->flash);

	// check if there is enough flash space for a header
	if (storage->it + alignedFlashHeaderSize <= storage->end) {
		// write header to flash
		FlashHeader header = {this->index, uint8_t(index), uint8_t(1), Op::ERASE};
		flash::write(storage->it, sizeof(FlashHeader), &header);
		storage->it += alignedFlashHeaderSize;
	} else {
		// defragment flash
		storage->switchFlashRegions();
	}

	// shrink allocated size of this array by one
	shrink(index, 1);

	return oldElement;
}

void Storage::ArrayData::move(int index, int newIndex) {
	if (index == newIndex)
		return;

	Storage *storage = this->storage;

	ElementInternal **e = this->elements;
	ElementInternal *element = e[index];

	// element sizes
	int alignedFlashHeaderSize = flashAlign(sizeof(FlashHeader));

	// move element to new index and elements in between by one
	if (newIndex > index) {
		for (int i = index; i < newIndex; ++i) {
			e[i] = e[i + 1];
		}
	} else {
		for (int i = index; i > newIndex; --i) {
			e[i] = e[i - 1];
		}
	}
	e[newIndex] = element;

	// check if there is enough space for a header
	if (storage->it + alignedFlashHeaderSize <= storage->end) {
		// yes: write header to flash
		FlashHeader header = {this->index, uint8_t(index), uint8_t(newIndex), Op::MOVE};
		flash::write(storage->it, sizeof(FlashHeader), &header);
		storage->it += alignedFlashHeaderSize;
	} else {
		// no: defragment flash
		storage->switchFlashRegions();
	}
}


// Storage

void Storage::init() {
	// detect current flash region by checking if op of first header is valid
	int pageCount = this->pageCount;
	const uint8_t *p = flash::getAddress(this->pageStart + pageCount);
	int clearStart;
	uint8_t op = p[3];
	if (((op ^ (op >> 1)) & 0x55) != 0x55) {
		// first flash region is current
		this->it = p - pageCount * FLASH_PAGE_SIZE;
		this->end = p;
		clearStart = pageStart + this->pageCount;
	} else {
		// second flash region is current
		this->it = p;
		this->end = p + pageCount * FLASH_PAGE_SIZE;
		clearStart = this->pageStart;
	}

	// ensure that the other flash region is empty
	for (int i = 0; i < pageCount; ++i) {
		if (!flash::isEmpty(clearStart + i))
			flash::erase(clearStart + i);
	}

	// initialize arrays by collecting the elements
	const uint8_t *it = this->it;
	while (it < this->end) {
		// get header
		const FlashHeader *header = reinterpret_cast<const FlashHeader *>(it);
		it += flashAlign(sizeof(FlashHeader));

		// get array by index
		ArrayData *arrayData = this->first;
		int arrayIndex = this->arrayCount - 1;
		while (arrayIndex > header->arrayIndex) {
			--arrayIndex;
			arrayData = arrayData->next;
		};
		void const **flashElements = (void const **)(arrayData->elements);

		// execute operation
		Op op = header->op;
		int index = header->index;
		if (op == Op::OVERWRITE) {
			// overwrite array elements, may enlarge the array
			int count = header->value;

			// check if we have to enlarge the array
			int newCount = index + count;
			if (newCount > arrayData->count) {
				arrayData->enlarge(newCount - arrayData->count);
			}

			// set elements
			for (int i = 0; i < count; ++i) {
				flashElements[index + i] = it;
				it += flashAlign(arrayData->sizeElement(it));
			}
		} else if (op == Op::ERASE) {
			// erase array elements
			int count = header->value;
			arrayData->shrink(index, count);
		} else if (op == Op::MOVE) {
			// move element
			int newIndex = header->value;
			const void **f = flashElements;
			const void *flashElement = f[index];
			if (index < newIndex) {
				for (int i = index; i < newIndex; ++i) {
					f[i] = f[i + 1];
				}
			} else {
				for (int i = index; i > newIndex; --i) {
					f[i] = f[i - 1];
				}
			}
			f[newIndex] = flashElement;
		} else {
			// invalid operation
			it -= flashAlign(sizeof(FlashHeader));
			break;
		}
	}
	this->it = it;

	// check if remaining flash is empty
	while (it < this->end) {
		if (*it != 0xff) {
			// remaining flash is not empty, try to fix by swapping flash pages
			switchFlashRegions();
			break;
		}
		++it;
	}

	// calculate accumulated size of all flash elements and allocate ram
	this->flashElementsSize = 0;
	ElementInternal **elements = this->elements;
	ArrayData *arrayData = this->first;
	while (arrayData != nullptr) {
		void const **flashElements = (void const **)(arrayData->elements);
		arrayData->elements = elements;

		// iterate over array elements
		for (int i = 0; i < arrayData->count; ++i) {
			auto flashElement = flashElements[i];
			this->flashElementsSize += arrayData->sizeElement(flashElement);
			elements[i] = arrayData->allocateElement(flashElement);
		}
		elements += arrayData->count;
		arrayData = arrayData->next;
	};
}

/*bool Storage::hasSpace(int elementCount, int byteSize) const {
	return this->elementCount + elementCount <= ::size(this->elements)
		&& (this->elementCount + 1) * (align(HEADER_SIZE) + WRITE_ALIGN) + this->elementsSize + byteSize < this->pageCount * FLASH_PAGE_SIZE;
}*/

void Storage::switchFlashRegions() {
	// switch flash regions
	int pageCount = this->pageCount;
	const uint8_t *p = flash::getAddress(this->pageStart + pageCount);
	int clearStart;
	int clearStart2;
	if (this->end > p) {
		// first region becomes current
		this->it = p - FLASH_PAGE_COUNT + FLASH_PAGE_SIZE;
		this->end = p;
		clearStart = this->pageStart;
		clearStart2 = this->pageStart + pageCount;
	} else {
		// second region becomes current
		this->it = p;
		this->end = p + FLASH_PAGE_COUNT + FLASH_PAGE_SIZE;
		clearStart = this->pageStart + pageCount;
		clearStart2 = this->pageStart;
	}

	// ensure that the new flash region is empty
	for (int i = 0; i < pageCount; ++i) {
		if (!flash::isEmpty(clearStart + i))
			flash::erase(clearStart + i);
	}

	// write arrays to new flash region
	this->elementCount = 0;
	this->flashElementsSize = 0;
	ArrayData *arrayData = this->first;
	const uint8_t *it = this->it;
	while (arrayData != nullptr) {
		// write header for whole array (except first header)
		if (arrayData != this->first) {
			FlashHeader header = {arrayData->index, 0, arrayData->count, Op::OVERWRITE};
			flash::write(it, sizeof(FlashHeader), &header);
		}
		it += flashAlign(sizeof(FlashHeader));

		// write array data
		for (int i = 0; i < arrayData->count; ++i) {
			void const *flashElement = arrayData->elements[i]->flash;
			int elementSize = arrayData->sizeElement(flashElement);
			arrayData->elements[i]->flash = it;
			flash::write(it, elementSize, flashElement);
			it += flashAlign(elementSize);

			// update element count and accumulated size
			++this->elementCount;
			this->flashElementsSize += elementSize;
		}

		// next array
		arrayData = arrayData->next;
	}

	// write first header last to make new flash region valid
	FlashHeader header = {this->first->index, 0, this->first->count, Op::OVERWRITE};
	flash::write(this->it, sizeof(FlashHeader), &header);

	// erase old flash region
	for (int i = 0; i < pageCount; ++i) {
		if (!flash::isEmpty(clearStart2 + i))
			flash::erase(clearStart2 + i);
	}

	this->it = it;
}
