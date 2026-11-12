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
#include "soc/soc_caps.h"
#if SOC_USB_SERIAL_JTAG_SUPPORTED
#include "driver/usb_serial_jtag.h"
#endif
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
RTC_DATA_ATTR bool rtcWifiPreferSecondary = false;
RTC_DATA_ATTR uint8_t rtcScreenIndex = 0;
RTC_DATA_ATTR int rtcBatteryPercent = -1;
RTC_DATA_ATTR float rtcBatteryVoltage = 0.0f;
RTC_DATA_ATTR time_t rtcLastKnownUnixTime = 0;
RTC_DATA_ATTR time_t rtcNextRefreshAt = 0;        // when the next data refresh is planned, 0 when unknown
RTC_DATA_ATTR time_t rtcLastScreenChangeAt = 0;   // when the shown screen last changed, for auto-rotate
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
uint32_t setupHoldStartedMs = 0;  // millis() when the SETUP press that opened setup began (0 if already let go), for the factory-reset hold
bool portalFactoryResetRequested = false;

// While a computer is attached over USB the clock never deep-sleeps (see goToSleepForSeconds); it
// waits awake in loop() and then runs the next wake in-process. These say what that wake stands in for.
enum SimulatedWake : uint8_t {
  SIMULATED_WAKE_NONE = 0,
  SIMULATED_WAKE_TIMER,
  SIMULATED_WAKE_FUNCTION,
  SIMULATED_WAKE_SETUP
};
static SimulatedWake simulatedWake = SIMULATED_WAKE_NONE;
static bool usbWaitActive = false;
static bool usbWaitHasTimer = false;
static uint32_t usbWaitStartedMs = 0;
static uint32_t usbWaitSeconds = 0;

enum SetupBootAction : uint8_t {
  SETUP_BOOT_ACTION_NONE = 0,
  SETUP_BOOT_ACTION_PORTAL = 1  // a factory-reset hold carries on inside the portal
};

#include "src/config_runtime.h"
#include "src/display_screens.h"
#include "src/block_data.h"
static void factoryResetAndShowWelcome();
static void goToWelcomeSleep();
static void returnFromSetupWithoutChanges();
#include "src/setup_portal.h"

static bool setupButtonPressed() {
  pinMode(PIN_SETUP_BUTTON, INPUT_PULLUP);
  delay(20);
  return digitalRead(PIN_SETUP_BUTTON) == LOW;
}

static esp_sleep_wakeup_cause_t wakeCause() {
  switch (simulatedWake) {
    case SIMULATED_WAKE_TIMER: return ESP_SLEEP_WAKEUP_TIMER;
    case SIMULATED_WAKE_FUNCTION:
    case SIMULATED_WAKE_SETUP: return ESP_SLEEP_WAKEUP_EXT1;
    default: return esp_sleep_get_wakeup_cause();
  }
}

static uint64_t wakeButtonMask() {
  switch (simulatedWake) {
    case SIMULATED_WAKE_FUNCTION: return FUNCTION_BUTTON_WAKE_MASK;
    case SIMULATED_WAKE_SETUP: return SETUP_BUTTON_WAKE_MASK;
    case SIMULATED_WAKE_TIMER: return 0;
    default: return esp_sleep_get_ext1_wakeup_status();
  }
}

static bool wasSetupButtonWake() {
  if (wakeCause() != ESP_SLEEP_WAKEUP_EXT1) return false;
  return (wakeButtonMask() & SETUP_BUTTON_WAKE_MASK) != 0;
}

static bool wasFunctionButtonWake() {
  if (wakeCause() != ESP_SLEEP_WAKEUP_EXT1) return false;
  return (wakeButtonMask() & FUNCTION_BUTTON_WAKE_MASK) != 0;
}

static bool setupWakeRequested() {
  return wasSetupButtonWake();
}

// Any SETUP press opens setup, however short: a tap only needs the switch to click, never a steady
// hold. Waking from deep sleep takes a moment, so the button may already be up; the wake source counts too.
static SetupBootAction detectSetupBootAction() {
  pinMode(PIN_SETUP_BUTTON, INPUT_PULLUP);
  delay(BUTTON_POLL_DELAY_MS);
  const bool stillDown = digitalRead(PIN_SETUP_BUTTON) == LOW;
  if (!stillDown && !setupWakeRequested()) return SETUP_BOOT_ACTION_NONE;
  setupHoldStartedMs = stillDown ? millis() : 0;
  return SETUP_BOOT_ACTION_PORTAL;
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

// True while a computer (not just a charger) is talking to the USB port: it sends start-of-frame
// packets that a power bank or wall adapter never does.
static bool usbHostConnected() {
#if SOC_USB_SERIAL_JTAG_SUPPORTED && defined(ARDUINO_USB_MODE) && ARDUINO_USB_MODE == 1
  return usb_serial_jtag_is_connected();
#else
  return false;
#endif
}

// Deep sleep powers the USB port down, so a computer loses the board, and an upload started
// while it sleeps (or that it falls asleep during) fails. With a computer attached, stay awake
// instead: loop() waits out the time, or a button press, and runs the next wake without a reset,
// so the port is always there for flashing. A charger alone still lets it deep-sleep.
static bool beginUsbAwakeWait(bool hasTimer, uint32_t seconds) {
  if (!usbHostConnected()) return false;
  Serial.printf("[BlockClock] USB host attached: staying awake instead of deep sleep (%s %lus)\n",
                hasTimer ? "next wake in" : "waiting for a button,", (unsigned long)seconds);
  usbWaitActive = true;
  usbWaitHasTimer = hasTimer;
  usbWaitSeconds = seconds;
  usbWaitStartedMs = millis();
  return true;
}

static void deepSleep(bool hasTimer, uint32_t seconds) {
  digitalWrite(PIN_EINK_POWER, LOW);

  rtc_gpio_pullup_en((gpio_num_t)PIN_FUNCTION_BUTTON);
  rtc_gpio_pulldown_dis((gpio_num_t)PIN_FUNCTION_BUTTON);
  rtc_gpio_pullup_en((gpio_num_t)PIN_SETUP_BUTTON);
  rtc_gpio_pulldown_dis((gpio_num_t)PIN_SETUP_BUTTON);
  esp_sleep_enable_ext1_wakeup_io(BUTTON_WAKE_MASK, ESP_EXT1_WAKEUP_ANY_LOW);

  if (hasTimer) esp_sleep_enable_timer_wakeup((uint64_t)seconds * 1000000ULL);
  Serial.flush();
  esp_deep_sleep_start();
}

static void goToSleepForSeconds(uint32_t seconds) {
  if (beginUsbAwakeWait(true, seconds)) return;
  deepSleep(true, seconds);
}

// Unix time of a wake `seconds` from now, or 0 while the clock has no valid time.
static time_t wakeTimeAfter(uint32_t seconds) {
  const time_t now = time(nullptr);
  return now >= 1700000000 ? now + (time_t)seconds : 0;
}

// Time left until the planned refresh, so flipping screens never postpones it.
static uint32_t secondsUntilNextRefresh() {
  const time_t nowTime = time(nullptr);
  if (nowTime >= 1700000000 && rtcNextRefreshAt >= 1700000000) {
    return rtcNextRefreshAt > nowTime + 5 ? (uint32_t)(rtcNextRefreshAt - nowTime) : 5;
  }
  const uint32_t interval = (uint32_t)deviceConfig.refreshIntervalMinutes * 60UL;
  const time_t now = time(nullptr);
  if (now < 1700000000 || rtcLastKnownUnixTime < 1700000000 || now < rtcLastKnownUnixTime) return skipQuietHours(deviceConfig, interval);
  const uint32_t elapsed = (uint32_t)(now - rtcLastKnownUnixTime);
  return skipQuietHours(deviceConfig, elapsed + 5 >= interval ? 5 : interval - elapsed);
}

static void advanceScreen() {
  rtcScreenIndex = (rtcScreenIndex + 1) % dashboardScreenCount(deviceConfig);
  rtcLastScreenChangeAt = time(nullptr);
}

// Auto-rotate: is it time to flip to the next screen?
static bool rotationDue() {
  if (!deviceConfig.rotateScreens) return false;
  const time_t nowTime = time(nullptr);
  if (nowTime < 1700000000) return false;
  if (rtcLastScreenChangeAt < 1700000000 || rtcLastScreenChangeAt > nowTime) return true;
  return (uint32_t)(nowTime - rtcLastScreenChangeAt) + 3 >= deviceConfig.rotateSeconds;
}

// How long to sleep given the next refresh is `refreshSeconds` away: with auto-rotate on, wake
// sooner for the next flip, unless that flip would land in quiet hours or right before the refresh.
static uint32_t sleepSecondsWithRotation(uint32_t refreshSeconds) {
  // Without cached data a flip wake could only retry the network, so wait for the refresh.
  if (!deviceConfig.rotateScreens || !rtcHasBlockData) return refreshSeconds;
  const time_t nowTime = time(nullptr);
  if (nowTime < 1700000000) return refreshSeconds;
  uint32_t untilFlip = deviceConfig.rotateSeconds;
  if (rtcLastScreenChangeAt >= 1700000000 && rtcLastScreenChangeAt <= nowTime) {
    const uint32_t sinceFlip = (uint32_t)(nowTime - rtcLastScreenChangeAt);
    untilFlip = sinceFlip >= deviceConfig.rotateSeconds ? 5 : deviceConfig.rotateSeconds - sinceFlip;
  }
  if (untilFlip + ROTATE_REFRESH_LEAD_SECONDS >= refreshSeconds) return refreshSeconds;
  if (skipQuietHours(deviceConfig, untilFlip) != untilFlip) return refreshSeconds;
  return untilFlip;
}

// A timer wake that only exists to flip screens: the refresh is still a while off.
static bool isRotationOnlyWake() {
  if (!deviceConfig.rotateScreens || !rtcHasBlockData) return false;
  const time_t nowTime = time(nullptr);
  return nowTime >= 1700000000 && rtcNextRefreshAt >= 1700000000
    && rtcNextRefreshAt > nowTime + ROTATE_REFRESH_LEAD_SECONDS;
}

// Next dashboard screen from the cached data, no network: a quick button tap, or an auto-rotate wake.
static void showNextScreenAndSleep(float batteryVoltage, int batteryPercent) {
  advanceScreen();
  BlockData data = rtcBlockData;
  data.batteryVoltage = batteryVoltage;
  data.batteryPercent = batteryPercent;
  const uint32_t refreshSeconds = secondsUntilNextRefresh();
  data.nextRefreshAt = wakeTimeAfter(refreshSeconds);
  clearScreenRenderWindow();
  drawBlockDashboard(data, deviceConfig, rtcScreenIndex);
  rtcLastScreenChangeAt = time(nullptr);  // the next auto-flip counts from when this screen appeared
  goToSleepForSeconds(sleepSecondsWithRotation(refreshSeconds));
}

static void goToWelcomeSleep() {
  if (beginUsbAwakeWait(false, 0)) return;
  deepSleep(false, 0);
}

static void resetRuntimeState() {
  rtcHasBlockData = false;
  rtcScreenIndex = 0;
  rtcBlockData = {};
  rtcBatteryPercent = -1;
  rtcBatteryVoltage = 0.0f;
  rtcLastKnownUnixTime = 0;
  rtcNextRefreshAt = 0;
  rtcLastScreenChangeAt = 0;
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

// One wake: from deep sleep (or power-on) via setup(), or in-process from loop() while USB keeps it awake.
static void runWake();

// HTTPS needs one large free block; long stretches awake (in-process wakes, the setup web server)
// can fragment the heap below that, and only a restart puts it back together.
static bool heapTooFragmentedForHttps() {
  return ESP.getMaxAllocHeap() < 48 * 1024;
}

// Setup closed with nothing saved (a button tap or the idle timeout): put the last screen back
// straight from the kept data instead of restarting, which would drop that data and refetch it
// all. A refresh that came due while setup was open runs right after.
static void returnFromSetupWithoutChanges() {
  WiFi.mode(WIFI_OFF);
  if (!rtcHasBlockData || heapTooFragmentedForHttps()) {
    // Nothing to put back (or no room for the next fetch): restart into a fresh fetch, as after a save.
    drawSetupClosedScreen();
    setPostSaveRestartPending(true);
    delay(CONFIG_PORTAL_RESTART_DELAY_MS);
    ESP.restart();
  }
  if (rtcScreenIndex >= dashboardScreenCount(deviceConfig)) rtcScreenIndex = 0;
  const uint32_t refreshSeconds = secondsUntilNextRefresh();
  BlockData data = rtcBlockData;
  data.batteryVoltage = rtcBatteryVoltage;
  data.batteryPercent = rtcBatteryPercent;
  data.nextRefreshAt = wakeTimeAfter(refreshSeconds);
  clearScreenRenderWindow();
  drawBlockDashboard(data, deviceConfig, rtcScreenIndex);
  Serial.printf("[BlockClock] back from setup without changes (next refresh in %lus)\n", (unsigned long)refreshSeconds);
  if (refreshSeconds <= 5) {
    // Due now: run the refresh as a timer wake would, in this same boot.
    simulatedWake = SIMULATED_WAKE_TIMER;
    runWake();
    simulatedWake = SIMULATED_WAKE_NONE;  // only reached while USB keeps the clock awake
    return;
  }
  goToSleepForSeconds(sleepSecondsWithRotation(refreshSeconds));
}

static void runWake() {
  const SetupBootAction bootAction = detectSetupBootAction();

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

  const esp_sleep_wakeup_cause_t cause = wakeCause();
  const uint8_t screenBeforeWake = rtcScreenIndex;
  const esp_reset_reason_t resetReason = esp_reset_reason();
  const bool functionWake = wasFunctionButtonWake();
  const bool manualWake = functionWake;
  // Mirror Freedom Clock: hardware-reset / power-on / brownout (wake cause undefined
  // and reset reason not "SW restart") with a configured device is a "user is
  // looking at the screen waiting for data" event. Without this case, HW-reset
  // boots fall through to the no-placeholder path and the user stares at the
  // previous image (or a blank screen on cold boot) for ~6-10 s.
  const bool homeButtonReset = cause == ESP_SLEEP_WAKEUP_UNDEFINED
    && deviceConfig.configured
    && bootAction == SETUP_BOOT_ACTION_NONE
    && resetReason != ESP_RST_SW;
  Serial.printf(
    "[BlockClock] wake cause=%d simulated=%d reset=%d ext1=0x%llX manualWake=%d homeReset=%d hasCache=%d postSave=%d\n",
    (int)cause,
    (int)simulatedWake,
    (int)resetReason,
    (unsigned long long)wakeButtonMask(),
    (int)manualWake,
    (int)homeButtonReset,
    (int)rtcHasBlockData,
    (int)postSaveRestartPending
  );

  float batteryVoltage = readBatteryVoltage();
  int batteryPercent = stabilizedBatteryPercentFromVoltage(batteryVoltage, rtcLastKnownUnixTime);
  rtcBatteryVoltage = batteryVoltage;
  rtcBatteryPercent = batteryPercent;

  // A FUNCTION tap flips to the next screen from cache (HOME/RST is the way to fetch fresh data now).
  // During quiet hours a tap means someone is looking, so it fetches live data instead of flipping from a stale cache.
  const bool quietTap = manualWake && nowIsQuiet(deviceConfig);
  if (quietTap) {
    advanceScreen();
  } else if (manualWake && rtcHasBlockData) {
    showNextScreenAndSleep(batteryVoltage, batteryPercent);
    return;
  }

  // Auto-rotate: a timer wake well before the next refresh only flips the screen, from cache.
  const bool timerWake = cause == ESP_SLEEP_WAKEUP_TIMER;
  if (timerWake && isRotationOnlyWake()) {
    if (rotationDue()) {
      showNextScreenAndSleep(batteryVoltage, batteryPercent);
    } else {
      goToSleepForSeconds(sleepSecondsWithRotation(secondsUntilNextRefresh()));
    }
    return;
  }
  // A refresh keeps whichever screen was last picked, unless auto-rotate says it is time to move on.
  if (timerWake && rotationDue()) advanceScreen();
  if (rtcScreenIndex >= dashboardScreenCount(deviceConfig)) rtcScreenIndex = 0;

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
    for (int i = 0; i < MAX_TICKERS; i++) pending.tickerQuotes[i].price = 0.0;  // watchlist shows "--" while loading
    pending.tickersUpdatedAt = 0;
    pending.wifiOk = true;
    pending.dataOk = false;
    pending.updatedAt = 0;
    pending.networkUpdatedAt = 0;
    pending.nextRefreshAt = 0;
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

  bool wifiOk = connectWiFi(deviceConfig, WIFI_CONNECT_TIMEOUT_MS, false, &rtcWifiPreferSecondary);

  bool dataOk = false;
  if (wifiOk) {
    syncClockFromNtp(deviceConfig);
    if (deviceConfig.dataSourceMode == DATA_SOURCE_MQTT) {
      dataOk = fetchBlockDataFromMqtt(deviceConfig, data);
    } else {
      dataOk = fetchBlockDataOnline(deviceConfig, data);
    }
    if (watchlistScreenShown(deviceConfig)) fetchTickerQuotes(deviceConfig, data);
  }

  data.batteryVoltage = batteryVoltage;
  data.batteryPercent = batteryPercent;
  data.wifiOk = wifiOk;
  data.dataOk = dataOk;
  // The fetchers stamp updatedAt, networkUpdatedAt and tickersUpdatedAt only for what actually came
  // in, so anything that failed keeps its old time and its screen says how old it is.
  const time_t nowTime = time(nullptr);
  if (nowTime >= 1700000000) rtcLastKnownUnixTime = nowTime;

  if (hasUsefulBlockData(data)) {
    rtcBlockData = data;
    rtcHasBlockData = true;
  }

  // With no network reachable, look again sooner than a long refresh interval would.
  const uint16_t sleepMinutes = wifiOk ? deviceConfig.refreshIntervalMinutes
    : (uint16_t)min((int)deviceConfig.refreshIntervalMinutes, (int)WIFI_OFFLINE_RETRY_MINUTES);
  const uint32_t refreshSeconds = skipQuietHours(deviceConfig, (uint32_t)sleepMinutes * 60UL);
  data.nextRefreshAt = wakeTimeAfter(refreshSeconds);
  rtcNextRefreshAt = data.nextRefreshAt;

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
  // A screen picked at the start of this wake only appeared now, after the fetch (and NTP sync),
  // so the next auto-flip counts from here.
  if (rtcScreenIndex != screenBeforeWake) rtcLastScreenChangeAt = time(nullptr);
  disconnectNetwork();
  goToSleepForSeconds(sleepSecondsWithRotation(refreshSeconds));
}

void setup() {
  Serial.begin(115200);
  delay(30);
  Serial.println();
  Serial.println("==== Block Clock boot ====");

  prepareHardware();
  buildPortalCredentials();
  runWake();
}

// Only does anything while USB keeps the clock awake instead of deep-sleeping.
void loop() {
  if (!usbWaitActive) {
    delay(100);
    return;
  }
  static bool functionArmed = false;
  static bool setupArmed = false;
  static uint8_t functionLowPolls = 0;
  static uint8_t setupLowPolls = 0;
  const uint32_t elapsedMs = millis() - usbWaitStartedMs;
  if (elapsedMs < 200) {
    // A button still held from the last wake must be released before it counts as a new press.
    functionArmed = setupArmed = false;
  }

  // A press counts once the pin has read low on two polls in a row, so contact bounce or a
  // glitch on the line never fakes one.
  const bool functionDown = digitalRead(PIN_FUNCTION_BUTTON) == LOW;
  const bool setupDown = digitalRead(PIN_SETUP_BUTTON) == LOW;
  functionLowPolls = functionDown ? (uint8_t)min(functionLowPolls + 1, 2) : 0;
  setupLowPolls = setupDown ? (uint8_t)min(setupLowPolls + 1, 2) : 0;
  SimulatedWake next = SIMULATED_WAKE_NONE;
  if (functionArmed && functionLowPolls >= 2) next = SIMULATED_WAKE_FUNCTION;
  else if (setupArmed && setupLowPolls >= 2) next = SIMULATED_WAKE_SETUP;
  else if (usbWaitHasTimer && elapsedMs >= usbWaitSeconds * 1000UL) next = SIMULATED_WAKE_TIMER;
  if (!functionDown) functionArmed = true;
  if (!setupDown) setupArmed = true;

  if (next == SIMULATED_WAKE_NONE && !usbHostConnected()) {
    // Unplugged from the computer: deep-sleep for whatever is left of the wait.
    usbWaitActive = false;
    const uint32_t elapsedSeconds = elapsedMs / 1000UL;
    const uint32_t remaining = usbWaitSeconds > elapsedSeconds + 1 ? usbWaitSeconds - elapsedSeconds : 1;
    Serial.println("[BlockClock] USB host gone: deep sleeping");
    deepSleep(usbWaitHasTimer, remaining);
    return;
  }
  if (next == SIMULATED_WAKE_NONE) {
    delay(BUTTON_POLL_DELAY_MS);
    return;
  }

  usbWaitActive = false;
  // Days of in-process wakes can fragment the heap; HTTPS needs a large free block, so start
  // clean if it runs short (the next boot fetches fresh data and redraws).
  if (heapTooFragmentedForHttps()) {
    Serial.printf("[BlockClock] largest free heap block %u bytes: restarting\n", (unsigned)ESP.getMaxAllocHeap());
    Serial.flush();
    ESP.restart();
  }
  simulatedWake = next;
  Serial.printf("[BlockClock] in-process wake (%s)\n",
                next == SIMULATED_WAKE_TIMER ? "timer" : next == SIMULATED_WAKE_FUNCTION ? "FUNCTION" : "SETUP");
  runWake();
  simulatedWake = SIMULATED_WAKE_NONE;
}
