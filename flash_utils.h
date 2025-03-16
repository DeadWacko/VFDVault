

#ifndef FLASH_UTILS_H
#define FLASH_UTILS_H

#include <stdint.h>
#include <stdbool.h>

/**
 * @brief Запись данных в сектор флеш-памяти
 * @param offset Смещение во флеш-памяти (должно быть выровнено по FLASH_SECTOR_SIZE)
 * @param data Указатель на данные
 * @param len Длина данных (не более FLASH_SECTOR_SIZE)
 * @return true если запись успешна, false в противном случае
 */
bool write_flash_sector(const uint32_t offset, const uint8_t *data, size_t len);

/**
 * @brief Очистка сектора флеш-памяти (заполнение 0xFF)
 * @param flash_offset Смещение во флеш-памяти
 * @return true если очистка успешна, false в противном случае
 */
bool erase_flash_sector(const uint32_t flash_offset);

#endif // FLASH_UTILS_H