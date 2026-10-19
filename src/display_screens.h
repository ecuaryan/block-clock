#pragma once

static constexpr int NO_WIFI_ICON_W = 14;
static constexpr int NO_WIFI_ICON_H = 12;

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

static void drawNoWifiStatus(DisplayThemeMode themeMode) {
  const DisplayThemeColors theme = getDisplayThemeColors(themeMode);
  const int iconX = DEVICE_DISPLAY_WIDTH - NO_WIFI_ICON_W - 3;
  drawNoWifiStatusIcon(iconX, 2, theme.foreground, theme.background);
}

static void drawBlockClockMark(int x, int y, int size, uint16_t color = BLACK, uint16_t background = WHITE) {
  const int wall = size >= 20 ? 4 : 3;
  display.fillRect(x, y, size, size, color);
  display.fillRect(x + wall, y + wall, size - (wall * 2), size - (wall * 2), background);
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

static void drawMetric(const char* value, const char* label, int x, int y, int textSize) {
  const int size = textSize;
  display.setTextSize(size);
  display.setCursor(x, y);
  display.print(value ? value : "--");
  display.setTextSize(1);
  display.setCursor(x, y + (8 * size) + 2);
  display.print(label ? label : "");
}

static void drawCenteredDashboardText(const char* text, int centerX, int y, int textSize) {
  const int textWidth = text ? (int)strlen(text) * 6 * textSize : 0;
  display.setTextSize(textSize);
  display.setCursor(centerX - (textWidth / 2), y);
  display.print(text ? text : "");
}

static uint8_t feeLabelGlyphRow(char character, int row) {
  static const uint8_t glyphs[26][7] = {
    { 14, 17, 17, 31, 17, 17, 17 }, { 30, 17, 17, 30, 17, 17, 30 },
    { 14, 17, 16, 16, 16, 17, 14 }, { 30, 17, 17, 17, 17, 17, 30 },
    { 31, 16, 16, 30, 16, 16, 31 }, { 31, 16, 16, 30, 16, 16, 16 },
    { 14, 17, 16, 23, 17, 17, 15 }, { 17, 17, 17, 31, 17, 17, 17 },
    { 14, 4, 4, 4, 4, 4, 14 }, { 7, 2, 2, 2, 18, 18, 12 },
    { 17, 18, 20, 24, 20, 18, 17 }, { 16, 16, 16, 16, 16, 16, 31 },
    { 17, 27, 21, 21, 17, 17, 17 }, { 17, 25, 21, 19, 17, 17, 17 },
    { 14, 17, 17, 17, 17, 17, 14 }, { 30, 17, 17, 30, 16, 16, 16 },
    { 14, 17, 17, 17, 21, 18, 13 }, { 30, 17, 17, 30, 20, 18, 17 },
    { 15, 16, 16, 14, 1, 1, 30 }, { 31, 4, 4, 4, 4, 4, 4 },
    { 17, 17, 17, 17, 17, 17, 14 }, { 17, 17, 17, 17, 17, 10, 4 },
    { 17, 17, 17, 21, 21, 21, 10 }, { 17, 17, 10, 4, 10, 17, 17 },
    { 17, 17, 10, 4, 4, 4, 4 }, { 31, 1, 2, 4, 8, 16, 31 }
  };
  if (character < 'A' || character > 'Z' || row < 0 || row >= 7) return 0;
  return glyphs[character - 'A'][row];
}

static void drawFeeLabel(const char* text, int centerX, int y, uint16_t color) {
  if (!text) return;
  const int length = (int)strlen(text);
  const int textWidth = length > 0 ? (length * 6) - 1 : 0;
  const int startX = centerX - (textWidth / 2);
  for (int index = 0; index < length; index++) {
    for (int row = 0; row < 7; row++) {
      const uint8_t bits = feeLabelGlyphRow(text[index], row);
      for (int column = 0; column < 5; column++) {
        if (bits & (1U << (4 - column))) {
          display.drawPixel(startX + (index * 6) + column, y + row, color);
        }
      }
    }
  }
}

static void drawBlockDashboard(const BlockData& data, const DeviceConfig& cfg) {
  const DisplayThemeMode themeMode = sanitizeThemeMode(cfg.displayThemeMode);
  const DisplayThemeColors theme = getDisplayThemeColors(themeMode);
  prepareScreen(themeMode);

  const int centerX = DEVICE_DISPLAY_WIDTH / 2;
  char priceText[24];
  const char* currencyPrefix = cfg.currencyCode == CURRENCY_USD ? "$" : currencyCodeLabel(cfg.currencyCode);
  const char* currencySeparator = cfg.currencyCode == CURRENCY_USD ? "" : " ";
  snprintf(priceText, sizeof(priceText), "%s%s%s", currencyPrefix, currencySeparator, data.priceValue);
  const int priceSize = fitTextSizeForWidth(priceText, DEVICE_DISPLAY_WIDTH - 24, 6);
  drawCenteredDashboardText(priceText, centerX, 3, priceSize);

  const time_t nowT = time(nullptr);
  const String timestamp = formatTimestamp(nowT >= 1700000000 ? nowT : data.updatedAt);
  char updatedText[48];
  snprintf(updatedText, sizeof(updatedText), "Updated %s", timestamp.c_str());
  drawCenteredDashboardText(updatedText, centerX, 51, 1);

  const int margin = 8;
  const int columnWidth = (DEVICE_DISPLAY_WIDTH - (2 * margin)) / 3;
  display.drawLine(margin, 62, DEVICE_DISPLAY_WIDTH - margin, 62, theme.foreground);
  for (int index = 1; index < 3; index++) {
    const int dividerX = margin + (index * columnWidth);
    display.drawLine(dividerX, 66, dividerX, 95, theme.foreground);
  }

  const char* topValues[] = { data.blockHeight, data.blocksToHalving, data.hashrateEhs };
  const char* topLabels[] = { "", "TO HALVING", "EH/s" };
  uint32_t minutesSinceLastBlock = 0;
  const bool hasBlockAge = data.lastBlockTimestamp > 0 && nowT >= data.lastBlockTimestamp;
  if (hasBlockAge) minutesSinceLastBlock = (uint32_t)((nowT - data.lastBlockTimestamp) / 60);
  char blockAge[32] = "Age unavailable";
  if (hasBlockAge) snprintf(blockAge, sizeof(blockAge), "%lu minutes ago", (unsigned long)minutesSinceLastBlock);

  for (int index = 0; index < 3; index++) {
    const int columnX = margin + (index * columnWidth);
    const int columnCenter = columnX + (columnWidth / 2);
    const int valueSize = fitTextSizeForWidth(topValues[index], columnWidth - 8, 2);
    drawCenteredDashboardText(topValues[index], columnCenter, 66, valueSize);
    if (topLabels[index][0] != '\0') {
      drawCenteredDashboardText(topLabels[index], columnCenter, 86, 1);
    }
  }
  drawCenteredDashboardText(blockAge, margin + (columnWidth / 2), 86, 1);

  const int feeDividerY = 98;
  display.drawLine(margin, feeDividerY, DEVICE_DISPLAY_WIDTH - margin, feeDividerY, theme.foreground);
  const int feeColumnWidth = (DEVICE_DISPLAY_WIDTH - (2 * margin)) / 3;
  for (int index = 1; index < 3; index++) {
    const int dividerX = margin + (index * feeColumnWidth);
    display.drawLine(dividerX, 103, dividerX, DEVICE_DISPLAY_HEIGHT - 1, theme.foreground);
  }

  char feeValues[3][12] = { "--", "--", "--" };
  const float feeRates[] = { data.lowFeeSatVb, data.mediumFeeSatVb, data.highFeeSatVb };
  for (int index = 0; index < 3; index++) {
    if (feeRates[index] < 0.0f) continue;
    const float roundedRate = roundf(feeRates[index]);
    if (fabsf(feeRates[index] - roundedRate) < 0.05f) {
      snprintf(feeValues[index], sizeof(feeValues[index]), "%.0f", roundedRate);
    } else {
      snprintf(feeValues[index], sizeof(feeValues[index]), "%.1f", feeRates[index]);
    }
  }
  const char* feeLabels[] = { "LOW", "MEDIUM", "HIGH" };
  for (int index = 0; index < 3; index++) {
    const int columnX = margin + (index * feeColumnWidth);
    const int columnCenter = columnX + (feeColumnWidth / 2);
    const int valueWidth = (int)strlen(feeValues[index]) * 6;
    const int unitWidth = 6 * 6;
    const int groupWidth = valueWidth + unitWidth + 3;
    const int valueX = columnCenter - (groupWidth / 2);
    drawFeeLabel(feeLabels[index], columnCenter, 101, theme.foreground);
    display.setTextSize(1);
    display.setCursor(valueX, 115);
    display.print(feeValues[index]);
    display.setCursor(valueX + valueWidth + 3, 115);
    display.print("sat/vB");
  }

  if (!data.wifiOk) {
    drawNoWifiStatus(themeMode);
  }

  display.update();
}
