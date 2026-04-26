#pragma once

#include <ArduinoJson.h>
#include <FS.h>

enum VictronSource { VICTRON_NONE = 0, VICTRON_VRM = 1, VICTRON_MODBUS = 2 };

namespace Config {
    char mqtt_server[80]    = "example.tld";
    char mqtt_username[24]  = "";
    char mqtt_password[24]  = "";
    char mqtt_discovery_prefix[32] = "homeassistant";
    bool mqtt_enabled           = false;
    bool mqtt_secure            = false;
    int  mqtt_port              = 1883;

    VictronSource victron_source    = VICTRON_VRM;
    char vrm_api_token[128]         = "";
    int  vrm_site_id                = 0;    // 0 = auto-select first installation
    char modbus_host[64]            = "";
    int   vrm_battery_instance      = -1;   // VRM: auto; Modbus: unit ID (0 = first battery)
    float vrm_discharge_threshold   = -0.5f;
    float vrm_charge_threshold      =  0.5f;
    int   vrm_interval              = 60;   // seconds between polls (VRM and Modbus)

    int  threshold_1        = 20;
    int  threshold_2        = 60;

    int  fade_pixels        = 3;
    int  num_pixels         = 38;
    int  led_pin            = 4;
    uint8_t pixel_color_order = 9;  // NEO_RBG=9, NEO_GRB=82, NEO_RGB=6, NEO_BRG=88, NEO_BGR=54, NEO_GBR=98
    int  pixel_khz          = 800;
    uint8_t middle_r        = 0;
    uint8_t middle_g        = 0;
    uint8_t middle_b        = 255;
    bool charge_anim          = true;
    int  charge_anim_interval = 10;
    uint8_t base_brightness   = 128;

    void save() {
        DynamicJsonDocument json(1024);
        json["mqtt_server"]    = mqtt_server;
        json["mqtt_username"]  = mqtt_username;
        json["mqtt_password"]  = mqtt_password;
        json["mqtt_discovery_prefix"] = mqtt_discovery_prefix;
        json["mqtt_enabled"]   = mqtt_enabled;
        json["mqtt_secure"]    = mqtt_secure;
        json["mqtt_port"]      = mqtt_port;

        json["victron_source"]          = (int)victron_source;
        json["vrm_api_token"]           = vrm_api_token;
        json["vrm_site_id"]             = vrm_site_id;
        json["modbus_host"]             = modbus_host;
        json["vrm_battery_instance"]    = vrm_battery_instance;
        json["vrm_discharge_threshold"] = vrm_discharge_threshold;
        json["vrm_charge_threshold"]    = vrm_charge_threshold;
        json["vrm_interval"]            = vrm_interval;

        json["threshold_1"]    = threshold_1;
        json["threshold_2"]    = threshold_2;

        json["fade_pixels"]    = fade_pixels;
        json["num_pixels"]     = num_pixels;
        json["led_pin"]        = led_pin;
        json["pixel_color_order"] = pixel_color_order;
        json["pixel_khz"]        = pixel_khz;
        json["middle_r"]       = middle_r;
        json["middle_g"]       = middle_g;
        json["middle_b"]       = middle_b;
        json["charge_anim"]          = charge_anim;
        json["charge_anim_interval"] = charge_anim_interval;
        json["base_brightness"]      = base_brightness;

        File configFile = SPIFFS.open("/config.json", "w");
        if (!configFile) {
            return;
        }

        serializeJson(json, configFile);
        configFile.close();
    }

    void load() {
        if (SPIFFS.exists("/config.json")) {
            File configFile = SPIFFS.open("/config.json", "r");

            if (configFile) {
                const size_t size = configFile.size();
                std::unique_ptr<char[]> buf(new char[size + 1]);

                configFile.readBytes(buf.get(), size);
                buf.get()[size] = '\0';
                DynamicJsonDocument json(1024);

                if (DeserializationError::Ok == deserializeJson(json, buf.get())) {
                    strlcpy(mqtt_server,   json["mqtt_server"]   | "", sizeof(mqtt_server));
                    strlcpy(mqtt_username, json["mqtt_username"] | "", sizeof(mqtt_username));
                    strlcpy(mqtt_password, json["mqtt_password"] | "", sizeof(mqtt_password));
                    strlcpy(mqtt_discovery_prefix, json["mqtt_discovery_prefix"] | "homeassistant", sizeof(mqtt_discovery_prefix));
                    mqtt_enabled    = json["mqtt_enabled"]     | false;
                    mqtt_secure     = json["mqtt_secure"]      | false;
                    mqtt_port       = json["mqtt_port"]        | 1883;

                    // Migrate from old vrm_enabled bool if present
                    if (json.containsKey("victron_source")) {
                        victron_source = (VictronSource)(json["victron_source"] | (int)VICTRON_VRM);
                    } else if (json["vrm_enabled"] | false) {
                        victron_source = VICTRON_VRM;
                    } else if (json["modbus_enabled"] | false) {
                        victron_source = VICTRON_MODBUS;
                    } else {
                        victron_source = VICTRON_NONE;
                    }

                    strlcpy(vrm_api_token, json["vrm_api_token"] | "", sizeof(vrm_api_token));
                    vrm_site_id             = json["vrm_site_id"]             | 0;
                    strlcpy(modbus_host, json["modbus_host"] | "", sizeof(modbus_host));
                    vrm_battery_instance    = json["vrm_battery_instance"]    | -1;
                    vrm_discharge_threshold = json["vrm_discharge_threshold"] | -0.5f;
                    vrm_charge_threshold    = json["vrm_charge_threshold"]    |  0.5f;
                    vrm_interval            = constrain(json["vrm_interval"] | 60, 10, 3600);

                    // Clear source if required credential is missing
                    if (victron_source == VICTRON_VRM    && vrm_api_token[0] == '\0') victron_source = VICTRON_NONE;
                    if (victron_source == VICTRON_MODBUS && modbus_host[0]   == '\0') victron_source = VICTRON_NONE;

                    threshold_1     = json["threshold_1"]     | 20;
                    threshold_2     = json["threshold_2"]     | 60;

                    fade_pixels     = json["fade_pixels"]     | 3;
                    num_pixels      = json["num_pixels"]      | 38;
                    led_pin         = json["led_pin"]         | 4;
                    pixel_color_order = (uint8_t)(json["pixel_color_order"] | 9);
                    pixel_khz         = json["pixel_khz"]         | 800;
                    middle_r        = (uint8_t)(json["middle_r"]        | 0);
                    middle_g        = (uint8_t)(json["middle_g"]        | 0);
                    middle_b        = (uint8_t)(json["middle_b"]        | 255);
                    charge_anim          = json["charge_anim"]          | true;
                    charge_anim_interval = json["charge_anim_interval"] | 10;
                    base_brightness      = (uint8_t)(json["base_brightness"] | 128);
                }
            }
        }
    }
} // namespace Config
