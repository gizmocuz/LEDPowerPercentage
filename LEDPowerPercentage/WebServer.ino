// ---------------------------------------------------------------------------
// handleWebRoot — Main status and control page
// ---------------------------------------------------------------------------

void handleWebRoot() {
    char colorHex[8];
    snprintf(colorHex, sizeof(colorHex), "#%02x%02x%02x",
             Config::middle_r, Config::middle_g, Config::middle_b);

    String html;
    html.reserve(2048);

    html += R"rawhtml(<!DOCTYPE html><html><head><meta name="viewport" content="width=device-width, initial-scale=1"><meta http-equiv="refresh" content="5"><link rel="icon" href="data:,"><style>body { font-family: Helvetica; max-width: 480px; margin: 0 auto; padding: 12px; text-align: center; }h1 { font-size: 1.4em; }.state-on  { display: inline-block; padding: 4px 16px; border-radius: 4px; background: #17A2FC; color: #fff; font-weight: bold; }.state-off { display: inline-block; padding: 4px 16px; border-radius: 4px; background: #888;    color: #fff; font-weight: bold; }.state-idle { display:inline-block; padding:4px 16px; border-radius:4px; background:#888; color:#fff; font-weight:bold; }.state-charging { display:inline-block; padding:4px 16px; border-radius:4px; background:#F5A623; color:#fff; font-weight:bold; }.state-discharging { display:inline-block; padding:4px 16px; border-radius:4px; background:#E74C3C; color:#fff; font-weight:bold; }input[type=range] { width: 80%; }.btn { display: inline-block; background: #195B6A; color: #fff; padding: 10px 28px;       border: none; border-radius: 4px; font-size: 1em; cursor: pointer; text-decoration: none; margin: 4px; }.btn-on  { background: #17A2FC; }.btn-off { background: #888; }hr { margin: 16px 0; }</style></head><body><h1>LED Power Percentage</h1><p><b>Device:</b> )rawhtml";
    html += identifier;
    html += R"rawhtml(</p><p><b>Firmware:</b> )rawhtml";
    html += app_version;
    html += R"rawhtml(</p><p><b>Author:</b> PA1DVB</p><hr>)rawhtml";

    // State indicator
    html += R"rawhtml(<p><b>State:</b> )rawhtml";
    if (ledState) {
        html += R"rawhtml(<span class="state-on">ON</span>)rawhtml";
    } else {
        html += R"rawhtml(<span class="state-off">OFF</span>)rawhtml";
    }
    html += R"rawhtml(</p>)rawhtml";

    // Level
    html += R"rawhtml(<p><b>Level:</b> )rawhtml"
        + String(ledPercent) +
        R"rawhtml(%</p>)rawhtml";

    // Charging status (3-state)
    html += R"rawhtml(<p><b>Charging:</b> )rawhtml";
    if (chargingState == STATE_CHARGING) {
        html += R"rawhtml(<span class="state-charging">Charging</span>)rawhtml";
    } else if (chargingState == STATE_DISCHARGING) {
        html += R"rawhtml(<span class="state-discharging">Discharging</span>)rawhtml";
    } else {
        html += R"rawhtml(<span class="state-idle">Idle</span>)rawhtml";
    }
    html += R"rawhtml(</p>)rawhtml";

    html += R"rawhtml(<hr>)rawhtml";

    // Slider form — submits automatically when the user releases the slider
    html += R"rawhtml(<form method="GET" action="/set"><input type="range" name="pct" min="0" max="100" value=")rawhtml"
        + String(ledPercent) +
        R"rawhtml(" oninput="this.nextElementSibling.value=this.value" onchange="this.form.submit()"><output>)rawhtml"
        + String(ledPercent) +
        R"rawhtml(</output>%<input type="hidden" name="state" value="on"></form>)rawhtml";

    html += R"rawhtml(<p><a class="btn btn-on" href="/set?state=on">Turn ON</a> <a class="btn btn-off" href="/set?state=off">Turn OFF</a></p>)rawhtml";

    html += R"rawhtml(<p><a class="btn" href="/set?charging_state=idle">Idle</a> <a class="btn btn-on" style="background:#F5A623" href="/set?charging_state=charging">Charging</a> <a class="btn btn-off" style="background:#E74C3C" href="/set?charging_state=discharging">Discharging</a></p>)rawhtml";

    html += R"rawhtml(<hr><p><a href="/config">Configuration</a></p></body></html>)rawhtml";

    webServer.send(200, "text/html", html);
}

// ---------------------------------------------------------------------------
// handleWebSet — Apply level / state from GET params, then redirect to /
// ---------------------------------------------------------------------------

void handleWebSet() {
    if (webServer.hasArg("state")) {
        ledState = webServer.arg("state") == "on";
        // Turn ON at 0% would be invisible — default to 100%
        if (ledState && ledPercent == 0) ledPercent = 100;
    }
    if (webServer.hasArg("pct")) {
        ledPercent = constrain(webServer.arg("pct").toInt(), 0, 100);
        ledState   = (ledPercent > 0);
    }
    if (webServer.hasArg("charging_state")) {
        String cs = webServer.arg("charging_state");
        if (cs == "charging")         chargingState = STATE_CHARGING;
        else if (cs == "discharging") chargingState = STATE_DISCHARGING;
        else                          chargingState = STATE_IDLE;
        // Ensure strip is on so the animation is visible
        if (chargingState != STATE_IDLE && !ledState) {
            if (ledPercent == 0) ledPercent = 100;
            ledState = true;
        }
    }
    updateLEDs();
    publishState();
    webServer.sendHeader("Location", "/");
    webServer.send(302, "text/plain", "");
}

// ---------------------------------------------------------------------------
// handleWebConfig — Configuration form (GET)
// ---------------------------------------------------------------------------

void handleWebConfig() {
    char colorHex[8];
    snprintf(colorHex, sizeof(colorHex), "#%02x%02x%02x",
             Config::middle_r, Config::middle_g, Config::middle_b);

    String html;
    html.reserve(2048);

    html += R"rawhtml(<!DOCTYPE html><html><head><meta name="viewport" content="width=device-width, initial-scale=1"><link rel="icon" href="data:,"><style>body { font-family: Helvetica; max-width: 480px; margin: 0 auto; padding: 12px; }h1 { font-size: 1.4em; text-align: center; }table { width: 100%; border-collapse: collapse; }td { padding: 4px 4px; vertical-align: middle; }td:first-child { width: 55%; font-weight: bold; }input[type=number], input[type=color] { width: 80px; }input[type=range] { width: 100%; }.btn { display: inline-block; background: #195B6A; color: #fff; padding: 10px 28px;       border: none; border-radius: 4px; font-size: 1em; cursor: pointer; text-decoration: none; }hr { margin: 16px 0; }</style></head><body><h1>Configuration</h1><form method="POST" action="/config"><table>)rawhtml";

    // mqtt_enabled
    html += R"rawhtml(<tr><td>MQTT Enabled:</td><td><input type="checkbox" id="mqtt_enabled" name="mqtt_enabled" onchange="if(this.checked)document.getElementById('vrm_enabled').checked=false;")rawhtml";
    if (Config::mqtt_enabled) {
        html += R"rawhtml( checked)rawhtml";
    }
    html += R"rawhtml(></td></tr>)rawhtml";

    // mqtt_server
    html += R"rawhtml(<tr><td>MQTT Server:</td><td><input type="text" name="mqtt_server" value=")rawhtml";
    html += Config::mqtt_server;
    html += R"rawhtml(" maxlength="79" style="width:200px"></td></tr>)rawhtml";

    // mqtt_port
    html += R"rawhtml(<tr><td>MQTT Port:</td><td><input type="number" name="mqtt_port" value=")rawhtml"
        + String(Config::mqtt_port) +
        R"rawhtml(" min="1" max="65535" style="width:90px"></td></tr>)rawhtml";

    // mqtt_username
    html += R"rawhtml(<tr><td>MQTT Username:</td><td><input type="text" name="mqtt_username" value=")rawhtml";
    html += Config::mqtt_username;
    html += R"rawhtml(" maxlength="23" style="width:160px"></td></tr>)rawhtml";

    // mqtt_password
    html += R"rawhtml(<tr><td>MQTT Password:</td><td><input type="password" name="mqtt_password" value=")rawhtml";
    html += Config::mqtt_password;
    html += R"rawhtml(" maxlength="23" autocomplete="off" style="width:160px"></td></tr>)rawhtml";

    // mqtt_secure
    html += R"rawhtml(<tr><td>MQTT Secure (TLS):</td><td><input type="checkbox" name="mqtt_secure")rawhtml";
    if (Config::mqtt_secure) html += R"rawhtml( checked)rawhtml";
    html += R"rawhtml(></td></tr>)rawhtml";

    // mqtt_discovery_prefix
    html += R"rawhtml(<tr><td>MQTT Discovery Prefix:</td><td><input type="text" name="mqtt_discovery_prefix" value=")rawhtml";
    html += Config::mqtt_discovery_prefix;
    html += R"rawhtml(" maxlength="31" style="width:160px"></td></tr>)rawhtml";

    // --- VRM section ---
    html += R"rawhtml(<tr><td colspan="2"><hr style="margin:8px 0"></td></tr>)rawhtml";

    html += R"rawhtml(<tr><td>Victron VRM Enabled:</td><td><input type="checkbox" id="vrm_enabled" name="vrm_enabled" onchange="if(this.checked)document.getElementById('mqtt_enabled').checked=false;")rawhtml";
    if (Config::vrm_enabled) html += R"rawhtml( checked)rawhtml";
    html += R"rawhtml(></td></tr>)rawhtml";

    html += R"rawhtml(<tr><td>VRM API Token:</td><td><input type="password" name="vrm_api_token" value=")rawhtml";
    html += Config::vrm_api_token;
    html += R"rawhtml(" maxlength="127" style="width:200px" autocomplete="off"><br><small style="color:#888">VRM Portal -> Preferences -> Integrations -> Access tokens</small></td></tr>)rawhtml";

    html += R"rawhtml(<tr><td>VRM Site ID:</td><td><input type="number" name="vrm_site_id" value=")rawhtml"
        + String(Config::vrm_site_id) +
        R"rawhtml(" min="0" style="width:100px"><br><small style="color:#888">0 = auto (first installation)</small></td></tr>)rawhtml";

    html += R"rawhtml(<tr><td>VRM Battery Instance:</td><td><input type="number" name="vrm_battery_instance" value=")rawhtml"
        + String(Config::vrm_battery_instance) +
        R"rawhtml(" min="-1" style="width:100px"><br><small style="color:#888">-1 = auto (first Battery Monitor)</small></td></tr>)rawhtml";

    html += R"rawhtml(<tr><td>VRM Poll interval (s):</td><td><input type="number" name="vrm_interval" value=")rawhtml"
        + String(Config::vrm_interval) +
        R"rawhtml(" min="10" max="3600" style="width:80px"></td></tr>)rawhtml";

    html += R"rawhtml(<tr><td colspan="2"><hr style="margin:8px 0"></td></tr>)rawhtml";

    // threshold_1
    html += R"rawhtml(<tr><td>Low threshold (%):</td><td><input type="number" name="threshold_1" value=")rawhtml"
        + String(Config::threshold_1) +
        R"rawhtml(" min="0" max="99"></td></tr>)rawhtml";

    // threshold_2
    html += R"rawhtml(<tr><td>High threshold (%):</td><td><input type="number" name="threshold_2" value=")rawhtml"
        + String(Config::threshold_2) +
        R"rawhtml(" min="1" max="100"></td></tr>)rawhtml";

    // num_pixels
    html += R"rawhtml(<tr><td>LED pixel count:</td><td><input type="number" name="num_pixels" value=")rawhtml";
    html += String(Config::num_pixels);
    html += R"rawhtml(" min="1" max="500"></td></tr>)rawhtml";

    // led_pin — dropdown restricted to valid ESP32-C3 Super Mini GPIO pins
    html += R"rawhtml(<tr><td>LED data pin (GPIO):</td><td><select name="led_pin">)rawhtml";
    int validPins[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 10, 20, 21};
    for (int pin : validPins) {
        html += R"rawhtml(<option value=")rawhtml" + String(pin) + R"rawhtml(")rawhtml";
        if (Config::led_pin == pin) html += R"rawhtml( selected)rawhtml";
        html += R"rawhtml(>GPIO )rawhtml" + String(pin) + R"rawhtml(</option>)rawhtml";
    }
    html += R"rawhtml(</select></td></tr>)rawhtml";

    // fade_pixels
    html += R"rawhtml(<tr><td>Fade pixels:</td><td><input type="number" name="fade_pixels" value=")rawhtml"
        + String(Config::fade_pixels) +
        R"rawhtml(" min="0" max="20"></td></tr>)rawhtml";

    // middle zone color
    html += R"rawhtml(<tr><td>Middle zone color:</td><td><input type="color" name="middle_color" value=")rawhtml";
    html += colorHex;
    html += R"rawhtml("></td></tr>)rawhtml";

    // charge_anim checkbox
    html += R"rawhtml(<tr><td>Charge/Discharge animation:</td><td><input type="checkbox" name="charge_anim")rawhtml";
    if (Config::charge_anim) {
        html += R"rawhtml( checked)rawhtml";
    }
    html += R"rawhtml(></td></tr>)rawhtml";

    // charge_anim_interval
    html += R"rawhtml(<tr><td>Animation interval (s):</td><td><input type="number" name="charge_anim_interval" value=")rawhtml"
        + String(Config::charge_anim_interval) +
        R"rawhtml(" min="2" max="60"></td></tr>)rawhtml";

    // base_brightness range
    html += R"rawhtml(<tr><td>Base brightness ()rawhtml"
        + String(Config::base_brightness) +
        R"rawhtml():</td><td><input type="range" name="base_brightness" value=")rawhtml"
        + String(Config::base_brightness) +
        R"rawhtml(" min="0" max="255" oninput="this.parentElement.previousElementSibling.textContent='Base brightness ('+this.value+'):';fetch('/api/brightness?v='+this.value)"></td></tr>)rawhtml";

    html += R"rawhtml(</table><p style="font-size:0.85em;color:#888"><br><div style="text-align:center"><button class="btn" type="submit">Save</button></div></form><hr><p style="text-align:center"><a href="/" >Back to status</a> &nbsp;|&nbsp; <a href="/reset" style="color:#E74C3C" onclick="return confirm('Reset WiFi settings and reboot into AP mode?')">Reset WiFi</a></p></body></html>)rawhtml";

    webServer.send(200, "text/html", html);
}

// ---------------------------------------------------------------------------
// handleWebConfigSave — Persist settings (POST), then redirect to /config
// ---------------------------------------------------------------------------

void handleWebConfigSave() {
    bool wasMqttEnabled = Config::mqtt_enabled;
    bool wasVrmEnabled  = Config::vrm_enabled;

    if (webServer.hasArg("threshold_1") && webServer.hasArg("threshold_2")) {
        int t1 = webServer.arg("threshold_1").toInt();
        int t2 = webServer.arg("threshold_2").toInt();
        if (t1 >= 0 && t2 <= 100 && t1 < t2) {
            Config::threshold_1 = t1;
            Config::threshold_2 = t2;
        }
    }

    if (webServer.hasArg("fade_pixels"))
        Config::fade_pixels = constrain(webServer.arg("fade_pixels").toInt(), 0, 20);

    if (webServer.hasArg("middle_color")) {
        String c = webServer.arg("middle_color");
        if (c.length() == 7 && c[0] == '#') {
            Config::middle_r = (uint8_t)strtol(c.substring(1, 3).c_str(), nullptr, 16);
            Config::middle_g = (uint8_t)strtol(c.substring(3, 5).c_str(), nullptr, 16);
            Config::middle_b = (uint8_t)strtol(c.substring(5, 7).c_str(), nullptr, 16);
        }
    }

    // Checkbox: present means checked, absent means unchecked; MQTT and VRM are mutually exclusive
    Config::charge_anim  = webServer.hasArg("charge_anim");
    Config::mqtt_enabled = webServer.hasArg("mqtt_enabled");
    Config::mqtt_secure  = webServer.hasArg("mqtt_secure");
    Config::vrm_enabled  = webServer.hasArg("vrm_enabled") && !Config::mqtt_enabled;

    if (webServer.hasArg("mqtt_server")) {
        String s = webServer.arg("mqtt_server"); s.trim();
        strlcpy(Config::mqtt_server, s.c_str(), sizeof(Config::mqtt_server));
    }
    if (webServer.hasArg("mqtt_port"))
        Config::mqtt_port = constrain((int)webServer.arg("mqtt_port").toInt(), 1, 65535);
    if (webServer.hasArg("mqtt_username")) {
        String u = webServer.arg("mqtt_username"); u.trim();
        strlcpy(Config::mqtt_username, u.c_str(), sizeof(Config::mqtt_username));
    }
    if (webServer.hasArg("mqtt_password") && webServer.arg("mqtt_password").length() > 0)
        strlcpy(Config::mqtt_password, webServer.arg("mqtt_password").c_str(), sizeof(Config::mqtt_password));

    if (webServer.hasArg("vrm_api_token") && webServer.arg("vrm_api_token").length() > 0) {
        String t = webServer.arg("vrm_api_token"); t.trim();
        strlcpy(Config::vrm_api_token, t.c_str(), sizeof(Config::vrm_api_token));
    }
    if (webServer.hasArg("vrm_site_id"))
        Config::vrm_site_id = max(0, (int)webServer.arg("vrm_site_id").toInt());
    if (webServer.hasArg("vrm_battery_instance"))
        Config::vrm_battery_instance = max(-1, (int)webServer.arg("vrm_battery_instance").toInt());
    if (webServer.hasArg("vrm_interval"))
        Config::vrm_interval = constrain(webServer.arg("vrm_interval").toInt(), 10, 3600);
    vrmReset(); // force re-login with new credentials

    if (webServer.hasArg("charge_anim_interval"))
        Config::charge_anim_interval = constrain(webServer.arg("charge_anim_interval").toInt(), 2, 60);

    if (webServer.hasArg("base_brightness")) {
        Config::base_brightness = (uint8_t)constrain(webServer.arg("base_brightness").toInt(), 0, 255);
        currentBrightness = Config::base_brightness;
    }

    if (webServer.hasArg("num_pixels")) {
        int n = webServer.arg("num_pixels").toInt();
        if (n >= 1 && n <= 500) Config::num_pixels = n;
    }
    if (webServer.hasArg("led_pin")) {
        int p = webServer.arg("led_pin").toInt();
        if (p >= 0 && p <= 21) Config::led_pin = p;
    }

    if (webServer.hasArg("mqtt_discovery_prefix")) {
        String p = webServer.arg("mqtt_discovery_prefix");
        p.trim();
        if (p.length() > 0) {
            strlcpy(Config::mqtt_discovery_prefix, p.c_str(), sizeof(Config::mqtt_discovery_prefix));
        }
    }

    Config::save();
    ws2812b.updateLength(Config::num_pixels);
    if ((uint8_t)Config::led_pin != ws2812b.getPin()) {
        ws2812b.setPin(Config::led_pin);
        ws2812b.begin(); // re-init RMT only when pin actually changes
    }

    // Handle MQTT / VRM state transitions
    bool mqttChanged = (Config::mqtt_enabled != wasMqttEnabled);
    bool vrmChanged  = (Config::vrm_enabled  != wasVrmEnabled);

    if (mqttChanged || vrmChanged) {
        // Stop VRM whenever it is now disabled or MQTT takes over
        if (!Config::vrm_enabled) vrmReset();

        // Disconnect MQTT whenever it is now disabled or VRM takes over
        if (!Config::mqtt_enabled && mqttClient.connected()) {
            mqttClient.publish(MQTT_TOPIC_AVAILABILITY, AVAILABILITY_OFFLINE, false);
            mqttClient.disconnect();
        }

        // Connect MQTT if just enabled
        if (Config::mqtt_enabled) {
            mqttClient.setCallback(mqttCallback);
            mqttReconnect();
        }

        blinkConnected();
    }

    updateLEDs();
    webServer.sendHeader("Location", "/");
    webServer.send(302, "text/plain", "");
}

// ---------------------------------------------------------------------------
// handleWebReset — Clear WiFi credentials and reboot into AP mode
// ---------------------------------------------------------------------------

void handleWebReset() {
    webServer.send(200, "text/plain", "Resetting WiFi settings. Device will restart into AP mode.");
    delay(500);
    resetWifiSettingsAndReboot();
}

// ---------------------------------------------------------------------------
// handleApiStateGet — GET /api/state → JSON state
// ---------------------------------------------------------------------------

void handleApiStateGet() {
    DynamicJsonDocument doc(256);
    const char* csStr = "idle";
    if (chargingState == STATE_CHARGING)    csStr = "charging";
    if (chargingState == STATE_DISCHARGING) csStr = "discharging";

    doc["state"]          = ledState ? "on" : "off";
    doc["brightness"]     = ledPercent;
    doc["charging_state"] = csStr;
    doc["ip"]             = WiFi.localIP().toString();
    doc["rssi"]           = WiFi.RSSI();

    char payload[256];
    serializeJson(doc, payload);
    webServer.send(200, "application/json", payload);
}

// ---------------------------------------------------------------------------
// handleApiStatePost — POST /api/state with JSON body → apply + return state
// ---------------------------------------------------------------------------

void handleApiStatePost() {
    if (!webServer.hasArg("plain") || webServer.arg("plain").length() == 0) {
        webServer.send(400, "application/json", "{\"error\":\"empty body\"}");
        return;
    }

    DynamicJsonDocument doc(256);
    DeserializationError err = deserializeJson(doc, webServer.arg("plain"));
    if (err != DeserializationError::Ok) {
        webServer.send(400, "application/json", "{\"error\":\"invalid JSON\"}");
        return;
    }

    if (doc.containsKey("state")) {
        const char* s = doc["state"].as<const char*>();
        if (s) ledState = strcmp(s, "on") == 0;
    }
    if (doc.containsKey("brightness")) {
        ledPercent = constrain((int)doc["brightness"], 0, 100);
        ledState   = (ledPercent > 0);
    }
    if (doc.containsKey("charging_state")) {
        const char* cs = doc["charging_state"].as<const char*>();
        if (cs) {
            if      (strcmp(cs, "charging")    == 0) chargingState = STATE_CHARGING;
            else if (strcmp(cs, "discharging") == 0) chargingState = STATE_DISCHARGING;
            else                                      chargingState = STATE_IDLE;
        }
    }

    updateLEDs();
    publishState();
    handleApiStateGet(); // respond with updated state
}

// ---------------------------------------------------------------------------
// handleApiBrightness — GET /api/brightness?v=<0-255> — live preview while slider drags
// ---------------------------------------------------------------------------

void handleApiBrightness() {
    if (webServer.hasArg("v")) {
        currentBrightness = (uint8_t)constrain(webServer.arg("v").toInt(), 0, 255);
        updateLEDs();
    }
    webServer.send(200, "text/plain", "");
}

// ---------------------------------------------------------------------------
// handleWebNotFound — 404 for unregistered routes
// ---------------------------------------------------------------------------

void handleWebNotFound() {
    webServer.send(404, "text/plain", "Not found: " + webServer.uri());
}
