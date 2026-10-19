#pragma once

static bool hasText(const char* s) {
  return s && s[0] != '\0';
}

static int clampInt(int value, int minValue, int maxValue) {
  if (value < minValue) return minValue;
  if (value > maxValue) return maxValue;
  return value;
}

static void safeCopy(char* dst, size_t dstSize, const char* src, size_t srcLen) {
  if (!dst || dstSize == 0) return;
  size_t n = (src && srcLen > 0) ? srcLen : 0;
  if (n > dstSize - 1) n = dstSize - 1;
  if (src && n > 0) memcpy(dst, src, n);
  dst[n] = '\0';
}

static void safeCopyCString(char* dst, size_t dstSize, const char* src) {
  if (!src) {
    safeCopy(dst, dstSize, "", 0);
    return;
  }
  safeCopy(dst, dstSize, src, strlen(src));
}

static void safeCopyString(char* dst, size_t dstSize, const String& src) {
  safeCopy(dst, dstSize, src.c_str(), src.length());
}

static bool checkboxArgIsTrue(const String& rawValue) {
  return rawValue == "1" || rawValue == "true" || rawValue == "on" || rawValue == "yes";
}

static uint16_t parsePort(const String& rawValue, uint16_t fallback) {
  const int parsed = rawValue.toInt();
  if (parsed <= 0 || parsed > 65535) return fallback;
  return (uint16_t)parsed;
}

static DataSourceMode sanitizeDataSourceMode(uint8_t rawValue) {
  return rawValue == DATA_SOURCE_MQTT ? DATA_SOURCE_MQTT : DATA_SOURCE_ONLINE;
}

static const char* dataSourceModeLabel(uint8_t rawValue) {
  return sanitizeDataSourceMode(rawValue) == DATA_SOURCE_MQTT ? "MQTT" : "ONLINE";
}

static DisplayThemeMode sanitizeThemeMode(uint8_t rawValue) {
  return rawValue == DISPLAY_THEME_DARK ? DISPLAY_THEME_DARK : DISPLAY_THEME_LIGHT;
}

static const char* themeModeLabel(DisplayThemeMode themeMode) {
  return themeMode == DISPLAY_THEME_DARK ? "DARK" : "LIGHT";
}

static RefreshIntervalUnit preferredRefreshIntervalUnit(uint16_t minutes) {
  if (minutes >= 1440 && (minutes % 1440) == 0) return REFRESH_INTERVAL_UNIT_DAYS;
  if (minutes >= 60 && (minutes % 60) == 0) return REFRESH_INTERVAL_UNIT_HOURS;
  return REFRESH_INTERVAL_UNIT_MINUTES;
}

static uint16_t refreshIntervalUnitMultiplier(RefreshIntervalUnit unit) {
  switch (unit) {
    case REFRESH_INTERVAL_UNIT_DAYS:
      return 1440;
    case REFRESH_INTERVAL_UNIT_HOURS:
      return 60;
    case REFRESH_INTERVAL_UNIT_MINUTES:
    default:
      return 1;
  }
}

static uint16_t refreshIntervalDisplayValue(uint16_t minutes, RefreshIntervalUnit unit) {
  const uint16_t multiplier = refreshIntervalUnitMultiplier(unit);
  if (multiplier == 0) return minutes;
  const uint16_t value = (uint16_t)(minutes / multiplier);
  return value > 0 ? value : 1;
}

static RefreshIntervalUnit parseRefreshIntervalUnit(const String& rawUnit) {
  const int unitValue = rawUnit.toInt();
  if (unitValue == (int)REFRESH_INTERVAL_UNIT_DAYS) return REFRESH_INTERVAL_UNIT_DAYS;
  if (unitValue == (int)REFRESH_INTERVAL_UNIT_HOURS) return REFRESH_INTERVAL_UNIT_HOURS;
  return REFRESH_INTERVAL_UNIT_MINUTES;
}

static uint16_t refreshIntervalMinutesFromForm(uint16_t rawValue, RefreshIntervalUnit unit) {
  const uint32_t totalMinutes = (uint32_t)rawValue * (uint32_t)refreshIntervalUnitMultiplier(unit);
  if (totalMinutes > 65535U) return 65535U;
  return (uint16_t)totalMinutes;
}

struct DisplayThemeColors {
  uint16_t foreground;
  uint16_t background;
};

static DisplayThemeColors getDisplayThemeColors(DisplayThemeMode themeMode) {
  if (themeMode == DISPLAY_THEME_DARK) {
    return { WHITE, BLACK };
  }
  return { BLACK, WHITE };
}

static DisplayThemeMode statusScreenThemeMode() {
  return deviceConfig.configured
    ? sanitizeThemeMode(deviceConfig.displayThemeMode)
    : DISPLAY_THEME_LIGHT;
}

static CurrencyCode sanitizeCurrencyCode(uint8_t rawValue) {
  switch (rawValue) {
    case CURRENCY_EUR:
      return CURRENCY_EUR;
    case CURRENCY_CHF:
      return CURRENCY_CHF;
    case CURRENCY_GBP:
      return CURRENCY_GBP;
    case CURRENCY_CAD:
      return CURRENCY_CAD;
    case CURRENCY_AUD:
      return CURRENCY_AUD;
    case CURRENCY_USD:
    default:
      return CURRENCY_USD;
  }
}

static const char* currencyCodeLabel(uint8_t rawValue) {
  switch (sanitizeCurrencyCode(rawValue)) {
    case CURRENCY_EUR:
      return "EUR";
    case CURRENCY_CHF:
      return "CHF";
    case CURRENCY_GBP:
      return "GBP";
    case CURRENCY_CAD:
      return "CAD";
    case CURRENCY_AUD:
      return "AUD";
    case CURRENCY_USD:
    default:
      return "USD";
  }
}

static const char* currencyCodeParam(uint8_t rawValue) {
  switch (sanitizeCurrencyCode(rawValue)) {
    case CURRENCY_EUR:
      return "eur";
    case CURRENCY_CHF:
      return "chf";
    case CURRENCY_GBP:
      return "gbp";
    case CURRENCY_CAD:
      return "cad";
    case CURRENCY_AUD:
      return "aud";
    case CURRENCY_USD:
    default:
      return "usd";
  }
}

static void buildDeviceId(char* dst, size_t dstSize) {
  if (!dst || dstSize == 0) return;
  uint64_t chipId = ESP.getEfuseMac();
  uint32_t hash = 2166136261UL;
  for (uint8_t i = 0; i < 6; i++) {
    hash ^= (uint8_t)((chipId >> (i * 8)) & 0xFF);
    hash *= 16777619UL;
  }

  static constexpr uint32_t BASE36_6 = 2176782336UL;
  uint32_t value = hash % BASE36_6;
  char id[7];
  id[6] = '\0';
  for (int i = 5; i >= 0; i--) {
    const uint8_t digit = value % 36;
    id[i] = (digit < 10) ? (char)('0' + digit) : (char)('A' + digit - 10);
    value /= 36;
  }
  safeCopyCString(dst, dstSize, id);
}

static void buildPortalCredentials() {
  buildDeviceId(deviceId, sizeof(deviceId));
  snprintf(portalApSsid, sizeof(portalApSsid), "%s%s", AP_SSID_PREFIX, deviceId);
  snprintf(portalApPassword, sizeof(portalApPassword), "%s%s", AP_PASSWORD_PREFIX, deviceId);
}

static void buildMqttClientId(char* dst, size_t dstSize) {
  if (!dst || dstSize == 0) return;
  if (deviceId[0] == '\0') buildDeviceId(deviceId, sizeof(deviceId));
  snprintf(dst, dstSize, "BlockClock-%s", deviceId);
}

static void normalizeDeviceConfig(DeviceConfig& cfg) {
  cfg.version = CONFIG_VERSION;
  cfg.dataSourceMode = (uint8_t)sanitizeDataSourceMode(cfg.dataSourceMode);
  cfg.currencyCode = (uint8_t)sanitizeCurrencyCode(cfg.currencyCode);
  cfg.displayThemeMode = (uint8_t)sanitizeThemeMode(cfg.displayThemeMode);
  cfg.refreshIntervalMinutes = (uint16_t)clampInt(
    (int)cfg.refreshIntervalMinutes,
    MIN_REFRESH_INTERVAL_MINUTES,
    MAX_REFRESH_INTERVAL_MINUTES
  );
  if (cfg.refreshIntervalMinutes == 0) cfg.refreshIntervalMinutes = DEFAULT_REFRESH_INTERVAL_MINUTES;
  if (cfg.mqttPort == 0) cfg.mqttPort = DEFAULT_MQTT_PORT;
  if (!hasText(cfg.mqttServer)) safeCopyCString(cfg.mqttServer, sizeof(cfg.mqttServer), DEFAULT_MQTT_SERVER);
  if (!hasText(cfg.topicHeight)) safeCopyCString(cfg.topicHeight, sizeof(cfg.topicHeight), DEFAULT_TOPIC_HEIGHT);
  if (!hasText(cfg.topicHalving)) safeCopyCString(cfg.topicHalving, sizeof(cfg.topicHalving), DEFAULT_TOPIC_HALVING);
  if (!hasText(cfg.topicHashrate)) safeCopyCString(cfg.topicHashrate, sizeof(cfg.topicHashrate), DEFAULT_TOPIC_HASHRATE);
  if (!hasText(cfg.topicPriceValue)) safeCopyCString(cfg.topicPriceValue, sizeof(cfg.topicPriceValue), DEFAULT_TOPIC_PRICE_VALUE);
}

static void applyDefaultDeviceConfig(DeviceConfig& cfg) {
  memset(&cfg, 0, sizeof(cfg));
  cfg.version = CONFIG_VERSION;
  cfg.configured = false;
  cfg.dataSourceMode = DATA_SOURCE_ONLINE;
  cfg.currencyCode = DEFAULT_CURRENCY_CODE;
  cfg.displayThemeMode = (uint8_t)DEFAULT_DISPLAY_THEME_MODE;
  cfg.refreshIntervalMinutes = DEFAULT_REFRESH_INTERVAL_MINUTES;
  cfg.showBatteryPercent = true;
  safeCopyCString(cfg.mqttServer, sizeof(cfg.mqttServer), DEFAULT_MQTT_SERVER);
  cfg.mqttPort = DEFAULT_MQTT_PORT;
  safeCopyCString(cfg.mqttUser, sizeof(cfg.mqttUser), DEFAULT_MQTT_USER);
  safeCopyCString(cfg.mqttPass, sizeof(cfg.mqttPass), DEFAULT_MQTT_PASS);
  safeCopyCString(cfg.topicHeight, sizeof(cfg.topicHeight), DEFAULT_TOPIC_HEIGHT);
  safeCopyCString(cfg.topicHalving, sizeof(cfg.topicHalving), DEFAULT_TOPIC_HALVING);
  safeCopyCString(cfg.topicHashrate, sizeof(cfg.topicHashrate), DEFAULT_TOPIC_HASHRATE);
  safeCopyCString(cfg.topicPriceValue, sizeof(cfg.topicPriceValue), DEFAULT_TOPIC_PRICE_VALUE);
}

static bool loadDeviceConfig(DeviceConfig& cfg) {
  applyDefaultDeviceConfig(cfg);
  if (!preferences.begin(CONFIG_NAMESPACE, true)) return false;
  const uint32_t storedVersion = preferences.getUInt("cfg_ver", 0);
  if (storedVersion != CONFIG_VERSION) {
    preferences.end();
    return false;
  }
  cfg.configured        = preferences.getBool("cfg_ok", false);
  preferences.getString("wifissid",    cfg.wifiSsid,       sizeof(cfg.wifiSsid));
  preferences.getString("wifipass",    cfg.wifiPass,       sizeof(cfg.wifiPass));
  cfg.dataSourceMode    = (uint8_t)preferences.getUInt("datasrc",    DATA_SOURCE_ONLINE);
  cfg.currencyCode      = (uint8_t)preferences.getUInt("currency",   DEFAULT_CURRENCY_CODE);
  cfg.displayThemeMode  = (uint8_t)preferences.getUInt("theme",      DEFAULT_DISPLAY_THEME_MODE);
  cfg.refreshIntervalMinutes = (uint16_t)preferences.getUInt("refreshmin", DEFAULT_REFRESH_INTERVAL_MINUTES);
  cfg.showBatteryPercent = preferences.getBool("showbatt", true);
  preferences.getString("mqttsrv",     cfg.mqttServer,     sizeof(cfg.mqttServer));
  cfg.mqttPort          = (uint16_t)preferences.getUInt("mqttport",  DEFAULT_MQTT_PORT);
  preferences.getString("mqttuser",    cfg.mqttUser,       sizeof(cfg.mqttUser));
  preferences.getString("mqttpass",    cfg.mqttPass,       sizeof(cfg.mqttPass));
  preferences.getString("topicheight", cfg.topicHeight,    sizeof(cfg.topicHeight));
  preferences.getString("topichalv",   cfg.topicHalving,   sizeof(cfg.topicHalving));
  preferences.getString("topichash",   cfg.topicHashrate,  sizeof(cfg.topicHashrate));
  preferences.getString("topicprice",  cfg.topicPriceValue,sizeof(cfg.topicPriceValue));
  preferences.end();
  normalizeDeviceConfig(cfg);
  return true;
}

static bool saveDeviceConfig(const DeviceConfig& cfg) {
  DeviceConfig copy = cfg;
  normalizeDeviceConfig(copy);
  if (!preferences.begin(CONFIG_NAMESPACE, false)) return false;
  // Write critical keys first and verify they took; silent NVS failures are possible
  // on fragmented flash. Do NOT call preferences.clear() before writing — the erase+
  // rewrite cycle is the main cause of silent write failures on worn NVS pages.
  const size_t verWritten = preferences.putUInt("cfg_ver", CONFIG_VERSION);
  const size_t okWritten  = preferences.putBool("cfg_ok",  true);
  if (verWritten == 0 || okWritten == 0) {
    preferences.end();
    return false;
  }
  preferences.putString("wifissid", copy.wifiSsid);
  preferences.putString("wifipass", copy.wifiPass);
  preferences.putUInt("datasrc",    (uint32_t)copy.dataSourceMode);
  preferences.putUInt("currency",   (uint32_t)copy.currencyCode);
  preferences.putUInt("theme",      (uint32_t)copy.displayThemeMode);
  preferences.putUInt("refreshmin", (uint32_t)copy.refreshIntervalMinutes);
  preferences.putBool("showbatt",   copy.showBatteryPercent);
  preferences.putString("mqttsrv",  copy.mqttServer);
  preferences.putUInt("mqttport",   (uint32_t)copy.mqttPort);
  preferences.putString("mqttuser", copy.mqttUser);
  preferences.putString("mqttpass", copy.mqttPass);
  preferences.putString("topicheight", copy.topicHeight);
  preferences.putString("topichalv",   copy.topicHalving);
  preferences.putString("topichash",   copy.topicHashrate);
  preferences.putString("topicprice",  copy.topicPriceValue);
  preferences.end();
  return true;
}

static bool clearSavedDeviceConfig() {
  // Erase the entire NVS partition so the next save starts with clean flash pages.
  // namespace-level clear() does not reclaim worn pages; nvs_flash_erase() does.
  nvs_flash_erase();
  nvs_flash_init();
  return true;
}

static void markWelcomeShown() {
  if (!preferences.begin(CONFIG_NAMESPACE, false)) return;
  preferences.putBool("welcome_seen", true);
  preferences.end();
}

static bool hasWelcomeBeenShown() {
  if (!preferences.begin(CONFIG_NAMESPACE, true)) return false;
  const bool seen = preferences.getBool("welcome_seen", false);
  preferences.end();
  return seen;
}

static void applySecretsBootstrap(DeviceConfig& cfg) {
#if HAS_SECRETS && USE_SECRETS_BOOTSTRAP
  if (!cfg.configured && strlen(BLOCKCLOCK_WIFI_SSID) > 0) {
    safeCopyCString(cfg.wifiSsid, sizeof(cfg.wifiSsid), BLOCKCLOCK_WIFI_SSID);
    safeCopyCString(cfg.wifiPass, sizeof(cfg.wifiPass), BLOCKCLOCK_WIFI_PASS);
    cfg.dataSourceMode = (uint8_t)sanitizeDataSourceMode(BLOCKCLOCK_DATA_SOURCE_MODE);
    cfg.currencyCode = (uint8_t)sanitizeCurrencyCode(BLOCKCLOCK_CURRENCY_CODE);
    cfg.displayThemeMode = (uint8_t)sanitizeThemeMode(BLOCKCLOCK_DISPLAY_THEME_MODE);
    safeCopyCString(cfg.mqttServer, sizeof(cfg.mqttServer), BLOCKCLOCK_MQTT_SERVER);
    cfg.mqttPort = BLOCKCLOCK_MQTT_PORT;
    safeCopyCString(cfg.mqttUser, sizeof(cfg.mqttUser), BLOCKCLOCK_MQTT_USER);
    safeCopyCString(cfg.mqttPass, sizeof(cfg.mqttPass), BLOCKCLOCK_MQTT_PASS);
    safeCopyCString(cfg.topicHeight, sizeof(cfg.topicHeight), BLOCKCLOCK_TOPIC_HEIGHT);
    safeCopyCString(cfg.topicHalving, sizeof(cfg.topicHalving), BLOCKCLOCK_TOPIC_HALVING);
    safeCopyCString(cfg.topicHashrate, sizeof(cfg.topicHashrate), BLOCKCLOCK_TOPIC_HASHRATE);
    safeCopyCString(cfg.topicPriceValue, sizeof(cfg.topicPriceValue), BLOCKCLOCK_TOPIC_PRICE_VALUE);
    normalizeDeviceConfig(cfg);
    cfg.configured = true;
    saveDeviceConfig(cfg);
  }
#else
  (void)cfg;
#endif
}

static BlockData emptyBlockData() {
  BlockData data = {};
  safeCopyCString(data.blockHeight, sizeof(data.blockHeight), "--");
  safeCopyCString(data.blocksToHalving, sizeof(data.blocksToHalving), "--");
  safeCopyCString(data.priceValue, sizeof(data.priceValue), "--");
  safeCopyCString(data.hashrateEhs, sizeof(data.hashrateEhs), "--");
  data.batteryVoltage = 0.0f;
  data.batteryPercent = -1;
  data.updatedAt = 0;
  data.wifiOk = false;
  data.dataOk = false;
  return data;
}

static bool isUsefulDataValue(const char* value) {
  return hasText(value) && strcmp(value, "--") != 0 && strcmp(value, LOADING_TEXT) != 0;
}

static bool hasUsefulBlockData(const BlockData& data) {
  return isUsefulDataValue(data.blockHeight) ||
    isUsefulDataValue(data.blocksToHalving) ||
    isUsefulDataValue(data.priceValue) ||
    isUsefulDataValue(data.hashrateEhs);
}

static int batteryPercentFromVoltage(float v) {
  // Calibrated from a logged discharge run; SOC is based on elapsed runtime.
  // The last clean sample was 3.26V, so the small missing tail is treated as empty.
  static constexpr int NUM_POINTS = 24;
  static constexpr float voltTable[NUM_POINTS] = {
    3.20f, 3.26f, 3.32f, 3.36f, 3.42f, 3.52f, 3.62f, 3.67f,
    3.71f, 3.74f, 3.76f, 3.78f, 3.82f, 3.87f, 3.89f, 3.90f,
    3.91f, 3.92f, 3.94f, 3.95f, 3.97f, 4.00f, 4.02f, 4.08f
  };
  static constexpr int socTable[NUM_POINTS] = {
    0, 0, 1, 2, 5, 10, 15, 20,
    25, 30, 38, 48, 55, 60, 63, 69,
    78, 83, 88, 91, 94, 98, 99, 100
  };

  if (v <= voltTable[0]) return socTable[0];
  if (v >= voltTable[NUM_POINTS - 1]) return socTable[NUM_POINTS - 1];

  for (int i = 0; i < NUM_POINTS - 1; i++) {
    float v1 = voltTable[i];
    float v2 = voltTable[i + 1];
    if (v >= v1 && v <= v2) {
      int soc1 = socTable[i];
      int soc2 = socTable[i + 1];
      float t = (v - v1) / (v2 - v1);
      float soc = soc1 + t * (soc2 - soc1);
      if (soc < 0) soc = 0;
      if (soc > 100) soc = 100;
      return (int)(soc + 0.5f);
    }
  }
  return 0;
}

static bool isValidBatteryGaugeTime(time_t t) {
  return t >= 1700000000;
}

static void startBatteryFullHold(time_t now) {
  batteryGaugeFullHoldActive = true;
  if (isValidBatteryGaugeTime(now)) {
    batteryGaugeFullHoldStartedUnixTime = now;
  } else if (!isValidBatteryGaugeTime(batteryGaugeFullHoldStartedUnixTime)) {
    batteryGaugeFullHoldStartedUnixTime = 0;
  }
}

static int batteryFullHoldAllowedDrop(time_t now) {
  static constexpr time_t SECONDS_PER_TOP_PERCENT = 24 * 60 * 60;
  int allowedDrop = 1;
  if (
    batteryGaugeFullHoldActive &&
    !isValidBatteryGaugeTime(batteryGaugeFullHoldStartedUnixTime) &&
    isValidBatteryGaugeTime(now)
  ) {
    batteryGaugeFullHoldStartedUnixTime = now;
  }
  if (
    isValidBatteryGaugeTime(now) &&
    isValidBatteryGaugeTime(batteryGaugeFullHoldStartedUnixTime) &&
    now >= batteryGaugeFullHoldStartedUnixTime
  ) {
    allowedDrop += (int)((now - batteryGaugeFullHoldStartedUnixTime) / SECONDS_PER_TOP_PERCENT);
  }
  return clampInt(allowedDrop, 1, 100);
}

static int stabilizedBatteryPercentFromVoltage(float v, time_t now = 0) {
  const int rawPercent = clampInt(batteryPercentFromVoltage(v), 0, 100);
  const bool stateLooksValid = batteryGaugeStateValid
    && batteryGaugePercent >= 0
    && batteryGaugePercent <= 100
    && batteryGaugeReferenceVoltage > 0.0f
    && batteryGaugeReferenceVoltage < 5.0f;

  if (!stateLooksValid || !(v > 0.0f)) {
    batteryGaugeStateValid = true;
    batteryGaugePercent = rawPercent;
    batteryGaugeReferenceVoltage = v;
    batteryGaugeFullHoldActive = false;
    batteryGaugeFullHoldStartedUnixTime = 0;
    if (v >= 4.02f && rawPercent >= 99) {
      startBatteryFullHold(now);
    }
    return rawPercent;
  }

  static constexpr float RECHARGE_DETECT_VOLTAGE_RISE = 0.06f;
  static constexpr float FULL_HOLD_START_VOLTAGE = 4.02f;
  static constexpr float FULL_SETTLED_MIN_VOLTAGE = 3.95f;
  static constexpr int FULL_SETTLED_MIN_RAW_PERCENT = 90;

  int stablePercent = rawPercent;
  const bool looksFullNow = v >= FULL_HOLD_START_VOLTAGE && rawPercent >= 99;
  if (looksFullNow) {
    startBatteryFullHold(now);
  }

  if (rawPercent > batteryGaugePercent) {
    const bool looksLikeRecharge =
      v >= (batteryGaugeReferenceVoltage + RECHARGE_DETECT_VOLTAGE_RISE) ||
      looksFullNow;
    if (!looksLikeRecharge) {
      stablePercent = batteryGaugePercent;
    }
  }

  const bool settlingAfterFreshFull =
    rawPercent < batteryGaugePercent &&
    rawPercent >= FULL_SETTLED_MIN_RAW_PERCENT &&
    v >= FULL_SETTLED_MIN_VOLTAGE &&
    (
      batteryGaugeFullHoldActive ||
      (batteryGaugePercent >= 99 && batteryGaugeReferenceVoltage >= FULL_HOLD_START_VOLTAGE)
    );
  if (settlingAfterFreshFull) {
    if (!batteryGaugeFullHoldActive) {
      startBatteryFullHold(now);
    }
    const int topHoldFloor = 100 - batteryFullHoldAllowedDrop(now);
    if (stablePercent < topHoldFloor) {
      stablePercent = topHoldFloor;
    }
  }

  if (
    batteryGaugeFullHoldActive &&
    !looksFullNow &&
    (
      stablePercent <= rawPercent ||
      rawPercent < FULL_SETTLED_MIN_RAW_PERCENT ||
      v < FULL_SETTLED_MIN_VOLTAGE
    )
  ) {
    batteryGaugeFullHoldActive = false;
    batteryGaugeFullHoldStartedUnixTime = 0;
  }

  if (stablePercent < batteryGaugePercent) {
    batteryGaugeReferenceVoltage = v;
  } else if (stablePercent == batteryGaugePercent && v < batteryGaugeReferenceVoltage) {
    batteryGaugeReferenceVoltage = v;
  } else if (stablePercent > batteryGaugePercent) {
    batteryGaugeReferenceVoltage = v;
  }
  batteryGaugePercent = stablePercent;
  batteryGaugeStateValid = true;
  return stablePercent;
}

static float readBatteryVoltage() {
  pinMode(PIN_ADC_CTRL, OUTPUT);
  digitalWrite(PIN_ADC_CTRL, HIGH);
  delay(BATTERY_ADC_SETTLE_DELAY_MS);

  pinMode(PIN_BAT_ADC, INPUT);
  analogReadResolution(12);
  analogSetPinAttenuation(PIN_BAT_ADC, ADC_0db);

  static constexpr uint8_t BATTERY_SAMPLE_COUNT = 12;
  uint32_t millivoltsSum = 0;
  for (uint8_t i = 0; i < BATTERY_SAMPLE_COUNT; i++) {
    millivoltsSum += (uint32_t)analogReadMilliVolts(PIN_BAT_ADC);
    delay(2);
  }

  digitalWrite(PIN_ADC_CTRL, LOW);

  const float adcVolts = ((float)millivoltsSum / (float)BATTERY_SAMPLE_COUNT) / 1000.0f;
  return adcVolts * VBAT_SCALE;
}

static bool syncClockFromNtp(uint16_t timeoutMs = NTP_SYNC_TIMEOUT_MS) {
  configTzTime("MST7MDT,M3.2.0,M11.1.0", "pool.ntp.org", "time.nist.gov");
  uint32_t start = millis();
  time_t now = time(nullptr);
  while (now < 1700000000 && (millis() - start) < timeoutMs) {
    delay(WIFI_POLL_DELAY_MS);
    now = time(nullptr);
  }
  return now >= 1700000000;
}

static String formatTimestamp(time_t when) {
  if (when < 1700000000) return "---";
  struct tm tmValue = {};
  if (!localtime_r(&when, &tmValue)) return "---";
  char buffer[28];
  strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M %Z", &tmValue);
  return String(buffer);
}
