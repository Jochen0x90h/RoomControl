#pragma once

#include <Coroutine.hpp>
#include <cstdint>


/**
 * Non-volatile storage, can be implemented on top of nvs of ESP-32 and Zephyr
 * ESP-32: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/storage/nvs_flash.html
 * Zephyr: https://docs.zephyrproject.org/latest/services/storage/nvs/nvs.html
 */
class Storage {
public:
	enum class Status {
		// operation completed successfully, also for partial read where the element is larger than the provided buffer
		OK,

		// element was read as zero length because of checksum error
		CHECKSUM_ERROR,

		// element was not read or written because the id is invalid
		INVALID_ID,

		// element was not written because the maximum data size was exceeded
		DATA_SIZE_EXCEEDED,

		// element was not written because storage is full
		OUT_OF_MEMORY_ERROR,

		// memory is not usable, e.g. not connected or end of life of flash memory
		FATAL_ERROR
	};

	struct ReadParameters {
		int id;
		int *size;
		void *data;
		Status *status;
	};

	struct WriteParameters {
		int index;
		int size;
		void const *data;
		Status *status;
	};

	struct ClearParameters {
		Status *status;
	};


	virtual ~Storage();

	/**
	 * READ an entry from the non-volatile storage into a given data buffer
	 * @param id id of element
	 * @param size in: size of provided data buffer in bytes, out: size of entry EVEN IF LARGER THAN BUFFER
	 * @param data data to read into
	 * @param status status of operation
	 * @return use co_await on return value to await completion
	 */
	[[nodiscard]] virtual Awaitable<ReadParameters> read(int id, int &size, void *data, Status &status) = 0;

	/**
	 * WRITE an entry to the non-volatile storage
	 * @param id id of element
	 * @param size size of data to write in bytes
	 * @param data data to write
	 * @param status status of operation
	 * @return use co_await on return value to await completion
	 */
	[[nodiscard]] virtual Awaitable<WriteParameters> write(int id, int size, void const *data, Status &status) = 0;

	/**
	 * Clear all entries in the non-volatile storage
	 * @param status status of operation
	 * @return use co_await on return value to await completion
	 */
	[[nodiscard]] virtual Awaitable<ClearParameters> clear(Status &status) = 0;


	/**
	 * READ an entry from the non-volatile storage into a given data buffer
	 * @param id id of element
	 * @param size in: size of provided data buffer in bytes, out: size of entry EVEN IF LARGER THAN BUFFER
	 * @param data data to read into
	 * @return status of operation
	 */
	virtual Status readBlocking(int id, int &size, void *data) = 0;

	/**
	 * WRITE an entry to the non-volatile storage
	 * @param id id of element
	 * @param size size of data to write in bytes
	 * @param data data to write
	 * @return status of operation
	 */
	virtual Status writeBlocking(int id, int size, void const *data) = 0;

	/**
	 * Clear all entries in the non-volatile storage
	 * @return status of operation
	 */
	virtual Status clearBlocking() = 0;


	/**
	 * Get size of an element
	 * @param id id of element
	 * @return size of the element in bytes
	 */
	int getSizeBlocking(int id) {
		int size = 0;
		readBlocking(id, size, nullptr);
		return size;
	}

	/**
	 * Erase an element, equivalent to writing data of length zero
	 * @param index index of element
	 */
	void eraseBlocking(int id) {
		writeBlocking(id, 0, nullptr);
	}
};
