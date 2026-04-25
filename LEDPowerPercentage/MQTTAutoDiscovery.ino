// MQTTAutoDiscovery.ino — MQTT state publishing and Home Assistant auto-discovery

void publishAnimationState() {
    if (!Config::mqtt_enabled) return;
    mqttClient.publish(MQTT_TOPIC_ANIMATION_STATE, animModeToStr(animMode), false);
}

// ---------------------------------------------------------------------------

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

void publishColorState() {
    if (!Config::mqtt_enabled) return;
    DynamicJsonDocument doc(128);
    char payload[128];
    doc["state"] = colorOverrideActive ? "ON" : "OFF";
    JsonObject color = doc.createNestedObject("color");
    color["r"] = colorOverrideR;
    color["g"] = colorOverrideG;
    color["b"] = colorOverrideB;
    serializeJson(doc, payload);
    mqttClient.publish(MQTT_TOPIC_COLOR_STATE, payload, false);
}

// ---------------------------------------------------------------------------

void publishAutoConfig() {
    if (!Config::mqtt_enabled) return;
    char mqttPayload[3072];
    DynamicJsonDocument autoconfPayload(3072);
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

    serializeJson(autoconfPayload, mqttPayload, sizeof(mqttPayload));
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

    serializeJson(autoconfPayload, mqttPayload, sizeof(mqttPayload));
    mqttClient.publish(MQTT_TOPIC_AUTOCONF_CHARGE, mqttPayload, true);

    autoconfPayload.clear();

    // --- Payload 3: RGB color light entity ---
    autoconfPayload["name"]               = String(identifier) + " Color";
    autoconfPayload["unique_id"]          = String(identifier) + "_color";
    autoconfPayload["schema"]             = "json";
    autoconfPayload["device"]             = device.as<JsonObject>();
    autoconfPayload["availability_topic"] = MQTT_TOPIC_AVAILABILITY;
    autoconfPayload["state_topic"]        = MQTT_TOPIC_COLOR_STATE;
    autoconfPayload["command_topic"]      = MQTT_TOPIC_COLOR_COMMAND;
    autoconfPayload["supported_color_modes"][0] = "rgb";
    autoconfPayload["icon"]               = "mdi:palette";

    serializeJson(autoconfPayload, mqttPayload, sizeof(mqttPayload));
    mqttClient.publish(MQTT_TOPIC_AUTOCONF_COLOR, mqttPayload, true);

    autoconfPayload.clear();

    // --- Payload 4: Animation select entity ---
    autoconfPayload["name"]               = String(identifier) + " Animation";
    autoconfPayload["unique_id"]          = String(identifier) + "_animation";
    autoconfPayload["device"]             = device.as<JsonObject>();
    autoconfPayload["availability_topic"] = MQTT_TOPIC_AVAILABILITY;
    autoconfPayload["state_topic"]        = MQTT_TOPIC_ANIMATION_STATE;
    autoconfPayload["command_topic"]      = MQTT_TOPIC_ANIMATION_SET;
    autoconfPayload["options"][0]          = "none";
    autoconfPayload["options"][1]          = "rainbow";
    autoconfPayload["options"][2]          = "fire";
    autoconfPayload["options"][3]          = "meteor";
    autoconfPayload["options"][4]          = "twinkle";
    autoconfPayload["options"][5]          = "breathe";
    autoconfPayload["options"][6]          = "lava";
    autoconfPayload["options"][7]          = "waterfall";
    autoconfPayload["options"][8]          = "gradient";
    autoconfPayload["options"][9]          = "pulse";
    autoconfPayload["options"][10]         = "rain";
    autoconfPayload["options"][11]         = "starfield";
    autoconfPayload["options"][12]         = "notification";
    autoconfPayload["icon"]               = "mdi:animation-play";

    serializeJson(autoconfPayload, mqttPayload, sizeof(mqttPayload));
    mqttClient.publish(MQTT_TOPIC_AUTOCONF_ANIMATION, mqttPayload, true);
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
        unsigned int copyLen = min((unsigned int)15, length);
        memcpy(msg, payload, copyLen);
        msg[copyLen] = '\0';
        if (strcmp(msg, "charging") == 0)         chargingState = STATE_CHARGING;
        else if (strcmp(msg, "discharging") == 0) chargingState = STATE_DISCHARGING;
        else                                       chargingState = STATE_IDLE;
        publishState();
    } else if (strcmp(topic, MQTT_TOPIC_COLOR_COMMAND) == 0) {
        DynamicJsonDocument doc(256);
        if (deserializeJson(doc, payload, length) != DeserializationError::Ok) {
            Serial.println("MQTT: failed to parse color command JSON");
            return;
        }
        bool turnOff = false;
        if (doc.containsKey("state")) {
            const char* s = doc["state"].as<const char*>();
            if (s && strcmp(s, "OFF") == 0) turnOff = true;
        }
        if (turnOff) {
            colorOverrideActive = false;
        } else if (doc.containsKey("color")) {
            colorOverrideR      = (uint8_t)constrain((int)doc["color"]["r"], 0, 255);
            colorOverrideG      = (uint8_t)constrain((int)doc["color"]["g"], 0, 255);
            colorOverrideB      = (uint8_t)constrain((int)doc["color"]["b"], 0, 255);
            colorOverrideActive = true;
            // Setting a solid colour cancels any running animation
            animMode = ANIM_NONE;
            publishAnimationState();
        }
        updateLEDs();
        publishColorState();
    } else if (strcmp(topic, MQTT_TOPIC_ANIMATION_SET) == 0) {
        char msg[32] = {0};
        unsigned int copyLen = min((unsigned int)31, length);
        memcpy(msg, payload, copyLen);
        msg[copyLen] = '\0';
        setAnimMode(strToAnimMode(msg));
        publishAnimationState();
    }
}
