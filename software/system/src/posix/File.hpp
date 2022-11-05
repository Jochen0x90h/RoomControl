#pragma once

#include <enum.hpp>
#include <string>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>


class File {
public:
	enum class Mode {
		READ = O_RDONLY,
		WRITE = O_WRONLY,
		READ_WRITE = O_RDWR,

		TRUNCATE = O_TRUNC
	};

	/**
	 * Constructor opens a file
	 * @param name file name
	 * @param mode combination of Mode elements
	 */
	File(const std::string &name, Mode mode) {
		this->fd = open(name.c_str(), O_CREAT | int(mode), S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH);
	}

	/**
	 * Destructor
	 */
	~File() {
		close(this->fd);
	}

	bool isOpen() {return this->fd != -1;}

	/**
	 * Get size of file
	 * @return file size
	 */
	int getSize() {
		struct stat stat;
		fstat(this->fd, &stat);
		return stat.st_size;
	}

	/**
	 * Set size of file
	 * @param value initial value
	 */
	void resize(int size, uint8_t value = 0) {
		if (value != 0) {
			int fileSize = getSize();
			fill(fileSize, size - fileSize, value);
		}
		ftruncate(this->fd, size);
	}

	int read(int offset, int length, void *data) {
		return pread(this->fd, data, length, offset);
	}

	int write(int offset, int length, const void *data) {
		return pwrite(this->fd, data, length, offset);
	}

	void fill(int offset, int length, uint8_t value) {
		uint8_t buffer[16] = {value, value, value, value, value, value, value, value, value, value, value, value, value, value, value, value};
		int count = length >> 4;
		for (int i = 0; i < count; ++i)
			pwrite(this->fd, buffer, 16, offset + i * 16);
		int remaining = length & 15;
		if (remaining > 0)
			pwrite(this->fd, buffer, remaining, offset + count * 16);
	}

protected:
	int fd;
};

FLAGS_ENUM(File::Mode)
