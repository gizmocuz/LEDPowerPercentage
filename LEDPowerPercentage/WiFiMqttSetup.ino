void saveConfigCallback() {
    shouldSaveConfig = true;
}

// ---------------------------------------------------------------------------

void setupWifi() {
    wifiManager.setDebugOutput(false);
    wifiManager.setSaveConfigCallback(saveConfigCallback);
    wifiManager.setConfigPortalBlocking(false);
    wifiManager.setConnectTimeout(60);      // retry saved WiFi for 60 s before opening portal
    wifiManager.setConfigPortalTimeout(180); // reboot automatically if portal unused for 3 min

    WiFi.setHostname(identifier);

    Serial.println("WiFi: connecting...");
    if (!wifiManager.autoConnect(identifier)) {
        // Saved credentials failed — portal is now open
        Serial.println("WiFi: AP portal open, waiting for credentials...");
        while (WiFi.status() != WL_CONNECTED) {
            wifiManager.process();
            showAPModeAnimation();
            delay(20);
        }
    }

    // WiFi is connected at this point
    Serial.printf("WiFi: connected.  IP: %s  RSSI: %d dBm\n",
                  WiFi.localIP().toString().c_str(), WiFi.RSSI());
    blinkConnected();
    if (Config::mqtt_enabled) {
        mqttClient.setCallback(mqttCallback);
    }

    webServer.on("/",        handleWebRoot);
    webServer.on("/set",     handleWebSet);
    webServer.on("/config",  HTTP_GET,  handleWebConfig);
    webServer.on("/config",  HTTP_POST, handleWebConfigSave);
    webServer.on("/reset",   handleWebReset);
    webServer.on("/api/state",      HTTP_GET,  handleApiStateGet);
    webServer.on("/api/state",      HTTP_POST, handleApiStatePost);
    webServer.on("/api/brightness", HTTP_GET,  handleApiBrightness);
    webServer.on("/api/color",      HTTP_POST, handleApiColor);
    webServer.onNotFound(handleWebNotFound);
    webServer.begin();

    if (shouldSaveConfig) {
        Config::save();
    }
}

// ---------------------------------------------------------------------------

void setupOTA() {
    ArduinoOTA.onStart([]() {
        Serial.println("OTA: start");
    });

    ArduinoOTA.onEnd([]() {
        Serial.println("\nOTA: end");
    });

    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
        Serial.printf("OTA progress: %u%%\r", (progress / (total / 100)));
    });

    ArduinoOTA.onError([](ota_error_t error) {
        Serial.printf("OTA error[%u]: ", error);
        if      (error == OTA_AUTH_ERROR)    Serial.println("Auth Failed");
        else if (error == OTA_BEGIN_ERROR)   Serial.println("Begin Failed");
        else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
        else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
        else if (error == OTA_END_ERROR)     Serial.println("End Failed");
    });

    ArduinoOTA.setHostname(identifier);
    ArduinoOTA.setPassword(identifier);
    ArduinoOTA.begin();
}

// ---------------------------------------------------------------------------

void mqttReconnect() {
    if (!Config::mqtt_enabled) return;
    if (Config::mqtt_secure) {
        wifiClientSecure.setInsecure();
        mqttClient.setClient(wifiClientSecure);
    } else {
        mqttClient.setClient(wifiClient);
    }
    mqttClient.setServer(Config::mqtt_server, Config::mqtt_port);
    Serial.printf("MQTT: connecting to %s:%d (%s)...\n",
                  Config::mqtt_server, Config::mqtt_port,
                  Config::mqtt_secure ? "TLS" : "plain");
    for (uint8_t attempt = 0; attempt < 3; ++attempt) {
        if (mqttClient.connect(identifier,
                               Config::mqtt_username,
                               Config::mqtt_password,
                               MQTT_TOPIC_AVAILABILITY, 1, false, AVAILABILITY_OFFLINE)) {
            Serial.println("MQTT: connected.");
            mqttClient.publish(MQTT_TOPIC_AVAILABILITY, AVAILABILITY_ONLINE, false);
            publishAutoConfig();
            publishState();
            mqttClient.subscribe(MQTT_TOPIC_COMMAND);
            mqttClient.subscribe(MQTT_TOPIC_CHARGING_SET);
            return;
        }

        Serial.printf("MQTT: attempt %d failed (state=%d), retrying in 5s...\n",
                      attempt + 1, mqttClient.state());
        uint32_t retryStart = millis();
        while (millis() - retryStart < 5000) {
            showConnectingAnimation();
            yield();
            delay(20);
        }
    }
    Serial.println("MQTT: all attempts failed, will retry in 60s.");
    updateLEDs(); // clear connecting animation from strip
}

// ---------------------------------------------------------------------------

void resetWifiSettingsAndReboot() {
    wifiManager.resetSettings();
    delay(3000);
    ESP.restart();
}
