

#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "settings.h"
#include "flash_utils.h"
#include "logging.h"
#include "config.h"

// Статический буфер для операций с флеш-памятью
static uint8_t flash_buffer[FLASH_SECTOR_SIZE];

// Функция шифрования/дешифрования пароля (XOR с SETTINGS_MAGIC)
static void xor_wifi_pass(char *pass, size_t len) {
#ifdef ENCRYPT_WIFI_PASS
    size_t real_len = strnlen(pass, len);
    LOG_INFO("Encrypting/Decrypting password: %s (len %u)", pass, (unsigned)real_len);
    for (size_t i = 0; i < real_len && i < WIFI_PASS_MAX_LEN; i++) {
        pass[i] ^= (SETTINGS_MAGIC >> (i % 32)) & 0xFF;
    }
    char temp[WIFI_PASS_MAX_LEN + 1];
    strncpy(temp, pass, WIFI_PASS_MAX_LEN);
    temp[WIFI_PASS_MAX_LEN] = '\0';
    LOG_INFO("Result: %s", temp);
    // TODO: Consider making encryption key configurable via CMake (e.g., -DENCRYPTION_KEY=0xDEADBEEF)
#else
    LOG_WARN("Encryption disabled - ENCRYPT_WIFI_PASS not defined");
#endif
}

uint32_t calculate_crc32(const uint8_t *data, size_t len) {
    if (!data || len == 0) {
        LOG_ERROR("Invalid input to calculate_crc32");
        return CRC32_ERROR;
    }

    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            crc = (crc >> 1) ^ (0xEDB88320 & -(crc & 1));
        }
    }
    return crc ^ 0xFFFFFFFF;
}

void settings_init_default(settings_t *cfg) {
    if (!cfg) {
        LOG_ERROR("Null pointer passed to settings_init_default");
        return;
    }

    memset(cfg, 0, sizeof(settings_t));
    cfg->magic = SETTINGS_MAGIC;
    cfg->version = SETTINGS_VERSION;
    cfg->size = sizeof(settings_t);
    snprintf(cfg->wifi_ssid, WIFI_SSID_MAX_LEN, "%s", DEFAULT_SSID);
    snprintf(cfg->wifi_pass, WIFI_PASS_MAX_LEN, "%s", DEFAULT_PASS);
    cfg->brightness = 50;
    cfg->night_off_hour = 22;
    cfg->night_on_hour = 6;
    cfg->anim_lags_period_s = 5;
    snprintf(cfg->ntp_servers[0], NTP_SERVER_MAX_LEN, "%s", DEFAULT_NTP_SERVER);
    cfg->ntp_sync_period_minutes = 60;
#ifdef ENCRYPT_WIFI_PASS
    if (!(cfg->flags & FLAG_SETTINGS_ENCRYPTED)) {
        LOG_INFO("Encryption enabled via ENCRYPT_WIFI_PASS - setting FLAG_SETTINGS_ENCRYPTED");
        cfg->flags |= FLAG_SETTINGS_ENCRYPTED;
    }
#else
    if (cfg->flags & FLAG_SETTINGS_ENCRYPTED) {
        LOG_WARN("Encryption flag set but ENCRYPT_WIFI_PASS not defined - disabling");
        cfg->flags &= ~FLAG_SETTINGS_ENCRYPTED;
    }
#endif
    cfg->crc32 = calculate_crc32((const uint8_t *)cfg, sizeof(settings_t) - sizeof(uint32_t));
}

bool settings_load(settings_t *cfg, const uint32_t flash_offset) {
    if (!cfg) {
        LOG_ERROR("Null pointer passed to settings_load");
        return false;
    }

    if (flash_offset % FLASH_SECTOR_SIZE != 0 || flash_offset >= PICO_FLASH_SIZE_BYTES) {
        LOG_ERROR("Invalid or unaligned flash offset 0x%08X", flash_offset);
        return false;
    }

    const uint8_t *flash = (const uint8_t *)(XIP_BASE + flash_offset);
    static const uint8_t blank[sizeof(settings_t)] = { [0 ... sizeof(settings_t)-1] = 0xFF };
    if (memcmp(flash, blank, sizeof(settings_t)) == 0) {
        LOG_WARN("Flash sector is blank at offset 0x%08X", flash_offset);
        return false;
    }

    memcpy(cfg, flash, sizeof(settings_t));
    if (cfg->magic != SETTINGS_MAGIC) {
        LOG_ERROR("Invalid magic number: 0x%08X (expected 0x%08X)", cfg->magic, SETTINGS_MAGIC);
        return false;
    }
    if (cfg->version != SETTINGS_VERSION) {
        LOG_ERROR("Unsupported version: 0x%04X (expected 0x%04X)", cfg->version, SETTINGS_VERSION);
        return false;
    }
    if (cfg->size != sizeof(settings_t)) {
        LOG_ERROR("Size mismatch: stored %u, expected %u", cfg->size, (unsigned)sizeof(settings_t));
        return false;
    }

    uint32_t computed_crc = calculate_crc32((const uint8_t *)cfg, sizeof(settings_t) - sizeof(uint32_t));
    if (computed_crc != cfg->crc32) {
        LOG_ERROR("CRC32 mismatch: computed 0x%08X, stored 0x%08X", computed_crc, cfg->crc32);
        return false;
    }

    // Дешифруем пароль после загрузки, если установлен флаг
    if (cfg->flags & FLAG_SETTINGS_ENCRYPTED) {
        xor_wifi_pass(cfg->wifi_pass, WIFI_PASS_MAX_LEN);
    }

    LOG_INFO("Settings loaded successfully from offset 0x%08X", flash_offset);
    return true;
}

bool settings_save(const settings_t *cfg, const uint32_t flash_offset) {
    if (!cfg) {
        LOG_ERROR("Null pointer passed to settings_save");
        return false;
    }

    if (cfg->magic != SETTINGS_MAGIC || cfg->version != SETTINGS_VERSION) {
        LOG_ERROR("Invalid magic (0x%08X) or version (0x%04X) in settings", cfg->magic, cfg->version);
        return false;
    }
    if (cfg->size != sizeof(settings_t)) {
        LOG_ERROR("Invalid size: %u (expected %u)", cfg->size, (unsigned)sizeof(settings_t));
        return false;
    }
    if (cfg->brightness > BRIGHTNESS_MAX) {
        LOG_ERROR("Brightness out of range: %u (max %u)", cfg->brightness, BRIGHTNESS_MAX);
        return false;
    }
    if (cfg->night_off_hour > HOUR_MAX || cfg->night_on_hour > HOUR_MAX) {
        LOG_ERROR("Invalid hour: night_off %u, night_on %u (max %u)", 
                  cfg->night_off_hour, cfg->night_on_hour, HOUR_MAX);
        return false;
    }
    if (strlen(cfg->wifi_ssid) >= WIFI_SSID_MAX_LEN) {
        LOG_ERROR("WiFi SSID too long: %s (max %u chars)", cfg->wifi_ssid, WIFI_SSID_MAX_LEN - 1);
        return false;
    }
    if (strlen(cfg->wifi_pass) >= WIFI_PASS_MAX_LEN) {
        LOG_ERROR("WiFi password too long: %s (max %u chars)", cfg->wifi_pass, WIFI_PASS_MAX_LEN - 1);
        return false;
    }
    for (int i = 0; i < NTP_MAX_SERVERS; i++) {
        if (strlen(cfg->ntp_servers[i]) >= NTP_SERVER_MAX_LEN) {
            LOG_ERROR("NTP server %d too long: %s (max %u chars)", i, cfg->ntp_servers[i], NTP_SERVER_MAX_LEN - 1);
            return false;
        }
    }

    memset(flash_buffer, 0xFF, FLASH_SECTOR_SIZE);
    settings_t *temp = (settings_t *)flash_buffer;
    memcpy(temp, cfg, sizeof(settings_t));
    // Шифруем пароль перед сохранением, если установлен флаг
    if (temp->flags & FLAG_SETTINGS_ENCRYPTED) {
        xor_wifi_pass(temp->wifi_pass, WIFI_PASS_MAX_LEN);
    }
    temp->crc32 = calculate_crc32((const uint8_t *)temp, sizeof(settings_t) - sizeof(uint32_t));

    if (!write_flash_sector(flash_offset, flash_buffer, FLASH_SECTOR_SIZE)) {
        return false;
    }

    LOG_INFO("Settings saved successfully to offset 0x%08X", flash_offset);
    return true;
}