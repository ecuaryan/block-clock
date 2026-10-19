#pragma once

static bool gotHeight = false;
static bool gotHalving = false;
static bool gotHashrate = false;
static bool gotPrice = false;
static BlockData* mqttTargetData = nullptr;

static bool connectWiFi(const DeviceConfig& cfg, uint16_t timeoutMs, bool keepPortalAp) {
  if (!hasText(cfg.wifiSsid)) return false;

  WiFi.persistent(false);
  WiFi.setSleep(false);
  WiFi.mode(keepPortalAp ? WIFI_AP_STA : WIFI_STA);
  WiFi.disconnect(false, false);
  delay(150);
  WiFi.begin(cfg.wifiSsid, cfg.wifiPass);

  const uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED && (millis() - start) < timeoutMs) {
    delay(WIFI_POLL_DELAY_MS);
  }
  return WiFi.status() == WL_CONNECTED;
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

static void fetchOnlineLastBlockTimestamp(BlockData& outData) {
  if (!hasText(outData.blockHeight) || !isUsefulDataValue(outData.blockHeight)) return;

  String hashUrl = MEMPOOL_BLOCK_HASH_URL_PREFIX;
  hashUrl += outData.blockHeight;
  String blockHash;
  if (!httpGetString(hashUrl.c_str(), blockHash)) return;

  String blockUrl = MEMPOOL_BLOCK_URL_PREFIX;
  blockUrl += blockHash;
  String payload;
  if (!httpGetString(blockUrl.c_str(), payload)) return;

  StaticJsonDocument<64> filter;
  filter["timestamp"] = true;
  StaticJsonDocument<128> doc;
  if (deserializeJson(doc, payload, DeserializationOption::Filter(filter)) || !doc["timestamp"].is<time_t>()) return;
  const time_t blockTimestamp = doc["timestamp"].as<time_t>();
  if (blockTimestamp > 0) outData.lastBlockTimestamp = blockTimestamp;
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
  if (heightOk) fetchOnlineLastBlockTimestamp(outData);
  return ok;
}
