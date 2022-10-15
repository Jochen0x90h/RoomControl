#include "FileStorage.hpp"
#include <cstdio>
#include <cstring>


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

Awaitable<Storage2::ReadParameters> FileStorage::read(int index, int &size, void *data, Status &status) {
	status = readBlocking(index, size, data);
	return {};
}

Awaitable<Storage2::WriteParameters> FileStorage::write(int index, int size, void const *data, Status &status) {
	status = writeBlocking(index, size, data);
	return {};
}

Awaitable<Storage2::ClearParameters> FileStorage::clear(Status &status) {
	status = clearBlocking();
	return {};
}

Storage2::Status FileStorage::readBlocking(int index, int &size, void *data) {
	if (index >= this->maxElementCount) {
		size = 0;
		return Status::ELEMENT_COUNT_EXCEEDED;
	}
	auto &element = this->elements[index];
	int len = min(size, int(element.size()));
	memcpy(data, element.data(), len);
	size = element.size(); // size is size of element even if it is larger than size
	return Status::OK;
}

Storage2::Status FileStorage::writeBlocking(int index, int size, void const *data) {
	if (index >= this->maxElementCount)
		return Status::ELEMENT_COUNT_EXCEEDED;
	if (size > this->maxElementSize)
		return Status::ELEMENT_SIZE_EXCEEDED;
	auto &element = this->elements[index];
	auto begin = reinterpret_cast<uint8_t const *>(data);
	element.assign(begin, begin + size);
	writeData();
	return Status::OK;
}

Storage2::Status FileStorage::clearBlocking() {
	this->elements.clear();
	writeData();
	return Status::OK;
}

void FileStorage::readData() {
	FILE *file = fopen(this->filename.c_str(), "rb");
	if (file == nullptr)
		return;

	Header header;
	while (fread(&header, sizeof(header), 1, file) == 1) {
		uint8_t data[1024];
		if (fread(data, 1, header.length, file) < header.length)
			break;
		this->elements[header.id].assign(data, data + header.length);
	}

	fclose(file);
}

void FileStorage::writeData() {
	FILE *file = fopen(this->filename.c_str(), "wb");
	if (file == nullptr)
		return;

	for (auto &p : this->elements) {
		Header header = {p.first, uint16_t(p.second.size())};
		fwrite(&header, sizeof(header), 1, file);
		fwrite(p.second.data(), 1, p.second.size(), file);
	}

	fclose(file);
}
