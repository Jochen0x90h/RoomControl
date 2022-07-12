#include "../Storage.hpp"
#include <util.hpp>
#include <boardConfig.hpp>
#include <vector>
#include <map>
#include <cstdio>
#include <memory.h>


namespace Storage {

struct Context {
	std::map<uint16_t, std::vector<uint8_t>> entries;
};

bool inited = false;
Context contexts[STORAGE_CONTEXT_COUNT];


struct Header {
	uint16_t id;
	uint16_t length;
};

void readData(Context &context) {
	FILE *file = fopen("storage.bin", "rb");
	if (file == nullptr)
		return;

	Header header;
	while (fread(&header, sizeof(header), 1, file) == 1) {
		uint8_t data[1024];
		if (fread(data, 1, header.length, file) < header.length)
			break;
		context.entries[header.id].assign(data, data + header.length);
	}

	fclose(file);
}

void writeData(Context &context) {
	FILE *file = fopen("storage.bin", "wb");
	if (file == nullptr)
		return;

	for (auto &p : context.entries) {
		Header header = {p.first, uint16_t(p.second.size())};
		fwrite(&header, sizeof(header), 1, file);
		fwrite(p.second.data(), 1, p.second.size(), file);
	}

	fclose(file);
}

void init() {
	Storage::inited = true;

	readData(Storage::contexts[0]);
}


int size(int index, uint16_t id) {
	assert(Storage::inited && uint(index) < STORAGE_CONTEXT_COUNT);
	auto &context = Storage::contexts[index];

	auto &entry = context.entries[id];
	return entry.size();
}

int read(int index, uint16_t id, int size, void *data) {
	assert(Storage::inited && uint(index) < STORAGE_CONTEXT_COUNT);
	auto &context = Storage::contexts[index];

	auto &entry = context.entries[id];
	int len = min(size, int(entry.size()));
	memcpy(data, entry.data(), len);
	return len;
}

bool write(int index, uint16_t id, int size, void const *data) {
	assert(Storage::inited && uint(index) < STORAGE_CONTEXT_COUNT);
	auto &context = Storage::contexts[index];

	auto &entry = context.entries[id];
	auto begin = reinterpret_cast<uint8_t const *>(data);
	entry.assign(begin, begin + size);

	writeData(context);
	return true;
}

bool erase(int index, uint16_t id) {
	assert(Storage::inited && uint(index) < STORAGE_CONTEXT_COUNT);
	auto &context = Storage::contexts[index];

	context.entries.erase(id);

	writeData(context);
	return true;
}

bool clear(int index) {
	assert(Storage::inited && uint(index) < STORAGE_CONTEXT_COUNT);
	auto &context = Storage::contexts[index];

	context.entries.clear();

	writeData(context);
	return true;
}

} // namespace Storage
