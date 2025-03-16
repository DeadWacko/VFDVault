

#ifndef SETTINGS_H
#define SETTINGS_H

#include <stdint.h>
#include <stdbool.h>

// Константы
#define SETTINGS_MAGIC          0xCAFE0000  ///< Уникальный идентификатор структуры
#define SETTINGS_VERSION        0x0100      ///< Версия структуры
#define NTP_MAX_SERVERS         4          ///< Максимальное количество NTP-серверов
#define WIFI_SSID_MAX_LEN       32         ///< Максимальная длина SSID (31 символ + \0)
#define WIFI_PASS_MAX_LEN       64         ///< Максимальная длина пароля (63 символа + \0)
#define NTP_SERVER_MAX_LEN      64         ///< Максимальная длина адреса NTP-сервера
#define BRIGHTNESS_MIN          0          ///< Минимальная яркость
#define BRIGHTNESS_MAX          100        ///< Максимальная яркость
#define HOUR_MIN                0          ///< Минимальное значение часа
#define HOUR_MAX                23         ///< Максимальное значение часа
#define CRC32_ERROR             0xFFFFFFFF ///< Значение CRC32 при ошибке вычисления

// Флаги для settings_t.flags
#define FLAG_ADAPTIVE_BRIGHTNESS 0x01      ///< Адаптивная яркость
#define FLAG_NIGHT_MODE         0x02       ///< Ночной режим
#define FLAG_SETTINGS_ENCRYPTED 0x04       ///< Пароль WiFi зашифрован

// Флаги для settings_t.anim_flags
#define ANIM_FLAG_1             0x01       ///< Анимация 1
#define ANIM_FLAG_2             0x02       ///< Анимация 2
#define ANIM_FLAG_3             0x04       ///< Анимация 3
#define ANIM_FLAG_4             0x08       ///< Анимация 4
#define ANIM_FLAG_LAGS          0x10       ///< Включение задержек

#ifdef __GNUC__
#pragma pack(push, 1)  // Выравнивание 1 байт для Pico SDK
#else
#error "Unsupported compiler: only GCC is supported due to #pragma pack requirement"
#endif

/**
 * @brief Структура настроек, сохраняемая во флеш-памяти
 */
typedef struct {
    uint32_t magic;                         ///< Уникальный идентификатор (SETTINGS_MAGIC)
    uint16_t version;                       ///< Версия структуры (SETTINGS_VERSION)
    uint16_t size;                          ///< Размер структуры для масштабируемости
    char wifi_ssid[WIFI_SSID_MAX_LEN];      ///< WiFi SSID
    char wifi_pass[WIFI_PASS_MAX_LEN];      ///< WiFi пароль (зашифрован с XOR и SETTINGS_MAGIC, если FLAG_SETTINGS_ENCRYPTED установлен)
    uint8_t brightness;                     ///< Уровень яркости (0-100)
    uint8_t flags;                          ///< Биты: 0 - адаптивная яркость, 1 - ночной режим, 2 - шифрование пароля
    uint8_t night_off_hour;                 ///< Час выключения ночного режима (0-23)
    uint8_t night_on_hour;                  ///< Час включения ночного режима (0-23)
    uint16_t anim_flags;                    ///< Флаги анимаций (см. ANIM_FLAG_*)
    uint16_t anim_lags_period_s;            ///< Период задержек анимации в секундах
    char ntp_servers[NTP_MAX_SERVERS][NTP_SERVER_MAX_LEN]; ///< Адреса NTP-серверов
    uint16_t ntp_sync_period_minutes;       ///< Период синхронизации NTP в минутах
    uint32_t crc32;                         ///< CRC32 для проверки целостности
} settings_t;

#ifdef __GNUC__
#pragma pack(pop)
#endif

/**
 * @brief Инициализация структуры настроек значениями по умолчанию
 * @param cfg Указатель на структуру настроек
 */
void settings_init_default(settings_t *cfg);

/**
 * @brief Загрузка настроек из флеш-памяти
 * @param cfg Указатель на структуру для загрузки
 * @param flash_offset Смещение во флеш-памяти
 * @return true если загрузка успешна, false в противном случае
 */
bool settings_load(settings_t *cfg, const uint32_t flash_offset);

/**
 * @brief Сохранение настроек во флеш-память
 * @param cfg Указатель на структуру с настройками
 * @param flash_offset Смещение во флеш-памяти
 * @return true если сохранение успешно, false в противном случае
 */
bool settings_save(const settings_t *cfg, const uint32_t flash_offset);

/**
 * @brief Вычисление CRC32 для данных
 * @param data Указатель на данные
 * @param len Длина данных в байтах
 * @return Вычисленное значение CRC32 или CRC32_ERROR при ошибке ввода (data == NULL или len == 0)
 */
uint32_t calculate_crc32(const uint8_t *data, size_t len);

// Проверка размера структуры
#include "hardware/flash.h"
static_assert(sizeof(settings_t) <= FLASH_SECTOR_SIZE, "Settings structure too large for flash sector");

#endif // SETTINGS_H