

#include <string.h>
#include "pico/stdlib.h"
#include "hardware/flash.h"
#include "hardware/sync.h"
#include "flash_utils.h"
#include "logging.h"

static uint8_t flash_buffer[FLASH_SECTOR_SIZE];

bool write_flash_sector(const uint32_t offset, const uint8_t *data, size_t len) {
    if (offset % FLASH_SECTOR_SIZE != 0 || offset >= PICO_FLASH_SIZE_BYTES) {
        LOG_ERROR("Invalid or unaligned flash offset 0x%08X", offset);
        return false;
    }
    if (len > FLASH_SECTOR_SIZE) {
        LOG_ERROR("Data length %u exceeds FLASH_SECTOR_SIZE %u", (unsigned)len, FLASH_SECTOR_SIZE);
        return false;
    }

    uint32_t ints = save_and_disable_interrupts();
    flash_range_erase(offset, FLASH_SECTOR_SIZE);
    flash_range_program(offset, data, FLASH_SECTOR_SIZE);
    restore_interrupts(ints);

    const uint8_t *flash = (const uint8_t *)(XIP_BASE + offset);
    if (memcmp(flash, data, len) != 0) {
        LOG_ERROR("Flash write verification failed at offset 0x%08X", offset);
        return false;
    }

    LOG_INFO("Flash write successful at offset 0x%08X", offset);
    return true;
}

bool erase_flash_sector(const uint32_t flash_offset) {
    memset(flash_buffer, 0xFF, FLASH_SECTOR_SIZE);
    return write_flash_sector(flash_offset, flash_buffer, FLASH_SECTOR_SIZE);
}