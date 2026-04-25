// ---------------------------------------------------------------------------
// handleWebRoot — Main status and control page
// ---------------------------------------------------------------------------

void handleWebRoot() {
    char buf[320];

    webServer.setContentLength(CONTENT_LENGTH_UNKNOWN);
    webServer.send(200, "text/html", "");

    webServer.sendContent(R"html(<!DOCTYPE html><html><head><meta charset="utf-8"><meta name="viewport" content="width=device-width, initial-scale=1"><link rel="icon" href="data:,"><style>body{font-family:Helvetica;max-width:480px;margin:0 auto;padding:12px;text-align:center}h1{font-size:1.4em}.state-on{display:inline-block;padding:4px 16px;border-radius:4px;background:#17A2FC;color:#fff;font-weight:bold}.state-off{display:inline-block;padding:4px 16px;border-radius:4px;background:#888;color:#fff;font-weight:bold}.state-idle{display:inline-block;padding:4px 16px;border-radius:4px;background:#888;color:#fff;font-weight:bold}.state-charging{display:inline-block;padding:4px 16px;border-radius:4px;background:#F5A623;color:#fff;font-weight:bold}.state-discharging{display:inline-block;padding:4px 16px;border-radius:4px;background:#E74C3C;color:#fff;font-weight:bold}input[type=range]{width:80%}.btn{display:inline-block;background:#195B6A;color:#fff;padding:10px 28px;border:none;border-radius:4px;font-size:1em;cursor:pointer;text-decoration:none;margin:4px}.btn-on{background:#17A2FC}.btn-off{background:#888}hr{margin:16px 0}</style></head><body><h1>LED Power Percentage</h1>)html");

    snprintf(buf, sizeof(buf), R"html(<p><b>Device:</b> %s</p><p><b>Firmware:</b> %s</p><p><b>Author:</b> PA1DVB</p><hr>)html",
             identifier, app_version);
    webServer.sendContent(buf);

    snprintf(buf, sizeof(buf), R"html(<p><b>State:</b> <span id="s-state" class="%s">%s</span></p>)html",
             ledState ? "state-on" : "state-off", ledState ? "ON" : "OFF");
    webServer.sendContent(buf);

    snprintf(buf, sizeof(buf), R"html(<p><b>Level:</b> <span id="s-level">%d</span>%%</p>)html", ledPercent);
    webServer.sendContent(buf);

    const char* chargeClass = (chargingState == STATE_CHARGING)   ? "state-charging" :
                              (chargingState == STATE_DISCHARGING) ? "state-discharging" : "state-idle";
    const char* chargeText  = (chargingState == STATE_CHARGING)   ? "Charging" :
                              (chargingState == STATE_DISCHARGING) ? "Discharging" : "Idle";
    snprintf(buf, sizeof(buf), R"html(<p><b>Charging:</b> <span id="s-charge" class="%s">%s</span></p>)html",
             chargeClass, chargeText);
    webServer.sendContent(buf);

    snprintf(buf, sizeof(buf), R"html(<p><b>Animation:</b> <span id="s-anim">%s</span></p><hr>)html",
             animModeToStr(animMode));
    webServer.sendContent(buf);

    snprintf(buf, sizeof(buf),
        R"html(<form method="GET" action="/set"><input id="s-slider" type="range" name="pct" min="0" max="100" value="%d" oninput="sliderTouched=Date.now();this.nextElementSibling.value=this.value" onchange="this.form.submit()"><output id="s-out">%d</output>%%<input type="hidden" name="state" value="on"></form>)html",
        ledPercent, ledPercent);
    webServer.sendContent(buf);

    webServer.sendContent(R"html(<p><a class="btn btn-on" href="/set?state=on">Turn ON</a> <a class="btn btn-off" href="/set?state=off">Turn OFF</a></p><p><a class="btn" href="/set?charging_state=idle">Idle</a> <a class="btn btn-on" style="background:#F5A623" href="/set?charging_state=charging">Charging</a> <a class="btn btn-off" style="background:#E74C3C" href="/set?charging_state=discharging">Discharging</a></p>)html");

    webServer.sendContent(R"html(<p><b>Animation:</b></p><p><select id="s-select" onchange="location.href='/set?animation='+this.value" style="font-size:1em;padding:6px;width:90%;max-width:340px">)html");

    auto sendOpt = [&](const char* value, const char* label, bool sel) {
        snprintf(buf, sizeof(buf), R"html(<option value="%s"%s>%s</option>)html",
                 value, sel ? " selected" : "", label);
        webServer.sendContent(buf);
    };
    sendOpt("none",      "-- Off --",         animMode == ANIM_NONE);
    webServer.sendContent(R"html(<optgroup label="Ambient / Relaxing">)html");
    sendOpt("lava",      "Lava Lamp",          animMode == ANIM_LAVA);
    sendOpt("waterfall", "Waterfall",          animMode == ANIM_WATERFALL);
    sendOpt("gradient",  "Breathing Gradient", animMode == ANIM_GRADIENT);
    sendOpt("breathe",   "Breathe",            animMode == ANIM_BREATHE);
    sendOpt("starfield", "Starfield",          animMode == ANIM_STARFIELD);
    webServer.sendContent(R"html(</optgroup><optgroup label="Dynamic / Energetic">)html");
    sendOpt("fire",         "Fire",         animMode == ANIM_FIRE);
    sendOpt("pulse",        "Energy Pulse", animMode == ANIM_PULSE);
    sendOpt("meteor",       "Meteor",       animMode == ANIM_METEOR);
    sendOpt("rain",         "Rain",         animMode == ANIM_RAIN);
    sendOpt("notification", "Notification", animMode == ANIM_NOTIFICATION);
    webServer.sendContent(R"html(</optgroup><optgroup label="Colorful">)html");
    sendOpt("rainbow", "Rainbow", animMode == ANIM_RAINBOW);
    sendOpt("twinkle", "Twinkle", animMode == ANIM_TWINKLE);
    webServer.sendContent(R"html(</optgroup></select></p>)html");

    webServer.sendContent(R"html(<hr><p><a href="/config">Configuration</a></p><script>var cs={idle:['state-idle','Idle'],charging:['state-charging','Charging'],discharging:['state-discharging','Discharging']};var sliderTouched=0;function upd(){fetch('/api/state').then(function(r){return r.json();}).then(function(d){var e=document.getElementById('s-state');e.className=d.state==='on'?'state-on':'state-off';e.textContent=d.state==='on'?'ON':'OFF';document.getElementById('s-level').textContent=d.brightness;var sl=document.getElementById('s-slider');if(Date.now()-sliderTouched>10000){sl.value=d.brightness;document.getElementById('s-out').value=d.brightness;}var c=cs[d.charging_state]||cs.idle;var cb=document.getElementById('s-charge');cb.className=c[0];cb.textContent=c[1];document.getElementById('s-select').value=d.animation||'none';document.getElementById('s-anim').textContent=d.animation||'none';}).catch(function(e){console.error('Status poll failed:',e);});}setInterval(upd,5000);</script></body></html>)html");
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
    if (webServer.hasArg("animation")) {
        AnimMode newMode = strToAnimMode(webServer.arg("animation").c_str());
        setAnimMode(newMode);
        publishAnimationState();
        webServer.sendHeader("Location", "/");
        webServer.send(302, "text/plain", "");
        return;
    }
    if (colorOverrideActive) {
        colorOverrideActive = false;
        publishColorState();
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
    char buf[512];
    char colorHex[8];
    snprintf(colorHex, sizeof(colorHex), "#%02x%02x%02x",
             Config::middle_r, Config::middle_g, Config::middle_b);

    webServer.setContentLength(CONTENT_LENGTH_UNKNOWN);
    webServer.send(200, "text/html", "");

    webServer.sendContent(R"html(<!DOCTYPE html><html><head><meta charset="utf-8"><meta name="viewport" content="width=device-width, initial-scale=1"><link rel="icon" href="data:,"><style>body{font-family:Helvetica;max-width:480px;margin:0 auto;padding:12px}h1{font-size:1.4em;text-align:center}table{width:100%;border-collapse:collapse}td{padding:4px 4px;vertical-align:middle}td:first-child{width:55%;font-weight:bold}input[type=number],input[type=color]{width:80px}input[type=range]{width:100%}.btn{display:inline-block;background:#195B6A;color:#fff;padding:10px 28px;border:none;border-radius:4px;font-size:1em;cursor:pointer;text-decoration:none}hr{margin:16px 0}</style></head><body><h1>Configuration</h1><form method="POST" action="/config"><table>)html");

    snprintf(buf, sizeof(buf), R"html(<tr><td>MQTT Enabled:</td><td><input type="checkbox" id="mqtt_enabled" name="mqtt_enabled" onchange="if(this.checked)document.getElementById('vrm_enabled').checked=false;"%s></td></tr>)html",
             Config::mqtt_enabled ? " checked" : "");
    webServer.sendContent(buf);

    snprintf(buf, sizeof(buf), R"html(<tr><td>MQTT Server:</td><td><input type="text" name="mqtt_server" value="%s" maxlength="79" style="width:200px"></td></tr>)html",
             Config::mqtt_server);
    webServer.sendContent(buf);

    snprintf(buf, sizeof(buf), R"html(<tr><td>MQTT Port:</td><td><input type="number" name="mqtt_port" value="%d" min="1" max="65535" style="width:90px"></td></tr>)html",
             Config::mqtt_port);
    webServer.sendContent(buf);

    snprintf(buf, sizeof(buf), R"html(<tr><td>MQTT Username:</td><td><input type="text" name="mqtt_username" value="%s" maxlength="23" style="width:160px"></td></tr>)html",
             Config::mqtt_username);
    webServer.sendContent(buf);

    snprintf(buf, sizeof(buf), R"html(<tr><td>MQTT Password:</td><td><input type="password" name="mqtt_password" value="%s" maxlength="23" autocomplete="off" style="width:160px"></td></tr>)html",
             Config::mqtt_password);
    webServer.sendContent(buf);

    snprintf(buf, sizeof(buf), R"html(<tr><td>MQTT Secure (TLS):</td><td><input type="checkbox" name="mqtt_secure"%s></td></tr>)html",
             Config::mqtt_secure ? " checked" : "");
    webServer.sendContent(buf);

    snprintf(buf, sizeof(buf), R"html(<tr><td>MQTT Discovery Prefix:</td><td><input type="text" name="mqtt_discovery_prefix" value="%s" maxlength="31" style="width:160px"></td></tr>)html",
             Config::mqtt_discovery_prefix);
    webServer.sendContent(buf);

    webServer.sendContent(R"html(<tr><td colspan="2"><hr style="margin:8px 0"></td></tr>)html");

    snprintf(buf, sizeof(buf), R"html(<tr><td>Victron VRM Enabled:</td><td><input type="checkbox" id="vrm_enabled" name="vrm_enabled" onchange="if(this.checked)document.getElementById('mqtt_enabled').checked=false;"%s></td></tr>)html",
             Config::vrm_enabled ? " checked" : "");
    webServer.sendContent(buf);

    snprintf(buf, sizeof(buf), R"html(<tr><td>VRM API Token:</td><td><input type="password" name="vrm_api_token" value="%s" maxlength="127" style="width:200px" autocomplete="off"><br><small style="color:#888">VRM Portal -> Preferences -> Integrations -> Access tokens</small></td></tr>)html",
             Config::vrm_api_token);
    webServer.sendContent(buf);

    snprintf(buf, sizeof(buf), R"html(<tr><td>VRM Site ID:</td><td><input type="number" name="vrm_site_id" value="%d" min="0" style="width:100px"><br><small style="color:#888">0 = auto (first installation)</small></td></tr>)html",
             Config::vrm_site_id);
    webServer.sendContent(buf);

    snprintf(buf, sizeof(buf), R"html(<tr><td>VRM Battery Instance:</td><td><input type="number" name="vrm_battery_instance" value="%d" min="-1" style="width:100px"><br><small style="color:#888">-1 = auto (first Battery Monitor)</small></td></tr>)html",
             Config::vrm_battery_instance);
    webServer.sendContent(buf);

    snprintf(buf, sizeof(buf), R"html(<tr><td>VRM Charge threshold (A):</td><td><input type="number" name="vrm_charge_threshold" value="%.1f" step="0.1" style="width:100px"><br><small style="color:#888">Current above this = charging (default 0.5)</small></td></tr>)html",
             Config::vrm_charge_threshold);
    webServer.sendContent(buf);

    snprintf(buf, sizeof(buf), R"html(<tr><td>VRM Discharge threshold (A):</td><td><input type="number" name="vrm_discharge_threshold" value="%.1f" step="0.1" style="width:100px"><br><small style="color:#888">Current below this = discharging (default -0.5)</small></td></tr>)html",
             Config::vrm_discharge_threshold);
    webServer.sendContent(buf);

    snprintf(buf, sizeof(buf), R"html(<tr><td>VRM Poll interval (s):</td><td><input type="number" name="vrm_interval" value="%d" min="10" max="3600" style="width:80px"></td></tr>)html",
             Config::vrm_interval);
    webServer.sendContent(buf);

    webServer.sendContent(R"html(<tr><td colspan="2"><hr style="margin:8px 0"></td></tr>)html");

    snprintf(buf, sizeof(buf), R"html(<tr><td>LED pixel count:</td><td><input type="number" name="num_pixels" value="%d" min="1" max="500"></td></tr>)html",
             Config::num_pixels);
    webServer.sendContent(buf);

    webServer.sendContent(R"html(<tr><td>LED data pin (GPIO):</td><td><select name="led_pin">)html");
    {
        int validPins[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 10, 20, 21};
        for (int pin : validPins) {
            snprintf(buf, sizeof(buf), R"html(<option value="%d"%s>GPIO %d</option>)html",
                     pin, Config::led_pin == pin ? " selected" : "", pin);
            webServer.sendContent(buf);
        }
    }
    webServer.sendContent(R"html(</select></td></tr>)html");

    {
        struct { const char* label; uint8_t val; } orders[] = {
            {"NEO_GRB (most strips)", 82},
            {"NEO_RGB", 6},
            {"NEO_RBG (default)", 9},
            {"NEO_BRG", 88},
            {"NEO_BGR", 54},
            {"NEO_GBR", 98},
        };
        webServer.sendContent(R"html(<tr><td>Pixel color order:</td><td><select name="pixel_color_order">)html");
        for (auto& o : orders) {
            snprintf(buf, sizeof(buf), R"html(<option value="%d"%s>%s</option>)html",
                     o.val, Config::pixel_color_order == o.val ? " selected" : "", o.label);
            webServer.sendContent(buf);
        }
        webServer.sendContent(R"html(</select></td></tr>)html");
    }

    snprintf(buf, sizeof(buf), R"html(<tr><td>LED signal speed:</td><td><select name="pixel_khz"><option value="800"%s>800 KHz (WS2812B)</option><option value="400"%s>400 KHz (WS2811)</option></select></td></tr>)html",
             Config::pixel_khz == 800 ? " selected" : "", Config::pixel_khz == 400 ? " selected" : "");
    webServer.sendContent(buf);

    snprintf(buf, sizeof(buf), R"html(<tr><td>Fade pixels:</td><td><input type="number" name="fade_pixels" value="%d" min="0" max="20"></td></tr>)html",
             Config::fade_pixels);
    webServer.sendContent(buf);

    snprintf(buf, sizeof(buf), R"html(<tr><td>Middle zone color:</td><td><input type="color" name="middle_color" value="%s"></td></tr>)html",
             colorHex);
    webServer.sendContent(buf);

    snprintf(buf, sizeof(buf), R"html(<tr><td>Charge/Discharge animation:</td><td><input type="checkbox" name="charge_anim"%s></td></tr>)html",
             Config::charge_anim ? " checked" : "");
    webServer.sendContent(buf);

    snprintf(buf, sizeof(buf), R"html(<tr><td>Animation interval (s):</td><td><input type="number" name="charge_anim_interval" value="%d" min="2" max="60"></td></tr>)html",
             Config::charge_anim_interval);
    webServer.sendContent(buf);

    snprintf(buf, sizeof(buf), R"html(<tr><td>Low threshold (%%):</td><td><input type="number" name="threshold_1" value="%d" min="0" max="99"></td></tr>)html",
             Config::threshold_1);
    webServer.sendContent(buf);

    snprintf(buf, sizeof(buf), R"html(<tr><td>High threshold (%%):</td><td><input type="number" name="threshold_2" value="%d" min="1" max="100"></td></tr>)html",
             Config::threshold_2);
    webServer.sendContent(buf);

    snprintf(buf, sizeof(buf), R"html(<tr><td>Base brightness (%d):</td><td><input type="range" name="base_brightness" value="%d" min="0" max="255" oninput="this.parentElement.previousElementSibling.textContent='Base brightness ('+this.value+'):';fetch('/api/brightness?v='+this.value)"></td></tr>)html",
             Config::base_brightness, Config::base_brightness);
    webServer.sendContent(buf);

    webServer.sendContent(R"html(</table><p style="font-size:0.85em;color:#888"><br><div style="text-align:center"><button class="btn" type="submit">Save</button></div></form><hr><p style="text-align:center"><a href="/">Back to status</a> &nbsp;|&nbsp; <a href="/reset" style="color:#E74C3C" onclick="return confirm('Reset WiFi settings and reboot into AP mode?')">Reset WiFi</a></p></body></html>)html");
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
    if (Config::vrm_enabled && Config::vrm_api_token[0] == '\0') {
        Config::vrm_enabled = false;
        Serial.println("VRM: disabled — API token is required");
    }
    if (webServer.hasArg("vrm_site_id"))
        Config::vrm_site_id = max(0, (int)webServer.arg("vrm_site_id").toInt());
    if (webServer.hasArg("vrm_battery_instance"))
        Config::vrm_battery_instance = max(-1, (int)webServer.arg("vrm_battery_instance").toInt());
    if (webServer.hasArg("vrm_charge_threshold")) {
        String v = webServer.arg("vrm_charge_threshold"); v.trim(); v.replace(',', '.');
        if (v.length() > 0 && v.length() <= 8) Config::vrm_charge_threshold = v.toFloat();
    }
    if (webServer.hasArg("vrm_discharge_threshold")) {
        String v = webServer.arg("vrm_discharge_threshold"); v.trim(); v.replace(',', '.');
        if (v.length() > 0 && v.length() <= 8) Config::vrm_discharge_threshold = v.toFloat();
    }
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
        int validLedPins[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 10, 20, 21};
        for (int vp : validLedPins) { if (p == vp) { Config::led_pin = p; break; } }
    }
    if (webServer.hasArg("pixel_color_order")) {
        uint8_t co = (uint8_t)webServer.arg("pixel_color_order").toInt();
        uint8_t validOrders[] = {6, 9, 82, 88, 54, 98};
        for (uint8_t v : validOrders) { if (co == v) { Config::pixel_color_order = co; break; } }
    }
    if (webServer.hasArg("pixel_khz")) {
        int k = webServer.arg("pixel_khz").toInt();
        if (k == 400 || k == 800) Config::pixel_khz = k;
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
    ws2812b.updateType((neoPixelType)(Config::pixel_color_order |
                       (Config::pixel_khz == 400 ? NEO_KHZ400 : NEO_KHZ800)));
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
    doc["animation"]      = animModeToStr(animMode);
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

    if (colorOverrideActive) {
        colorOverrideActive = false;
        publishColorState();
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
// handleApiColor — POST /api/color {"r":255,"g":0,"b":0} — fill strip with solid color
// ---------------------------------------------------------------------------

void handleApiColor() {
    if (!webServer.hasArg("plain") || webServer.arg("plain").length() == 0) {
        webServer.send(400, "application/json", "{\"error\":\"empty body\"}");
        return;
    }

    DynamicJsonDocument doc(128);
    if (deserializeJson(doc, webServer.arg("plain")) != DeserializationError::Ok) {
        webServer.send(400, "application/json", "{\"error\":\"invalid JSON\"}");
        return;
    }

    uint8_t r = (uint8_t)constrain((int)doc["r"], 0, 255);
    uint8_t g = (uint8_t)constrain((int)doc["g"], 0, 255);
    uint8_t b = (uint8_t)constrain((int)doc["b"], 0, 255);

    colorOverrideR      = r;
    colorOverrideG      = g;
    colorOverrideB      = b;
    colorOverrideActive = (r || g || b);
    // Setting a solid colour cancels any running animation
    animMode = ANIM_NONE;
    publishAnimationState();
    updateLEDs();
    publishColorState();

    char payload[64];
    snprintf(payload, sizeof(payload), "{\"r\":%d,\"g\":%d,\"b\":%d}", r, g, b);
    webServer.send(200, "application/json", payload);
}

// ---------------------------------------------------------------------------
// handleApiAnimation — POST /api/animation {"animation":"rainbow"} — set decorative mode
// ---------------------------------------------------------------------------

void handleApiAnimation() {
    if (!webServer.hasArg("plain") || webServer.arg("plain").length() == 0) {
        webServer.send(400, "application/json", "{\"error\":\"empty body\"}");
        return;
    }

    DynamicJsonDocument doc(128);
    if (deserializeJson(doc, webServer.arg("plain")) != DeserializationError::Ok) {
        webServer.send(400, "application/json", "{\"error\":\"invalid JSON\"}");
        return;
    }

    const char* animStr = doc["animation"].as<const char*>();
    if (animStr == nullptr) {
        webServer.send(400, "application/json", "{\"error\":\"missing animation field\"}");
        return;
    }

    AnimMode newMode = strToAnimMode(animStr);
    setAnimMode(newMode);
    publishAnimationState();

    char payload[64];
    snprintf(payload, sizeof(payload), "{\"animation\":\"%s\"}", animModeToStr(newMode));
    webServer.send(200, "application/json", payload);
}

// ---------------------------------------------------------------------------
// handleWebNotFound — 404 for unregistered routes
// ---------------------------------------------------------------------------

void handleWebNotFound() {
    webServer.send(404, "text/plain", "Not found: " + webServer.uri());
}
