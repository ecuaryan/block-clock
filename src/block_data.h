#pragma once

static bool gotHeight = false;
static bool gotHalving = false;
static bool gotHashrate = false;
static bool gotPrice = false;
static BlockData* mqttTargetData = nullptr;

static bool connectToNetwork(const char* ssid, const char* pass, uint16_t timeoutMs, bool keepPortalAp) {
  WiFi.persistent(false);
  WiFi.setSleep(false);
  WiFi.mode(keepPortalAp ? WIFI_AP_STA : WIFI_STA);
  WiFi.disconnect(false, false);
  delay(150);
  WiFi.begin(ssid, pass);

  const uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED && (millis() - start) < timeoutMs) {
    delay(WIFI_POLL_DELAY_MS);
  }
  return WiFi.status() == WL_CONNECTED;
}

// Tries the primary network, then the backup. `preferSecondary` remembers which one worked last
// (pass a pointer to a value kept across deep sleep) so a clock that travels tries its current
// network first instead of burning a full timeout on the one it left behind.
static bool connectWiFi(const DeviceConfig& cfg, uint16_t timeoutMs, bool keepPortalAp, bool* preferSecondary = nullptr) {
  if (!hasText(cfg.wifiSsid)) return false;
  const bool hasSecondary = hasText(cfg.wifiSsid2);
  const bool secondFirst = hasSecondary && preferSecondary && *preferSecondary;

  for (int attempt = 0; attempt < (hasSecondary ? 2 : 1); attempt++) {
    const bool useSecondary = secondFirst ? attempt == 0 : attempt == 1;
    const char* ssid = useSecondary ? cfg.wifiSsid2 : cfg.wifiSsid;
    const char* pass = useSecondary ? cfg.wifiPass2 : cfg.wifiPass;
    if (connectToNetwork(ssid, pass, timeoutMs, keepPortalAp)) {
      if (preferSecondary) *preferSecondary = useSecondary;
      return true;
    }
  }
  return false;
}

static void mqttCallback(char* topic, byte* payload, unsigned int length) {
  if (!topic || !payload || !mqttTargetData) return;

  char value[32];
  safeCopy(value, sizeof(value), (const char*)payload, length);

  if (strcmp(topic, deviceConfig.topicHeight) == 0) {
    safeCopyCString(mqttTargetData->blockHeight, sizeof(mqttTargetData->blockHeight), value);
    gotHeight = true;
  } else if (strcmp(topic, deviceConfig.topicHalving) == 0) {
    safeCopyCString(mqttTargetData->blocksToHalving, sizeof(mqttTargetData->blocksToHalving), value);
    gotHalving = true;
  } else if (strcmp(topic, deviceConfig.topicHashrate) == 0) {
    safeCopyCString(mqttTargetData->hashrateEhs, sizeof(mqttTargetData->hashrateEhs), value);
    gotHashrate = true;
  } else if (strcmp(topic, deviceConfig.topicPriceValue) == 0) {
    safeCopyCString(mqttTargetData->priceValue, sizeof(mqttTargetData->priceValue), value);
    gotPrice = true;
  }
}

static bool connectMQTT(const DeviceConfig& cfg, uint16_t timeoutMs) {
  if (!hasText(cfg.mqttServer) || cfg.mqttPort == 0) return false;

  mqttClient.setServer(cfg.mqttServer, cfg.mqttPort);
  mqttClient.setCallback(mqttCallback);

  char clientId[32];
  buildMqttClientId(clientId, sizeof(clientId));

  const uint32_t start = millis();
  while (!mqttClient.connected() && (millis() - start) < timeoutMs) {
    bool connected;
    if (hasText(cfg.mqttUser)) {
      connected = mqttClient.connect(clientId, cfg.mqttUser, cfg.mqttPass);
    } else {
      connected = mqttClient.connect(clientId);
    }
    if (connected) break;
    delay(MQTT_RETRY_DELAY_MS);
  }

  if (!mqttClient.connected()) return false;

  if (hasText(cfg.topicHeight)) mqttClient.subscribe(cfg.topicHeight);
  if (hasText(cfg.topicHalving)) mqttClient.subscribe(cfg.topicHalving);
  if (hasText(cfg.topicHashrate)) mqttClient.subscribe(cfg.topicHashrate);
  if (hasText(cfg.topicPriceValue)) mqttClient.subscribe(cfg.topicPriceValue);
  return true;
}

static bool fetchBlockDataFromMqtt(const DeviceConfig& cfg, BlockData& outData) {
  gotHeight = gotHalving = gotHashrate = gotPrice = false;
  mqttTargetData = &outData;

  if (!connectMQTT(cfg, MQTT_CONNECT_TIMEOUT_MS)) {
    mqttTargetData = nullptr;
    return false;
  }

  const uint32_t start = millis();
  while ((millis() - start) < MQTT_WAIT_FOR_MESSAGES_MS) {
    mqttClient.loop();
    if (gotHeight && gotHalving && gotHashrate && gotPrice) break;
    delay(10);
  }

  mqttTargetData = nullptr;
  return gotHeight || gotHalving || gotHashrate || gotPrice;
}

static bool configureOnlineClient(WiFiClientSecure& client) {
  client.setInsecure();
  return true;
}

static bool httpGetString(const char* url, String& payload) {
  if (WiFi.status() != WL_CONNECTED || !url || !url[0]) return false;

  WiFiClientSecure secureClient;
  configureOnlineClient(secureClient);

  HTTPClient http;
  http.setTimeout(HTTP_TIMEOUT_MS);
  http.setConnectTimeout(HTTP_TIMEOUT_MS);
  if (!http.begin(secureClient, url)) return false;

  http.addHeader("Accept", "application/json,text/plain");
  http.addHeader("User-Agent", "BlockClock/2026");
  const int httpCode = http.GET();
  if (httpCode != HTTP_CODE_OK) {
    http.end();
    return false;
  }

  payload = http.getString();
  http.end();
  payload.trim();
  return payload.length() > 0;
}

static uint32_t blocksToNextHalving(uint32_t height) {
  const uint32_t nextHalving = ((height / HALVING_INTERVAL_BLOCKS) + 1UL) * HALVING_INTERVAL_BLOCKS;
  return nextHalving > height ? (nextHalving - height) : 0;
}

static bool fetchOnlineHeight(BlockData& outData) {
  String payload;
  if (!httpGetString(MEMPOOL_TIP_HEIGHT_URL, payload)) return false;
  const uint32_t height = (uint32_t)payload.toInt();
  if (height == 0) return false;

  snprintf(outData.blockHeight, sizeof(outData.blockHeight), "%lu", (unsigned long)height);
  snprintf(outData.blocksToHalving, sizeof(outData.blocksToHalving), "%lu", (unsigned long)blocksToNextHalving(height));
  return true;
}

static bool fetchMempoolPrice(const DeviceConfig& cfg, BlockData& outData) {
  String payload;
  if (!httpGetString(MEMPOOL_PRICE_URL, payload)) return false;

  StaticJsonDocument<384> doc;
  if (deserializeJson(doc, payload)) return false;
  // mempool.space can serve a frozen price while still returning 200 OK; if its
  // timestamp is over 4 minutes old, fail so the caller falls back to CoinGecko.
  const time_t now = time(nullptr);
  const time_t priceTime = doc["time"].as<time_t>();
  if (now >= 1700000000 && priceTime > 0 && (now - priceTime) > MEMPOOL_PRICE_MAX_AGE_SECONDS) return false;
  const char* currency = currencyCodeLabel(cfg.currencyCode);
  if (!doc[currency].is<float>() && !doc[currency].is<int>()) return false;
  const long price = doc[currency].as<long>();
  if (price <= 0) return false;
  snprintf(outData.priceValue, sizeof(outData.priceValue), "%ld", price);
  return true;
}

static bool fetchCoinGeckoPrice(const DeviceConfig& cfg, BlockData& outData) {
  String payload;
  String url = COINGECKO_PRICE_URL_PREFIX;
  url += currencyCodeParam(cfg.currencyCode);
  url += COINGECKO_PRICE_URL_SUFFIX;
  if (!httpGetString(url.c_str(), payload)) return false;

  StaticJsonDocument<256> doc;
  if (deserializeJson(doc, payload)) return false;
  const char* currency = currencyCodeParam(cfg.currencyCode);
  if (!doc["bitcoin"][currency].is<float>() && !doc["bitcoin"][currency].is<int>()) return false;
  const long price = doc["bitcoin"][currency].as<long>();
  if (price <= 0) return false;
  snprintf(outData.priceValue, sizeof(outData.priceValue), "%ld", price);
  return true;
}

static bool fetchOnlineHashrate(BlockData& outData) {
  String payload;
  if (!httpGetString(MEMPOOL_HASHRATE_URL, payload)) return false;

  StaticJsonDocument<1024> doc;
  if (deserializeJson(doc, payload)) return false;
  const double rawHashrate = doc["currentHashrate"].as<double>();
  if (!(rawHashrate > 0.0)) return false;
  const long ehs = lround(rawHashrate / 1000000000000000000.0);
  if (ehs <= 0) return false;
  snprintf(outData.hashrateEhs, sizeof(outData.hashrateEhs), "%ld", ehs);
  return true;
}

static void fetchOnlinePreciseFees(BlockData& outData) {
  String payload;
  if (!httpGetString(MEMPOOL_PRECISE_FEES_URL, payload)) return;

  StaticJsonDocument<256> doc;
  if (deserializeJson(doc, payload)) return;
  if (doc["hourFee"].is<float>() || doc["hourFee"].is<int>()) {
    outData.lowFeeSatVb = doc["hourFee"].as<float>();
  }
  if (doc["halfHourFee"].is<float>() || doc["halfHourFee"].is<int>()) {
    outData.mediumFeeSatVb = doc["halfHourFee"].as<float>();
  }
  if (doc["fastestFee"].is<float>() || doc["fastestFee"].is<int>()) {
    outData.highFeeSatVb = doc["fastestFee"].as<float>();
  }
}

static void fetchOnlineLastBlockInfo(BlockData& outData) {
  if (!hasText(outData.blockHeight) || !isUsefulDataValue(outData.blockHeight)) return;

  String hashUrl = MEMPOOL_BLOCK_HASH_URL_PREFIX;
  hashUrl += outData.blockHeight;
  String blockHash;
  if (!httpGetString(hashUrl.c_str(), blockHash)) return;

  String blockUrl = MEMPOOL_BLOCK_URL_PREFIX;
  blockUrl += blockHash;
  String payload;
  if (!httpGetString(blockUrl.c_str(), payload)) return;

  StaticJsonDocument<96> filter;
  filter["timestamp"] = true;
  filter["extras"]["totalFees"] = true;
  StaticJsonDocument<192> doc;
  if (deserializeJson(doc, payload, DeserializationOption::Filter(filter))) return;
  if (doc["timestamp"].is<time_t>()) {
    const time_t blockTimestamp = doc["timestamp"].as<time_t>();
    if (blockTimestamp > 0) outData.lastBlockTimestamp = blockTimestamp;
  }
  if (doc["extras"]["totalFees"].is<int64_t>()) {
    const int64_t totalFees = doc["extras"]["totalFees"].as<int64_t>();
    if (totalFees >= 0) outData.lastBlockFeesSats = totalFees;
  }
}

// Unix time for a UTC calendar date (days-from-civil), since newlib has no timegm.
static time_t unixFromUtc(int year, int month, int day, int hour, int minute, int second) {
  year -= month <= 2;
  const int era = (year >= 0 ? year : year - 399) / 400;
  const unsigned yearOfEra = (unsigned)(year - era * 400);
  const unsigned dayOfYear = (153 * (month + (month > 2 ? -3 : 9)) + 2) / 5 + day - 1;
  const unsigned dayOfEra = yearOfEra * 365 + yearOfEra / 4 - yearOfEra / 100 + dayOfYear;
  const long days = (long)era * 146097L + (long)dayOfEra - 719468L;
  return (time_t)(days * 86400L + hour * 3600L + minute * 60L + second);
}

static bool parseIsoUtc(const char* text, time_t& out) {
  int year, month, day, hour, minute, second;
  if (!text || sscanf(text, "%d-%d-%dT%d:%d:%d", &year, &month, &day, &hour, &minute, &second) != 6) return false;
  out = unixFromUtc(year, month, day, hour, minute, second);
  return out > 0;
}

// The all-time high rarely changes, so it is kept in flash. A hard reset or power loss clears RTC
// memory, and CoinGecko can turn the first request after that away with a rate limit.
static void loadSavedAth(const DeviceConfig& cfg, BlockData& data) {
  if (data.athPrice > 0) return;
  if (!preferences.begin(CONFIG_NAMESPACE, true)) return;
  const long price = (long)preferences.getLong("ath_price", 0);
  const uint8_t currency = preferences.getUChar("ath_cur", 255);
  if (price > 0 && currency == cfg.currencyCode) {
    data.athPrice = price;
    data.athTimestamp = (time_t)preferences.getLong64("ath_time", 0);
    data.athCheckedAt = (time_t)preferences.getLong64("ath_chk", 0);
    data.athCurrency = currency;
  }
  preferences.end();
}

static void saveAth(const BlockData& data) {
  if (!preferences.begin(CONFIG_NAMESPACE, false)) return;
  preferences.putLong("ath_price", (int32_t)data.athPrice);
  preferences.putLong64("ath_time", (int64_t)data.athTimestamp);
  preferences.putLong64("ath_chk", (int64_t)data.athCheckedAt);
  preferences.putUChar("ath_cur", data.athCurrency);
  preferences.end();
}

// The all-time high and its date, from CoinGecko. Checked every few hours, not every wake.
static void fetchCoinGeckoAth(const DeviceConfig& cfg, BlockData& outData) {
  const time_t now = time(nullptr);
  if (now < 1700000000) return;
  if (outData.athPrice > 0 && outData.athCurrency != cfg.currencyCode) {
    outData.athPrice = 0;
    outData.athTimestamp = 0;
    outData.athCheckedAt = 0;
  }
  const uint32_t waitSeconds = outData.athPrice > 0 ? ATH_REFRESH_SECONDS : ATH_RETRY_SECONDS;
  if (now >= outData.athCheckedAt && (now - outData.athCheckedAt) < (time_t)waitSeconds) return;
  outData.athCheckedAt = now;  // count this as an attempt, so a rate limit does not retry on every wake

  String url = COINGECKO_MARKETS_URL_PREFIX;
  url += currencyCodeParam(cfg.currencyCode);
  url += COINGECKO_MARKETS_URL_SUFFIX;
  String payload;
  if (!httpGetString(url.c_str(), payload)) return;

  StaticJsonDocument<64> filter;
  filter[0]["ath"] = true;
  filter[0]["ath_date"] = true;
  StaticJsonDocument<192> doc;
  if (deserializeJson(doc, payload, DeserializationOption::Filter(filter))) return;
  const double ath = doc[0]["ath"] | 0.0;
  time_t athTime = 0;
  if (ath <= 0.0 || !parseIsoUtc(doc[0]["ath_date"] | "", athTime)) return;

  outData.athPrice = lround(ath);
  outData.athTimestamp = athTime;
  outData.athCurrency = cfg.currencyCode;
  saveAth(outData);
}

static bool fetchBlockDataOnline(const DeviceConfig& cfg, BlockData& outData) {
  bool ok = false;
  const bool heightOk = fetchOnlineHeight(outData);
  ok = heightOk || ok;
  ok = fetchOnlineHashrate(outData) || ok;
  if (!fetchMempoolPrice(cfg, outData)) {
    ok = fetchCoinGeckoPrice(cfg, outData) || ok;
  } else {
    ok = true;
  }
  fetchOnlinePreciseFees(outData);
  if (heightOk) fetchOnlineLastBlockInfo(outData);
  fetchCoinGeckoAth(cfg, outData);
  // A price above the stored high is a new high, whatever the last check said.
  const long price = atol(outData.priceValue);
  if (price > 0 && outData.athPrice > 0 && price > outData.athPrice) {
    outData.athPrice = price;
    if (time(nullptr) >= 1700000000) outData.athTimestamp = time(nullptr);
  }
  return ok;
}
