#include "FileStorage.hpp"
#include <cstring>
#include <fcntl.h>
#include <unistd.h>


namespace {

struct Header {
	uint16_t id;
	uint16_t length;
};

}

FileStorage::FileStorage(std::string const &filename, int maxElementCount, int maxElementSize)
	: filename(filename), maxElementCount(maxElementCount), maxElementSize(maxElementSize)
{
	readData();
}

Awaitable<Storage::ReadParameters> FileStorage::read(int index, int &size, void *data, Status &status) {
	status = readBlocking(index, size, data);
	return {};
}

Awaitable<Storage::WriteParameters> FileStorage::write(int index, int size, void const *data, Status &status) {
	status = writeBlocking(index, size, data);
	return {};
}

Awaitable<Storage::ClearParameters> FileStorage::clear(Status &status) {
	status = clearBlocking();
	return {};
}

Storage::Status FileStorage::readBlocking(int index, int &size, void *data) {
	if (index >= this->maxElementCount) {
		assert(false);
		size = 0;
		return Status::ELEMENT_COUNT_EXCEEDED;
	}
	auto &element = this->elements[index];
	int len = min(size, int(element.size()));
	memcpy(data, element.data(), len);
	size = element.size(); // size is size of element even if it is larger than size
	return Status::OK;
}

Storage::Status FileStorage::writeBlocking(int index, int size, void const *data) {
	if (index >= this->maxElementCount) {
		assert(false);
		return Status::ELEMENT_COUNT_EXCEEDED;
	}
	if (size > this->maxElementSize) {
		assert(false);
		return Status::ELEMENT_SIZE_EXCEEDED;
	}
	auto &element = this->elements[index];
	auto begin = reinterpret_cast<uint8_t const *>(data);
	element.assign(begin, begin + size);
	writeData();
	return Status::OK;
}

Storage::Status FileStorage::clearBlocking() {
	this->elements.clear();
	writeData();
	return Status::OK;
}

void FileStorage::readData() {
	int file = open(filename.c_str(), O_RDONLY);
	if (file == -1)
		return;

	Header header;
	while (::read(file, &header, sizeof(header)) == sizeof(header)) {
		uint8_t data[1024];
		if (::read(file, data, header.length) < header.length)
			break;
		this->elements[header.id].assign(data, data + header.length);
	}

	close(file);
}

void FileStorage::writeData() {
	int file = open(filename.c_str(), O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH);
	if (file == -1)
		return;

	for (auto &p : this->elements) {
		Header header = {p.first, uint16_t(p.second.size())};
		::write(file, &header, sizeof(header));
		::write(file, p.second.data(), p.second.size());
	}

	close(file);
}
