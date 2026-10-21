#pragma once

// ============================================================
// Board profile
// ============================================================
#if defined(HELTEC_VISION_MASTER_E290) || defined(ARDUINO_HELTEC_VISION_MASTER_E290) || defined(Vision_Master_E290)
#define DEVICE_PROFILE_E290 1
#else
#define DEVICE_PROFILE_E290 0
#endif

#if defined(HELTEC_VISION_MASTER_E_213) || defined(ARDUINO_HELTEC_VISION_MASTER_E_213) || defined(Vision_Master_E213)
#define DEVICE_PROFILE_E213 1
#else
#define DEVICE_PROFILE_E213 0
#endif

#if DEVICE_PROFILE_E290
using BlockClockDisplay = EInkDisplay_VisionMasterE290;
static constexpr char DEVICE_MODEL_NAME[] = "E290";
static constexpr int DEVICE_DISPLAY_WIDTH = 296;
static constexpr int DEVICE_DISPLAY_HEIGHT = 128;
#elif DEVICE_PROFILE_E213
using BlockClockDisplay = EInkDisplay_VisionMasterE213;
static constexpr char DEVICE_MODEL_NAME[] = "E213";
static constexpr int DEVICE_DISPLAY_WIDTH = 250;
static constexpr int DEVICE_DISPLAY_HEIGHT = 122;
#else
using BlockClockDisplay = EInkDisplay_VisionMasterE213;
static constexpr char DEVICE_MODEL_NAME[] = "E213";
static constexpr int DEVICE_DISPLAY_WIDTH = 250;
static constexpr int DEVICE_DISPLAY_HEIGHT = 122;
#endif

// ============================================================
// Config model
// ============================================================
enum DataSourceMode : uint8_t {
  DATA_SOURCE_MQTT = 0,
  DATA_SOURCE_ONLINE = 1
};

enum DisplayThemeMode : uint8_t {
  DISPLAY_THEME_LIGHT = 0,
  DISPLAY_THEME_DARK = 1
};

enum RefreshIntervalUnit : uint8_t {
  REFRESH_INTERVAL_UNIT_MINUTES = 0,
  REFRESH_INTERVAL_UNIT_HOURS = 1,
  REFRESH_INTERVAL_UNIT_DAYS = 2
};

enum CurrencyCode : uint8_t {
  CURRENCY_USD = 0,
  CURRENCY_EUR = 1,
  CURRENCY_CHF = 2,
  CURRENCY_GBP = 3,
  CURRENCY_CAD = 4,
  CURRENCY_AUD = 5
};

enum BatteryDisplayMode : uint8_t {
  BATTERY_DISPLAY_HIDDEN = 0,
  BATTERY_DISPLAY_ICON = 1,
  BATTERY_DISPLAY_ICON_PERCENT = 2
};

struct DeviceConfig {
  uint32_t version;
  bool configured;
  char wifiSsid[33];
  char wifiPass[65];
  uint8_t dataSourceMode;
  uint8_t currencyCode;
  uint8_t timezoneIndex;
  uint8_t displayThemeMode;
  uint16_t refreshIntervalMinutes;
  uint8_t batteryDisplayMode;
  char mqttServer[64];
  uint16_t mqttPort;
  char mqttUser[64];
  char mqttPass[64];
  char topicHeight[96];
  char topicHalving[96];
  char topicHashrate[96];
  char topicPriceValue[96];
};

struct BlockData {
  char blockHeight[16];
  char blocksToHalving[16];
  char priceValue[16];
  char hashrateEhs[16];
  float lowFeeSatVb;
  float mediumFeeSatVb;
  float highFeeSatVb;
  time_t lastBlockTimestamp;
  float batteryVoltage;
  int batteryPercent;
  time_t updatedAt;
  bool wifiOk;
  bool dataOk;
};

static constexpr uint8_t DEFAULT_CURRENCY_CODE = CURRENCY_USD;
static constexpr uint8_t DEFAULT_TIMEZONE_INDEX = 2;  // US Mountain, matches TIMEZONE_OPTIONS
static constexpr DisplayThemeMode DEFAULT_DISPLAY_THEME_MODE = DISPLAY_THEME_LIGHT;
static constexpr char LOADING_TEXT[] = "---";

static constexpr uint32_t CONFIG_VERSION = 6;
static constexpr char CONFIG_NAMESPACE[] = "blockclock";
static constexpr char DIAGNOSTICS_NAMESPACE[] = "bcdiag";
static constexpr uint16_t CONFIG_PORTAL_PORT = 80;
static constexpr uint16_t CONFIG_DNS_PORT = 53;
static constexpr uint32_t CONFIG_PORTAL_DELAY_MS = 10;
static constexpr uint32_t CONFIG_PORTAL_EXIT_GRACE_MS = 2500;
static constexpr uint32_t CONFIG_PORTAL_RESTART_DELAY_MS = 1200;
static constexpr uint32_t FACTORY_RESET_HOLD_MS = 10000;
static constexpr uint16_t BUTTON_POLL_DELAY_MS = 20;
static constexpr uint16_t WIFI_CONNECT_TIMEOUT_MS = 15000;
static constexpr uint16_t WIFI_POLL_DELAY_MS = 250;
static constexpr uint16_t MQTT_CONNECT_TIMEOUT_MS = 5000;
static constexpr uint16_t MQTT_WAIT_FOR_MESSAGES_MS = 5000;
static constexpr uint16_t MQTT_RETRY_DELAY_MS = 500;
static constexpr uint16_t HTTP_TIMEOUT_MS = 9000;
static constexpr uint16_t NTP_SYNC_TIMEOUT_MS = 10000;
static constexpr uint16_t EINK_POWER_UP_DELAY_MS = 100;
static constexpr uint16_t BATTERY_ADC_SETTLE_DELAY_MS = 5;
static constexpr uint16_t MIN_REFRESH_INTERVAL_MINUTES = 1;
static constexpr uint16_t MAX_REFRESH_INTERVAL_MINUTES = 10080;
static constexpr uint16_t DEFAULT_REFRESH_INTERVAL_MINUTES = 10;
static constexpr uint64_t MICROSECONDS_PER_MINUTE = 60ULL * 1000000ULL;

static constexpr char DEFAULT_MQTT_SERVER[] = "mqtt.local";
static constexpr uint16_t DEFAULT_MQTT_PORT = 1883;
static constexpr char DEFAULT_MQTT_USER[] = "";
static constexpr char DEFAULT_MQTT_PASS[] = "";
static constexpr char DEFAULT_TOPIC_HEIGHT[] = "home/bitcoin/height";
static constexpr char DEFAULT_TOPIC_HALVING[] = "home/bitcoin/halving/blocks_remaining";
static constexpr char DEFAULT_TOPIC_HASHRATE[] = "home/bitcoin/hashrate_ehs";
static constexpr char DEFAULT_TOPIC_PRICE_VALUE[] = "home/bitcoin/price/usd";

static constexpr char MEMPOOL_TIP_HEIGHT_URL[] = "https://mempool.space/api/blocks/tip/height";
static constexpr uint32_t MEMPOOL_PRICE_MAX_AGE_SECONDS = 240;
static constexpr char MEMPOOL_PRICE_URL[] = "https://mempool.space/api/v1/prices";
static constexpr char MEMPOOL_HASHRATE_URL[] = "https://mempool.space/api/v1/mining/hashrate/3d";
static constexpr char MEMPOOL_PRECISE_FEES_URL[] = "https://mempool.space/api/v1/fees/precise";
static constexpr char MEMPOOL_BLOCK_HASH_URL_PREFIX[] = "https://mempool.space/api/block-height/";
static constexpr char MEMPOOL_BLOCK_URL_PREFIX[] = "https://mempool.space/api/block/";
static constexpr char COINGECKO_PRICE_URL_PREFIX[] = "https://api.coingecko.com/api/v3/simple/price?ids=bitcoin&vs_currencies=";
static constexpr char COINGECKO_PRICE_URL_SUFFIX[] = "&precision=0";
static constexpr uint32_t HALVING_INTERVAL_BLOCKS = 210000UL;

// ============================================================
// Hardware pins
// ============================================================
static constexpr int PIN_EINK_POWER = 18;
static constexpr int PIN_BAT_ADC = 7;
static constexpr int PIN_ADC_CTRL = 46;
static constexpr int PIN_FUNCTION_BUTTON = 21;
static constexpr int PIN_SETUP_BUTTON = 0;
static constexpr uint64_t FUNCTION_BUTTON_WAKE_MASK = (1ULL << PIN_FUNCTION_BUTTON);
static constexpr uint64_t SETUP_BUTTON_WAKE_MASK = (1ULL << PIN_SETUP_BUTTON);
static constexpr uint64_t BUTTON_WAKE_MASK = FUNCTION_BUTTON_WAKE_MASK | SETUP_BUTTON_WAKE_MASK;

static constexpr float VBAT_SCALE = 4.9f;

// ============================================================
// Setup AP
// ============================================================
static constexpr size_t DEVICE_ID_SIZE = 7;
static constexpr size_t AP_SSID_SIZE = 32;
static constexpr size_t AP_PASSWORD_SIZE = 20;
static constexpr char AP_SSID_PREFIX[] = "BLOCK_CLOCK_";
static constexpr char AP_PASSWORD_PREFIX[] = "setup-";
static const IPAddress CONFIG_AP_IP(192, 168, 4, 1);
static const IPAddress CONFIG_AP_GATEWAY(192, 168, 4, 1);
static const IPAddress CONFIG_AP_SUBNET(255, 255, 255, 0);

static constexpr char FIRMWARE_VERSION[] = "2026.06.12.1";
