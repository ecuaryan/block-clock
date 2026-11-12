#pragma once

// millis() of the last request from a phone or computer on the setup page; setup closes after
// CONFIG_PORTAL_IDLE_TIMEOUT_MS without one.
static uint32_t portalLastActivityMs = 0;
static void notePortalActivity() { portalLastActivityMs = millis(); }

// Persisted across the post-save restart so the next boot can immediately draw a
// pending dashboard with placeholders (Freedom Clock parity). Without this the
// e-ink keeps the "SETTINGS SAVED" image visible during the entire WiFi+fetch
// window (10–15 s), which the user reads as "the device is frozen."
static void setPostSaveRestartPending(bool pending) {
  if (!preferences.begin(CONFIG_NAMESPACE, false)) return;
  preferences.putBool("post_save", pending);
  preferences.end();
}

static bool takePostSaveRestartPending() {
  if (!preferences.begin(CONFIG_NAMESPACE, false)) return false;
  const bool pending = preferences.getBool("post_save", false);
  if (pending) preferences.putBool("post_save", false);
  preferences.end();
  return pending;
}

static String htmlEscape(const char* s) {
  String out;
  if (!s) return out;
  while (*s) {
    switch (*s) {
      case '&': out += F("&amp;"); break;
      case '<': out += F("&lt;"); break;
      case '>': out += F("&gt;"); break;
      case '"': out += F("&quot;"); break;
      default: out += *s; break;
    }
    s++;
  }
  return out;
}

static String checkedAttr(bool checked) {
  return checked ? F(" checked") : F("");
}

static String selectedAttr(bool selected) {
  return selected ? F(" selected") : F("");
}

static String jsonEscape(const char* src) {
  if (!src) return String();
  String escaped;
  escaped.reserve(strlen(src) + 16);
  for (size_t i = 0; src[i] != '\0'; i++) {
    switch (src[i]) {
      case '\\': escaped += "\\\\"; break;
      case '"':  escaped += "\\\""; break;
      case '\n': escaped += "\\n";  break;
      case '\r': escaped += "\\r";  break;
      case '\t': escaped += "\\t";  break;
      default:   escaped += src[i]; break;
    }
  }
  return escaped;
}

static void appendPortalBrandHeader(String& html) {
  html += F("<div class=\"brand\"><svg class=\"brand-logo\" viewBox=\"0 0 32 32\" aria-hidden=\"true\">");
  html += F("<rect x=\"2\" y=\"2\" width=\"28\" height=\"28\" fill=\"#2b1700\"/>");
  html += F("<rect x=\"9\" y=\"9\" width=\"14\" height=\"14\" fill=\"#f7931a\"/>");
  html += F("</svg><h1>BLOCK CLOCK</h1></div>");
}

// Type-ahead for the watchlist boxes. The phone has no internet while it is on the setup Wi-Fi,
// so this is a fixed list of popular Yahoo symbols; any other symbol can still be typed.
static const char PORTAL_TICKER_SUGGESTIONS[] PROGMEM =
  "<datalist id=\"ticker_suggestions\">"
  "<option value=\"MSTR\">Strategy</option><option value=\"STRK\">Strategy preferred (Strike)</option>"
  "<option value=\"STRF\">Strategy preferred (Strife)</option><option value=\"ASST\">Strive</option>"
  "<option value=\"SATA\">Strive preferred</option><option value=\"MTPLF\">Metaplanet</option>"
  "<option value=\"COIN\">Coinbase</option><option value=\"HOOD\">Robinhood</option>"
  "<option value=\"XYZ\">Block</option><option value=\"MARA\">MARA Holdings</option>"
  "<option value=\"RIOT\">Riot Platforms</option><option value=\"CLSK\">CleanSpark</option>"
  "<option value=\"HUT\">Hut 8</option><option value=\"IREN\">IREN</option>"
  "<option value=\"CORZ\">Core Scientific</option><option value=\"IBIT\">iShares Bitcoin Trust</option>"
  "<option value=\"FBTC\">Fidelity Bitcoin Fund</option><option value=\"GBTC\">Grayscale Bitcoin Trust</option>"
  "<option value=\"AAPL\">Apple</option><option value=\"NVDA\">Nvidia</option>"
  "<option value=\"TSLA\">Tesla</option><option value=\"MSFT\">Microsoft</option>"
  "<option value=\"AMZN\">Amazon</option><option value=\"GOOGL\">Alphabet (Google)</option>"
  "<option value=\"META\">Meta</option><option value=\"NFLX\">Netflix</option>"
  "<option value=\"AVGO\">Broadcom</option><option value=\"BRK-B\">Berkshire Hathaway</option>"
  "<option value=\"SPY\">S&amp;P 500 ETF</option><option value=\"VOO\">Vanguard S&amp;P 500 ETF</option>"
  "<option value=\"QQQ\">Nasdaq 100 ETF</option><option value=\"DIA\">Dow Jones ETF</option>"
  "<option value=\"IWM\">Russell 2000 ETF</option><option value=\"^GSPC\">S&amp;P 500 index</option>"
  "<option value=\"^IXIC\">Nasdaq Composite</option><option value=\"^DJI\">Dow Jones index</option>"
  "<option value=\"^VIX\">VIX volatility</option><option value=\"^TNX\">10-year Treasury yield</option>"
  "<option value=\"GC=F\">Gold</option><option value=\"DX-Y.NYB\">US dollar index</option>"
  "</datalist>";

static String portalPage() {
  String html;
  const RefreshIntervalUnit refreshUnit = preferredRefreshIntervalUnit(deviceConfig.refreshIntervalMinutes);
  const uint16_t refreshValue = refreshIntervalDisplayValue(deviceConfig.refreshIntervalMinutes, refreshUnit);
  html.reserve(30000);
  html += F("<!doctype html><html lang=\"en\"><head><meta charset=\"utf-8\">");
  html += F("<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">");
  html += F("<title>BLOCK CLOCK</title>");
  html += F("<style>");
  html += F("html{-webkit-text-size-adjust:100%;text-size-adjust:100%;}");
  html += F("body{margin:0;font-family:Arial,sans-serif;background:#f3f0e8;color:#171717;line-height:1.45;}");
  html += F(".wrap{max-width:860px;margin:0 auto;padding:24px 18px 48px;}");
  html += F(".hero{background:#f7931a;color:#2b1700;padding:24px;border-radius:18px;box-shadow:0 14px 40px rgba(120,64,0,.18);}");
  html += F(".brand{display:flex;align-items:center;gap:.28em;margin:0 0 8px;font-size:30px;}");
  html += F(".brand-logo{width:.98em;height:.98em;width:1.3cap;height:1.3cap;flex:0 0 auto;display:block;overflow:visible;transform:translateY(-.04em);}");
  html += F(".brand h1{margin:0;font-size:1em;line-height:1.05;letter-spacing:.02em;}");
  html += F(".hero p{margin:0;line-height:1.5;color:#5b3300;}");
  html += F("form{margin-top:22px;display:grid;gap:18px;}");
  html += F(".card{background:#fff;border-radius:18px;padding:18px;box-shadow:0 12px 30px rgba(18,18,18,.08);}");
  html += F(".card h2{margin:0 0 14px;font-size:18px;}");
  html += F(".grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(220px,1fr));gap:14px;}");
  html += F(".dual{display:grid;grid-template-columns:minmax(92px,1fr) 112px;gap:10px;align-items:start;}");
  html += F(".stack{display:grid;gap:14px;}");
  html += F(".tickers{display:grid;grid-template-columns:repeat(3,minmax(0,1fr));gap:10px;}.tickers input{text-transform:uppercase;}");
  html += F("@media(max-width:480px){.tickers{grid-template-columns:repeat(2,minmax(0,1fr));}}");
  // Ticker suggestions drawn by the page itself: phones' setup-Wi-Fi sign-in sheets ignore <datalist>.
  html += F(".tk{position:relative;}.suggest{position:absolute;top:100%;margin-top:4px;z-index:50;width:max(100%,210px);background:#fff;border:1px solid #ded4c2;border-radius:12px;box-shadow:0 10px 26px rgba(18,18,18,.16);overflow:hidden;}");
  html += F(".suggest button{display:flex;gap:8px;width:100%;text-align:left;border-radius:0;background:#fff;color:#171717;padding:10px 12px;font-size:15px;font-weight:400;}.suggest button+button{border-top:1px solid #f0ebe1;}.suggest button:active{background:#fdf1e1;}.suggest span{color:#6b6257;white-space:nowrap;overflow:hidden;text-overflow:ellipsis;}");
  html += F(".inline{display:flex;flex-wrap:wrap;gap:10px;align-items:flex-start;}.inline select{flex:1;min-width:160px;}");
  html += F("label{display:block;font-size:13px;font-weight:700;margin-bottom:6px;color:#3a342d;}");
  html += F("input,select{width:100%;box-sizing:border-box;min-height:44px;height:44px;padding:10px 12px;border:1px solid #cfc6b7;border-radius:12px;font-size:16px;line-height:1.2;background:#fdfbf6;color:#171717;}");
  html += F("input[type=password]{letter-spacing:.08em;}");
  html += F(".password-wrap{position:relative;}.password-wrap input{padding-right:12px;}.password-wrap.has-secret input{padding-right:78px;}.password-wrap input[type=text]{letter-spacing:normal;}");
  html += F(".password-toggle{display:none;position:absolute;right:6px;top:6px;min-height:32px;height:32px;padding:0 11px;border-radius:999px;font-size:13px;background:#ded4c2;color:#211d19;border:none;cursor:pointer;}.password-wrap.has-secret .password-toggle{display:block;}");
  html += F(".hint{font-size:12px;color:#6b6258;margin-top:6px;line-height:1.45;}");
  html += F(".hidden{display:none!important;}");
  html += F(".actions{display:flex;flex-wrap:wrap;gap:12px;align-items:center;margin-top:4px;}");
  html += F("@keyframes fc-spin{to{transform:rotate(360deg)}}");
  html += F(".save-overlay{position:fixed;inset:0;background:rgba(253,251,246,.94);display:flex;flex-direction:column;align-items:center;justify-content:center;gap:18px;z-index:9999;}");
  html += F(".save-overlay-spinner{width:36px;height:36px;border:3.5px solid rgba(247,147,26,.25);border-top-color:#f7931a;border-radius:50%;animation:fc-spin .9s linear infinite;}");
  html += F(".save-overlay-text{font-size:17px;font-weight:700;color:#2b1700;text-align:center;line-height:1.6;}");
  html += F(".overlay-buttons{display:flex;gap:12px;}");
  html += F("button{border:none;border-radius:999px;padding:13px 18px;font-size:16px;font-weight:700;cursor:pointer;background:#f7931a;color:#2b1700;touch-action:manipulation;}");
  html += F("button.secondary{background:#ded4c2;color:#211d19;}");
  html += F("button[disabled],input[disabled]{opacity:.45;cursor:not-allowed;}");
  html += F("@media(max-width:640px){.brand{font-size:25px;}}");
  html += F("</style></head><body>");

  // Focus guard prevents browser from auto-focusing the first input on page load.
  // Must be a real input with autofocus so it absorbs the browser's initial focus
  // (a div with tabindex=-1 cannot receive input focus on mobile browsers).
  html += F("<input id=\"focus_guard\" tabindex=\"-1\" aria-hidden=\"true\" type=\"text\" readonly autofocus autocomplete=\"off\" style=\"position:fixed;top:0;left:0;width:1px;height:1px;opacity:0;pointer-events:none;outline:none;border:none;background:transparent;font-size:16px;\">");
  html += F("<div id=\"save_overlay\" class=\"save-overlay hidden\"><div id=\"save_overlay_spinner\" class=\"save-overlay-spinner\"></div><div id=\"save_overlay_text\" class=\"save-overlay-text\">Saving settings...</div><div class=\"overlay-buttons\"><button id=\"save_overlay_action\" class=\"secondary hidden\" type=\"button\">Continue</button><button id=\"save_overlay_confirm\" class=\"hidden\" type=\"button\">Save anyway</button></div></div>");
  html += F("<div class=\"wrap\">");
  html += F("<section class=\"hero\">");
  appendPortalBrandHeader(html);
  html += F("<p>To get back here, tap the square button on the clock. Tapping it again, or leaving this page for 10 minutes, closes setup without saving.</p>");
  html += F("</section>");
  html += F("<form id=\"setup_form\" method=\"post\" action=\"/save\">");

  // ── Data ──────────────────────────────────────────────────────────────────
  html += F("<section class=\"card\"><h2>Data</h2><div class=\"grid\"><div>");
  html += F("<label for=\"data_source\">Source</label>");
  html += F("<select id=\"data_source\" name=\"data_source\">");
  html += F("<option value=\"online\"");
  html += selectedAttr(sanitizeDataSourceMode(deviceConfig.dataSourceMode) == DATA_SOURCE_ONLINE);
  html += F(">Online</option>");
  html += F("<option value=\"mqtt\"");
  html += selectedAttr(sanitizeDataSourceMode(deviceConfig.dataSourceMode) == DATA_SOURCE_MQTT);
  html += F(">MQTT</option>");
  html += F("</select>");
  html += F("<div class=\"hint\">Online uses public Bitcoin data. MQTT uses retained local topics.</div>");
  html += F("</div><div>");
  html += F("<label for=\"currency\">Currency</label>");
  html += F("<select id=\"currency\" name=\"currency\">");
  html += F("<option value=\"0\""); html += selectedAttr(sanitizeCurrencyCode(deviceConfig.currencyCode) == CURRENCY_USD); html += F(">USD</option>");
  html += F("<option value=\"1\""); html += selectedAttr(sanitizeCurrencyCode(deviceConfig.currencyCode) == CURRENCY_EUR); html += F(">EUR</option>");
  html += F("<option value=\"2\""); html += selectedAttr(sanitizeCurrencyCode(deviceConfig.currencyCode) == CURRENCY_CHF); html += F(">CHF</option>");
  html += F("<option value=\"3\""); html += selectedAttr(sanitizeCurrencyCode(deviceConfig.currencyCode) == CURRENCY_GBP); html += F(">GBP</option>");
  html += F("<option value=\"4\""); html += selectedAttr(sanitizeCurrencyCode(deviceConfig.currencyCode) == CURRENCY_CAD); html += F(">CAD</option>");
  html += F("<option value=\"5\""); html += selectedAttr(sanitizeCurrencyCode(deviceConfig.currencyCode) == CURRENCY_AUD); html += F(">AUD</option>");
  html += F("</select>");
  html += F("</div><div>");
  html += F("<label for=\"timezone\">Timezone</label>");
  html += F("<select id=\"timezone\" name=\"timezone\">");
  for (uint8_t index = 0; index < TIMEZONE_OPTION_COUNT; index++) {
    html += F("<option value=\"");
    html += String(index);
    html += F("\"");
    html += selectedAttr(sanitizeTimezoneIndex(deviceConfig.timezoneIndex) == index);
    html += F(">");
    html += TIMEZONE_OPTIONS[index].label;
    html += F("</option>");
  }
  html += F("</select>");
  html += F("<div class=\"hint\">Used for the time shown on the display.</div>");
  html += F("</div></div></section>");

  // ── Wi-Fi ─────────────────────────────────────────────────────────────────
  html += F("<section class=\"card\"><h2>Wi-Fi</h2><div class=\"stack\">");
  html += F("<div><label for=\"wifi_ssid_select\">Available networks</label>");
  html += F("<div class=\"inline\"><select id=\"wifi_ssid_select\" disabled><option value=\"\">Refresh to scan nearby networks</option></select>");
  html += F("<button id=\"scan_wifi_button\" class=\"secondary\" type=\"button\">Refresh</button></div>");
  html += F("<div class=\"hint\">Choose a scanned network or type the name below for hidden networks.</div></div>");
  html += F("<div class=\"grid\"><div>");
  html += F("<label for=\"wifi_ssid\">Network name</label>");
  html += F("<input id=\"wifi_ssid\" name=\"wifi_ssid\" maxlength=\"32\" value=\"");
  html += htmlEscape(deviceConfig.wifiSsid);
  html += F("\"></div><div>");
  html += F("<label for=\"wifi_pass\">Password</label>");
  html += F("<div class=\"password-wrap\"><input id=\"wifi_pass\" name=\"wifi_pass\" type=\"password\" maxlength=\"64\" value=\"");
  html += htmlEscape(deviceConfig.wifiPass);
  html += F("\"><button class=\"password-toggle\" type=\"button\" data-password-toggle=\"wifi_pass\">Show</button></div>");
  html += F("</div></div>");
  html += F("<div><label for=\"wifi_ssid2\">Backup network (optional)</label>");
  html += F("<div class=\"grid\"><div><input id=\"wifi_ssid2\" name=\"wifi_ssid2\" maxlength=\"32\" placeholder=\"Network name\" value=\"");
  html += htmlEscape(deviceConfig.wifiSsid2);
  html += F("\"></div><div><div class=\"password-wrap\"><input id=\"wifi_pass2\" name=\"wifi_pass2\" type=\"password\" maxlength=\"64\" placeholder=\"Password\" value=\"");
  html += htmlEscape(deviceConfig.wifiPass2);
  html += F("\"><button class=\"password-toggle\" type=\"button\" data-password-toggle=\"wifi_pass2\">Show</button></div></div></div>");
  html += F("<div class=\"hint\">Used when the main network is out of reach, such as a phone hotspot when you take the clock with you. The clock only joins 2.4 GHz networks; on an iPhone turn on Maximize Compatibility in Personal Hotspot.</div></div>");
  html += F("</div></section>");

  // ── Display ───────────────────────────────────────────────────────────────
  html += F("<section class=\"card\"><h2>Display</h2><div class=\"grid\"><div>");
  html += F("<label for=\"display_theme_mode\">Display theme</label><select id=\"display_theme_mode\" name=\"display_theme_mode\">");
  html += F("<option value=\"1\""); html += selectedAttr(sanitizeThemeMode(deviceConfig.displayThemeMode) == DISPLAY_THEME_DARK); html += F(">Dark</option>");
  html += F("<option value=\"0\""); html += selectedAttr(sanitizeThemeMode(deviceConfig.displayThemeMode) == DISPLAY_THEME_LIGHT); html += F(">Light</option>");
  html += F("</select></div><div>");
  html += F("<label for=\"battery_mode\">Battery indicator</label>");
  html += F("<select id=\"battery_mode\" name=\"battery_mode\"><option value=\"0\"");
  html += selectedAttr(deviceConfig.batteryDisplayMode == BATTERY_DISPLAY_HIDDEN);
  html += F(">Hidden</option><option value=\"1\"");
  html += selectedAttr(deviceConfig.batteryDisplayMode == BATTERY_DISPLAY_ICON);
  html += F(">Icon only</option><option value=\"2\"");
  html += selectedAttr(deviceConfig.batteryDisplayMode == BATTERY_DISPLAY_ICON_PERCENT);
  html += F(">Icon + percentage</option></select></div><div>");
  html += F("<label for=\"refresh_interval_value\">Refresh interval</label><div class=\"dual\"><input id=\"refresh_interval_value\" name=\"refresh_interval_value\" type=\"text\" inputmode=\"numeric\" pattern=\"[0-9]*\" min=\"1\" step=\"1\" value=\"");
  html += String(refreshValue);
  html += F("\"><select id=\"refresh_interval_unit\" name=\"refresh_interval_unit\"><option value=\"0\"");
  html += selectedAttr(refreshUnit == REFRESH_INTERVAL_UNIT_MINUTES);
  html += F(">Minutes</option><option value=\"1\"");
  html += selectedAttr(refreshUnit == REFRESH_INTERVAL_UNIT_HOURS);
  html += F(">Hours</option><option value=\"2\"");
  html += selectedAttr(refreshUnit == REFRESH_INTERVAL_UNIT_DAYS);
  html += F(">Days</option></select></div><div class=\"hint\">Shorter intervals use more battery.</div></div></div>");
  html += F("<div class=\"grid\" style=\"margin-top:14px\"><div><label for=\"quiet_hours\">Quiet hours</label><select id=\"quiet_hours\" name=\"quiet_hours\"><option value=\"0\"");
  html += selectedAttr(!deviceConfig.quietHoursEnabled);
  html += F(">Off</option><option value=\"1\"");
  html += selectedAttr(deviceConfig.quietHoursEnabled);
  html += F(">Pause refreshing</option></select><div class=\"hint\">No refreshes during these hours, so the screen stops flashing overnight. It keeps its last image, and a button press still refreshes.</div></div><div id=\"quiet_times\"><label>From / until (local time)</label><div class=\"dual\">");
  for (uint8_t which = 0; which < 2; which++) {
    const uint8_t selectedHour = which == 0 ? deviceConfig.quietStartHour : deviceConfig.quietEndHour;
    html += which == 0 ? F("<select name=\"quiet_start\">") : F("<select name=\"quiet_end\">");
    for (uint8_t hour = 0; hour < 24; hour++) {
      html += F("<option value=\"");
      html += String(hour);
      html += F("\"");
      html += selectedAttr(selectedHour == hour);
      html += F(">");
      const int hour12 = (hour % 12) == 0 ? 12 : (hour % 12);
      html += String(hour12);
      html += hour < 12 ? F(" AM") : F(" PM");
      html += F("</option>");
    }
    html += F("</select>");
  }
  html += F("</div></div></div>");
  const bool rotateInMinutes = deviceConfig.rotateSeconds >= 60 && (deviceConfig.rotateSeconds % 60) == 0;
  html += F("<div class=\"grid\" style=\"margin-top:14px\"><div><label for=\"rotate_screens\">Rotate screens</label><select id=\"rotate_screens\" name=\"rotate_screens\"><option value=\"0\"");
  html += selectedAttr(!deviceConfig.rotateScreens);
  html += F(">Off, buttons only</option><option value=\"1\"");
  html += selectedAttr(deviceConfig.rotateScreens);
  html += F(">Automatically</option></select><div class=\"hint\">Flips to the next screen on its own, between data refreshes. Each flip redraws the e-ink, so short intervals use more battery. Pauses during quiet hours.</div></div>");
  html += F("<div id=\"rotate_every\"><label for=\"rotate_value\">Every</label><div class=\"dual\"><input id=\"rotate_value\" name=\"rotate_value\" type=\"text\" inputmode=\"numeric\" value=\"");
  html += String(rotateInMinutes ? deviceConfig.rotateSeconds / 60 : deviceConfig.rotateSeconds);
  html += F("\"><select id=\"rotate_unit\" name=\"rotate_unit\"><option value=\"1\"");
  html += selectedAttr(!rotateInMinutes);
  html += F(">Seconds</option><option value=\"60\"");
  html += selectedAttr(rotateInMinutes);
  html += F(">Minutes</option></select></div><div class=\"hint\">From 15 seconds to 12 hours.</div></div></div></section>");

  // ── Watchlist ─────────────────────────────────────────────────────────────
  html += F("<section class=\"card\"><h2>Watchlist</h2><div class=\"stack\"><div><label for=\"watchlist\">Watchlist screen</label><select id=\"watchlist\" name=\"watchlist\"><option value=\"0\"");
  html += selectedAttr(!deviceConfig.watchlistEnabled);
  html += F(">Off</option><option value=\"1\"");
  html += selectedAttr(deviceConfig.watchlistEnabled);
  html += F(">Show my tickers</option></select><div class=\"hint\">Adds a fourth screen with the price and day change of up to six tickers.</div></div>");
  html += F("<div id=\"ticker_fields\"><div class=\"tickers\">");
  for (int i = 0; i < MAX_TICKERS; i++) {
    html += F("<div class=\"tk\"><input name=\"ticker");
    html += String(i);
    html += F("\" maxlength=\"12\" autocapitalize=\"characters\" autocomplete=\"off\" spellcheck=\"false\" placeholder=\"Ticker ");
    html += String(i + 1);
    html += F("\" value=\"");
    html += htmlEscape(deviceConfig.tickers[i]);
    html += F("\"></div>");
  }
  html += F("</div>");
  html += FPSTR(PORTAL_TICKER_SUGGESTIONS);
  html += F("<div class=\"hint\">Type for suggestions, or use any symbol from finance.yahoo.com.</div></div></div></section>");

  // ── MQTT ──────────────────────────────────────────────────────────────────
  html += F("<section id=\"mqtt_settings\" class=\"card\"><h2>MQTT</h2>");
  html += F("<div class=\"grid\"><div><label for=\"mqtt_server\">Server</label><input id=\"mqtt_server\" name=\"mqtt_server\" maxlength=\"63\" value=\"");
  html += htmlEscape(deviceConfig.mqttServer);
  html += F("\"></div><div><label for=\"mqtt_port\">Port</label><input id=\"mqtt_port\" name=\"mqtt_port\" type=\"number\" min=\"1\" max=\"65535\" value=\"");
  html += String(deviceConfig.mqttPort);
  html += F("\"></div></div>");
  html += F("<div class=\"grid\" style=\"margin-top:14px\"><div><label for=\"mqtt_user\">Username</label><input id=\"mqtt_user\" name=\"mqtt_user\" maxlength=\"63\" value=\"");
  html += htmlEscape(deviceConfig.mqttUser);
  html += F("\"></div><div><label for=\"mqtt_pass\">Password</label>");
  html += F("<div class=\"password-wrap\"><input id=\"mqtt_pass\" name=\"mqtt_pass\" type=\"password\" maxlength=\"63\" value=\"");
  html += htmlEscape(deviceConfig.mqttPass);
  html += F("\"><button class=\"password-toggle\" type=\"button\" data-password-toggle=\"mqtt_pass\">Show</button></div>");
  html += F("</div></div>");
  html += F("<div class=\"grid\" style=\"margin-top:14px\"><div>");
  html += F("<label for=\"topic_height\">Block height topic</label><input id=\"topic_height\" name=\"topic_height\" maxlength=\"95\" value=\"");
  html += htmlEscape(deviceConfig.topicHeight);
  html += F("\"></div><div>");
  html += F("<label for=\"topic_halving\">Blocks to halving topic</label><input id=\"topic_halving\" name=\"topic_halving\" maxlength=\"95\" value=\"");
  html += htmlEscape(deviceConfig.topicHalving);
  html += F("\"></div><div>");
  html += F("<label for=\"topic_hashrate\">Hashrate topic</label><input id=\"topic_hashrate\" name=\"topic_hashrate\" maxlength=\"95\" value=\"");
  html += htmlEscape(deviceConfig.topicHashrate);
  html += F("\"></div><div>");
  html += F("<label for=\"topic_price\">BTC price topic</label><input id=\"topic_price\" name=\"topic_price\" maxlength=\"95\" value=\"");
  html += htmlEscape(deviceConfig.topicPriceValue);
  html += F("\"></div></div>");
  html += F("<div class=\"hint\">Publish the BTC price in the selected currency: ");
  html += currencyCodeLabel(deviceConfig.currencyCode);
  html += F(".</div>");
  html += F("</section>");

  html += F("<div class=\"actions\"><button id=\"save_button\" type=\"submit\">Save</button></div>");
  html += F("</form>");
  html += F("<section class=\"card\"><h2>Factory reset</h2><div class=\"hint\">Clears every setting, including Wi-Fi, and returns the clock to its welcome screen.</div>");
  html += F("<div class=\"actions\"><button id=\"factory_reset\" class=\"secondary\" type=\"button\">Factory reset</button></div></section>");
  html += F("<script>");

  // ── Data source visibility ─────────────────────────────────────────────────
  html += F("(function(){var source=document.getElementById('data_source');var mqtt=document.getElementById('mqtt_settings');function sync(){if(!source||!mqtt)return;mqtt.classList.toggle('hidden',source.value!=='mqtt');}if(source)source.addEventListener('change',sync);sync();})();");

  // ── Rotate / watchlist visibility ─────────────────────────────────────────
  html += F("(function(){function link(selectId,targetId){var sel=document.getElementById(selectId);var target=document.getElementById(targetId);function sync(){if(sel&&target)target.classList.toggle('hidden',sel.value!=='1');}if(sel)sel.addEventListener('change',sync);sync();}link('quiet_hours','quiet_times');link('rotate_screens','rotate_every');link('watchlist','ticker_fields');})();");

  html += F("(function(){var dl=document.getElementById('ticker_suggestions');if(!dl)return;var opts=[].map.call(dl.options,function(o){return [o.value,o.textContent];});var box=document.createElement('div');box.className='suggest hidden';function hide(){box.classList.add('hidden');}");
  html += F("function show(inp){var q=inp.value.trim().toUpperCase();var m=opts.filter(function(o){return !q||o[0].indexOf(q)===0||o[1].toUpperCase().indexOf(q)>=0;});if(m.length===1&&m[0][0]===q)m=[];m=m.slice(0,6);if(!m.length){hide();return;}box.innerHTML='';");
  html += F("m.forEach(function(o){var b=document.createElement('button');b.type='button';b.appendChild(document.createElement('b')).textContent=o[0];b.appendChild(document.createElement('span')).textContent=o[1];b.addEventListener('mousedown',function(e){e.preventDefault();});b.addEventListener('click',function(){inp.value=o[0];hide();});box.appendChild(b);});");
  html += F("inp.parentNode.appendChild(box);box.style.left='0';box.style.right='auto';box.classList.remove('hidden');if(box.getBoundingClientRect().right>window.innerWidth-8){box.style.left='auto';box.style.right='0';}}");
  html += F("[].forEach.call(document.querySelectorAll('.tickers input'),function(inp){inp.addEventListener('input',function(){show(inp);});inp.addEventListener('focus',function(){show(inp);});inp.addEventListener('blur',function(){setTimeout(function(){if(document.activeElement!==inp&&box.parentNode===inp.parentNode)hide();},150);});inp.addEventListener('keydown',function(e){if(e.key==='Escape'||e.key==='Enter')hide();});});})();");

  // ── Save overlay ───────────────────────────────────────────────────────────
  html += F("(function(){var form=document.getElementById('setup_form');var saveButton=document.getElementById('save_button');var overlay=document.getElementById('save_overlay');var overlayText=document.getElementById('save_overlay_text');var overlaySpinner=document.getElementById('save_overlay_spinner');var overlayAction=document.getElementById('save_overlay_action');var overlayConfirm=document.getElementById('save_overlay_confirm');var saveInFlight=false;var pendingBody=null;");
  html += F("var headers={'Content-Type':'application/x-www-form-urlencoded;charset=UTF-8'};");
  html += F("function escapeHtml(s){return String(s||'').replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;').replace(/\"/g,'&quot;').replace(/'/g,'&#39;');}");
  html += F("function setOverlayText(text){if(!overlayText)return;overlayText.innerHTML=escapeHtml(text).replace(/\\n/g,'<br>');}");
  html += F("function showOverlay(text){document.body.style.overflow='hidden';if(overlay)overlay.classList.remove('hidden');if(overlaySpinner)overlaySpinner.classList.remove('hidden');if(overlayAction)overlayAction.classList.add('hidden');if(overlayConfirm)overlayConfirm.classList.add('hidden');setOverlayText(text);}");
  html += F("function showOverlayHtml(html){document.body.style.overflow='hidden';if(overlay)overlay.classList.remove('hidden');if(overlayText)overlayText.innerHTML=html;}");
  html += F("function hideOverlay(){document.body.style.overflow='';if(overlay)overlay.classList.add('hidden');if(overlayAction)overlayAction.classList.add('hidden');if(overlayConfirm)overlayConfirm.classList.add('hidden');if(overlaySpinner)overlaySpinner.classList.remove('hidden');}");
  html += F("function failOverlay(text){if(overlaySpinner)overlaySpinner.classList.add('hidden');setOverlayText(text);if(overlayAction){overlayAction.textContent='Continue';overlayAction.classList.remove('hidden');}if(overlayConfirm)overlayConfirm.classList.add('hidden');if(saveButton)saveButton.disabled=false;saveInFlight=false;}");
  html += F("function askOverlay(text){failOverlay(text);if(overlayAction)overlayAction.textContent='Go back';if(overlayConfirm)overlayConfirm.classList.remove('hidden');}");
  html += F("function save(body,checks){showOverlay('Saving settings...');return fetch('/save',{method:'POST',headers:headers,body:body,cache:'no-store'}).then(function(r){return r.json();}).then(function(saved){if(!saved||!saved.ok){failOverlay((saved&&saved.error)||'Save failed.');return;}showOverlayHtml('Settings saved!'+(checks?'<br><span style=\\'font-weight:400;font-size:15px\\'>'+escapeHtml(checks).replace(/\\n/g,'<br>')+'</span>':'')+'<br>Device is restarting...');});}");
  html += F("function lostConnection(){failOverlay('Settings check failed.\\nReconnect to BLOCK CLOCK setup Wi-Fi and try again.');}");
  html += F("var resetButton=document.getElementById('factory_reset');if(resetButton)resetButton.addEventListener('click',function(){if(saveInFlight||!confirm('Clear all settings, including Wi-Fi? The clock goes back to its welcome screen.'))return;saveInFlight=true;showOverlay('Clearing settings...');fetch('/factory-reset',{method:'POST',headers:headers,body:'confirm=1',cache:'no-store'}).then(function(r){return r.json();}).then(function(){showOverlayHtml('Settings cleared.<br>The clock is restarting.');}).catch(function(){failOverlay('Could not reach the clock.\\nReconnect to its setup Wi-Fi and try again.');});});");
  html += F("if(overlayAction)overlayAction.addEventListener('click',hideOverlay);");
  html += F("if(overlayConfirm)overlayConfirm.addEventListener('click',function(){if(!pendingBody)return;saveInFlight=true;if(saveButton)saveButton.disabled=true;save(pendingBody).catch(lostConnection);});");
  html += F("if(!form)return;");
  html += F("form.addEventListener('submit',function(event){if(saveInFlight)return;if(!window.fetch)return;event.preventDefault();saveInFlight=true;if(saveButton)saveButton.disabled=true;showOverlay('Checking settings...\\nThis can take up to a minute with two networks.');var body=new URLSearchParams(new FormData(form)).toString();pendingBody=body;fetch('/validate',{method:'POST',headers:headers,body:body,cache:'no-store'}).then(function(r){return r.json();}).then(function(data){if(!data||!data.ok){failOverlay((data&&data.message)||'Settings check failed.');return null;}if(data.warn){askOverlay(data.message||'');return null;}return save(body,data.message);}).catch(lostConnection);});})();");

  // ── Password show/hide ─────────────────────────────────────────────────────
  html += F("(function(){");
  html += F("function syncPwToggle(input){var wrap=input&&input.closest?input.closest('.password-wrap'):null;var btn=wrap&&wrap.querySelector?wrap.querySelector('[data-password-toggle]'):null;var has=!!(input&&input.value);if(wrap&&wrap.classList)wrap.classList.toggle('has-secret',has);if(btn){btn.disabled=!has;if(!has){input.type='password';btn.textContent='Show';}}}");
  html += F("function syncAll(){['wifi_pass','wifi_pass2','mqtt_pass'].forEach(function(id){var el=document.getElementById(id);if(el)syncPwToggle(el);});}");
  html += F("document.addEventListener('input',function(e){syncPwToggle(e.target);},true);");
  html += F("document.addEventListener('click',function(e){var b=e.target.closest&&e.target.closest('[data-password-toggle]');if(!b)return;e.preventDefault();var input=document.getElementById(b.getAttribute('data-password-toggle'));if(!input||!input.value){syncPwToggle(input);return;}var show=input.type==='password';input.type=show?'text':'password';b.textContent=show?'Hide':'Show';},true);");
  html += F("syncAll();");
  html += F("})();");

  // ── Wi-Fi scan ─────────────────────────────────────────────────────────────
  html += F("(function(){");
  html += F("var wifiSelect=document.getElementById('wifi_ssid_select');");
  html += F("var wifiSsidInput=document.getElementById('wifi_ssid');");
  html += F("var scanButton=document.getElementById('scan_wifi_button');");
  html += F("async function refreshWifiList(){if(scanButton)scanButton.disabled=true;if(wifiSelect){wifiSelect.disabled=true;wifiSelect.innerHTML='<option value=\"\">Scanning nearby networks...</option>';}try{var response=await fetch('/wifi-list',{cache:'no-store'});var data=await response.json();if(!data.ok)throw new Error(data.message||'Wi-Fi scan failed.');var networks=Array.isArray(data.networks)?data.networks:[];if(wifiSelect){wifiSelect.innerHTML='';var ph=document.createElement('option');ph.value='';ph.textContent=networks.length?'Choose a nearby network':'No networks found';wifiSelect.appendChild(ph);var cur=wifiSsidInput?wifiSsidInput.value:'';networks.forEach(function(n){var o=document.createElement('option');o.value=n.ssid||'';o.textContent=n.ssid||'';if(cur&&o.value===cur)o.selected=true;wifiSelect.appendChild(o);});wifiSelect.disabled=false;if(wifiSsidInput&&wifiSelect.value)wifiSsidInput.value=wifiSelect.value;}}catch(err){if(wifiSelect){wifiSelect.innerHTML='<option value=\"\">Type your Wi-Fi name manually</option>';wifiSelect.disabled=false;}}if(scanButton)scanButton.disabled=false;}");
  html += F("if(wifiSelect)wifiSelect.addEventListener('change',function(){if(wifiSsidInput&&wifiSelect.value)wifiSsidInput.value=wifiSelect.value;});");
  html += F("if(scanButton)scanButton.addEventListener('click',refreshWifiList);");
  html += F("refreshWifiList();");
  html += F("})();");

  // ── Suppress initial input focus / scroll to top ───────────────────────────
  // Matches Freedom Clock's pattern: tracks user interaction so the suppress
  // logic only fires BEFORE the user has touched/clicked anything. Without this
  // flag, late-firing setTimeouts can blur a select the user just opened,
  // causing focus to jump to a different field after the toggle.
  html += F("(function(){");
  html += F("var setupUserInteracted=false;");
  html += F("['pointerdown','touchstart','keydown'].forEach(function(name){window.addEventListener(name,function(){setupUserInteracted=true;},{once:true,passive:true});});");
  html += F("var guard=document.getElementById('focus_guard');");
  html += F("function blurEditable(){var a=document.activeElement;if(!a)return;var t=String(a.tagName||'').toUpperCase();if((t==='INPUT'||t==='SELECT'||t==='TEXTAREA')&&a.blur)a.blur();}");
  html += F("document.addEventListener('focusin',function onEarlyFocus(e){if(setupUserInteracted){document.removeEventListener('focusin',onEarlyFocus,true);return;}var el=e.target;if(el&&el.id==='focus_guard')return;var tag=el?String(el.tagName||'').toUpperCase():'';if(tag==='INPUT'||tag==='SELECT'||tag==='TEXTAREA'){if(el.blur)el.blur();}},true);");
  html += F("function suppress(){if(setupUserInteracted)return;if(guard&&guard.focus){try{guard.focus({preventScroll:true});}catch(e){}}blurEditable();if((window.scrollY||document.documentElement.scrollTop||0)<80)window.scrollTo(0,0);}");
  html += F("if('scrollRestoration' in history)history.scrollRestoration='manual';");
  html += F("[0,100,300,600,1000].forEach(function(d){setTimeout(suppress,d);});");
  html += F("window.addEventListener('load',function(){[0,100,300].forEach(function(d){setTimeout(suppress,d);});});");
  html += F("window.addEventListener('pageshow',function(){setTimeout(suppress,0);});");
  html += F("})();");

  html += F("</script></div></body></html>");
  return html;
}

static void handlePortalRoot() {
  notePortalActivity();
  portalServer.send(200, "text/html", portalPage());
}

static void portalSendJson(bool ok, const char* message, bool warn = false) {
  String json = F("{\"ok\":");
  json += ok ? F("true") : F("false");
  if (warn) json += F(",\"warn\":true");
  json += F(",\"message\":\"");
  json += jsonEscape(message ? message : "");
  json += F("\"}");

  portalServer.sendHeader("Cache-Control", "no-store, no-cache, must-revalidate");
  portalServer.sendHeader("Pragma", "no-cache");
  portalServer.send(200, "application/json; charset=utf-8", json);
}

static void loadSubmittedPortalConfig(DeviceConfig& cfg) {
  safeCopyString(cfg.wifiSsid, sizeof(cfg.wifiSsid), portalServer.arg("wifi_ssid"));
  safeCopyString(cfg.wifiPass, sizeof(cfg.wifiPass), portalServer.arg("wifi_pass"));
  safeCopyString(cfg.wifiSsid2, sizeof(cfg.wifiSsid2), portalServer.arg("wifi_ssid2"));
  safeCopyString(cfg.wifiPass2, sizeof(cfg.wifiPass2), portalServer.arg("wifi_pass2"));
  cfg.dataSourceMode = portalServer.arg("data_source") == "mqtt" ? DATA_SOURCE_MQTT : DATA_SOURCE_ONLINE;
  cfg.currencyCode = (uint8_t)sanitizeCurrencyCode((uint8_t)portalServer.arg("currency").toInt());
  cfg.timezoneIndex = sanitizeTimezoneIndex((uint8_t)portalServer.arg("timezone").toInt());
  cfg.displayThemeMode = (uint8_t)sanitizeThemeMode((uint8_t)portalServer.arg("display_theme_mode").toInt());
  const RefreshIntervalUnit refreshUnit = parseRefreshIntervalUnit(portalServer.arg("refresh_interval_unit"));
  cfg.refreshIntervalMinutes = (uint16_t)clampInt(
    refreshIntervalMinutesFromForm((uint16_t)portalServer.arg("refresh_interval_value").toInt(), refreshUnit),
    MIN_REFRESH_INTERVAL_MINUTES,
    MAX_REFRESH_INTERVAL_MINUTES
  );
  cfg.quietHoursEnabled = portalServer.arg("quiet_hours") == "1";
  cfg.quietStartHour = (uint8_t)clampInt(portalServer.arg("quiet_start").toInt(), 0, 23);
  cfg.quietEndHour = (uint8_t)clampInt(portalServer.arg("quiet_end").toInt(), 0, 23);
  cfg.rotateScreens = portalServer.arg("rotate_screens") == "1";
  const long rotateUnit = portalServer.arg("rotate_unit") == "60" ? 60 : 1;
  cfg.rotateSeconds = (uint16_t)clampInt((int)(clampInt(portalServer.arg("rotate_value").toInt(), 0, MAX_ROTATE_SECONDS) * rotateUnit), MIN_ROTATE_SECONDS, MAX_ROTATE_SECONDS);
  cfg.watchlistEnabled = portalServer.arg("watchlist") == "1";
  for (int i = 0; i < MAX_TICKERS; i++) {
    safeCopyString(cfg.tickers[i], sizeof(cfg.tickers[i]), portalServer.arg(String("ticker") + i));
  }
  cfg.batteryDisplayMode = (uint8_t)clampInt(portalServer.arg("battery_mode").toInt(), BATTERY_DISPLAY_HIDDEN, BATTERY_DISPLAY_ICON_PERCENT);
  safeCopyString(cfg.mqttServer, sizeof(cfg.mqttServer), portalServer.arg("mqtt_server"));
  cfg.mqttPort = parsePort(portalServer.arg("mqtt_port"), DEFAULT_MQTT_PORT);
  safeCopyString(cfg.mqttUser, sizeof(cfg.mqttUser), portalServer.arg("mqtt_user"));
  safeCopyString(cfg.mqttPass, sizeof(cfg.mqttPass), portalServer.arg("mqtt_pass"));
  safeCopyString(cfg.topicHeight, sizeof(cfg.topicHeight), portalServer.arg("topic_height"));
  safeCopyString(cfg.topicHalving, sizeof(cfg.topicHalving), portalServer.arg("topic_halving"));
  safeCopyString(cfg.topicHashrate, sizeof(cfg.topicHashrate), portalServer.arg("topic_hashrate"));
  safeCopyString(cfg.topicPriceValue, sizeof(cfg.topicPriceValue), portalServer.arg("topic_price"));
}

// Which of the two networks the scan can see. A network that is in range but will not connect almost
// always has a mistyped password; one that is out of range (or hidden) cannot be tested right now.
static void scanForNetworks(const DeviceConfig& cfg, bool& primaryInRange, bool& backupInRange) {
  primaryInRange = backupInRange = false;
  WiFi.mode(WIFI_AP_STA);
  const int found = WiFi.scanNetworks();
  for (int i = 0; i < found; i++) {
    const String ssid = WiFi.SSID(i);
    if (ssid == cfg.wifiSsid) primaryInRange = true;
    if (hasText(cfg.wifiSsid2) && ssid == cfg.wifiSsid2) backupInRange = true;
  }
  WiFi.scanDelete();
}

static void portalSendWifiResult(const String& status, const String& warning) {
  notePortalActivity();  // the Wi-Fi checks can take a while; count from when they finish
  if (warning.length()) portalSendJson(true, (warning + F("\n\nSave anyway?")).c_str(), true);
  else portalSendJson(true, status.c_str());
}

// Settings save as long as one of the two networks connects, so the clock can be set up away from
// home on the backup alone. A network that is in range but refuses to connect is usually a wrong
// password, so the page asks before saving rather than letting the owner find out on the road.
static void handlePortalValidate() {
  notePortalActivity();
  DeviceConfig submitted = deviceConfig;
  loadSubmittedPortalConfig(submitted);
  submitted.configured = true;
  normalizeDeviceConfig(submitted);

  if (!hasText(submitted.wifiSsid)) {
    portalSendJson(false, "Wi-Fi: missing\nEnter a Wi-Fi network name.");
    return;
  }

  // Cheap checks first, so a typo never waits behind a minute of Wi-Fi tests.
  if (submitted.quietHoursEnabled && submitted.quietStartHour == submitted.quietEndHour) {
    portalSendJson(false, "Quiet hours: check the times\nPick different start and end hours, or turn quiet hours off.");
    return;
  }
  if (submitted.rotateScreens) {
    const long typed = portalServer.arg("rotate_value").toInt();
    const long seconds = typed * (portalServer.arg("rotate_unit") == "60" ? 60L : 1L);
    if (typed <= 0 || seconds < MIN_ROTATE_SECONDS || seconds > MAX_ROTATE_SECONDS) {
      portalSendJson(false, "Rotate screens: check the interval\nPick anything from 15 seconds to 12 hours (720 minutes).");
      return;
    }
  }
  if (submitted.watchlistEnabled && tickerCount(submitted) == 0) {
    portalSendJson(false, "Watchlist: no tickers\nAdd at least one ticker, such as MSTR or IBIT, or turn the watchlist off.");
    return;
  }

  const bool hasBackup = hasText(submitted.wifiSsid2);
  bool primaryInRange = false;
  bool backupInRange = false;
  scanForNetworks(submitted, primaryInRange, backupInRange);

  // Try the backup first when it is the one in range, so the connection left up for the MQTT
  // and watchlist checks below is a working one.
  const bool backupFirst = hasBackup && backupInRange && !primaryInRange;
  bool primaryOk = false;
  bool backupOk = false;
  if (backupFirst) {
    backupOk = connectToNetwork(submitted.wifiSsid2, submitted.wifiPass2, WIFI_CONNECT_TIMEOUT_MS, true);
    if (!backupOk) primaryOk = connectToNetwork(submitted.wifiSsid, submitted.wifiPass, WIFI_CONNECT_TIMEOUT_MS, true);
  } else {
    if (hasBackup && backupInRange) backupOk = connectToNetwork(submitted.wifiSsid2, submitted.wifiPass2, WIFI_CONNECT_TIMEOUT_MS, true);
    primaryOk = connectToNetwork(submitted.wifiSsid, submitted.wifiPass, WIFI_CONNECT_TIMEOUT_MS, true);
    // The backup may also be a hidden network that never shows in a scan.
    if (!primaryOk && hasBackup && !backupInRange) backupOk = connectToNetwork(submitted.wifiSsid2, submitted.wifiPass2, WIFI_CONNECT_TIMEOUT_MS, true);
    // The failed main attempt dropped the backup connection; rejoin it for the checks below.
    if (!primaryOk && backupOk && WiFi.status() != WL_CONNECTED) {
      backupOk = connectToNetwork(submitted.wifiSsid2, submitted.wifiPass2, WIFI_CONNECT_TIMEOUT_MS, true);
    }
  }

  if (!primaryOk && !backupOk) {
    portalSendJson(false, hasBackup
      ? "Wi-Fi: failed\nNeither network would connect. Check the names and passwords, then try again."
      : "Wi-Fi: failed\nCheck the SSID and password, then try again.");
    return;
  }
  // One network works, so saving is allowed; a second one that is in range but refused is worth a
  // look first, though, so the page asks before saving (weak signal can cause it too).
  String warning;
  if (!primaryOk && primaryInRange) warning = F("Main Wi-Fi: failed\nThe main network is in range but would not connect. Check its password.");
  if (hasBackup && !backupOk && backupInRange) warning = F("Backup Wi-Fi: failed\nThe backup network is in range but would not connect. Check its password.");

  String wifiStatus = primaryOk ? F("Wi-Fi: OK")
    : primaryInRange ? F("Backup Wi-Fi: OK")
    : F("Backup Wi-Fi: OK\nMain Wi-Fi is out of range, so it is saved untested.");

  if (submitted.watchlistEnabled && tickerCount(submitted) > 0) {
    BlockData scratch = {};
    String unknown;
    if (fetchTickerQuotes(submitted, scratch, &unknown) && unknown.length()) {
      String message = wifiStatus + F("\nWatchlist: not found\nYahoo Finance does not know ") + unknown + F(". Check the spelling on finance.yahoo.com, e.g. ^GSPC for the S&P 500.");
      portalSendJson(false, message.c_str());
      return;
    }
  }

  if (sanitizeDataSourceMode(submitted.dataSourceMode) != DATA_SOURCE_MQTT) {
    portalSendWifiResult(wifiStatus, warning);
    return;
  }

  if (!hasText(submitted.mqttServer) || submitted.mqttPort == 0) {
    portalSendJson(false, (wifiStatus + F("\nMQTT: missing\nEnter the broker host and port.")).c_str());
    return;
  }

  if (!connectMQTT(submitted, MQTT_CONNECT_TIMEOUT_MS)) {
    if (mqttClient.connected()) mqttClient.disconnect();
    portalSendJson(false, (wifiStatus + F("\nMQTT: failed\nCheck the broker host, port, and credentials.")).c_str());
    return;
  }

  if (mqttClient.connected()) mqttClient.disconnect();
  portalSendWifiResult(wifiStatus + F("\nMQTT: OK"), warning);
}

static void handlePortalSave() {
  notePortalActivity();
  DeviceConfig cfg = deviceConfig;
  loadSubmittedPortalConfig(cfg);
  cfg.configured = true;
  normalizeDeviceConfig(cfg);

  if (!saveDeviceConfig(cfg)) {
    portalServer.send(200, "application/json", F("{\"ok\":false,\"error\":\"Could not write settings to storage. Try again, or use Factory reset at the bottom of this page.\"}"));
    return;
  }
  deviceConfig = cfg;

  setPostSaveRestartPending(true);
  portalShouldRestart = true;
  portalServer.send(200, "application/json", F("{\"ok\":true}"));
}

static void handlePortalWifiList() {
  notePortalActivity();
  String json = F("{\"ok\":true,\"networks\":[");
  bool first = true;

  WiFi.mode(WIFI_AP_STA);
  delay(50);
  WiFi.scanNetworks(true);
  const uint32_t scanStart = millis();
  while (WiFi.scanComplete() < 0 && (millis() - scanStart) < 8000) {
    yield();
    delay(20);
  }
  const int networkCount = WiFi.scanComplete();
  if (networkCount < 0) {
    portalServer.send(200, "application/json", F("{\"ok\":false,\"message\":\"Wi-Fi scan failed. You can still type the SSID manually.\"}"));
    return;
  }

  for (int i = 0; i < networkCount; i++) {
    String ssid = WiFi.SSID(i);
    if (ssid.length() == 0) continue;
    bool duplicate = false;
    for (int j = 0; j < i; j++) {
      if (ssid == WiFi.SSID(j)) { duplicate = true; break; }
    }
    if (duplicate) continue;
    if (!first) json += ",";
    json += F("{\"ssid\":\"");
    json += jsonEscape(ssid.c_str());
    json += F("\"}");
    first = false;
  }
  WiFi.scanDelete();
  json += F("]}");

  portalServer.sendHeader("Cache-Control", "no-store, no-cache, must-revalidate");
  portalServer.send(200, "application/json; charset=utf-8", json);
}

static void handlePortalRedirect() {
  notePortalActivity();
  portalServer.sendHeader("Location", String("http://") + CONFIG_AP_IP.toString(), true);
  portalServer.send(302, "text/plain", "");
}

static void handlePortalFactoryReset() {
  notePortalActivity();
  portalServer.send(200, "application/json", F("{\"ok\":true}"));
  portalFactoryResetRequested = true;
  portalShouldRestart = true;
}

static void startPortalServers() {
  portalServer.on("/", HTTP_GET, handlePortalRoot);
  portalServer.on("/factory-reset", HTTP_POST, handlePortalFactoryReset);
  portalServer.on("/validate", HTTP_POST, handlePortalValidate);
  portalServer.on("/save", HTTP_POST, handlePortalSave);
  portalServer.on("/wifi-list", HTTP_GET, handlePortalWifiList);
  portalServer.on("/generate_204", HTTP_GET, handlePortalRedirect);
  portalServer.on("/hotspot-detect.html", HTTP_GET, handlePortalRedirect);
  portalServer.on("/connecttest.txt", HTTP_GET, handlePortalRedirect);
  portalServer.on("/ncsi.txt", HTTP_GET, handlePortalRedirect);
  portalServer.onNotFound(handlePortalRedirect);
  portalServer.begin();
}

// A clock button as setup sees it: a press counts once it has been let go for BUTTON_RELEASE_MS,
// and the press that opened setup never counts as a tap.
struct PortalButton {
  uint8_t pin;
  bool down;
  bool countsAsTap;
  uint32_t downAtMs;
  uint32_t lastLowMs;
};

static void beginPortalButton(PortalButton& b, uint8_t pin, uint32_t downAtMs) {
  b.pin = pin;
  b.down = digitalRead(pin) == LOW;
  b.countsAsTap = false;
  b.downAtMs = downAtMs;
  b.lastLowMs = millis();
}

// Returns true on the release that ends a tap.
static bool updatePortalButton(PortalButton& b, uint32_t nowMs) {
  if (digitalRead(b.pin) == LOW) {
    b.lastLowMs = nowMs;
    if (!b.down) {
      b.down = true;
      b.countsAsTap = true;
      b.downAtMs = nowMs;
    }
    return false;
  }
  if (!b.down || (nowMs - b.lastLowMs) < BUTTON_RELEASE_MS) return false;
  b.down = false;
  return b.countsAsTap;
}

static void stopPortalNetwork() {
  portalServer.stop();
  portalDnsServer.stop();
  WiFi.softAPdisconnect(true);
}

static void runSetupPortal() {
  portalShouldRestart = false;
  portalFactoryResetRequested = false;
  WiFi.persistent(false);
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(CONFIG_AP_IP, CONFIG_AP_GATEWAY, CONFIG_AP_SUBNET);
  WiFi.softAP(portalApSsid, portalApPassword);

  portalDnsServer.start(CONFIG_DNS_PORT, "*", CONFIG_AP_IP);
  startPortalServers();
  drawSetupPortalReadyScreen();
  notePortalActivity();

  // A tap on either button closes setup without saving. Holding SETUP about 10 s factory-resets,
  // counting from the press that opened setup if it is still held.
  const uint32_t openedMs = millis();
  PortalButton setupButton;
  PortalButton functionButton;
  beginPortalButton(setupButton, PIN_SETUP_BUTTON, setupHoldStartedMs != 0 ? setupHoldStartedMs : openedMs);
  beginPortalButton(functionButton, PIN_FUNCTION_BUTTON, openedMs);
  setupHoldStartedMs = 0;
  bool closeRequested = false;
  while (!portalShouldRestart && !closeRequested) {
    portalDnsServer.processNextRequest();
    portalServer.handleClient();
    const uint32_t nowMs = millis();
    if (updatePortalButton(setupButton, nowMs) || updatePortalButton(functionButton, nowMs)) {
      Serial.println("[BlockClock] setup closed by a button tap");
      closeRequested = true;
    } else if (setupButton.down && (nowMs - setupButton.downAtMs) >= FACTORY_RESET_HOLD_MS) {
      portalFactoryResetRequested = true;
      portalShouldRestart = true;
    } else if ((nowMs - portalLastActivityMs) >= CONFIG_PORTAL_IDLE_TIMEOUT_MS) {
      Serial.println("[BlockClock] setup closed after 10 idle minutes");
      closeRequested = true;
    }
    delay(CONFIG_PORTAL_DELAY_MS);
  }

  if (closeRequested) {
    stopPortalNetwork();
    if (!deviceConfig.configured) {
      // Nothing to show yet: back to the welcome screen, which waits for a button.
      drawWelcomeScreen();
      goToWelcomeSleep();
      return;
    }
    returnFromSetupWithoutChanges();
    return;
  }

  if (portalFactoryResetRequested) {
    // Let a factory-reset request's response reach the browser before the AP goes away.
    const uint32_t flushStart = millis();
    while ((millis() - flushStart) < 500) {
      portalServer.handleClient();
      delay(CONFIG_PORTAL_DELAY_MS);
    }
    stopPortalNetwork();
    factoryResetAndShowWelcome();
    return;
  }

  // Keep serving clients during exit grace so the save response reaches the browser
  // before the AP shuts down, then show the e-ink saved screen and restart.
  const uint32_t gracePeriodStart = millis();
  while ((millis() - gracePeriodStart) < CONFIG_PORTAL_EXIT_GRACE_MS) {
    portalServer.handleClient();
    delay(CONFIG_PORTAL_DELAY_MS);
  }

  stopPortalNetwork();

  drawSetupSavedScreen();
  delay(CONFIG_PORTAL_RESTART_DELAY_MS);
  ESP.restart();
}
