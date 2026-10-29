#pragma once

#if defined(ARDUINO)
#include <Fonts/FreeSans24pt7b.h>
#include <Fonts/FreeSansBold24pt7b.h>
#else
static const GFXfont FreeSans24pt7b = { false, 0 };
static const GFXfont FreeSansBold24pt7b = { true, 1 };
#endif

static constexpr int BATTERY_BODY_W = 20;
static constexpr int BATTERY_BODY_H = 10;
static constexpr int BATTERY_TIP_W = 1;
static constexpr int BATTERY_TIP_H = 5;
static constexpr int BATTERY_RIGHT_MARGIN = 3;
static constexpr int BATTERY_TEXT_GAP = 2;
static constexpr int BATTERY_ICON_Y = 3;
static constexpr int BATTERY_TEXT_Y = 5;
static constexpr int NO_WIFI_ICON_W = 14;
static constexpr int NO_WIFI_ICON_H = 12;
static constexpr int STATUS_ICON_GAP = 5;

// Partial-refresh window state. When active, prepareScreen uses fillRect
// (keeping the existing panel image outside the window) so display.fastmodeOn()
// can do a true partial refresh — one quick blink instead of a full waveform.
static bool screenRenderWindowActive = false;
static int screenRenderWindowX = 0;
static int screenRenderWindowY = 0;
static int screenRenderWindowW = DEVICE_DISPLAY_WIDTH;
static int screenRenderWindowH = DEVICE_DISPLAY_HEIGHT;

static void setScreenRenderWindow(int x, int y, int w, int h) {
  screenRenderWindowActive = true;
  screenRenderWindowX = clampInt(x, 0, DEVICE_DISPLAY_WIDTH - 1);
  screenRenderWindowY = clampInt(y, 0, DEVICE_DISPLAY_HEIGHT - 1);
  screenRenderWindowW = clampInt(w, 1, DEVICE_DISPLAY_WIDTH - screenRenderWindowX);
  screenRenderWindowH = clampInt(h, 1, DEVICE_DISPLAY_HEIGHT - screenRenderWindowY);
}

static void clearScreenRenderWindow() {
  screenRenderWindowActive = false;
  screenRenderWindowX = 0;
  screenRenderWindowY = 0;
  screenRenderWindowW = DEVICE_DISPLAY_WIDTH;
  screenRenderWindowH = DEVICE_DISPLAY_HEIGHT;
}

static void prepareScreen(DisplayThemeMode themeMode = statusScreenThemeMode()) {
  const DisplayThemeColors theme = getDisplayThemeColors(themeMode);
  display.setRotation(1);
  if (screenRenderWindowActive) {
    // Partial refresh path: keep the existing panel image and only redraw the requested window.
    // No clearMemory — buffer keeps the pending frame so fastmode only diffs the changed region.
    display.setWindow(screenRenderWindowX, screenRenderWindowY, screenRenderWindowW, screenRenderWindowH);
    display.fillRect(screenRenderWindowX, screenRenderWindowY, screenRenderWindowW, screenRenderWindowH, theme.background);
  } else {
    // fastmodeOff() resets hardware mode but not winrot_*; this ensures update() refreshes the full screen.
    display.setWindow(0, 0, DEVICE_DISPLAY_WIDTH, DEVICE_DISPLAY_HEIGHT);
    display.clearMemory();
    display.fillScreen(theme.background);
  }
  display.setTextColor(theme.foreground, theme.background);
}

static void drawNoWifiStatusIcon(int x, int y, uint16_t color, uint16_t background) {
  // Tiny Wi-Fi-with-slash icon, tuned manually for 1-bit e-ink.
  display.fillRect(x, y, NO_WIFI_ICON_W, NO_WIFI_ICON_H, background);
  display.drawLine(x + 3, y + 1, x + 10, y + 1, color);
  display.drawPixel(x + 2, y + 2, color);
  display.drawPixel(x + 11, y + 2, color);
  display.drawLine(x + 4, y + 4, x + 9, y + 4, color);
  display.drawPixel(x + 3, y + 5, color);
  display.drawPixel(x + 10, y + 5, color);
  display.drawPixel(x + 5, y + 7, color);
  display.drawPixel(x + 6, y + 6, color);
  display.drawPixel(x + 7, y + 6, color);
  display.drawPixel(x + 8, y + 7, color);
  display.fillRect(x + 6, y + 9, 2, 2, color);
  display.drawLine(x + 2, y + 1, x + 11, y + 10, color);
}

static void drawNoWifiStatusLeftOfBattery(int batteryIconX, DisplayThemeMode themeMode, int batteryIconY = BATTERY_ICON_Y) {
  const DisplayThemeColors theme = getDisplayThemeColors(themeMode);
  const int iconX = batteryIconX - STATUS_ICON_GAP - NO_WIFI_ICON_W;
  if (iconX < 1) return;
  drawNoWifiStatusIcon(iconX, batteryIconY - 1, theme.foreground, theme.background);
}

static void drawBlockClockMark(int x, int y, int size, uint16_t color = BLACK, uint16_t background = WHITE) {
  const int wall = size >= 20 ? 4 : 3;
  display.fillRect(x, y, size, size, color);
  display.fillRect(x + wall, y + wall, size - (wall * 2), size - (wall * 2), background);
}

static int estimateTextWidthSize1(const char* text) {
  return text ? (int)strlen(text) * 6 : 0;
}

static void drawBatteryIcon(
  int x,
  int y,
  int pct,
  int bodyW,
  int bodyH,
  int tipW,
  int tipH,
  DisplayThemeMode themeMode = DISPLAY_THEME_LIGHT
) {
  const DisplayThemeColors theme = getDisplayThemeColors(themeMode);
  if (pct < 0) pct = 0;
  if (pct > 100) pct = 100;

  const int pad = (bodyW >= 28) ? 3 : 2;

  display.drawRoundRect(x, y, bodyW, bodyH, 3, theme.foreground);
  display.fillRect(x + bodyW, y + (bodyH - tipH) / 2, tipW, tipH, theme.foreground);

  const int innerX = x + pad;
  const int innerY = y + pad;
  const int innerW = bodyW - (pad * 2);
  const int innerH = bodyH - (pad * 2);

  display.fillRect(innerX, innerY, innerW, innerH, theme.background);

  int fillW = (innerW * pct) / 100;
  if (fillW > 0) {
    display.fillRect(innerX, innerY, fillW, innerH, theme.foreground);
  }
}

static int drawBatteryStatus(int batteryPct, uint8_t mode, DisplayThemeMode themeMode, int iconY = BATTERY_ICON_Y) {
  // Hidden: draw nothing, but report an icon position so the Wi-Fi icon still sits at the edge.
  if (mode == BATTERY_DISPLAY_HIDDEN) return DEVICE_DISPLAY_WIDTH - BATTERY_RIGHT_MARGIN + STATUS_ICON_GAP;

  int iconX;
  if (mode == BATTERY_DISPLAY_ICON_PERCENT && batteryPct >= 0) {
    char pctText[8];
    snprintf(pctText, sizeof(pctText), "%d%%", clampInt(batteryPct, 0, 100));
    const int textW = estimateTextWidthSize1(pctText);
    const int textX = DEVICE_DISPLAY_WIDTH - BATTERY_RIGHT_MARGIN - textW;
    iconX = textX - BATTERY_TEXT_GAP - BATTERY_BODY_W - BATTERY_TIP_W;
    display.setTextSize(1);
    display.setCursor(textX, iconY + (BATTERY_TEXT_Y - BATTERY_ICON_Y));
    display.print(pctText);
  } else {
    iconX = DEVICE_DISPLAY_WIDTH - BATTERY_RIGHT_MARGIN - BATTERY_BODY_W - BATTERY_TIP_W;
  }

  drawBatteryIcon(
    iconX,
    iconY,
    batteryPct >= 0 ? batteryPct : 0,
    BATTERY_BODY_W,
    BATTERY_BODY_H,
    BATTERY_TIP_W,
    BATTERY_TIP_H,
    themeMode
  );
  return iconX;
}

static void drawTitleLine(const char* title, int x, int y, int textSize, DisplayThemeMode themeMode = statusScreenThemeMode()) {
  const DisplayThemeColors theme = getDisplayThemeColors(themeMode);
  const int textH = 8 * textSize;
  const int markSize = textH + 8;
  const int markY = y - ((markSize - textH) / 2);
  drawBlockClockMark(x, markY, markSize, theme.foreground, theme.background);
  display.setTextSize(textSize);
  display.setCursor(x + markSize + (6 * textSize), y);
  display.print(title ? title : "");
}

static void drawStatusScreen(const char* title, const char* subtitle) {
  const DisplayThemeMode themeMode = statusScreenThemeMode();
  prepareScreen(themeMode);
  const int titleSize = 2;
  const int titleW = (title ? (int)strlen(title) : 0) * 6 * titleSize;
  const int iconSize = (8 * titleSize) + 8;
  const int groupW = titleW + iconSize + (6 * titleSize);
  const int x = (DEVICE_DISPLAY_WIDTH - groupW) / 2;
  const int y = (DEVICE_DISPLAY_HEIGHT / 2) - 14;
  drawTitleLine(title, x, y, titleSize, themeMode);
  display.setTextSize(1);
  display.setCursor(x + iconSize + (6 * titleSize), y + 26);
  display.print(subtitle ? subtitle : "");
  display.update();
}
static void animateEllipsis(int x, int y, DisplayThemeMode themeMode = statusScreenThemeMode()) {
  const DisplayThemeColors theme = getDisplayThemeColors(themeMode);
  display.fastmodeOn(false);
  for (int i = 0; i < 3; i++) {
    display.setTextSize(1);
    display.setTextColor(theme.foreground, theme.background);
    display.setCursor(x + (i * 6), y);
    display.print(".");
    display.update();
    if (i < 2) delay(500);
  }
  display.fastmodeOff();
  display.setTextColor(theme.foreground, theme.background);
}

static int drawBrandedStatusScreen(
  const char* title,
  const char* subtitle,
  bool animateSubtitleEllipsis
) {
  const DisplayThemeMode themeMode = statusScreenThemeMode();
  const DisplayThemeColors theme = getDisplayThemeColors(themeMode);
  prepareScreen(themeMode);

  static constexpr int TITLE_SIZE = 2;
  static constexpr int TITLE_CHAR_W = 6 * TITLE_SIZE;
  static constexpr int TITLE_CHAR_H = 8 * TITLE_SIZE;
  static constexpr int TITLE_ICON_SIZE = TITLE_CHAR_H + 8;
  static constexpr int TITLE_ICON_GAP = TITLE_CHAR_W;
  const int titleLen = title ? (int)strlen(title) : 0;
  const int centerY = DEVICE_DISPLAY_HEIGHT / 2;
  const int titleTextW = titleLen * TITLE_CHAR_W;
  const int titleGroupW = TITLE_ICON_SIZE + TITLE_ICON_GAP + titleTextW;
  const int titleX = (DEVICE_DISPLAY_WIDTH - titleGroupW) / 2;
  const int titleY = centerY - TITLE_CHAR_H - 4;
  const int titleIconY = titleY - ((TITLE_ICON_SIZE - TITLE_CHAR_H) / 2);
  drawBlockClockMark(titleX, titleIconY, TITLE_ICON_SIZE, theme.foreground, theme.background);

  display.setTextSize(TITLE_SIZE);
  const int titleTextX = titleX + TITLE_ICON_SIZE + TITLE_ICON_GAP;
  display.setCursor(titleTextX, titleY);
  display.print(title ? title : "");

  display.setTextSize(1);
  const int subtitleY = centerY + 4;
  display.setCursor(titleTextX, subtitleY);
  display.print(subtitle ? subtitle : "");

  display.update();
  if (animateSubtitleEllipsis && subtitle && subtitle[0] != '\0') {
    animateEllipsis(titleTextX + ((int)strlen(subtitle) * 6), subtitleY, themeMode);
  }
  return titleTextX;
}

static void drawWelcomeScreen() {
  drawStatusScreen("BLOCK CLOCK", "Press any button to begin");
}

static void portalQrRenderCallback(esp_qrcode_handle_t qrcode) {
  const DisplayThemeMode themeMode = statusScreenThemeMode();
  const DisplayThemeColors theme = getDisplayThemeColors(themeMode);
  static constexpr int QUIET_PX = 4;
  const int modules = esp_qrcode_get_size(qrcode);
  int modulePx = (DEVICE_DISPLAY_HEIGHT - 10 - 2 * QUIET_PX) / modules;
  if (modulePx < 2) modulePx = 2;
  if (modulePx > 4) modulePx = 4;
  const int contentPx = modules * modulePx;
  const int totalPx = contentPx + 2 * QUIET_PX;
  const int originX = DEVICE_DISPLAY_WIDTH - totalPx - 14;
  const int originY = (DEVICE_DISPLAY_HEIGHT - totalPx) / 2;
  display.fillRect(originX, originY, totalPx, totalPx, theme.background);
  for (int row = 0; row < modules; row++) {
    for (int col = 0; col < modules; col++) {
      if (esp_qrcode_get_module(qrcode, col, row)) {
        display.fillRect(
          originX + QUIET_PX + col * modulePx,
          originY + QUIET_PX + row * modulePx,
          modulePx,
          modulePx,
          theme.foreground
        );
      }
    }
  }
}

static void drawSetupPortalReadyScreen() {
  const DisplayThemeMode themeMode = statusScreenThemeMode();
  prepareScreen(themeMode);
  drawTitleLine("SETUP", 9, 9, 2, themeMode);

  display.setTextSize(1);
  display.setCursor(9, 40);
  display.print("Scan QR to join Wi-Fi,");
  display.setCursor(9, 54);
  display.print("then open 192.168.4.1");

  static constexpr int LABEL_X = 9;
  static constexpr int VALUE_X = 51;
#if DEVICE_PROFILE_E213
  display.setCursor(9, 80);
  display.print("Wi-Fi:");
  display.setCursor(9, 94);
  display.print(portalApSsid);
  display.setCursor(9, 108);
  display.print("PW:");
  display.setCursor(VALUE_X, 108);
  display.print(portalApPassword);
#else
  display.setCursor(LABEL_X, 80);
  display.print("Wi-Fi:");
  display.setCursor(VALUE_X, 80);
  display.print(portalApSsid);
  display.setCursor(LABEL_X, 94);
  display.print("PW:");
  display.setCursor(VALUE_X, 94);
  display.print(portalApPassword);
#endif

  char qrData[96];
  snprintf(qrData, sizeof(qrData), "WIFI:T:WPA;S:%s;P:%s;;", portalApSsid, portalApPassword);
  esp_qrcode_config_t qrCfg = {
    .display_func = portalQrRenderCallback,
    .max_qrcode_version = 5,
    .qrcode_ecc_level = ESP_QRCODE_ECC_LOW
  };
  esp_qrcode_generate(&qrCfg, qrData);

  display.update();
}

static void drawSetupSavedScreen() {
  drawBrandedStatusScreen("SETTINGS SAVED", "Restarting ", true);
}

static void drawFactoryResetScreen() {
  drawBrandedStatusScreen("FACTORY RESET", "Restarting ", true);
}

static int fitTextSizeForWidth(const char* text, int maxWidth, int preferredSize) {
  const int len = text ? (int)strlen(text) : 0;
  for (int size = preferredSize; size > 1; size--) {
    if (len * 6 * size <= maxWidth) return size;
  }
  return 1;
}

static void drawCenteredPriceText(const char* text, int centerX, int baselineY) {
  if (text && text[0] == '$' && text[1] != '\0') {
  int16_t boundsX = 0;
  int16_t boundsY = 0;
  uint16_t boundsW = 0;
  uint16_t boundsH = 0;

    display.setFont(&FreeSans24pt7b);
    display.setTextSize(1);
    display.getTextBounds("$", 0, 0, &boundsX, &boundsY, &boundsW, &boundsH);
    const int dollarWidth = boundsW;
    const int dollarOffset = boundsX;

    int16_t amountBoundsX = 0;
    int16_t amountBoundsY = 0;
    uint16_t amountBoundsW = 0;
    uint16_t amountBoundsH = 0;
  display.setFont(&FreeSansBold24pt7b);
    display.getTextBounds(text + 1, 0, 0, &amountBoundsX, &amountBoundsY, &amountBoundsW, &amountBoundsH);
  static constexpr int DOLLAR_AMOUNT_GAP = 4;
  const int groupWidth = dollarWidth + DOLLAR_AMOUNT_GAP + amountBoundsW;
    const int groupLeft = centerX - (groupWidth / 2);

    display.setFont(&FreeSans24pt7b);
    display.setCursor(groupLeft - dollarOffset, baselineY);
    display.print("$");
    display.setFont(&FreeSansBold24pt7b);
    display.setCursor(groupLeft + dollarWidth + DOLLAR_AMOUNT_GAP - amountBoundsX, baselineY);
    display.print(text + 1);
  } else {
    int16_t boundsX = 0;
    int16_t boundsY = 0;
    uint16_t boundsW = 0;
    uint16_t boundsH = 0;
    display.setFont(&FreeSansBold24pt7b);
    display.setTextSize(1);
    display.getTextBounds(text ? text : "", 0, 0, &boundsX, &boundsY, &boundsW, &boundsH);
    display.setCursor(centerX - ((int)boundsW / 2) - boundsX, baselineY);
    display.print(text ? text : "");
  }
  display.setFont();
  display.setTextSize(1);
}

static void formatPriceWithCommas(const char* value, char* output, size_t outputSize) {
  if (!output || outputSize == 0) return;
  output[0] = '\0';
  if (!value) return;

  size_t integerDigits = 0;
  for (const char* cursor = value; *cursor && *cursor != '.'; cursor++) {
    if (*cursor >= '0' && *cursor <= '9') integerDigits++;
  }

  const char* decimalPoint = strchr(value, '.');
  size_t written = 0;
  size_t seenDigits = 0;
  for (const char* cursor = value; *cursor && written < outputSize - 1; cursor++) {
    if (*cursor == ',') continue;
    if (*cursor >= '0' && *cursor <= '9') {
      if (!decimalPoint || cursor < decimalPoint) {
        if (seenDigits > 0 && (integerDigits - seenDigits) % 3 == 0) {
          if (written >= outputSize - 1) break;
          output[written++] = ',';
        }
        seenDigits++;
      }
    }
    output[written++] = *cursor;
  }
  output[written] = '\0';
}

static constexpr int DASH_MARGIN = 8;

// Inverted label: filled block with background-coloured text.
static int drawChip(const char* text, int x, int y, const DisplayThemeColors& theme) {
  const int width = estimateTextWidthSize1(text) + 3;
  display.fillRect(x, y, width, 9, theme.foreground);
  display.setTextSize(1);
  // Transparent text (one colour, not fg + bg): the library's opaque size-1 path fills the spacing
  // column with a horizontal bar instead of a vertical line, leaving a stub past the chip's corner.
  display.setTextColor(theme.background);
  display.setCursor(x + 2, y + 1);
  display.print(text);
  display.setTextColor(theme.foreground, theme.background);
  return width;
}

static void drawTextSize1(const char* text, int x, int y) {
  display.setTextSize(1);
  display.setCursor(x, y);
  display.print(text ? text : "");
}

static uint64_t blockSubsidySats(uint32_t blockHeight) {
  const uint32_t halvings = blockHeight / HALVING_INTERVAL_BLOCKS;
  return halvings >= 64 ? 0 : (5000000000ULL >> halvings);
}

// Whole BTC with `decimals` places, optionally trimming trailing zeros (keeping two).
static void formatBtcFromSats(int64_t sats, int decimals, bool trimZeros, char* output, size_t outputSize) {
  int64_t scale = 1;
  for (int index = decimals; index < 8; index++) scale *= 10;
  int64_t unit = 1;
  for (int index = 0; index < decimals; index++) unit *= 10;
  const int64_t rounded = (sats + (scale / 2)) / scale;
  snprintf(output, outputSize, "%lld.%0*lld", (long long)(rounded / unit), decimals, (long long)(rounded % unit));
  if (!trimZeros) return;
  size_t length = strlen(output);
  while (length > 0 && output[length - 1] == '0' && output[length - 3] != '.') output[--length] = '\0';
}

static void formatFiatFromSats(int64_t sats, long price, const char* prefix, char* output, size_t outputSize) {
  char raw[24];
  char grouped[28];
  snprintf(raw, sizeof(raw), "%lld", (long long)(((sats * price) + 50000000LL) / 100000000LL));
  formatPriceWithCommas(raw, grouped, sizeof(grouped));
  snprintf(output, outputSize, "%s%s", prefix, grouped);
}


static constexpr uint64_t MAX_SUPPLY_SATS = 2100000000000000ULL;
static constexpr int DASHBOARD_SCREEN_COUNT = 3;

// Coins issued through `tipHeight` (genesis included), in sats.
static uint64_t issuedSupplySats(uint32_t tipHeight) {
  uint64_t supply = 0;
  uint64_t remaining = (uint64_t)tipHeight + 1;
  for (uint32_t epoch = 0; remaining > 0 && epoch < 64; epoch++) {
    const uint64_t blocks = remaining < HALVING_INTERVAL_BLOCKS ? remaining : HALVING_INTERVAL_BLOCKS;
    supply += blocks * blockSubsidySats(epoch * HALVING_INTERVAL_BLOCKS);
    remaining -= blocks;
  }
  return supply;
}

// 1.35T / 210.7B / 4.2M style amounts.
static void formatCompactAmount(double value, const char* prefix, char* output, size_t outputSize) {
  const char* suffix = "";
  double scaled = value;
  if (value >= 1e12) { scaled = value / 1e12; suffix = "T"; }
  else if (value >= 1e9) { scaled = value / 1e9; suffix = "B"; }
  else if (value >= 1e6) { scaled = value / 1e6; suffix = "M"; }
  snprintf(output, outputSize, "%s%.*f%s", prefix, scaled >= 100.0 ? 0 : (scaled >= 10.0 ? 1 : 2), scaled, suffix);
}

static void formatGroupedInteger(unsigned long long value, char* output, size_t outputSize) {
  char raw[24];
  snprintf(raw, sizeof(raw), "%llu", value);
  formatPriceWithCommas(raw, output, outputSize);
}

static void drawSizedText(const char* text, int x, int y, int size) {
  display.setTextSize(size);
  display.setCursor(x, y);
  display.print(text ? text : "");
}

// Large number with a small unit after it, bottom aligned. Returns the total width.
static int drawValueUnit(const char* value, const char* unit, int x, int y, int size) {
  const int valueW = (int)strlen(value) * 6 * size;
  drawSizedText(value, x, y, size);
  if (unit && unit[0]) drawTextSize1(unit, x + valueW + 4, y + (8 * size) - 8);
  return valueW + (unit && unit[0] ? 4 + estimateTextWidthSize1(unit) : 0);
}

// The built-in font's '~' sits near the top of the cell, well above the middle of the digits.
// This draws a 5x5 tilde centred on the digit rows, then the text after it.
static void drawApproxText(const char* text, int x, int y, int size, const DisplayThemeColors& theme) {
  static constexpr uint8_t TILDE_ROWS[5] = { 3, 2, 3, 4, 3 };
  for (int column = 0; column < 5; column++) {
    display.fillRect(x + (column * size), y + (TILDE_ROWS[column] * size), size, size, theme.foreground);
  }
  display.setTextSize(size);
  display.setCursor(x + (6 * size), y);
  display.print(text ? text : "");
}

// "next 6:00 AM", or "next Sat 6:00 AM" when that is not today: lets a glance tell a sleeping clock from a dead one.
static void formatNextRefresh(time_t when, char* output, size_t outputSize) {
  output[0] = '\0';
  const time_t now = time(nullptr);
  struct tm next = {};
  struct tm today = {};
  if (when < 1700000000 || now < 1700000000 || !localtime_r(&when, &next) || !localtime_r(&now, &today)) return;
  const int hour12 = (next.tm_hour % 12) == 0 ? 12 : (next.tm_hour % 12);
  const char* meridiem = next.tm_hour < 12 ? "AM" : "PM";
  if (next.tm_yday == today.tm_yday && next.tm_year == today.tm_year) {
    snprintf(output, outputSize, "next %d:%02d %s", hour12, next.tm_min, meridiem);
  } else {
    char weekday[8];
    strftime(weekday, sizeof(weekday), "%a", &next);
    snprintf(output, outputSize, "next %s %d:%02d %s", weekday, hour12, next.tm_min, meridiem);
  }
}

// Footer: what this screen is about (left), when the next refresh is due, and which screen this is (right).
static void drawFooter(int current, const char* title, const DisplayThemeColors& theme, time_t nextRefreshAt) {
  static constexpr int DOT = 5;
  static constexpr int PITCH = 9;
  const int y = DEVICE_DISPLAY_HEIGHT - 13;  // clear of the case lip
  drawTextSize1(title, DASH_MARGIN, y);
  const int left = DEVICE_DISPLAY_WIDTH - DASH_MARGIN - ((DASHBOARD_SCREEN_COUNT * PITCH) - (PITCH - DOT));
  char nextText[24];
  formatNextRefresh(nextRefreshAt, nextText, sizeof(nextText));
  if (nextText[0]) drawTextSize1(nextText, left - 8 - estimateTextWidthSize1(nextText), y);
  for (int index = 0; index < DASHBOARD_SCREEN_COUNT; index++) {
    const int x = left + (index * PITCH);
    display.fillRect(x, y + 1, DOT, DOT, theme.foreground);
    if (index != current) display.fillRect(x + 1, y + 2, DOT - 2, DOT - 2, theme.background);
  }
}

static void drawStatusIcons(const BlockData& data, const DeviceConfig& cfg, DisplayThemeMode themeMode) {
  const int batteryIconX = drawBatteryStatus(data.batteryPercent, cfg.batteryDisplayMode, themeMode);
  if (!data.wifiOk) drawNoWifiStatusLeftOfBattery(batteryIconX, themeMode);
}

static const char* fiatPrefixFor(const DeviceConfig& cfg) {
  return cfg.currencyCode == CURRENCY_USD ? "$" : currencyCodeLabel(cfg.currencyCode);
}

// Screen 1: price, sats per unit, update time, market cap and the all-time high.
static void drawPriceScreen(const BlockData& data, const DeviceConfig& cfg) {
  const DisplayThemeMode themeMode = sanitizeThemeMode(cfg.displayThemeMode);
  const DisplayThemeColors theme = getDisplayThemeColors(themeMode);
  prepareScreen(themeMode);

  const int centerX = DEVICE_DISPLAY_WIDTH / 2;
  const bool usd = cfg.currencyCode == CURRENCY_USD;
  const char* fiatPrefix = fiatPrefixFor(cfg);
  char groupedPrice[32];
  char priceText[24];
  formatPriceWithCommas(data.priceValue, groupedPrice, sizeof(groupedPrice));
  snprintf(priceText, sizeof(priceText), "%s%s%s", fiatPrefix, usd ? "" : " ", groupedPrice);
  drawCenteredPriceText(priceText, centerX, 37);

  const long price = atol(data.priceValue);
  const time_t nowT = time(nullptr);

  // Second tier: sats per currency unit (left) and when the data was last updated (right).
  static constexpr int TIER2_Y = 47;
  char satsNumber[16] = "--";
  if (price > 0) {
    char satsRaw[16];
    snprintf(satsRaw, sizeof(satsRaw), "%ld", (100000000L + (price / 2)) / price);
    formatPriceWithCommas(satsRaw, satsNumber, sizeof(satsNumber));
  }
  char satsUnit[12];
  snprintf(satsUnit, sizeof(satsUnit), "sats/%s", fiatPrefix);
  const int satsGroupW = ((int)strlen(satsNumber) * 12) + 6 + estimateTextWidthSize1(satsUnit);
  drawValueUnit(satsNumber, satsUnit, ((DASH_MARGIN + centerX) / 2) - (satsGroupW / 2), TIER2_Y, 2);

  const String timestamp = formatTimestamp(nowT >= 1700000000 ? nowT : data.updatedAt);
  char clockText[16];
  char dateText[16] = "";
  const char* comma = strchr(timestamp.c_str(), ',');
  if (comma) {
    snprintf(dateText, sizeof(dateText), "%.*s", (int)(comma - timestamp.c_str()), timestamp.c_str());
    snprintf(clockText, sizeof(clockText), "%s", comma + 2);
  } else {
    snprintf(clockText, sizeof(clockText), "%s", timestamp.c_str());
  }
  const int updatedGroupW = ((int)strlen(clockText) * 12) + (dateText[0] ? 6 + estimateTextWidthSize1(dateText) : 0);
  const int updatedX = clampInt(((centerX + DEVICE_DISPLAY_WIDTH - DASH_MARGIN) / 2) - (updatedGroupW / 2), centerX + 10, DEVICE_DISPLAY_WIDTH - DASH_MARGIN - updatedGroupW);
  drawValueUnit(clockText, dateText, updatedX, TIER2_Y, 2);

  display.drawLine(DASH_MARGIN, 67, DEVICE_DISPLAY_WIDTH - DASH_MARGIN, 67, theme.foreground);

  // Third tier: market cap, all-time high, and how far the price sits below it.
  static constexpr int CELL_Y = 71;
  static constexpr int CELL_X[3] = { DASH_MARGIN, 100, 210 };
  display.drawLine(94, CELL_Y, 94, CELL_Y + 36, theme.foreground);
  display.drawLine(204, CELL_Y, 204, CELL_Y + 36, theme.foreground);

  const uint32_t height = (uint32_t)strtoul(data.blockHeight, nullptr, 10);
  const uint64_t supplySats = height > 0 ? issuedSupplySats(height) : 0;
  char marketCap[24] = "--";
  if (price > 0 && supplySats > 0) {
    formatCompactAmount(((double)supplySats / 1e8) * (double)price, fiatPrefix, marketCap, sizeof(marketCap));
  }

  char athText[24] = "--";
  char athDate[24] = "";
  char fromAth[16] = "--";
  char athAge[24] = "";
  if (data.athPrice > 0) {
    char grouped[24];
    formatGroupedInteger((unsigned long long)data.athPrice, grouped, sizeof(grouped));
    snprintf(athText, sizeof(athText), "%s%s", fiatPrefix, grouped);
    if (price > 0) snprintf(fromAth, sizeof(fromAth), "%+.1f%%", ((double)price / (double)data.athPrice - 1.0) * 100.0);
  }
  if (data.athTimestamp > 0) {
    struct tm athTm = {};
    const time_t athWhen = data.athTimestamp;
    if (localtime_r(&athWhen, &athTm)) {
      char month[8];
      strftime(month, sizeof(month), "%b", &athTm);
      snprintf(athDate, sizeof(athDate), "%s %d, %d", month, athTm.tm_mday, athTm.tm_year + 1900);
    }
    if (nowT >= data.athTimestamp) snprintf(athAge, sizeof(athAge), "%ld days ago", (long)((nowT - data.athTimestamp) / 86400));
  }

  drawChip("MARKET CAP", CELL_X[0], CELL_Y, theme);
  drawSizedText(marketCap, CELL_X[0], CELL_Y + 12, 2);
  drawChip("ALL-TIME HIGH", CELL_X[1], CELL_Y, theme);
  drawSizedText(athText, CELL_X[1], CELL_Y + 12, 2);
  drawTextSize1(athDate, CELL_X[1], CELL_Y + 31);
  drawChip("FROM HIGH", CELL_X[2], CELL_Y, theme);
  drawSizedText(fromAth, CELL_X[2], CELL_Y + 12, 2);
  drawTextSize1(athAge, CELL_X[2], CELL_Y + 31);

  drawFooter(0, "WHAT IT IS WORTH", theme, data.nextRefreshAt);
  drawStatusIcons(data, cfg, themeMode);
  display.update();
}

// Screen 2: the chain tip, what the last block paid, hashrate and mempool fee rates.
static void drawNetworkScreen(const BlockData& data, const DeviceConfig& cfg) {
  const DisplayThemeMode themeMode = sanitizeThemeMode(cfg.displayThemeMode);
  const DisplayThemeColors theme = getDisplayThemeColors(themeMode);
  prepareScreen(themeMode);

  const char* fiatPrefix = fiatPrefixFor(cfg);
  const long price = atol(data.priceValue);
  const time_t nowT = time(nullptr);
  const uint32_t height = (uint32_t)strtoul(data.blockHeight, nullptr, 10);

  static constexpr int RIGHT_X = 154;
  char blockAge[24] = "";
  if (data.lastBlockTimestamp > 0 && nowT >= data.lastBlockTimestamp) {
    const unsigned long minutes = (unsigned long)((nowT - data.lastBlockTimestamp) / 60);
    if (minutes >= 60) snprintf(blockAge, sizeof(blockAge), "%luh %lum ago", minutes / 60, minutes % 60);
    else snprintf(blockAge, sizeof(blockAge), "%lu min ago", minutes);
  }
  const int blockChipW = drawChip("BLOCK", DASH_MARGIN, 3, theme);
  drawTextSize1(blockAge, DASH_MARGIN + blockChipW + 6, 4);
  char groupedHeight[24];
  formatPriceWithCommas(data.blockHeight, groupedHeight, sizeof(groupedHeight));
  // Seven digits and a comma fit at size 3; 1,000,000 and up steps down to size 2.
  drawSizedText(groupedHeight, DASH_MARGIN, 15, fitTextSizeForWidth(groupedHeight, RIGHT_X - 6 - DASH_MARGIN - 2, 3));

  display.drawLine(RIGHT_X - 6, 3, RIGHT_X - 6, 38, theme.foreground);
  drawChip("HASHRATE", RIGHT_X, 3, theme);
  // No caption under it, so it gets the same size as the block height unless it would not fit.
  const int hashrateSize = ((int)strlen(data.hashrateEhs) * 18) + 4 + estimateTextWidthSize1("EH/s") <= DEVICE_DISPLAY_WIDTH - DASH_MARGIN - RIGHT_X ? 3 : 2;
  drawValueUnit(data.hashrateEhs, "EH/s", RIGHT_X, 15, hashrateSize);

  display.drawLine(DASH_MARGIN, 43, DEVICE_DISPLAY_WIDTH - DASH_MARGIN, 43, theme.foreground);

  // Block reward (subsidy) and the last block's fees, in BTC and fiat.
  static constexpr int CELL_Y = 46;
  display.drawLine(RIGHT_X - 6, CELL_Y, RIGHT_X - 6, CELL_Y + 34, theme.foreground);

  char rewardBtc[24] = "--";
  char rewardFiat[24] = "--";
  if (height > 0) {
    const int64_t subsidy = (int64_t)blockSubsidySats(height + 1);
    formatBtcFromSats(subsidy, 4, true, rewardBtc, sizeof(rewardBtc));
    if (price > 0) formatFiatFromSats(subsidy, price, fiatPrefix, rewardFiat, sizeof(rewardFiat));
  }
  char feesBtc[24] = "--";
  char feesFiat[24] = "--";
  if (data.lastBlockFeesSats >= 0) {
    formatBtcFromSats(data.lastBlockFeesSats, 4, false, feesBtc, sizeof(feesBtc));
    if (price > 0) formatFiatFromSats(data.lastBlockFeesSats, price, fiatPrefix, feesFiat, sizeof(feesFiat));
  }
  drawChip("BLOCK REWARD", DASH_MARGIN, CELL_Y, theme);
  drawValueUnit(rewardBtc, "BTC", DASH_MARGIN, CELL_Y + 11, 2);
  drawTextSize1(rewardFiat, DASH_MARGIN, CELL_Y + 29);
  drawChip("LAST BLOCK FEES", RIGHT_X, CELL_Y, theme);
  drawValueUnit(feesBtc, "BTC", RIGHT_X, CELL_Y + 11, 2);
  drawTextSize1(feesFiat, RIGHT_X, CELL_Y + 29);

  display.drawLine(DASH_MARGIN, 84, DEVICE_DISPLAY_WIDTH - DASH_MARGIN, 84, theme.foreground);

  // Mempool fee rates: three equal columns, chip label over a size-2 value.
  const char* feeLabels[] = { "LOW", "MED", "HIGH" };
  const float feeRates[] = { data.lowFeeSatVb, data.mediumFeeSatVb, data.highFeeSatVb };
  const int feeColumnWidth = 72;
  for (int index = 0; index < 3; index++) {
    char value[12] = "--";
    if (feeRates[index] >= 0.0f) {
      const float roundedRate = roundf(feeRates[index]);
      if (fabsf(feeRates[index] - roundedRate) < 0.05f) snprintf(value, sizeof(value), "%.0f", roundedRate);
      else snprintf(value, sizeof(value), "%.1f", feeRates[index]);
    }
    const int x = DASH_MARGIN + (index * feeColumnWidth);
    drawChip(feeLabels[index], x, 87, theme);
    drawSizedText(value, x, 98, 2);
  }
  drawTextSize1("sat/vB", DASH_MARGIN + (3 * feeColumnWidth) + 4, 103);

  drawFooter(1, "THE NETWORK RIGHT NOW", theme, data.nextRefreshAt);
  drawStatusIcons(data, cfg, themeMode);
  display.update();
}

// Screen 3: how much of the 21M is out, what is left, and the next halving.
static void drawSupplyScreen(const BlockData& data, const DeviceConfig& cfg) {
  const DisplayThemeMode themeMode = sanitizeThemeMode(cfg.displayThemeMode);
  const DisplayThemeColors theme = getDisplayThemeColors(themeMode);
  prepareScreen(themeMode);

  const uint32_t height = (uint32_t)strtoul(data.blockHeight, nullptr, 10);
  const uint64_t supplySats = height > 0 ? issuedSupplySats(height) : 0;
  const double issuedPct = ((double)supplySats / (double)MAX_SUPPLY_SATS) * 100.0;

  static constexpr int RIGHT_X = 154;
  const int issuedChipW = drawChip("ISSUED", DASH_MARGIN, 3, theme);
  drawTextSize1("of 21M", DASH_MARGIN + issuedChipW + 6, 4);
  char issuedText[16] = "--";
  if (supplySats > 0) snprintf(issuedText, sizeof(issuedText), "%.2f%%", issuedPct);
  drawSizedText(issuedText, DASH_MARGIN, 15, 3);

  display.drawLine(RIGHT_X - 6, 3, RIGHT_X - 6, 38, theme.foreground);
  drawChip("NEXT HALVING", RIGHT_X, 3, theme);
  char halvingText[24];
  formatPriceWithCommas(data.blocksToHalving, halvingText, sizeof(halvingText));
  drawValueUnit(halvingText, "blocks", RIGHT_X, 15, 2);
  const long halvingBlocks = atol(data.blocksToHalving);
  char halvingEta[24] = "";
  if (halvingBlocks > 0) {
    const long days = (halvingBlocks * 10L) / (60L * 24L);
    snprintf(halvingEta, sizeof(halvingEta), "%ld days", days);
  }
  if (halvingEta[0]) drawApproxText(halvingEta, RIGHT_X, 31, 1, theme);

  display.drawLine(DASH_MARGIN, 42, DEVICE_DISPLAY_WIDTH - DASH_MARGIN, 42, theme.foreground);

  // Progress bar with a tick at each halving's cumulative share (50 / 75 / 87.5 / 93.75 %).
  static constexpr int BAR_X = DASH_MARGIN;
  static constexpr int BAR_Y = 52;
  static constexpr int BAR_H = 12;
  const int barW = DEVICE_DISPLAY_WIDTH - (2 * DASH_MARGIN);
  display.drawRoundRect(BAR_X, BAR_Y, barW, BAR_H, 2, theme.foreground);
  if (supplySats > 0) {
    const int fillW = (int)(((double)(barW - 4)) * (issuedPct / 100.0));
    display.fillRect(BAR_X + 2, BAR_Y + 2, fillW, BAR_H - 4, theme.foreground);
  }
  const double halvingShares[] = { 50.0, 75.0, 87.5, 93.75, 96.875 };
  for (double share : halvingShares) {
    const int tickX = BAR_X + 2 + (int)(((double)(barW - 4)) * (share / 100.0));
    display.drawLine(tickX, BAR_Y - 4, tickX, BAR_Y - 2, theme.foreground);
  }

  static constexpr int CELL_Y = 71;
  static constexpr int CELL_X[3] = { DASH_MARGIN, 100, 204 };
  display.drawLine(94, CELL_Y, 94, CELL_Y + 38, theme.foreground);
  display.drawLine(198, CELL_Y, 198, CELL_Y + 38, theme.foreground);

  char supplyShort[16] = "--";
  char leftShort[16] = "--";
  char perDay[16] = "--";
  if (supplySats > 0) {
    const uint64_t leftSats = MAX_SUPPLY_SATS - supplySats;
    snprintf(supplyShort, sizeof(supplyShort), "%.2fM", (double)supplySats / 1e14);
    formatGroupedInteger((unsigned long long)(leftSats / 100000000ULL), leftShort, sizeof(leftShort));
    snprintf(perDay, sizeof(perDay), "%llu", (unsigned long long)((blockSubsidySats(height + 1) * 144ULL) / 100000000ULL));
  }
  drawChip("SUPPLY", CELL_X[0], CELL_Y, theme);
  drawSizedText(supplyShort, CELL_X[0], CELL_Y + 12, 2);
  drawTextSize1("BTC", CELL_X[0], CELL_Y + 31);
  drawChip("LEFT TO MINE", CELL_X[1], CELL_Y, theme);
  drawSizedText(leftShort, CELL_X[1], CELL_Y + 12, 2);
  drawTextSize1("BTC", CELL_X[1], CELL_Y + 31);
  drawChip("NEW COINS", CELL_X[2], CELL_Y, theme);
  drawApproxText(perDay, CELL_X[2], CELL_Y + 12, 2, theme);
  drawTextSize1("BTC per day", CELL_X[2], CELL_Y + 31);

  drawFooter(2, "HOW SCARCE IT IS", theme, data.nextRefreshAt);
  drawStatusIcons(data, cfg, themeMode);
  display.update();
}

static void drawBlockDashboard(const BlockData& data, const DeviceConfig& cfg, int screenIndex = 0) {
  switch (screenIndex) {
    case 1: drawNetworkScreen(data, cfg); break;
    case 2: drawSupplyScreen(data, cfg); break;
    default: drawPriceScreen(data, cfg); break;
  }
}
