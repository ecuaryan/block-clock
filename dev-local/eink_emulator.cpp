#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

using String = std::string;

struct GFXfont {
  bool bold;
};

class IPAddress {
public:
  IPAddress(int, int, int, int) {}
};

class EInkDisplay_VisionMasterE290 {};
class EInkDisplay_VisionMasterE213 {};

#if defined(PROFILE_E213)
#define HELTEC_VISION_MASTER_E_213 1
#else
#define HELTEC_VISION_MASTER_E290 1
#endif
#include "../src/config.h"

static constexpr uint16_t BLACK = 0x0000;
static constexpr uint16_t WHITE = 0xffff;

struct DisplayThemeColors {
  uint16_t foreground;
  uint16_t background;
};

static DisplayThemeMode statusScreenThemeMode();
static DisplayThemeColors getDisplayThemeColors(DisplayThemeMode mode);

template <typename T>
static T min(T left, T right) {
  return std::min(left, right);
}

static void delay(uint32_t) {}

static std::string svgColor(uint16_t color) {
  if (color == BLACK) return "#000000";
  if (color == WHITE) return "#ffffff";
  return "#000000";
}

static std::string escapeXml(const std::string& value) {
  std::string escaped;
  for (char character : value) {
    switch (character) {
      case '&': escaped += "&amp;"; break;
      case '<': escaped += "&lt;"; break;
      case '>': escaped += "&gt;"; break;
      case '"': escaped += "&quot;"; break;
      case '\'': escaped += "&apos;"; break;
      default: escaped += character; break;
    }
  }
  return escaped;
}

class SvgDisplay {
public:
  void setRotation(int) {}
  void setWindow(int, int, int, int) {}
  void clearMemory() { elements.clear(); }
  void setTextColor(uint16_t foreground, uint16_t) { textColor = foreground; }
  void setTextSize(int size) { textSize = std::max(1, size); }
  void setFont(const void* font = nullptr) {
    customFont = font != nullptr;
    customBold = font && static_cast<const GFXfont*>(font)->bold;
  }
  void getTextBounds(const char* text, int16_t, int16_t, int16_t* boundsX, int16_t* boundsY,
                     uint16_t* boundsW, uint16_t* boundsH) const {
    if (boundsX) *boundsX = 0;
    if (boundsY) *boundsY = customFont ? -35 : -8;
    if (boundsW) *boundsW = (uint16_t)(text ? std::strlen(text) * (customFont ? 26 : 6 * textSize) : 0);
    if (boundsH) *boundsH = customFont ? 42 : (uint16_t)(8 * textSize);
  }
  void setCursor(int x, int y) { cursorX = x; cursorY = y; }
  void fastmodeOn(bool) {}
  void fastmodeOff() {}
  void update() {}

  void fillScreen(uint16_t color) {
    fillRect(0, 0, DEVICE_DISPLAY_WIDTH, DEVICE_DISPLAY_HEIGHT, color);
  }

  void fillRect(int x, int y, int width, int height, uint16_t color) {
    if (width <= 0 || height <= 0) return;
    std::ostringstream element;
    element << "<rect x=\"" << x << "\" y=\"" << y << "\" width=\"" << width
            << "\" height=\"" << height << "\" fill=\"" << svgColor(color) << "\"/>";
    elements.push_back(element.str());
  }

  void drawRoundRect(int x, int y, int width, int height, int radius, uint16_t color) {
    std::ostringstream element;
    element << "<rect x=\"" << x << "\" y=\"" << y << "\" width=\"" << width
            << "\" height=\"" << height << "\" rx=\"" << radius
            << "\" fill=\"none\" stroke=\"" << svgColor(color) << "\" stroke-width=\"1\"/>";
    elements.push_back(element.str());
  }

  void drawPixel(int x, int y, uint16_t color) {
    fillRect(x, y, 1, 1, color);
  }

  void drawLine(int x1, int y1, int x2, int y2, uint16_t color) {
    std::ostringstream element;
    element << "<line x1=\"" << x1 << "\" y1=\"" << y1 << "\" x2=\"" << x2
            << "\" y2=\"" << y2 << "\" stroke=\"" << svgColor(color)
            << "\" stroke-width=\"1\"/>";
    elements.push_back(element.str());
  }

  void print(const char* text) {
    if (!text) return;
    std::ostringstream element;
        element << "<text x=\"" << cursorX << "\" y=\""
          << (customFont ? cursorY : cursorY + 8 * textSize)
          << "\" fill=\"" << svgColor(textColor) << "\" font-family=\""
          << (customFont ? "Arial, Helvetica, sans-serif" : "monospace") << "\""
          << (customBold ? " font-weight=\"bold\"" : "")
          << " font-size=\"" << (customFont ? 42 : 10 * textSize) << "\" xml:space=\"preserve\">"
            << escapeXml(text) << "</text>";
    elements.push_back(element.str());
        cursorX += (int)std::strlen(text) * (customFont ? 26 : 6 * textSize);
  }

  void print(const String& text) { print(text.c_str()); }

  bool writeSvg(const std::string& path, const std::string& title) const {
    std::ofstream output(path);
    if (!output) return false;
    const DisplayThemeColors theme = getDisplayThemeColors(statusScreenThemeMode());
    output << "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"" << DEVICE_DISPLAY_WIDTH
           << "\" height=\"" << DEVICE_DISPLAY_HEIGHT << "\" viewBox=\"0 0 "
           << DEVICE_DISPLAY_WIDTH << ' ' << DEVICE_DISPLAY_HEIGHT
           << "\" shape-rendering=\"crispEdges\">\n"
           << "<title>" << escapeXml(title) << "</title>\n"
           << "<rect width=\"100%\" height=\"100%\" fill=\"" << svgColor(theme.background)
           << "\"/>\n";
    for (const std::string& element : elements) output << element << '\n';
    output << "</svg>\n";
    return output.good();
  }

private:
  std::vector<std::string> elements;
  int cursorX = 0;
  int cursorY = 0;
  int textSize = 1;
  uint16_t textColor = BLACK;
  bool customFont = false;
  bool customBold = false;
};

static SvgDisplay display;
static DeviceConfig deviceConfig = {};
static char portalApSsid[AP_SSID_SIZE] = "BLOCK_CLOCK_DEMO";
static char portalApPassword[AP_PASSWORD_SIZE] = "setup-demo";

static int clampInt(int value, int minimum, int maximum) {
  return std::max(minimum, std::min(maximum, value));
}

static DisplayThemeMode sanitizeThemeMode(uint8_t mode) {
  return mode == DISPLAY_THEME_DARK ? DISPLAY_THEME_DARK : DISPLAY_THEME_LIGHT;
}

static DisplayThemeMode statusScreenThemeMode() {
  return deviceConfig.configured
    ? sanitizeThemeMode((DisplayThemeMode)deviceConfig.displayThemeMode)
    : DISPLAY_THEME_LIGHT;
}

static DisplayThemeColors getDisplayThemeColors(DisplayThemeMode mode) {
  if (mode == DISPLAY_THEME_DARK) return { WHITE, BLACK };
  return { BLACK, WHITE };
}

static const char* currencyCodeLabel(uint8_t code) {
  switch (code) {
    case CURRENCY_EUR: return "EUR";
    case CURRENCY_CHF: return "CHF";
    case CURRENCY_GBP: return "GBP";
    case CURRENCY_CAD: return "CAD";
    case CURRENCY_AUD: return "AUD";
    default: return "USD";
  }
}

static String formatTimestamp(time_t when) {
  if (when < 1700000000) return "---";
  struct tm tmValue = {};
  if (!localtime_r(&when, &tmValue)) return "---";
  char month[8];
  strftime(month, sizeof(month), "%b", &tmValue);
  const int hour12 = (tmValue.tm_hour % 12) == 0 ? 12 : (tmValue.tm_hour % 12);
  char buffer[28];
  snprintf(buffer, sizeof(buffer), "%s %d, %d:%02d %s", month, tmValue.tm_mday, hour12, tmValue.tm_min, tmValue.tm_hour < 12 ? "AM" : "PM");
  return String(buffer);
}

struct EmulatedQrCode {
  int size = 29;
  std::string data;
};

using esp_qrcode_handle_t = EmulatedQrCode*;

struct esp_qrcode_config_t {
  void (*display_func)(esp_qrcode_handle_t);
  int max_qrcode_version;
  int qrcode_ecc_level;
};

static constexpr int ESP_QRCODE_ECC_LOW = 0;

static int esp_qrcode_get_size(esp_qrcode_handle_t code) {
  return code ? code->size : 0;
}

static bool qrFinderModule(int x, int y, int originX, int originY) {
  const int dx = x - originX;
  const int dy = y - originY;
  if (dx < 0 || dy < 0 || dx > 6 || dy > 6) return false;
  return dx == 0 || dx == 6 || dy == 0 || dy == 6 || (dx >= 2 && dx <= 4 && dy >= 2 && dy <= 4);
}

static bool esp_qrcode_get_module(esp_qrcode_handle_t code, int x, int y) {
  if (!code || x < 0 || y < 0 || x >= code->size || y >= code->size) return false;
  if (qrFinderModule(x, y, 0, 0) || qrFinderModule(x, y, code->size - 7, 0)
      || qrFinderModule(x, y, 0, code->size - 7)) return true;
  if ((x < 8 && y < 8) || (x >= code->size - 8 && y < 8) || (x < 8 && y >= code->size - 8)) return false;
  uint32_t seed = 2166136261U;
  for (unsigned char character : code->data) seed = (seed ^ character) * 16777619U;
  seed ^= (uint32_t)(x * 374761393U) ^ (uint32_t)(y * 668265263U);
  seed ^= seed >> 13;
  seed *= 1274126177U;
  return ((seed >> 16) & 1U) != 0;
}

static void esp_qrcode_generate(const esp_qrcode_config_t* config, const char* data) {
  if (!config || !config->display_func) return;
  EmulatedQrCode code;
  code.data = data ? data : "";
  config->display_func(&code);
}

#include "../src/display_screens.h"

static BlockData demoData() {
  BlockData data = {};
  std::snprintf(data.blockHeight, sizeof(data.blockHeight), "%s", "950123");
  std::snprintf(data.blocksToHalving, sizeof(data.blocksToHalving), "%s", "99877");
  std::snprintf(data.priceValue, sizeof(data.priceValue), "%s", "67420");
  std::snprintf(data.hashrateEhs, sizeof(data.hashrateEhs), "%s", "812.4");
  data.batteryPercent = 78;
  data.batteryVoltage = 3.91f;
  data.lowFeeSatVb = 0.53f;
  data.mediumFeeSatVb = 1.169f;
  data.highFeeSatVb = 2.175f;
  data.lastBlockTimestamp = std::time(nullptr) - (12 * 60);
  data.updatedAt = std::time(nullptr);
  data.wifiOk = true;
  data.dataOk = true;
  return data;
}

static void renderScreen(const std::string& screen) {
  if (screen == "main") {
    BlockData data = demoData();
    deviceConfig.configured = true;
    deviceConfig.currencyCode = DEFAULT_CURRENCY_CODE;
    deviceConfig.displayThemeMode = DEFAULT_DISPLAY_THEME_MODE;
    deviceConfig.batteryDisplayMode = BATTERY_DISPLAY_ICON_PERCENT;
    drawBlockDashboard(data, deviceConfig);
  } else if (screen == "loading") {
    drawBrandedStatusScreen("CONNECTING", "Fetching data", false);
  } else if (screen == "welcome") {
    drawWelcomeScreen();
  } else if (screen == "setup") {
    drawSetupPortalReadyScreen();
  } else if (screen == "saved") {
    drawSetupSavedScreen();
  } else if (screen == "reset") {
    drawFactoryResetScreen();
  } else {
    std::cerr << "Unknown screen: " << screen << '\n';
    std::exit(2);
  }
}

int main(int argc, char** argv) {
  std::string profile = DEVICE_MODEL_NAME;
  std::string screen = "main";
  std::string outputPath = "screen.svg";
  for (int index = 1; index < argc; index++) {
    const std::string argument = argv[index];
    if (argument == "--help" || argument == "-h") {
      std::cout << "Usage: eink_emulator [--profile e290|e213] [--screen main|loading|welcome|setup|saved|reset] [--out path.svg]\n";
      return 0;
    }
    if ((argument == "--profile" || argument == "--screen" || argument == "--out") && index + 1 < argc) {
      const std::string value = argv[++index];
      if (argument == "--profile") profile = value;
      else if (argument == "--screen") screen = value;
      else outputPath = value;
    } else {
      std::cerr << "Unexpected argument: " << argument << '\n';
      return 2;
    }
  }

  if (profile != "e290" && profile != "E290" && profile != "e213" && profile != "E213") {
    std::cerr << "Profile must be e290 or e213.\n";
    return 2;
  }
  const std::string requestedProfile = (profile == "e213" || profile == "E213") ? "e213" : "e290";
  const std::string compiledProfile = DEVICE_PROFILE_E290 ? "e290" : "e213";
  if (requestedProfile != compiledProfile) {
    std::cerr << "This binary was built for " << compiledProfile
              << "; rebuild it for " << requestedProfile << ".\n";
    return 2;
  }

  setenv("TZ", "MST7MDT,M3.2.0,M11.1.0", 1);
  tzset();
  renderScreen(screen);
  if (!display.writeSvg(outputPath, compiledProfile + " " + screen)) {
    std::cerr << "Could not write SVG: " << outputPath << '\n';
    return 1;
  }
  std::cout << outputPath << '\n';
  return 0;
}