#include "FlashStorage.hpp"
#include <crc.hpp>
#include <util.hpp>
#include <cstddef>


inline uint16_t calcChecksum(FlashStorage::Entry &entry) {
	return crc16(offsetof(FlashStorage::Entry, checksum), &entry);
}


FlashStorage::FlashStorage(Flash &flash)
	: flash(flash), info(flash.getInfo())
{
	// calculate the size of an allocation table entry
	this->entrySize = int(sizeof(Entry) + this->info.blockSize - 1) & ~(this->info.blockSize - 1);

	/*
		Cases for recovery of sector state
		E = Empty
	 	O = Open
	 	C = Closed

		Three sectors, first is head, last is tail but still empty
	 	O E E
	 	C E E (closed head)
	 	C O E (new entry)

		Three sectors, first is head, last is tail
	 	O E C
	 	C E C (closed head)
	 	C O C (copied tail to empty sector)
	 	C O E (erased tail)

	 	Two sectors, first is head and tail, second is empty
	 	O E
	 	C E (closed head)
	 	C O (copied tail to empty sector)
	 	E O (erased tail)
	*/

	// find the current sector
	int lastI = this->info.sectorCount - 1;
	auto lastState = detectSectorState(lastI);
	int head = 0;
	bool foundEmpty = false;
	auto foundState = SectorState::EMPTY;
	for (int i = 0; i < this->info.sectorCount; ++i) {
		auto state = detectSectorState(i);

		// if a non-empty sector is followed by an empty sector, the non-empty sector is head
		if (lastState != SectorState::EMPTY && state == SectorState::EMPTY) {
			head = lastI;
			foundEmpty = true;
			foundState = lastState;
		}

		// if a closed sector is followed by an open sector, the closed sector is head
		if (!foundEmpty && lastState == SectorState::CLOSED && state == SectorState::OPEN) {
			head = lastI;
			foundState = lastState;
		}

		lastI = i;
		lastState = state;
	}

	// make sure the next sector is empty which is not the case when copy of tail to empty sector was interrupted
	int next = head + 1 == this->info.sectorCount ? 0 : head + 1;
	flash.eraseSectorBlocking(next);

	switch (foundState) {
	case SectorState::EMPTY:
		// this happens if the flash is empty, make sure the sector is really empty
		flash.eraseSectorBlocking(head);
		this->sectorIndex = head;
		this->sector = this->sectorIndex * this->info.sectorSize;

		// set entry and data offsets
		this->entryWriteOffset = this->entrySize;
		this->dataWriteOffset = this->info.sectorSize;
		break;
	case SectorState::OPEN: {
		// typical case where one sector is open for write
		this->sectorIndex = head;
		this->sector = this->sectorIndex * this->info.sectorSize;

		// set entry and data offsets
		auto p = detectOffsets(head);
		this->entryWriteOffset = p.first;
		this->dataWriteOffset = p.second;
		break;
	}
	case SectorState::CLOSED:
		// we were interrupted in the garbage collection process

		// go to next sector
		this->sectorIndex = next;
		this->sector = this->sectorIndex * this->info.sectorSize;

		// set entry and data offsets
		this->entryWriteOffset = this->entrySize;
		this->dataWriteOffset = this->info.sectorSize;

		// garbage collect tail sector
		gc(next);

		break;
	}
}

Awaitable<Storage::ReadParameters> FlashStorage::read(int id, int &size, void *data, Status &status) {
	status = readBlocking(id, size, data);
	return {};
}

Awaitable<Storage::WriteParameters> FlashStorage::write(int id, int size, void const *data, Status &status) {
	status = writeBlocking(id, size, data);
	return {};
}

Awaitable<Storage::ClearParameters> FlashStorage::clear(Status &status) {
	status = clearBlocking();
	return {};
}

Storage::Status FlashStorage::readBlocking(int id, int &size, void *data) {
	if (id > 0xffff) {
		assert(false);
		size = 0;
		return Status::INVALID_ID;
	}

	int sectorIndex = this->sectorIndex;
	int sector = sectorIndex * this->info.sectorSize;
	int entryOffset = this->entryWriteOffset - this->entrySize;
	int dataOffset = this->info.sectorSize;

	// iterate over sectors
	int i = 0;
	while (true) {
		// iterate over entries
		while (entryOffset > 0) {
			Entry entry;
			this->flash.readBlocking(sector + entryOffset, sizeof(entry), &entry);

			// check if entry is valid
			if (isEntryValid(entryOffset, dataOffset, entry)) {
				// check if found
				if (entry.id == id) {
					size = entry.size;
					this->flash.readBlocking(sector + entry.offset, entry.size, data);
					return Status::OK;
				}
			}
			entryOffset -= this->entrySize;
		}

		++i;
		if (i == this->info.sectorCount - 1)
			break;

		// go to previous sector
		sectorIndex = sectorIndex == 0 ? info.sectorCount - 1 : sectorIndex - 1;
		sector = sectorIndex * this->info.sectorSize;

		// get offset of last entry in allocation table
		entryOffset = getLastEntry(sector);
	}

	// not found
	size = 0;
	return Status::OK;
}

Storage::Status FlashStorage::writeBlocking(int id, int size, void const *data) {
	if (id > 0xffff) {
		assert(false);
		return Status::INVALID_ID;
	}
	if (size > this->info.sectorSize - this->entrySize * 2) {
		assert(false);
		return Status::DATA_SIZE_EXCEEDED;
	}

	// check if entry exists and has same data
	// todo

	// check if entry will fit
	while (this->entryWriteOffset + this->entrySize + size > this->dataWriteOffset) {
		// data does not fit, we need to start a new sector

		// close current sector and go to next sector (which is erased)
		closeSector();

		gc(this->sectorIndex);
	}

	// write data
	this->dataWriteOffset -= (size + this->info.blockSize - 1) & ~(this->info.blockSize - 1);
	this->flash.writeBlocking(this->sector + this->dataWriteOffset, size, data);

	// write entry
	writeEntry(id, size);

	return Status::OK;
}

Storage::Status FlashStorage::clearBlocking() {
	// erase flash
	for (int i = 0; i < this->info.sectorCount; ++i) {
		this->flash.eraseSectorBlocking(i);
	}

	// initialize member variables
	this->sectorIndex = 0;
	this->sector = 0;
	this->entryWriteOffset = this->entrySize;
	this->dataWriteOffset = this->info.sectorSize;

	return Status::OK;

}

FlashStorage::SectorState FlashStorage::detectSectorState(int sectorIndex) {
	int sector = sectorIndex * this->info.sectorSize;

	Entry entry;
	flash.readBlocking(sector, sizeof(entry), &entry);

	if (entry.isEmpty()) {
		// section is empty or open: read first entry
		flash.readBlocking(sector + this->entrySize, sizeof(entry), &entry);
		if (entry.isEmpty()) {
			// section is empty
			return SectorState::EMPTY;
		} else {
			// section is open
			return SectorState::OPEN;
		}
	} else {
		// section is closed
		return SectorState::CLOSED;
	}
}

std::pair<int, int> FlashStorage::detectOffsets(int sectorIndex) {
	Entry entry;
	int sector = sectorIndex * this->info.sectorSize;
	int entryOffset = this->entrySize;
	int dataOffset = this->info.sectorSize;

	// iterate over entries
	while (entryOffset <= dataOffset) {
		// read next entry
		flash.readBlocking(sector + entryOffset, sizeof(entry), &entry);

		// end of list is indicated by an empty entry
		if (entry.isEmpty())
			break;

		// check if entry is valid
		if (isEntryValid(entryOffset, dataOffset, entry)) {
			// set new data offset
			dataOffset = entry.offset;
		}
		entryOffset += this->entrySize;
	}

	// check if data is actually empty and does not contain incomplete writes
	uint8_t buffer[BUFFER_SIZE];
	int size = dataOffset - entryOffset;
	int o = entryOffset;
	while (size > 0) {
		int toCheck = min(size, BUFFER_SIZE);

		this->flash.readBlocking(sector + o, toCheck, buffer);
		for (int i = 0; i < toCheck; ++i) {
			if (buffer[i] != 0xff) {
				dataOffset = (o + i) & ~(this->info.blockSize - 1);
			}
		}
		size -= toCheck;
		o += toCheck;
	}

	return {entryOffset, dataOffset};
}

int FlashStorage::getLastEntry(int sector) {
	// read close entry (assumption is that it is present and valid)
	Entry entry;
	flash.readBlocking(sector, sizeof(entry), &entry);

	// check if empty, should not happen as the sector is assumed to be closed
	// todo: report malfunction
	if (entry.isEmpty())
		return 0;

	// return offset if close entry is valid
	if (isCloseEntryValid(entry)) {
		return entry.offset;
	}

	// close entry is not valid, therefore detect last entry
	//return detectLastEntry(sectorIndex);
	int entryOffset = this->entrySize;
	int validOffset = 0;
	int dataOffset = this->info.sectorSize;

	// iterate over entries
	while (entryOffset <= dataOffset) {
		// read next entry
		flash.readBlocking(sector + entryOffset, sizeof(entry), &entry);

		// end of list is indicated by an empty entry
		if (entry.isEmpty())
			break;

		// check if entry is valid
		if (isEntryValid(entryOffset, dataOffset, entry)) {
			validOffset = entryOffset;

			// set new data offset
			dataOffset = entry.offset;
		}
		entryOffset += this->entrySize;
	}

	return validOffset;
}

bool FlashStorage::isEntryValid(int entryOffset, int dataOffset, Entry &entry) const {
	// check checksum
	if (entry.checksum != calcChecksum(entry))
		return false;

	// check if offset is aligned to block size (which is power of two)
	if ((entry.offset & (this->info.blockSize - 1)) != 0)
		return false;

	// check if data is in valid range
	if (entry.offset < entryOffset + this->entrySize || entry.offset + entry.size > dataOffset)
		return false;

	return true;
}

void FlashStorage::closeSector() {
	// create entry
	Entry entry;
	entry.id = 0xffff;
	entry.size = 0;
	entry.offset = this->entryWriteOffset - this->entrySize;
	entry.checksum = calcChecksum(entry);

	// write close entry at end of sector
	this->flash.writeBlocking(this->sector, sizeof(entry), &entry);

	// use next sector
	this->sectorIndex = this->sectorIndex + 1 == this->info.sectorCount ? 0 : this->sectorIndex + 1;
	this->sector = this->sectorIndex * this->info.sectorSize;

	this->entryWriteOffset = this->entrySize;
	this->dataWriteOffset = this->info.sectorSize;
}

bool FlashStorage::isCloseEntryValid(Entry &entry) const {
	// check checksum
	if (entry.checksum != calcChecksum(entry))
		return false;

	// check if index is 0xffff and length is 0
	if (entry.id != 0xffff && entry.size != 0)
		return false;

	// check if offset of last entry is a multiple of entry size (which is power of two)
	if ((entry.offset & (this->entrySize - 1)) != 0)
		return false;

	// check if there is at least one entry and the offset is inside the sector
	if (entry.offset >= this->entrySize && entry.offset < this->info.sectorSize)//this->info.sectorSize - this->entrySize)
		return false;

	return true;
}

bool FlashStorage::contains(Entry &entry, int sectorIndex, int entryOffset) {
	int dataOffset = entry.offset;

	// iterate over sectors
	for (int i = 0; i < this->info.sectorCount - 1; ++i) {
		int sector = sectorIndex * this->info.sectorSize;

		// get offset of last entry in allocation table
		int lastEntryOffset = getLastEntry(sector);

		// iterate over entries
		while (entryOffset <= lastEntryOffset) {
			Entry e;
			this->flash.readBlocking(sector + entryOffset, sizeof(e), &e);

			// check if entry is valid
			if (isEntryValid(entryOffset, dataOffset, e)) {
				// check if found
				if (e.id == entry.id)
					return true;

				// set new data offset
				dataOffset = e.offset;
			}
			entryOffset += this->entrySize;
		}

		// go to next sector
		sectorIndex = sectorIndex + 1 == info.sectorCount ? 0 : sectorIndex + 1;
		entryOffset = this->entrySize;
		dataOffset = this->info.sectorSize;
	}
	return false;
}

void FlashStorage::writeEntry(uint16_t id, uint16_t size) {
	// create entry
	Entry entry;
	entry.id = id;
	entry.size = size;
	entry.offset = this->dataWriteOffset;
	entry.checksum = calcChecksum(entry);

	// write entry
	this->flash.writeBlocking(this->sector + this->entryWriteOffset, sizeof(entry), &entry);

	// advance entry write offset
	this->entryWriteOffset += this->entrySize;
}

void FlashStorage::copyEntry(int sector, Entry &entry) {
	this->dataWriteOffset -= (entry.size + this->info.blockSize - 1) & ~(this->info.blockSize - 1);

	// copy data
	uint8_t buffer[BUFFER_SIZE];
	int toCopy = entry.size;
	int srcAddress = sector + entry.offset;
	int dstAddress = this->sector + this->dataWriteOffset;
	while (toCopy > 0) {
		int size = min(toCopy, BUFFER_SIZE);
		flash.readBlocking(srcAddress, size, buffer);
		flash.writeBlocking(dstAddress, size, buffer);
		srcAddress += size;
		dstAddress += size;
		toCopy -= size;
	}

	// write entry
	writeEntry(entry.id, entry.size);
}

void FlashStorage::gc(int emptySectorIndex) {
	// get sector at tail
	int tailSectorIndex = emptySectorIndex + 1 == this->info.sectorCount ? 0 : sectorIndex + 1;
	int tailSector = tailSectorIndex * this->info.sectorSize;

	// copy all entries from sector at tail to the sector at head
	int dataOffset = this->info.sectorSize;
	int entryOffset = this->entrySize;
	int lastEntryOffset = getLastEntry(tailSector);
	while (entryOffset <= lastEntryOffset) {
		// read entry
		Entry entry;
		this->flash.readBlocking(tailSector + entryOffset, sizeof(entry), &entry);

		if (isEntryValid(entryOffset, dataOffset, entry)) {
			// check if the entry is outdated (contains a newer entry with same id)
			if (!contains(entry, tailSectorIndex, entryOffset + this->entrySize)) {
				// no: copy it from tail to head
				copyEntry(tailSector, entry);
			}

			// set new data offset, only for verification
			dataOffset = entry.offset;
		}
		entryOffset += this->entrySize;
	}

	// erase sector at tail
	flash.eraseSectorBlocking(tailSectorIndex);
}