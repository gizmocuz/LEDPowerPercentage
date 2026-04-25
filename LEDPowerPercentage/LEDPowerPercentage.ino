// ============================================================
// LEDPowerPercentage — ESP32-C3 Super Mini
// Main sketch file: includes, defines, globals, setup, loop
//
// (c) PA1DVB
// ============================================================

#include <Adafruit_NeoPixel.h>
#include <ArduinoJson.h>
#include <ArduinoOTA.h>
#include <DNSServer.h>
#include <PubSubClient.h>
#include <WiFiClientSecure.h>
#include <WiFiManager.h>
#include <WebServer.h>
#include <SPIFFS.h>
#include "Config.h"

// --- Hardware bootstrap defaults (overridden at runtime by Config::led_pin / Config::num_pixels) ---
#define PIN_WS2812B   4
#define NUM_PIXELS    38

// Optional potentiometer (uncomment to enable in a future version)
// #define POT_PIN    2     // GPIO2: ADC pin for brightness potmeter

// --- Firmware identity ---
#define FIRMWARE_PREFIX      "esp32-ledpower"
#define app_version          "2026.04.24 rev 1.0"
#define AVAILABILITY_ONLINE  "online"
#define AVAILABILITY_OFFLINE "offline"

// --- Charging state ---
enum ChargingState { STATE_IDLE, STATE_CHARGING, STATE_DISCHARGING };

// --- Decorative animation mode ---
enum AnimMode { ANIM_NONE = 0, ANIM_RAINBOW, ANIM_FIRE, ANIM_METEOR, ANIM_TWINKLE, ANIM_BREATHE,
                ANIM_LAVA, ANIM_WATERFALL, ANIM_GRADIENT, ANIM_PULSE, ANIM_RAIN, ANIM_STARFIELD,
                ANIM_NOTIFICATION };

// --- Device identifier (built from MAC address in setup) ---
char identifier[24];

// --- MQTT topic buffers ---
char MQTT_TOPIC_AVAILABILITY[128];
char MQTT_TOPIC_STATE[128];
char MQTT_TOPIC_COMMAND[128];
char MQTT_TOPIC_CHARGING_SET[128];
char MQTT_TOPIC_COLOR_COMMAND[128];
char MQTT_TOPIC_COLOR_STATE[128];
char MQTT_TOPIC_AUTOCONF_LIGHT[128];
char MQTT_TOPIC_AUTOCONF_CHARGE[128];
char MQTT_TOPIC_AUTOCONF_COLOR[128];
char MQTT_TOPIC_ANIMATION_SET[128];
char MQTT_TOPIC_ANIMATION_STATE[128];
char MQTT_TOPIC_AUTOCONF_ANIMATION[128];

// --- Hardware objects ---
Adafruit_NeoPixel ws2812b(NUM_PIXELS, PIN_WS2812B, NEO_RBG + NEO_KHZ800);
WiFiClient        wifiClient;
WiFiClientSecure  wifiClientSecure;
PubSubClient      mqttClient;
WebServer         webServer(80);
WiFiManager       wifiManager;

// --- State ---
bool     ledState                  = false;
int      ledPercent                = 0;
ChargingState chargingState        = STATE_IDLE;
uint8_t  currentBrightness         = 128;
bool     shouldSaveConfig          = false;
uint32_t lastMqttConnectionAttempt = 0;

// --- Color override state (solid-color display mode via MQTT or REST) ---
bool    colorOverrideActive = false;
uint8_t colorOverrideR      = 0;
uint8_t colorOverrideG      = 0;
uint8_t colorOverrideB      = 0;

// --- Active decorative animation ---
AnimMode animMode = ANIM_NONE;

// ============================================================
// setup
// ============================================================
void setup() {
    Serial.begin(115200);

    // Load config before LED init so the strip starts on the correct pin/length
    if (!SPIFFS.begin(false)) {
        Serial.println("SPIFFS mount failed — formatting (config will reset to defaults)");
        SPIFFS.format();
        SPIFFS.begin(false);
    }
    Config::load();
    currentBrightness = Config::base_brightness;

    // Single begin() with the correct pin, length and type already set
    ws2812b.updateLength(Config::num_pixels);
    ws2812b.setPin(Config::led_pin);
    ws2812b.updateType((neoPixelType)(Config::pixel_color_order |
                       (Config::pixel_khz == 400 ? NEO_KHZ400 : NEO_KHZ800)));
    ws2812b.begin();
    ws2812b.clear();
    ws2812b.show();

    // Build device identifier from MAC — WiFi.mode() required before macAddress() on ESP32
    WiFi.mode(WIFI_STA);
    uint8_t mac[6];
    WiFi.macAddress(mac);
    snprintf(identifier, sizeof(identifier), "LEDPOWER-%02X%02X%02X%02X%02X%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    Serial.printf("App version:  %s\n", app_version);
    Serial.printf("LED pin:      GPIO%d,  pixels: %d\n", Config::led_pin, Config::num_pixels);
    Serial.printf("CPU freq:     %u MHz\n", ESP.getCpuFreqMHz());
    Serial.printf("Device ID:    %s\n", identifier);

    // Build all MQTT topic strings
    snprintf(MQTT_TOPIC_AVAILABILITY,    sizeof(MQTT_TOPIC_AVAILABILITY)    - 1, "%s/%s/status",       FIRMWARE_PREFIX, identifier);
    snprintf(MQTT_TOPIC_STATE,           sizeof(MQTT_TOPIC_STATE)           - 1, "%s/%s/state",        FIRMWARE_PREFIX, identifier);
    snprintf(MQTT_TOPIC_COMMAND,         sizeof(MQTT_TOPIC_COMMAND)         - 1, "%s/%s/command",      FIRMWARE_PREFIX, identifier);
    snprintf(MQTT_TOPIC_CHARGING_SET,    sizeof(MQTT_TOPIC_CHARGING_SET)    - 1, "%s/%s/charging_state/set", FIRMWARE_PREFIX, identifier);
    snprintf(MQTT_TOPIC_AUTOCONF_LIGHT,  sizeof(MQTT_TOPIC_AUTOCONF_LIGHT)  - 1, "%s/light/%s/%s/config",
             Config::mqtt_discovery_prefix, FIRMWARE_PREFIX, identifier);
    snprintf(MQTT_TOPIC_AUTOCONF_CHARGE, sizeof(MQTT_TOPIC_AUTOCONF_CHARGE) - 1, "%s/select/%s_%s_charging_state/config",
             Config::mqtt_discovery_prefix, FIRMWARE_PREFIX, identifier);
    snprintf(MQTT_TOPIC_COLOR_COMMAND,   sizeof(MQTT_TOPIC_COLOR_COMMAND)   - 1, "%s/%s/color/command",  FIRMWARE_PREFIX, identifier);
    snprintf(MQTT_TOPIC_COLOR_STATE,     sizeof(MQTT_TOPIC_COLOR_STATE)     - 1, "%s/%s/color/state",    FIRMWARE_PREFIX, identifier);
    snprintf(MQTT_TOPIC_AUTOCONF_COLOR,  sizeof(MQTT_TOPIC_AUTOCONF_COLOR)  - 1, "%s/light/%s_%s_color/config",
             Config::mqtt_discovery_prefix, FIRMWARE_PREFIX, identifier);
    snprintf(MQTT_TOPIC_ANIMATION_SET,   sizeof(MQTT_TOPIC_ANIMATION_SET)   - 1, "%s/%s/animation/set",   FIRMWARE_PREFIX, identifier);
    snprintf(MQTT_TOPIC_ANIMATION_STATE, sizeof(MQTT_TOPIC_ANIMATION_STATE) - 1, "%s/%s/animation/state", FIRMWARE_PREFIX, identifier);
    snprintf(MQTT_TOPIC_AUTOCONF_ANIMATION, sizeof(MQTT_TOPIC_AUTOCONF_ANIMATION) - 1, "%s/select/%s_%s_animation/config",
             Config::mqtt_discovery_prefix, FIRMWARE_PREFIX, identifier);

    Serial.printf("MQTT availability: %s\n", MQTT_TOPIC_AVAILABILITY);

    // Connect WiFi (shows AP or connecting animation)
    setupWifi();
    Serial.printf("WiFi connected. IP: %s\n", WiFi.localIP().toString().c_str());

    // Belt-and-suspenders: ensure keep-alive and buffer size are set
    mqttClient.setKeepAlive(10);
    mqttClient.setBufferSize(2048);

    // OTA
    setupOTA();

    // First MQTT connect — shows connecting animation on strip
    if (Config::mqtt_enabled) mqttReconnect();

    // Show initial LED state
    updateLEDs();

    Serial.println("Setup complete.");
}

// ============================================================
// loop
// ============================================================
void loop() {
    ArduinoOTA.handle();
    mqttClient.loop();
    webServer.handleClient();
    handleAnimation();
    handleStateAnimation();
    vrmPoll();

    const uint32_t now = millis();
    if (Config::mqtt_enabled && !mqttClient.connected() && now - lastMqttConnectionAttempt >= 60000) {
        lastMqttConnectionAttempt = now;
        mqttReconnect();
    }
}
