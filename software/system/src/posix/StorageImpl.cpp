#include "StorageImpl.hpp"
#include "File.hpp"
#include <util.hpp>
#include <cstring>


namespace {

struct Header {
	uint16_t id;
	uint16_t length;
};

}

StorageImpl::StorageImpl(std::string const &filename, int maxId, int maxDataSize)
	: filename(filename), maxId(maxId), maxDataSize(maxDataSize)
{
	readData();
}

Awaitable<Storage::ReadParameters> StorageImpl::read(int id, int &size, void *data, Status &status) {
	status = readBlocking(id, size, data);
	return {};
}

Awaitable<Storage::WriteParameters> StorageImpl::write(int id, int size, void const *data, Status &status) {
	status = writeBlocking(id, size, data);
	return {};
}

Awaitable<Storage::ClearParameters> StorageImpl::clear(Status &status) {
	status = clearBlocking();
	return {};
}

Storage::Status StorageImpl::readBlocking(int id, int &size, void *data) {
	if (id > this->maxId) {
		assert(false);
		size = 0;
		return Status::INVALID_ID;
	}
	auto &element = this->elements[id];
	int len = min(size, int(element.size()));
	memcpy(data, element.data(), len);
	size = element.size(); // size is size of element even if it is larger than size
	return Status::OK;
}

Storage::Status StorageImpl::writeBlocking(int id, int size, void const *data) {
	if (id > this->maxId) {
		assert(false);
		return Status::INVALID_ID;
	}
	if (size > this->maxDataSize) {
		assert(false);
		return Status::DATA_SIZE_EXCEEDED;
	}
	auto &element = this->elements[id];
	auto begin = reinterpret_cast<uint8_t const *>(data);
	element.assign(begin, begin + size);
	writeData();
	return Status::OK;
}

Storage::Status StorageImpl::clearBlocking() {
	this->elements.clear();
	writeData();
	return Status::OK;
}

void StorageImpl::readData() {
	File file(filename, File::Mode::READ);

	int offset = 0;
	Header header;
	while (file.read(offset, sizeof(header), &header) == sizeof(header)) {
		offset += sizeof(header);
		uint8_t data[1024];
		if (file.read(offset, header.length, data) < header.length)
			break;
		this->elements[header.id].assign(data, data + header.length);
		offset += header.length;
	}
}

void StorageImpl::writeData() {
	File file(filename, File::Mode::WRITE | File::Mode::TRUNCATE);
	if (!file.isOpen())
		return;

	int offset = 0;
	for (auto &p : this->elements) {
		Header header = {p.first, uint16_t(p.second.size())};
		file.write(offset, sizeof(header), &header);
		offset += sizeof(header);
		file.write(offset, p.second.size(), p.second.data());
		offset += p.second.size();
	}
}
