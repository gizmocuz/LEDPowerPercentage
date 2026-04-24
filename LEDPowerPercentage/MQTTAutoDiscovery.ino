// MQTTAutoDiscovery.ino — MQTT state publishing and Home Assistant auto-discovery

void publishState() {
    if (!Config::mqtt_enabled) return;
    DynamicJsonDocument doc(256);
    char payload[256];

    doc["state"]      = ledState ? "ON" : "OFF";
    doc["brightness"] = ledPercent;

    const char* csStr = "idle";
    if (chargingState == STATE_CHARGING)    csStr = "charging";
    if (chargingState == STATE_DISCHARGING) csStr = "discharging";
    doc["charging_state"] = csStr;

    serializeJson(doc, payload);
    mqttClient.publish(MQTT_TOPIC_STATE, payload, false);
}

// ---------------------------------------------------------------------------

void publishAutoConfig() {
    if (!Config::mqtt_enabled) return;
    char mqttPayload[1024];
    DynamicJsonDocument autoconfPayload(1024);
    DynamicJsonDocument device(256);
    StaticJsonDocument<64> identifiersDoc;
    JsonArray identifiers = identifiersDoc.to<JsonArray>();

    identifiers.add(identifier);

    device["identifiers"]  = identifiers;
    device["manufacturer"] = "PA1DVB";
    device["model"]        = "LEDPOWER";
    device["name"]         = identifier;
    device["sw_version"]   = app_version;

    // --- Payload 1: Light entity ---
    autoconfPayload["name"]              = String(identifier);
    autoconfPayload["unique_id"]         = String(identifier) + "_light";
    autoconfPayload["schema"]            = "json";
    autoconfPayload["device"]            = device.as<JsonObject>();
    autoconfPayload["availability_topic"] = MQTT_TOPIC_AVAILABILITY;
    autoconfPayload["state_topic"]       = MQTT_TOPIC_STATE;
    autoconfPayload["command_topic"]     = MQTT_TOPIC_COMMAND;
    autoconfPayload["brightness"]        = true;
    autoconfPayload["brightness_scale"]  = 100;
    autoconfPayload["icon"]              = "mdi:led-strip-variant";

    serializeJson(autoconfPayload, mqttPayload);
    mqttClient.publish(MQTT_TOPIC_AUTOCONF_LIGHT, mqttPayload, true);

    autoconfPayload.clear();

    // --- Payload 2: Charging state select entity ---
    autoconfPayload["name"]               = String(identifier) + " Charging State";
    autoconfPayload["unique_id"]          = String(identifier) + "_charging_state";
    autoconfPayload["device"]             = device.as<JsonObject>();
    autoconfPayload["availability_topic"] = MQTT_TOPIC_AVAILABILITY;
    autoconfPayload["state_topic"]        = MQTT_TOPIC_STATE;
    autoconfPayload["value_template"]     = "{{ value_json.charging_state }}";
    autoconfPayload["command_topic"]      = MQTT_TOPIC_CHARGING_SET;
    autoconfPayload["options"][0]         = "idle";
    autoconfPayload["options"][1]         = "charging";
    autoconfPayload["options"][2]         = "discharging";
    autoconfPayload["icon"]               = "mdi:battery-charging";

    serializeJson(autoconfPayload, mqttPayload);
    mqttClient.publish(MQTT_TOPIC_AUTOCONF_CHARGE, mqttPayload, true);
}

// ---------------------------------------------------------------------------

void mqttCallback(char* topic, uint8_t* payload, unsigned int length) {
    if (strcmp(topic, MQTT_TOPIC_COMMAND) == 0) {
        DynamicJsonDocument doc(256);
        if (deserializeJson(doc, payload, length) != DeserializationError::Ok) {
            Serial.println("MQTT: failed to parse command JSON");
            return;
        }
        if (doc.containsKey("state")) {
            const char* stateStr = doc["state"].as<const char*>();
            if (stateStr != nullptr) {
                ledState = strcmp(stateStr, "ON") == 0;
            }
        }
        if (doc.containsKey("brightness")) {
            ledPercent = constrain((int)doc["brightness"], 0, 100);
            ledState   = (ledPercent > 0);
        }
        updateLEDs();
        publishState();
    } else if (strcmp(topic, MQTT_TOPIC_CHARGING_SET) == 0) {
        char msg[16] = {0};
        memcpy(msg, payload, min((unsigned int)15, length));
        if (strcmp(msg, "charging") == 0)         chargingState = STATE_CHARGING;
        else if (strcmp(msg, "discharging") == 0) chargingState = STATE_DISCHARGING;
        else                                       chargingState = STATE_IDLE;
        publishState();
    }
}
