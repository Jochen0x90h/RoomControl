#include <cstdint>


/**
 * Non-volatile storage, can be implemented on top of nvs of ESP-32 and Zephyr
 * ESP-32: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/storage/nvs_flash.html
 * Zephyr: https://docs.zephyrproject.org/latest/services/storage/nvs/nvs.html
 */
namespace Storage {

/**
 * Initialize and "mount" the storage
 */
void init();

/**
 * Get size of an element
 * @param index context index
 * @param id id of entry
 * @return true on success, false on error
 */
int size(int index, uint16_t id);

/**
 * Read an entry from the non-volatile storage into a given data buffer
 * @param index context index
 * @param id id of entry
 * @param size size of provided data buffer in bytes
 * @param data data to read into
 * @return number of true on success, false on error
 */
int read(int index, uint16_t id, int size, void *data);

/**
 * Write an entry to the non-volatile storage
 * @param index context index
 * @param id id of entry
 * @param size size of data to write in bytes
 * @param data data to write
 * @return true on success, false on error
 */
bool write(int index, uint16_t id, int size, void const *data);

/**
 * Erase an entry in the non-volatile storage
 * @param index context index
 * @param id id of entry
 * @return true on success, false on error
 */
bool erase(int index, uint16_t id);

/**
 * Clear all entries in the non-volatile storage
 * @param index context index
 * @return true on success, false on error
 */
bool clear(int index);

} // namespace Storage
