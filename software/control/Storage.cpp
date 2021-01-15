#include "util.hpp"
#include "Flash.hpp"
#include "Storage.hpp"


// Storage::ArrayData

void Storage::ArrayData::enlarge(int count) {
	// update number of elements of this array and of all arrays
	this->count += count;
	this->storage->elementCount += count;

	// flash
	{
		// find range of flash elements of following arrays to move
		void const **begin = this->flashElements + this->count;
		void const **it = begin;
		ArrayData *arrayData = this;
		while ((arrayData = arrayData->next) != nullptr) {
			arrayData->flashElements = it;
			it += arrayData->count;
		}
		
		// move flash elements of following arrays
		while (it != begin) {
			--it;
			it[0] = it[-count];
		}
	}
	
	// ram
	{
		// find range of ram elements of following arrays to move
		uint32_t **begin = this->ramElements + this->count;
		uint32_t **it = begin;
		ArrayData *arrayData = this;
		while ((arrayData = arrayData->next) != nullptr) {
			arrayData->ramElements = it;
			it += arrayData->count;
		}
		
		// set "end" pointer behind last element pointer
		it[0] = it[-count];

		// move ram elements of following arrays
		while (it != begin) {
			--it;
			it[0] = it[-count];
		}
	}
}

void Storage::ArrayData::shrink(int count) {
	// flash
	{
		// find range of flash elements of following arrays to move
		void const **end = this->flashElements + this->count;
		void const **it = end;
		ArrayData *arrayData = this;
		while ((arrayData = arrayData->next) != nullptr) {
			arrayData->flashElements = end;
			end += arrayData->count;
		}
		
		// move elements of following arrays
		while (it != end) {
			it[-count] = it[0];
			++it;
		}
	}
	
	// ram
	{
		// find range of flash elements of following arrays to move
		uint32_t **end = this->ramElements + this->count;
		uint32_t **it = end;
		ArrayData *arrayData = this;
		while ((arrayData = arrayData->next) != nullptr) {
			arrayData->ramElements = end;
			end += arrayData->count;
		}
		
		// move elements of following arrays
		while (it != end) {
			it[-count] = it[0];
			++it;
		}

		// set "end" pointer behind last element pointer
		it[-count] = it[0];
	}
	
	// update number of elements of this array and of all arrays
	this->count -= count;
	this->storage->elementCount -= count;
}
/*
void Storage::ArrayData::write(int op, int index, int value) {
	ArrayData *arrayData = this;
	Storage *storage = this->storage;
	
	// calc size of elements (in 32 bit units)
	int size = 1; // for header
	for (int i = 0; i < value; ++i) {
		size += (arrayData->byteSize(arrayData->elements[index + i]) + 3) >> 2;
	}
	
	// check if there is enough space
	//int size = 1 + (op == OVERWRITE ? arrayData->elementSize * value : 0);
	if (storage->it <= storage->end - size) {
		// write header to flash
		Header header = {arrayData->index, uint8_t(index), uint8_t(value), uint8_t(op)};
		Flash::write(storage->it, reinterpret_cast<uint32_t*>(&header), 1);
		++storage->it;
		
		// write data to flash
		if (op == OVERWRITE) {
			for (int i = 0; i < value; ++i) {
				int elementSize = (arrayData->byteSize(arrayData->elements[index + i]) + 3) >> 2;
				Flash::write(storage->it, arrayData->elements[index + i], elementSize);
				arrayData->elements[index + i] = storage->it;
				storage->it += elementSize;
			}
		}
	} else {
		storage->switchFlashRegions();
	}
}
*/

bool Storage::ArrayData::hasSpace(void const *flashElement) {
	Storage *storage = this->storage;
	if (storage->elementCount >= MAX_ELEMENT_COUNT)
		return false;
	int flashSize = storage->flashElementsSize + storage->elementCount * 4
		+ this->flashSize(flashElement);
	if (flashSize > storage->pageCount * (FLASH_PAGE_SIZE * 2 / 3))
		return false;
	int ramSize = &storage->ram[RAM_SIZE] - storage->ramElements[storage->elementCount]
		+ this->ramSize(flashElement);
	if (ramSize > RAM_SIZE)
		return false;
	return true;
}

bool Storage::ArrayData::write(int index, void const *flashElement, void const* ramElement) {
	Storage *storage = this->storage;

	// element sizes
	int flashElementSize = this->flashSize(flashElement);
	int flashSizeChange = flashElementSize;

	int ramElementSize = this->ramSize(flashElement);
	int oldRamElementSize = 0;

	// automatically enlarge array by one element if write at end
	if (index != this->count) {
		// subtract size of old element
		flashSizeChange -= this->flashSize(this->flashElements[index]);
		oldRamElementSize = this->ramSize(this->flashElements[index]);
	}
	int ramSizeChange = ramAlign(ramElementSize) - ramAlign(oldRamElementSize);

	// check if new element will fit
	int flashSize = storage->flashElementsSize + storage->elementCount * 4 + flashSizeChange;
	if (flashSize > storage->pageCount * (FLASH_PAGE_SIZE * 2 / 3))
		return false;
	int ramSize = storage->ramElements[storage->elementCount] - storage->ram + ramSizeChange;
	if (ramSize > RAM_SIZE)
		return false;

	if (index == this->count) {
		if (storage->elementCount >= MAX_ELEMENT_COUNT)
			return false;
		enlarge(1);
	}
	storage->flashElementsSize += flashSizeChange;

	// check if there is enough flash space
	int alignedFlashHeaderSize = flashAlign(sizeof(FlashHeader));
	int alignedFlashElementSize = flashAlign(flashElementSize);
	if (storage->it + alignedFlashHeaderSize + alignedFlashElementSize <= storage->end) {
		// write header to flash
		FlashHeader flashHeader = {this->index, uint8_t(index), uint8_t(1), Op::OVERWRITE};
		Flash::write(storage->it, &flashHeader, sizeof(FlashHeader));
		storage->it += alignedFlashHeaderSize;
		
		// set element in flash
		this->flashElements[index] = storage->it;

		// write element to flash
		Flash::write(storage->it, flashElement, flashElementSize);
		storage->it += alignedFlashElementSize;
	} else {
		// set element, gets copied to flash in switchFlashRegions()
		this->flashElements[index] = flashElement;
		
		// defragment flash
		storage->switchFlashRegions();
	}
	
	// reallocate ram
	storage->ramInsert(this->ramElements + index, ramSizeChange);

	// write ram element
	uint8_t *dst = reinterpret_cast<uint8_t*>(this->ramElements[index]);
	if (ramElement != nullptr) {
		uint8_t const *src = reinterpret_cast<uint8_t const*>(ramElement);
		for (int i = 0; i < ramElementSize; ++i) {
			dst[i] = src[i];
		}
	} else {
		for (int i = oldRamElementSize; i < ramElementSize; ++i) {
			dst[i] = 0;
		}
	}
	
	// todo: report out of memory
	return true;
}

void Storage::ArrayData::erase(int index) {
	Storage *storage = this->storage;

	void const **f = this->flashElements;
	uint32_t **r = this->ramElements;

	// element sizes
	int alignedFlashHeaderSize = flashAlign(sizeof(FlashHeader));
	storage->flashElementsSize -= this->flashSize(f[index]);
	int alignedRamElementSize = r[index + 1] - r[index];

	// erase element
	for (int i = index; i < this->count - 1; ++i) {
		f[i] = f[i + 1];
		r[i] = r[i + 1];
	}

	// shrink allocated size of this array by one
	shrink(1);

	// check if there is enough flash space for a header
	if (storage->it + alignedFlashHeaderSize <= storage->end) {
		// write header to flash
		FlashHeader header = {this->index, uint8_t(index), uint8_t(1), Op::ERASE};
		Flash::write(storage->it, &header, sizeof(FlashHeader));
		storage->it += alignedFlashHeaderSize;
	} else {
		// defragment flash
		storage->switchFlashRegions();
	}
	
	// move ram
	storage->ramInsert(r + index, -alignedRamElementSize);
}

void Storage::ArrayData::move(int index, int newIndex) {
	if (index == newIndex)
		return;
		
	Storage *storage = this->storage;
	
	void const **f = this->flashElements;
	void const *flashElement = f[index];
	
	uint32_t **r = this->ramElements;
	uint32_t *ramElement = r[index];
	uint32_t *nextRamElement = r[index + 1];

	// element sizes
	int alignedFlashHeaderSize = flashAlign(sizeof(FlashHeader));
	int alignedRamElementSize = nextRamElement - ramElement;

	// move element to new index and elements in between by one
	if (newIndex > index) {
		// move ram (to lower addresses)
		uint32_t *src = nextRamElement;
		uint32_t *dst = ramElement;
		uint32_t *end = r[newIndex + 1];
		while (src != end) {
			*dst = *src;
			++src, ++dst;
		}

		for (int i = index; i < newIndex; ++i) {
			f[i] = f[i + 1];
			r[i] = r[i + 1] - alignedRamElementSize;
		}
		r[newIndex] = r[newIndex + 1] - alignedRamElementSize;

	} else {
		// move ram (to higher addresses)
		uint32_t *src = ramElement;
		uint32_t *dst = nextRamElement;
		uint32_t *begin = this->ramElements[newIndex];
		while (src != begin) {
			--src, --dst;
			*dst = *src;
		}
		
		for (int i = index; i > newIndex; --i) {
			f[i] = f[i - 1];
			r[i] = r[i - 1] + alignedRamElementSize;
		}
	}
	f[newIndex] = flashElement;

	// check if there is enough space for a header
	if (storage->it + alignedFlashHeaderSize <= storage->end) {
		// write header to flash
		FlashHeader header = {this->index, uint8_t(index), uint8_t(newIndex), Op::MOVE};
		Flash::write(storage->it, &header, sizeof(FlashHeader));
		storage->it += alignedFlashHeaderSize;
	} else {
		// defragment flash
		storage->switchFlashRegions();
	}
}


// Storage

void Storage::init() {
	// detect current pages
	const uint8_t *p = Flash::getAddress(this->pageStart);
	int pageStart = this->pageStart;
	int pageCount = this->pageCount;
	int pageStart2 = pageStart + pageCount;
	
	// check if op of first header is valid
	if (p[3] != 0xff) {
		// first flash region is current
		this->it = p;
		this->end = Flash::getAddress(pageStart2);
		
		// ensure that second flash region is empty
		for (int i = 0; i < pageCount; ++i) {
			if (!Flash::isEmpty(pageStart2 + i))
				Flash::erase(pageStart2 + i);
		}
	} else {
		// second flash region is current
		this->it = Flash::getAddress(pageStart2);
		this->end = Flash::getAddress(pageStart2 + pageCount);

		// ensure that first flash region is empty
		for (int i = 0; i < pageCount; ++i) {
			if (!Flash::isEmpty(pageStart + i))
				Flash::erase(pageStart + i);
		}
	}

	// initialize arrays by collecting the elements 
	const uint8_t *it = this->it;
	while (it < this->end) {
		// get header
		const FlashHeader *header = reinterpret_cast<const FlashHeader *>(it);
		
		// stop if header is invalid
		if (header->op == Op::INVALID)
			break;
		it += flashAlign(sizeof(FlashHeader));
		
		// get array by index
		ArrayData *arrayData = this->first;
		int arrayIndex = this->arrayCount - 1;
		while (arrayIndex > header->arrayIndex) {
			--arrayIndex;
			arrayData = arrayData->next;
		};
		
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
				arrayData->flashElements[index + i] = it;
				it += flashAlign(arrayData->flashSize(it));
			}
		} else if (op == Op::ERASE) {
			// erase array elements
			int count = header->value;
			for (int i = index; i < arrayData->count - count; ++i) {
				arrayData->flashElements[i] = arrayData->flashElements[i + count];
			}
			arrayData->shrink(count);
		} else if (op == Op::MOVE) {
			// move element
			int newIndex = header->value;
			const void **f = arrayData->flashElements;
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
		}
	}
	this->it = it;
	
	// calculate accumulated size of all flash elements and allocate ram
	this->flashElementsSize = 0;
	uint32_t **ramElements = this->ramElements;
	uint32_t *ram = this->ram;
	ArrayData *arrayData = this->first;
	while (arrayData != nullptr) {
		arrayData->ramElements = ramElements;
		
		// iterate over array elements
		for (int i = 0; i < arrayData->count; ++i) {
			this->flashElementsSize += arrayData->flashSize(arrayData->flashElements[i]);
		
			ramElements[i] = ram;
			ram += ramAlign(arrayData->ramSize(arrayData->flashElements[i]));
		}
		ramElements += arrayData->count;
		arrayData = arrayData->next;
	};
	
	// write "end" pointer behint last ram element pointer
	ramElements[0] = ram;
	
	// clear ram
	for (uint32_t *it = this->ram; it != ram; ++it) {
		*it = 0;
	}
}

/*bool Storage::hasSpace(int elementCount, int byteSize) const {
	return this->elementCount + elementCount <= ::size(this->elements)
		&& (this->elementCount + 1) * (align(HEADER_SIZE) + WRITE_ALIGN) + this->elementsSize + byteSize < this->pageCount * FLASH_PAGE_SIZE;
}*/

void Storage::switchFlashRegions() {
	// switch flash regions
	const uint8_t *p = Flash::getAddress(this->pageStart);
	uint8_t pageStart2 = this->pageStart + this->pageCount;
	if (p[3] != 0xff) {
		// second region becomes current
		this->it = Flash::getAddress(pageStart2);
		this->end = Flash::getAddress(pageStart2 + this->pageCount);
	} else {
		// first region becomes current
		this->it = p;
		this->end = Flash::getAddress(pageStart2);
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
			Flash::write(it, &header, sizeof(FlashHeader));
		}
		it += flashAlign(sizeof(FlashHeader));

		// write array data
		for (int i = 0; i < arrayData->count; ++i) {
			int elementSize = arrayData->flashSize(arrayData->flashElements[i]);
			arrayData->flashElements[i] = it;
			Flash::write(it, arrayData->flashElements[i], elementSize);
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
	Flash::write(this->it, &header, sizeof(FlashHeader));
	
	// erase old flash region
	if (this->it != p) {
		// erase first region
		for (int i = 0; i < this->pageCount; ++i) {
			Flash::erase(this->pageStart + i);
		}
	} else {
		// erase second region
		for (int i = 0; i < this->pageCount; ++i) {
			Flash::erase(pageStart2 + i);
		}
	}

	this->it = it;
}


void Storage::ramInsert(uint32_t **ramElement, int sizeChange) {
	if (sizeChange == 0)
		return;
		
	uint32_t **e = this->ramElements + this->elementCount;
	
	// move ram contents
	if (sizeChange > 0) {
		uint32_t *src = *e;
		uint32_t *dst = src + sizeChange;
		uint32_t *begin = *ramElement;
		while (src != begin) {
			--src;
			--dst;
			*dst = *src;
		}
	} else {
		uint32_t *src = *ramElement - sizeChange;
		uint32_t *dst = *ramElement;
		uint32_t *end = *e;
		for (; src != end; ++src, ++dst) {
			*dst = *src;
		}
	}

	// update ram element pointers
	for (uint32_t **it = ramElement + 1; it != e; ++it) {
		*it += sizeChange;
	}

	// update end pointer
	*e += sizeChange;
}
