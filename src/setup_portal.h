#pragma once

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

static String portalPage() {
  String html;
  const RefreshIntervalUnit refreshUnit = preferredRefreshIntervalUnit(deviceConfig.refreshIntervalMinutes);
  const uint16_t refreshValue = refreshIntervalDisplayValue(deviceConfig.refreshIntervalMinutes, refreshUnit);
  html.reserve(20000);
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
  html += F("button{border:none;border-radius:999px;padding:13px 18px;font-size:16px;font-weight:700;cursor:pointer;background:#f7931a;color:#2b1700;touch-action:manipulation;}");
  html += F("button.secondary{background:#ded4c2;color:#211d19;}");
  html += F("button[disabled],input[disabled]{opacity:.45;cursor:not-allowed;}");
  html += F("@media(max-width:640px){.brand{font-size:25px;}}");
  html += F("</style></head><body>");

  // Focus guard prevents browser from auto-focusing the first input on page load.
  // Must be a real input with autofocus so it absorbs the browser's initial focus
  // (a div with tabindex=-1 cannot receive input focus on mobile browsers).
  html += F("<input id=\"focus_guard\" tabindex=\"-1\" aria-hidden=\"true\" type=\"text\" readonly autofocus autocomplete=\"off\" style=\"position:fixed;top:0;left:0;width:1px;height:1px;opacity:0;pointer-events:none;outline:none;border:none;background:transparent;font-size:16px;\">");
  html += F("<div id=\"save_overlay\" class=\"save-overlay hidden\"><div id=\"save_overlay_spinner\" class=\"save-overlay-spinner\"></div><div id=\"save_overlay_text\" class=\"save-overlay-text\">Saving settings...</div><button id=\"save_overlay_action\" class=\"secondary hidden\" type=\"button\">Continue</button></div>");
  html += F("<div class=\"wrap\">");
  html += F("<section class=\"hero\">");
  appendPortalBrandHeader(html);
  html += F("<p>Press SETUP once to enter setup mode, or hold SETUP for about 10 seconds to clear all settings (factory reset).</p>");
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
  html += F("</div></div></div></section>");

  // ── Display ───────────────────────────────────────────────────────────────
  html += F("<section class=\"card\"><h2>Display</h2><div class=\"grid\"><div>");
  html += F("<label for=\"display_theme_mode\">Display theme</label><select id=\"display_theme_mode\" name=\"display_theme_mode\">");
  html += F("<option value=\"1\""); html += selectedAttr(sanitizeThemeMode(deviceConfig.displayThemeMode) == DISPLAY_THEME_DARK); html += F(">Dark</option>");
  html += F("<option value=\"0\""); html += selectedAttr(sanitizeThemeMode(deviceConfig.displayThemeMode) == DISPLAY_THEME_LIGHT); html += F(">Light</option>");
  html += F("</select></div><div>");
  html += F("<label for=\"battery_pct\">Battery percentage</label>");
  html += F("<select id=\"battery_pct\" name=\"battery_pct\"><option value=\"0\"");
  html += selectedAttr(!deviceConfig.showBatteryPercent);
  html += F(">Off</option><option value=\"1\"");
  html += selectedAttr(deviceConfig.showBatteryPercent);
  html += F(">On</option></select></div><div>");
  html += F("<label for=\"refresh_interval_value\">Refresh interval</label><div class=\"dual\"><input id=\"refresh_interval_value\" name=\"refresh_interval_value\" type=\"text\" inputmode=\"numeric\" pattern=\"[0-9]*\" min=\"1\" step=\"1\" value=\"");
  html += String(refreshValue);
  html += F("\"><select id=\"refresh_interval_unit\" name=\"refresh_interval_unit\"><option value=\"0\"");
  html += selectedAttr(refreshUnit == REFRESH_INTERVAL_UNIT_MINUTES);
  html += F(">Minutes</option><option value=\"1\"");
  html += selectedAttr(refreshUnit == REFRESH_INTERVAL_UNIT_HOURS);
  html += F(">Hours</option><option value=\"2\"");
  html += selectedAttr(refreshUnit == REFRESH_INTERVAL_UNIT_DAYS);
  html += F(">Days</option></select></div><div class=\"hint\">Shorter intervals use more battery.</div></div></div></section>");

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
  html += F("</form><script>");

  // ── Data source visibility ─────────────────────────────────────────────────
  html += F("(function(){var source=document.getElementById('data_source');var mqtt=document.getElementById('mqtt_settings');function sync(){if(!source||!mqtt)return;mqtt.classList.toggle('hidden',source.value!=='mqtt');}if(source)source.addEventListener('change',sync);sync();})();");

  // ── Save overlay ───────────────────────────────────────────────────────────
  html += F("(function(){var form=document.getElementById('setup_form');var saveButton=document.getElementById('save_button');var overlay=document.getElementById('save_overlay');var overlayText=document.getElementById('save_overlay_text');var overlaySpinner=document.getElementById('save_overlay_spinner');var overlayAction=document.getElementById('save_overlay_action');var saveInFlight=false;");
  html += F("function escapeHtml(s){return String(s||'').replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;').replace(/\"/g,'&quot;').replace(/'/g,'&#39;');}");
  html += F("function setOverlayText(text){if(!overlayText)return;overlayText.innerHTML=escapeHtml(text).replace(/\\n/g,'<br>');}");
  html += F("function showOverlay(text){document.body.style.overflow='hidden';if(overlay)overlay.classList.remove('hidden');if(overlaySpinner)overlaySpinner.classList.remove('hidden');if(overlayAction)overlayAction.classList.add('hidden');setOverlayText(text);}");
  html += F("function showOverlayHtml(html){document.body.style.overflow='hidden';if(overlay)overlay.classList.remove('hidden');if(overlayText)overlayText.innerHTML=html;}");
  html += F("function hideOverlay(){document.body.style.overflow='';if(overlay)overlay.classList.add('hidden');if(overlayAction)overlayAction.classList.add('hidden');if(overlaySpinner)overlaySpinner.classList.remove('hidden');}");
  html += F("function failOverlay(text){if(overlaySpinner)overlaySpinner.classList.add('hidden');setOverlayText(text);if(overlayAction){overlayAction.textContent='Continue';overlayAction.classList.remove('hidden');}if(saveButton)saveButton.disabled=false;saveInFlight=false;}");
  html += F("if(overlayAction)overlayAction.addEventListener('click',hideOverlay);");
  html += F("if(!form)return;");
  html += F("form.addEventListener('submit',function(event){if(saveInFlight)return;if(!window.fetch)return;event.preventDefault();saveInFlight=true;if(saveButton)saveButton.disabled=true;showOverlay('Checking settings...');var body=new URLSearchParams(new FormData(form)).toString();var headers={'Content-Type':'application/x-www-form-urlencoded;charset=UTF-8'};fetch('/validate',{method:'POST',headers:headers,body:body,cache:'no-store'}).then(function(r){return r.json();}).then(function(data){if(!data||!data.ok){failOverlay((data&&data.message)||'Settings check failed.');return null;}showOverlay('Saving settings...');return fetch('/save',{method:'POST',headers:headers,body:body,cache:'no-store'}).then(function(r){return r.json();}).then(function(saved){if(!saved||!saved.ok){failOverlay((saved&&saved.error)||'Save failed.');return;}showOverlayHtml('Settings saved!<br>Device is restarting...');});}).catch(function(){failOverlay('Settings check failed.\\nReconnect to BLOCK CLOCK setup Wi-Fi and try again.');});});})();");

  // ── Password show/hide ─────────────────────────────────────────────────────
  html += F("(function(){");
  html += F("function syncPwToggle(input){var wrap=input&&input.closest?input.closest('.password-wrap'):null;var btn=wrap&&wrap.querySelector?wrap.querySelector('[data-password-toggle]'):null;var has=!!(input&&input.value);if(wrap&&wrap.classList)wrap.classList.toggle('has-secret',has);if(btn){btn.disabled=!has;if(!has){input.type='password';btn.textContent='Show';}}}");
  html += F("function syncAll(){['wifi_pass','mqtt_pass'].forEach(function(id){var el=document.getElementById(id);if(el)syncPwToggle(el);});}");
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
  portalServer.send(200, "text/html", portalPage());
}

static void portalSendJson(bool ok, const char* message) {
  String json = F("{\"ok\":");
  json += ok ? F("true") : F("false");
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
  cfg.showBatteryPercent = portalServer.arg("battery_pct") != "0";
  safeCopyString(cfg.mqttServer, sizeof(cfg.mqttServer), portalServer.arg("mqtt_server"));
  cfg.mqttPort = parsePort(portalServer.arg("mqtt_port"), DEFAULT_MQTT_PORT);
  safeCopyString(cfg.mqttUser, sizeof(cfg.mqttUser), portalServer.arg("mqtt_user"));
  safeCopyString(cfg.mqttPass, sizeof(cfg.mqttPass), portalServer.arg("mqtt_pass"));
  safeCopyString(cfg.topicHeight, sizeof(cfg.topicHeight), portalServer.arg("topic_height"));
  safeCopyString(cfg.topicHalving, sizeof(cfg.topicHalving), portalServer.arg("topic_halving"));
  safeCopyString(cfg.topicHashrate, sizeof(cfg.topicHashrate), portalServer.arg("topic_hashrate"));
  safeCopyString(cfg.topicPriceValue, sizeof(cfg.topicPriceValue), portalServer.arg("topic_price"));
}

static void handlePortalValidate() {
  DeviceConfig submitted = deviceConfig;
  loadSubmittedPortalConfig(submitted);
  submitted.configured = true;
  normalizeDeviceConfig(submitted);

  if (!hasText(submitted.wifiSsid)) {
    portalSendJson(false, "Wi-Fi: missing\nEnter a Wi-Fi network name.");
    return;
  }

  if (!connectWiFi(submitted, WIFI_CONNECT_TIMEOUT_MS, true)) {
    portalSendJson(false, "Wi-Fi: failed\nCheck the SSID and password, then try again.");
    return;
  }

  if (sanitizeDataSourceMode(submitted.dataSourceMode) != DATA_SOURCE_MQTT) {
    portalSendJson(true, "Wi-Fi: OK");
    return;
  }

  if (!hasText(submitted.mqttServer) || submitted.mqttPort == 0) {
    portalSendJson(false, "Wi-Fi: OK\nMQTT: missing\nEnter the broker host and port.");
    return;
  }

  if (!connectMQTT(submitted, MQTT_CONNECT_TIMEOUT_MS)) {
    if (mqttClient.connected()) mqttClient.disconnect();
    portalSendJson(false, "Wi-Fi: OK\nMQTT: failed\nCheck the broker host, port, and credentials.");
    return;
  }

  if (mqttClient.connected()) mqttClient.disconnect();
  portalSendJson(true, "Wi-Fi: OK\nMQTT: OK");
}

static void handlePortalSave() {
  DeviceConfig cfg = deviceConfig;
  loadSubmittedPortalConfig(cfg);
  cfg.configured = true;
  normalizeDeviceConfig(cfg);

  if (!saveDeviceConfig(cfg)) {
    portalServer.send(200, "application/json", F("{\"ok\":false,\"error\":\"Could not write settings to storage. Try again or factory reset (hold SETUP 10 s).\"}"));
    return;
  }
  deviceConfig = cfg;

  setPostSaveRestartPending(true);
  portalShouldRestart = true;
  portalServer.send(200, "application/json", F("{\"ok\":true}"));
}

static void handlePortalWifiList() {
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
  portalServer.sendHeader("Location", String("http://") + CONFIG_AP_IP.toString(), true);
  portalServer.send(302, "text/plain", "");
}

static void startPortalServers() {
  portalServer.on("/", HTTP_GET, handlePortalRoot);
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

  bool factoryResetHoldActive = false;
  uint32_t factoryResetHoldStartedMs = 0;
  while (!portalShouldRestart) {
    portalDnsServer.processNextRequest();
    portalServer.handleClient();
    if (digitalRead(PIN_SETUP_BUTTON) == LOW) {
      if (!factoryResetHoldActive) {
        factoryResetHoldActive = true;
        factoryResetHoldStartedMs = millis();
      } else if ((millis() - factoryResetHoldStartedMs) >= FACTORY_RESET_HOLD_MS) {
        portalFactoryResetRequested = true;
        portalShouldRestart = true;
      }
    } else {
      factoryResetHoldActive = false;
    }
    delay(CONFIG_PORTAL_DELAY_MS);
  }

  if (portalFactoryResetRequested) {
    portalServer.stop();
    portalDnsServer.stop();
    WiFi.softAPdisconnect(true);
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

  portalServer.stop();
  portalDnsServer.stop();
  WiFi.softAPdisconnect(true);

  drawSetupSavedScreen();
  delay(CONFIG_PORTAL_RESTART_DELAY_MS);
  ESP.restart();
}
