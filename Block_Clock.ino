#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <Preferences.h>
#include <PubSubClient.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <heltec-eink-modules.h>
#include "esp_sleep.h"
#include "driver/rtc_io.h"
#include "nvs_flash.h"
#include "qrcode.h"
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <ctime>

#include "src/config.h"

#if __has_include("secrets.h")
#include "secrets.h"
#define HAS_SECRETS 1
#else
#define HAS_SECRETS 0
#endif

#ifndef USE_SECRETS_BOOTSTRAP
#define USE_SECRETS_BOOTSTRAP 0
#endif

#ifndef BLOCKCLOCK_WIFI_SSID
#define BLOCKCLOCK_WIFI_SSID ""
#endif

#ifndef BLOCKCLOCK_WIFI_PASS
#define BLOCKCLOCK_WIFI_PASS ""
#endif

#ifndef BLOCKCLOCK_MQTT_SERVER
#define BLOCKCLOCK_MQTT_SERVER DEFAULT_MQTT_SERVER
#endif

#ifndef BLOCKCLOCK_MQTT_PORT
#define BLOCKCLOCK_MQTT_PORT DEFAULT_MQTT_PORT
#endif

#ifndef BLOCKCLOCK_MQTT_USER
#define BLOCKCLOCK_MQTT_USER DEFAULT_MQTT_USER
#endif

#ifndef BLOCKCLOCK_MQTT_PASS
#define BLOCKCLOCK_MQTT_PASS DEFAULT_MQTT_PASS
#endif

#ifndef BLOCKCLOCK_DATA_SOURCE_MODE
#define BLOCKCLOCK_DATA_SOURCE_MODE DATA_SOURCE_ONLINE
#endif

#ifndef BLOCKCLOCK_CURRENCY_CODE
#define BLOCKCLOCK_CURRENCY_CODE DEFAULT_CURRENCY_CODE
#endif

#ifndef BLOCKCLOCK_DISPLAY_THEME_MODE
#define BLOCKCLOCK_DISPLAY_THEME_MODE DEFAULT_DISPLAY_THEME_MODE
#endif

#ifndef BLOCKCLOCK_TOPIC_HEIGHT
#define BLOCKCLOCK_TOPIC_HEIGHT DEFAULT_TOPIC_HEIGHT
#endif

#ifndef BLOCKCLOCK_TOPIC_HALVING
#define BLOCKCLOCK_TOPIC_HALVING DEFAULT_TOPIC_HALVING
#endif

#ifndef BLOCKCLOCK_TOPIC_HASHRATE
#define BLOCKCLOCK_TOPIC_HASHRATE DEFAULT_TOPIC_HASHRATE
#endif

#ifndef BLOCKCLOCK_TOPIC_PRICE_VALUE
#define BLOCKCLOCK_TOPIC_PRICE_VALUE DEFAULT_TOPIC_PRICE_VALUE
#endif

RTC_DATA_ATTR BlockData rtcBlockData = {};
RTC_DATA_ATTR bool rtcHasBlockData = false;
RTC_DATA_ATTR uint8_t rtcScreenIndex = 0;
RTC_DATA_ATTR int rtcBatteryPercent = -1;
RTC_DATA_ATTR float rtcBatteryVoltage = 0.0f;
RTC_DATA_ATTR time_t rtcLastKnownUnixTime = 0;
RTC_DATA_ATTR bool batteryGaugeStateValid = false;
RTC_DATA_ATTR int batteryGaugePercent = 0;
RTC_DATA_ATTR float batteryGaugeReferenceVoltage = 0.0f;
RTC_DATA_ATTR bool batteryGaugeFullHoldActive = false;
RTC_DATA_ATTR time_t batteryGaugeFullHoldStartedUnixTime = 0;

WiFiClient espClient;
PubSubClient mqttClient(espClient);
BlockClockDisplay display;
WebServer portalServer(CONFIG_PORTAL_PORT);
DNSServer portalDnsServer;
Preferences preferences;

DeviceConfig deviceConfig;
BlockData currentBlockData;
char deviceId[DEVICE_ID_SIZE] = "";
char portalApSsid[AP_SSID_SIZE] = "";
char portalApPassword[AP_PASSWORD_SIZE] = "";
bool portalShouldRestart = false;
bool portalFactoryResetRequested = false;

enum SetupBootAction : uint8_t {
  SETUP_BOOT_ACTION_NONE = 0,
  SETUP_BOOT_ACTION_PORTAL = 1,
  SETUP_BOOT_ACTION_FACTORY_RESET = 2,
  SETUP_BOOT_ACTION_CYCLE = 3
};

#include "src/config_runtime.h"
#include "src/display_screens.h"
#include "src/block_data.h"
static void factoryResetAndShowWelcome();
#include "src/setup_portal.h"

static bool setupButtonPressed() {
  pinMode(PIN_SETUP_BUTTON, INPUT_PULLUP);
  delay(20);
  return digitalRead(PIN_SETUP_BUTTON) == LOW;
}

static bool wasSetupButtonWake() {
  if (esp_sleep_get_wakeup_cause() != ESP_SLEEP_WAKEUP_EXT1) return false;
  return (esp_sleep_get_ext1_wakeup_status() & SETUP_BUTTON_WAKE_MASK) != 0;
}

static bool wasFunctionButtonWake() {
  if (esp_sleep_get_wakeup_cause() != ESP_SLEEP_WAKEUP_EXT1) return false;
  return (esp_sleep_get_ext1_wakeup_status() & FUNCTION_BUTTON_WAKE_MASK) != 0;
}

static bool setupWakeRequested() {
  return wasSetupButtonWake();
}

static SetupBootAction detectSetupBootAction() {
  pinMode(PIN_SETUP_BUTTON, INPUT_PULLUP);
  delay(BUTTON_POLL_DELAY_MS);
  if (digitalRead(PIN_SETUP_BUTTON) != LOW) {
    return setupWakeRequested() ? SETUP_BOOT_ACTION_CYCLE : SETUP_BOOT_ACTION_NONE;
  }

  const uint32_t start = millis();
  while (digitalRead(PIN_SETUP_BUTTON) == LOW) {
    if ((millis() - start) >= FACTORY_RESET_HOLD_MS) {
      return SETUP_BOOT_ACTION_FACTORY_RESET;
    }
    delay(BUTTON_POLL_DELAY_MS);
  }
  // A deliberate hold opens setup; a quick tap flips screens, like FUNCTION does.
  return (millis() - start) >= SETUP_PORTAL_HOLD_MS ? SETUP_BOOT_ACTION_PORTAL : SETUP_BOOT_ACTION_CYCLE;
}

static void prepareHardware() {
  // Release buttons from RTC mode before begin(). Without this, EXT1 wake
  // status can fail to propagate, so wasFunctionButtonWake() returns false
  // and we lose the pending-frame fast path on button presses.
  rtc_gpio_deinit((gpio_num_t)PIN_FUNCTION_BUTTON);
  rtc_gpio_deinit((gpio_num_t)PIN_SETUP_BUTTON);
  pinMode(PIN_FUNCTION_BUTTON, INPUT_PULLUP);
  pinMode(PIN_SETUP_BUTTON, INPUT_PULLUP);

  pinMode(PIN_EINK_POWER, OUTPUT);
  digitalWrite(PIN_EINK_POWER, HIGH);
  delay(EINK_POWER_UP_DELAY_MS);
  display.begin();
  display.setRotation(1);
}

static void disconnectNetwork() {
  if (mqttClient.connected()) mqttClient.disconnect();
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
}

static void goToSleepForSeconds(uint32_t seconds) {
  digitalWrite(PIN_EINK_POWER, LOW);

  rtc_gpio_pullup_en((gpio_num_t)PIN_FUNCTION_BUTTON);
  rtc_gpio_pulldown_dis((gpio_num_t)PIN_FUNCTION_BUTTON);
  rtc_gpio_pullup_en((gpio_num_t)PIN_SETUP_BUTTON);
  rtc_gpio_pulldown_dis((gpio_num_t)PIN_SETUP_BUTTON);
  esp_sleep_enable_ext1_wakeup_io(BUTTON_WAKE_MASK, ESP_EXT1_WAKEUP_ANY_LOW);

  esp_sleep_enable_timer_wakeup((uint64_t)seconds * 1000000ULL);
  Serial.flush();
  esp_deep_sleep_start();
}

static void goToSleep(uint16_t refreshMinutes) {
  goToSleepForSeconds((uint32_t)refreshMinutes * 60UL);
}

// Time left in the current refresh interval, so flipping screens never postpones the next refresh.
static uint32_t secondsUntilNextRefresh() {
  const uint32_t interval = (uint32_t)deviceConfig.refreshIntervalMinutes * 60UL;
  const time_t now = time(nullptr);
  if (now < 1700000000 || rtcLastKnownUnixTime < 1700000000 || now < rtcLastKnownUnixTime) return interval;
  const uint32_t elapsed = (uint32_t)(now - rtcLastKnownUnixTime);
  return elapsed + 5 >= interval ? 5 : interval - elapsed;
}

static bool functionButtonHeld(uint32_t holdMs) {
  const uint32_t start = millis();
  while (digitalRead(PIN_FUNCTION_BUTTON) == LOW) {
    if ((millis() - start) >= holdMs) return true;
    delay(BUTTON_POLL_DELAY_MS);
  }
  return false;
}

// Short FUNCTION press: next dashboard screen from the cached data, no network.
static void showNextScreenAndSleep(float batteryVoltage, int batteryPercent) {
  rtcScreenIndex = (rtcScreenIndex + 1) % DASHBOARD_SCREEN_COUNT;
  BlockData data = rtcBlockData;
  data.batteryVoltage = batteryVoltage;
  data.batteryPercent = batteryPercent;
  clearScreenRenderWindow();
  drawBlockDashboard(data, deviceConfig, rtcScreenIndex);
  goToSleepForSeconds(secondsUntilNextRefresh());
}

static void goToWelcomeSleep() {
  digitalWrite(PIN_EINK_POWER, LOW);

  rtc_gpio_pullup_en((gpio_num_t)PIN_FUNCTION_BUTTON);
  rtc_gpio_pulldown_dis((gpio_num_t)PIN_FUNCTION_BUTTON);
  rtc_gpio_pullup_en((gpio_num_t)PIN_SETUP_BUTTON);
  rtc_gpio_pulldown_dis((gpio_num_t)PIN_SETUP_BUTTON);
  esp_sleep_enable_ext1_wakeup_io(BUTTON_WAKE_MASK, ESP_EXT1_WAKEUP_ANY_LOW);

  Serial.flush();
  esp_deep_sleep_start();
}

static void resetRuntimeState() {
  rtcHasBlockData = false;
  rtcScreenIndex = 0;
  rtcBlockData = {};
  rtcBatteryPercent = -1;
  rtcBatteryVoltage = 0.0f;
  rtcLastKnownUnixTime = 0;
  batteryGaugeStateValid = false;
  batteryGaugePercent = 0;
  batteryGaugeReferenceVoltage = 0.0f;
  batteryGaugeFullHoldActive = false;
  batteryGaugeFullHoldStartedUnixTime = 0;
}

static void factoryResetAndShowWelcome() {
  clearSavedDeviceConfig();
  applyDefaultDeviceConfig(deviceConfig);
  resetRuntimeState();
  drawFactoryResetScreen();
  delay(1200);

  const uint32_t releaseWaitStartedMs = millis();
  while (digitalRead(PIN_SETUP_BUTTON) == LOW && (millis() - releaseWaitStartedMs) < 5000) {
    delay(BUTTON_POLL_DELAY_MS);
  }

  markWelcomeShown();
  drawWelcomeScreen();
  delay(500);
  goToWelcomeSleep();
}

void setup() {
  Serial.begin(115200);
  delay(30);
  Serial.println();
  Serial.println("==== Block Clock boot ====");

  prepareHardware();
  buildPortalCredentials();
  const SetupBootAction bootAction = detectSetupBootAction();
  if (bootAction == SETUP_BOOT_ACTION_FACTORY_RESET) {
    factoryResetAndShowWelcome();
    return;
  }

  loadDeviceConfig(deviceConfig);
  applySecretsBootstrap(deviceConfig);
  applyTimezone(deviceConfig);
  const bool postSaveRestartPending = takePostSaveRestartPending();

  if (!deviceConfig.configured && !hasWelcomeBeenShown()) {
    markWelcomeShown();
    drawWelcomeScreen();
    goToWelcomeSleep();
    return;
  }

  const bool enterSetup = !deviceConfig.configured || bootAction == SETUP_BOOT_ACTION_PORTAL;
  if (enterSetup) {
    runSetupPortal();
    return;
  }

  const esp_sleep_wakeup_cause_t wakeCause = esp_sleep_get_wakeup_cause();
  const esp_reset_reason_t resetReason = esp_reset_reason();
  const bool functionWake = wasFunctionButtonWake();
  const bool manualWake = functionWake || bootAction == SETUP_BOOT_ACTION_CYCLE;
  // Mirror Freedom Clock: hardware-reset / power-on / brownout (wake cause undefined
  // and reset reason not "SW restart") with a configured device is a "user is
  // looking at the screen waiting for data" event. Without this case, HW-reset
  // boots fall through to the no-placeholder path and the user stares at the
  // previous image (or a blank screen on cold boot) for ~6-10 s.
  const bool homeButtonReset = wakeCause == ESP_SLEEP_WAKEUP_UNDEFINED
    && deviceConfig.configured
    && bootAction == SETUP_BOOT_ACTION_NONE
    && resetReason != ESP_RST_SW;
  Serial.printf(
    "[BlockClock] wake cause=%d reset=%d ext1=0x%llX manualWake=%d homeReset=%d hasCache=%d postSave=%d\n",
    (int)wakeCause,
    (int)resetReason,
    (unsigned long long)esp_sleep_get_ext1_wakeup_status(),
    (int)manualWake,
    (int)homeButtonReset,
    (int)rtcHasBlockData,
    (int)postSaveRestartPending
  );

  float batteryVoltage = readBatteryVoltage();
  int batteryPercent = stabilizedBatteryPercentFromVoltage(batteryVoltage, rtcLastKnownUnixTime);
  rtcBatteryVoltage = batteryVoltage;
  rtcBatteryPercent = batteryPercent;

  // A quick tap on FUNCTION or SETUP flips screens from cache; holding FUNCTION falls through to a full refresh.
  if (manualWake && rtcHasBlockData && !(functionWake && functionButtonHeld(FUNCTION_LONG_PRESS_MS))) {
    showNextScreenAndSleep(batteryVoltage, batteryPercent);
    return;
  }
  // A refresh keeps whichever screen was last picked; only a button tap changes it.
  if (rtcScreenIndex >= DASHBOARD_SCREEN_COUNT) rtcScreenIndex = 0;

  // Freedom Clock pattern: when this boot is a known "user is waiting for data"
  // case — button wake with cache, or the very first boot after a portal save —
  // draw a pending frame immediately with "---" placeholders so the user gets
  // visible feedback that the screen loaded and is fetching. Without this, the
  // e-ink keeps showing the previous image (cached dashboard, or "SETTINGS SAVED")
  // for the entire 10-15 s WiFi+fetch window, which reads as "frozen device."
  // The final frame then partial-refreshes the real values over this base.
  const bool drawPendingFrame = (manualWake && rtcHasBlockData) || homeButtonReset || postSaveRestartPending;
  bool pendingFrameDrawn = false;
  if (drawPendingFrame) {
    const char* reason = postSaveRestartPending ? "post-save"
                       : homeButtonReset ? "home-reset"
                       : "button-wake";
    Serial.printf("[BlockClock] pending frame: drawing loading placeholders (reason=%s)\n", reason);
    BlockData pending = (manualWake && rtcHasBlockData) ? rtcBlockData : emptyBlockData();
    pending.batteryVoltage = batteryVoltage;
    pending.batteryPercent = batteryPercent;
    safeCopyCString(pending.blockHeight, sizeof(pending.blockHeight), LOADING_TEXT);
    safeCopyCString(pending.blocksToHalving, sizeof(pending.blocksToHalving), LOADING_TEXT);
    safeCopyCString(pending.hashrateEhs, sizeof(pending.hashrateEhs), LOADING_TEXT);
    safeCopyCString(pending.priceValue, sizeof(pending.priceValue), LOADING_TEXT);
    pending.wifiOk = true;
    pending.dataOk = false;
    pending.updatedAt = 0;
    clearScreenRenderWindow();
    drawBlockDashboard(pending, deviceConfig, rtcScreenIndex);
    pendingFrameDrawn = true;
  } else {
    Serial.printf("[BlockClock] pending frame skipped (manualWake=%d homeReset=%d hasCache=%d postSave=%d)\n",
                  (int)manualWake, (int)homeButtonReset, (int)rtcHasBlockData, (int)postSaveRestartPending);
  }

  BlockData data = rtcHasBlockData ? rtcBlockData : emptyBlockData();
  loadSavedAth(deviceConfig, data);
  data.batteryVoltage = batteryVoltage;
  data.batteryPercent = batteryPercent;
  data.wifiOk = true;
  data.dataOk = false;
  data.updatedAt = rtcLastKnownUnixTime;

  bool wifiOk = connectWiFi(deviceConfig, WIFI_CONNECT_TIMEOUT_MS, false);

  bool dataOk = false;
  if (wifiOk) {
    syncClockFromNtp(deviceConfig);
    if (deviceConfig.dataSourceMode == DATA_SOURCE_MQTT) {
      dataOk = fetchBlockDataFromMqtt(deviceConfig, data);
    } else {
      dataOk = fetchBlockDataOnline(deviceConfig, data);
    }
  }

  data.batteryVoltage = batteryVoltage;
  data.batteryPercent = batteryPercent;
  data.wifiOk = wifiOk;
  data.dataOk = dataOk;
  data.updatedAt = time(nullptr);
  if (data.updatedAt < 1700000000 && rtcHasBlockData) {
    data.updatedAt = rtcBlockData.updatedAt;
  }
  if (data.updatedAt >= 1700000000) {
    rtcLastKnownUnixTime = data.updatedAt;
  }

  if (hasUsefulBlockData(data)) {
    rtcBlockData = data;
    rtcHasBlockData = true;
  }

  if (pendingFrameDrawn) {
    // Partial refresh: full-screen window (avoids partial-window edge artifacts),
    // fastmode on, draw (prepareScreen uses fillRect not clearMemory, keeping the
    // pending frame as the base image), fastmode off.
    setScreenRenderWindow(0, 0, DEVICE_DISPLAY_WIDTH, DEVICE_DISPLAY_HEIGHT);
    display.fastmodeOn(false);
    drawBlockDashboard(data, deviceConfig, rtcScreenIndex);
    display.fastmodeOff();
    clearScreenRenderWindow();
  } else {
    // Timer wake: e-ink retains previous image during deep sleep, so we let it
    // sit during fetch and do one clean full refresh at the end.
    drawBlockDashboard(data, deviceConfig, rtcScreenIndex);
  }
  disconnectNetwork();
  goToSleep(deviceConfig.refreshIntervalMinutes);
}

void loop() {
  delay(100);
}
