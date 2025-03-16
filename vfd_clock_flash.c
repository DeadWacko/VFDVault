
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "pico/stdlib.h"
#include "hardware/flash.h"
#include "hardware/sync.h"
#include "settings.h"
#include "flash_utils.h"
#include "logging.h"

// Смещение во флеш-памяти
#define FLASH_OFFSET (PICO_FLASH_SIZE_BYTES - FLASH_SECTOR_SIZE)

// Проверка, что строка не состоит только из пробелов
static bool is_string_non_empty(const char *str) {
    while (*str) {
        if (!isspace((unsigned char)*str)) return true;
        str++;
    }
    return false;
}

static void print_settings(const settings_t *cfg, const char *title, const uint32_t flash_offset) {
    printf(COLOR_YELLOW "\n=== %s ===\n" COLOR_RESET, title);
    printf(COLOR_CYAN "Flash Offset       : " COLOR_RESET "0x%08X\n", flash_offset);
    printf(COLOR_CYAN "Settings Size      : " COLOR_RESET "%u bytes\n", cfg->size);
    printf(COLOR_CYAN "Magic Number       : " COLOR_RESET "0x%08X\n", cfg->magic);
    printf(COLOR_CYAN "Version            : " COLOR_RESET "0x%04X\n", cfg->version);
    printf(COLOR_CYAN "WiFi SSID          : " COLOR_RESET "%s\n", cfg->wifi_ssid);
    printf(COLOR_CYAN "WiFi Password      : " COLOR_RESET "%s\n", cfg->wifi_pass);
    printf(COLOR_CYAN "Brightness         : " COLOR_RESET "%u%%\n", cfg->brightness);
    printf(COLOR_CYAN "Adaptive Brightness: " COLOR_RESET "%s\n", 
           (cfg->flags & FLAG_ADAPTIVE_BRIGHTNESS) ? COLOR_GREEN "Enabled" COLOR_RESET : COLOR_RED "Disabled" COLOR_RESET);
    printf(COLOR_CYAN "Night Mode         : " COLOR_RESET "%s\n", 
           (cfg->flags & FLAG_NIGHT_MODE) ? COLOR_GREEN "Enabled" COLOR_RESET : COLOR_RED "Disabled" COLOR_RESET);
    printf(COLOR_CYAN "Password Encrypted : " COLOR_RESET "%s\n", 
           (cfg->flags & FLAG_SETTINGS_ENCRYPTED) ? 
           COLOR_GREEN "Yes" COLOR_RESET : 
           COLOR_RED "No (ENCRYPT_WIFI_PASS disabled)" COLOR_RESET);
    printf(COLOR_CYAN "Night Off Hour     : " COLOR_RESET "%02u:00\n", cfg->night_off_hour);
    printf(COLOR_CYAN "Night On Hour      : " COLOR_RESET "%02u:00\n", cfg->night_on_hour);
    printf(COLOR_CYAN "Animations         : " COLOR_RESET "[%c] 1 [%c] 2 [%c] 3 [%c] 4 [%c] Lags\n",
           (cfg->anim_flags & ANIM_FLAG_1) ? 'X' : ' ',
           (cfg->anim_flags & ANIM_FLAG_2) ? 'X' : ' ',
           (cfg->anim_flags & ANIM_FLAG_3) ? 'X' : ' ',
           (cfg->anim_flags & ANIM_FLAG_4) ? 'X' : ' ',
           (cfg->anim_flags & ANIM_FLAG_LAGS) ? 'X' : ' ');
    printf(COLOR_CYAN "Anim Lags Period   : " COLOR_RESET "%u sec\n", cfg->anim_lags_period_s);
    printf(COLOR_CYAN "NTP Servers        :\n" COLOR_RESET);
    for (int i = 0; i < NTP_MAX_SERVERS; i++) {
        if (is_string_non_empty(cfg->ntp_servers[i])) {
            printf(COLOR_BLUE "  %d: " COLOR_RESET "%s\n", i + 1, cfg->ntp_servers[i]);
        }
    }
    printf(COLOR_CYAN "NTP Sync Period    : " COLOR_RESET "%u min\n", cfg->ntp_sync_period_minutes);
    printf(COLOR_CYAN "CRC32              : " COLOR_RESET "0x%08X\n", cfg->crc32);
}

static void print_flash_contents(const uint32_t flash_offset) {
    const uint8_t *flash = (const uint8_t *)(XIP_BASE + flash_offset);
    printf(COLOR_YELLOW "Flash Contents (first 32 bytes):\n" COLOR_RESET);
    for (int i = 0; i < 32; i++) {
        printf(COLOR_GREEN "%02X " COLOR_RESET, flash[i]);
        if ((i + 1) % 16 == 0) printf("\n");
    }
    printf("\n");
}

static bool compare_settings(const settings_t *cfg1, const settings_t *cfg2) {
    if (strcmp(cfg1->wifi_ssid, cfg2->wifi_ssid) != 0 ||
        strcmp(cfg1->wifi_pass, cfg2->wifi_pass) != 0 ||
        cfg1->brightness != cfg2->brightness ||
        cfg1->flags != cfg2->flags ||
        cfg1->night_off_hour != cfg2->night_off_hour ||
        cfg1->night_on_hour != cfg2->night_on_hour ||
        cfg1->anim_flags != cfg2->anim_flags ||
        cfg1->anim_lags_period_s != cfg2->anim_lags_period_s ||
        cfg1->ntp_sync_period_minutes != cfg2->ntp_sync_period_minutes) {
        return false;
    }
    for (int i = 0; i < NTP_MAX_SERVERS; i++) {
        if (strcmp(cfg1->ntp_servers[i], cfg2->ntp_servers[i]) != 0) {
            return false;
        }
    }
    return true;
}

static bool test_default_settings(void) {
    LOG_INFO("Test 1: Default Settings Save/Load");
    if (!erase_flash_sector(FLASH_OFFSET)) return false;

    settings_t cfg, loaded_cfg;
    settings_init_default(&cfg);
    print_settings(&cfg, "Default Settings", FLASH_OFFSET);

    if (!settings_save(&cfg, FLASH_OFFSET)) {
        LOG_ERROR("Test 1 failed at save");
        return false;
    }
    if (!settings_load(&loaded_cfg, FLASH_OFFSET)) {
        LOG_ERROR("Test 1 failed at load");
        return false;
    }
    if (!compare_settings(&cfg, &loaded_cfg)) {
        LOG_ERROR("Test 1 failed: data mismatch");
        return false;
    }
    print_settings(&loaded_cfg, "Loaded Default Settings", FLASH_OFFSET);
    LOG_INFO("Test 1 completed successfully");
    return true;
}

static bool test_edge_cases(void) {
    LOG_INFO("Test 2: Edge Case Settings Save/Load");
    if (!erase_flash_sector(FLASH_OFFSET)) return false;

    settings_t cfg, loaded_cfg;
    settings_init_default(&cfg);
    snprintf(cfg.wifi_ssid, WIFI_SSID_MAX_LEN, "SuperLongSSID1234567890123456789");
    snprintf(cfg.wifi_pass, WIFI_PASS_MAX_LEN, "ThisIsAVeryLongPasswordWithLotsOfCharsToTestTheLimits1234567890");
    cfg.brightness = BRIGHTNESS_MAX;
    cfg.flags |= FLAG_ADAPTIVE_BRIGHTNESS | FLAG_NIGHT_MODE | FLAG_SETTINGS_ENCRYPTED;
    cfg.night_off_hour = 23;
    cfg.night_on_hour = 0;
    cfg.anim_flags = ANIM_FLAG_1 | ANIM_FLAG_2 | ANIM_FLAG_3 | ANIM_FLAG_4 | ANIM_FLAG_LAGS;
    cfg.anim_lags_period_s = 65535;
    snprintf(cfg.ntp_servers[1], NTP_SERVER_MAX_LEN, "time.google.com");
    snprintf(cfg.ntp_servers[2], NTP_SERVER_MAX_LEN, "ntp.ubuntu.com");
    snprintf(cfg.ntp_servers[3], NTP_SERVER_MAX_LEN, "tick.usno.navy.mil");
    cfg.ntp_sync_period_minutes = 1440;
    cfg.crc32 = calculate_crc32((const uint8_t *)&cfg, sizeof(settings_t) - sizeof(uint32_t));
    print_settings(&cfg, "Edge Case Settings", FLASH_OFFSET);

    if (!settings_save(&cfg, FLASH_OFFSET)) {
        LOG_ERROR("Test 2 failed at save");
        return false;
    }
    if (!settings_load(&loaded_cfg, FLASH_OFFSET)) {
        LOG_ERROR("Test 2 failed at load");
        return false;
    }
    if (!compare_settings(&cfg, &loaded_cfg)) {
        LOG_ERROR("Test 2 failed: data mismatch");
        return false;
    }
    print_settings(&loaded_cfg, "Loaded Edge Case Settings", FLASH_OFFSET);
    LOG_INFO("Test 2 completed successfully");
    return true;
}

static bool test_invalid_data(void) {
    LOG_INFO("Test 3: Invalid Data Handling");
    if (!erase_flash_sector(FLASH_OFFSET)) return false;

    settings_t cfg;
    settings_init_default(&cfg);
    if (!settings_save(&cfg, FLASH_OFFSET)) {
        LOG_ERROR("Test 3 failed at initial save");
        return false;
    }

    uint8_t temp_buffer[FLASH_SECTOR_SIZE];
    memset(temp_buffer, 0xFF, FLASH_SECTOR_SIZE);
    settings_t *temp = (settings_t *)temp_buffer;
    memcpy(temp, &cfg, sizeof(settings_t));
    temp->crc32 = 0xDEADBEEF;
    write_flash_sector(FLASH_OFFSET, temp_buffer, FLASH_SECTOR_SIZE);

    if (settings_load(&cfg, FLASH_OFFSET)) {
        LOG_ERROR("Test 3 failed: accepted invalid CRC");
        return false;
    }

    if (!erase_flash_sector(FLASH_OFFSET)) return false;
    if (settings_load(&cfg, FLASH_OFFSET)) {
        LOG_ERROR("Test 3 failed: accepted blank flash");
        return false;
    }

    LOG_INFO("Test 3 completed successfully");
    print_flash_contents(FLASH_OFFSET);
    return true;
}

static bool test_corrupted_data(void) {
    LOG_INFO("Test 4: Corrupted Data Handling");
    if (!erase_flash_sector(FLASH_OFFSET)) return false;

    settings_t cfg;
    settings_init_default(&cfg);
    if (!settings_save(&cfg, FLASH_OFFSET)) {
        LOG_ERROR("Test 4 failed at initial save");
        return false;
    }

    uint8_t temp_buffer[FLASH_SECTOR_SIZE];
    memcpy(temp_buffer, &cfg, sizeof(settings_t));
    temp_buffer[10] ^= 0xFF;  // Инверсия одного байта
    write_flash_sector(FLASH_OFFSET, temp_buffer, FLASH_SECTOR_SIZE);

    if (settings_load(&cfg, FLASH_OFFSET)) {
        LOG_ERROR("Test 4 failed: accepted corrupted data");
        return false;
    }

    LOG_INFO("Test 4 completed successfully");
    return true;
}

static bool test_buffer_overflow(void) {
    LOG_INFO("Test 5: Buffer Overflow Handling");
    if (!erase_flash_sector(FLASH_OFFSET)) return false;

    settings_t cfg;
    settings_init_default(&cfg);
    char long_ssid[WIFI_SSID_MAX_LEN + 2];
    memset(long_ssid, 'A', WIFI_SSID_MAX_LEN + 1);
    long_ssid[WIFI_SSID_MAX_LEN + 1] = '\0';
    strncpy(cfg.wifi_ssid, long_ssid, WIFI_SSID_MAX_LEN + 1);  // Обход snprintf для теста переполнения

    if (settings_save(&cfg, FLASH_OFFSET)) {
        LOG_ERROR("Test 5 failed: accepted overflowed SSID");
        return false;
    }

    LOG_INFO("Test 5 completed successfully");
    return true;
}

static bool test_encryption(void) {
    LOG_INFO("Test 6: WiFi Password Encryption/Decryption");
    if (!erase_flash_sector(FLASH_OFFSET)) return false;

    settings_t cfg, loaded_cfg;
    settings_init_default(&cfg);
    const char *test_pass = "SecretPass123";
    snprintf(cfg.wifi_pass, WIFI_PASS_MAX_LEN, "%s", test_pass);
#ifdef ENCRYPT_WIFI_PASS
    cfg.flags |= FLAG_SETTINGS_ENCRYPTED;  // Убеждаемся, что флаг установлен
#endif
    print_settings(&cfg, "Settings Before Encryption", FLASH_OFFSET);

    if (!settings_save(&cfg, FLASH_OFFSET)) {
        LOG_ERROR("Test 6 failed at save");
        return false;
    }

    const uint8_t *flash = (const uint8_t *)(XIP_BASE + FLASH_OFFSET);
    settings_t *flash_cfg = (settings_t *)flash;
    if (memcmp(flash_cfg->wifi_pass, test_pass, strlen(test_pass)) == 0) {
        LOG_ERROR("Test 6 failed: password not encrypted on flash");
        return false;
    }

    if (!settings_load(&loaded_cfg, FLASH_OFFSET)) {
        LOG_ERROR("Test 6 failed at load");
        return false;
    }

    print_settings(&loaded_cfg, "Loaded Settings After Decryption", FLASH_OFFSET);
    if (strcmp(cfg.wifi_pass, loaded_cfg.wifi_pass) != 0) {
        LOG_ERROR("Test 6 failed: decrypted password mismatch (expected %s, got %s)", 
                  cfg.wifi_pass, loaded_cfg.wifi_pass);
        return false;
    }

    LOG_INFO("Test 6 completed successfully");
    return true;
}

static bool test_invalid_values(void) {
    LOG_INFO("Test 7: Invalid Values Handling");
    if (!erase_flash_sector(FLASH_OFFSET)) return false;

    settings_t cfg;
    settings_init_default(&cfg);
    cfg.brightness = BRIGHTNESS_MAX + 1;
    if (settings_save(&cfg, FLASH_OFFSET)) {
        LOG_ERROR("Test 7 failed: accepted invalid brightness");
        return false;
    }

    LOG_INFO("Test 7 completed successfully");
    return true;
}

int main(void) {
    stdio_init_all();
    sleep_ms(1000);  // Ожидание стабилизации UART (TODO: Replace with proper uart_init in production firmware)

    printf(COLOR_YELLOW "\n=== Settings Management Test Suite ===\n" COLOR_RESET);
    printf(COLOR_CYAN "Expected Settings Size: " COLOR_RESET "%u bytes\n", (unsigned)sizeof(settings_t));

    int passed_tests = 0;
    int failed_tests = 0;
    bool result;

    result = test_default_settings();
    result ? passed_tests++ : failed_tests++;
    result = test_edge_cases();
    result ? passed_tests++ : failed_tests++;
    result = test_invalid_data();
    result ? passed_tests++ : failed_tests++;
    result = test_corrupted_data();
    result ? passed_tests++ : failed_tests++;
    result = test_buffer_overflow();
    result ? passed_tests++ : failed_tests++;
    result = test_encryption();
    result ? passed_tests++ : failed_tests++;
    result = test_invalid_values();
    result ? passed_tests++ : failed_tests++;

    if (failed_tests == 0) {
        LOG_INFO("All tests passed successfully");
    } else {
        LOG_ERROR("Some tests failed");
    }

    printf(COLOR_YELLOW "\nTest Summary:\n" COLOR_RESET);
    printf(COLOR_GREEN "Passed: %d\n" COLOR_RESET, passed_tests);
    printf(COLOR_RED "Failed: %d\n" COLOR_RESET, failed_tests);
    printf(COLOR_CYAN "Total: %d\n" COLOR_RESET, passed_tests + failed_tests);

    if (!erase_flash_sector(FLASH_OFFSET)) {
        LOG_ERROR("Failed to clear flash at end");
    }
    print_flash_contents(FLASH_OFFSET);
    printf(COLOR_YELLOW "=== Test Suite Completed ===\n" COLOR_RESET);

    return failed_tests == 0 ? 0 : 1;
}