// VRMClient.ino — Victron VRM API integration (personal access token)

#include <HTTPClient.h>
#include <WiFiClientSecure.h>

static int      vrmUserId          = 0;
static int      vrmSiteId          = 0;
static int      vrmBatteryInstance = -1; // -1 = not yet resolved
static uint32_t lastVrmPoll        = 0;
static bool     vrmReady           = false; // true once site + battery instance resolved

// ---------------------------------------------------------------------------
// vrmGetUserId — GET /v2/users/me to resolve user ID for installation lookup
// ---------------------------------------------------------------------------
bool vrmGetUserId() {
    WiFiClientSecure wifiClient;
    wifiClient.setInsecure();

    HTTPClient http;
    http.begin(wifiClient, "https://vrmapi.victronenergy.com/v2/users/me");
    http.addHeader("X-Authorization", String("Token ") + Config::vrm_api_token);

    int httpCode = http.GET();
    if (httpCode != 200) {
        Serial.printf("[VRM] /users/me failed, HTTP %d\n", httpCode);
        http.end();
        return false;
    }

    String body = http.getString();
    http.end();

    DynamicJsonDocument respDoc(512);
    DeserializationError err = deserializeJson(respDoc, body);
    if (err) {
        Serial.printf("[VRM] /users/me JSON parse error: %s\n", err.c_str());
        return false;
    }

    vrmUserId = respDoc["user"]["id"] | 0;
    if (vrmUserId == 0) {
        Serial.printf("[VRM] /users/me: missing user ID. Body: %s\n", body.c_str());
        return false;
    }

    Serial.printf("[VRM] Authenticated, userId=%d\n", vrmUserId);
    return true;
}

// ---------------------------------------------------------------------------
// vrmFetchSite — resolve the site ID to poll
// ---------------------------------------------------------------------------
bool vrmFetchSite() {
    if (Config::vrm_site_id > 0) {
        vrmSiteId = Config::vrm_site_id;
        Serial.printf("[VRM] Using configured site ID %d\n", vrmSiteId);
        return true;
    }

    if (vrmUserId == 0) {
        if (!vrmGetUserId()) return false;
    }

    WiFiClientSecure wifiClient;
    wifiClient.setInsecure();

    char url[80];
    snprintf(url, sizeof(url),
             "https://vrmapi.victronenergy.com/v2/users/%d/installations",
             vrmUserId);

    HTTPClient http;
    http.begin(wifiClient, url);
    http.addHeader("X-Authorization", String("Token ") + Config::vrm_api_token);

    int httpCode = http.GET();
    if (httpCode != 200) {
        Serial.printf("[VRM] Fetch installations failed, HTTP %d\n", httpCode);
        http.end();
        return false;
    }

    String body = http.getString();
    http.end();

    DynamicJsonDocument respDoc(1024);
    DeserializationError err = deserializeJson(respDoc, body);
    if (err) {
        Serial.printf("[VRM] Installations JSON parse error: %s\n", err.c_str());
        Serial.printf("[VRM] Body: %.200s\n", body.c_str());
        return false;
    }

    JsonArray records = respDoc["records"].as<JsonArray>();
    if (!records || records.size() == 0) {
        Serial.println("[VRM] No installations found");
        return false;
    }

    vrmSiteId = records[0]["idSite"] | 0;
    const char* siteName = records[0]["name"] | "(unknown)";
    Serial.printf("[VRM] Using site \"%s\" (ID %d)\n", siteName, vrmSiteId);
    return (vrmSiteId != 0);
}

// ---------------------------------------------------------------------------
// vrmFetchBatteryInstance — resolve battery device instance (once only)
// ---------------------------------------------------------------------------
bool vrmFetchBatteryInstance() {
    if (Config::vrm_battery_instance >= 0) {
        vrmBatteryInstance = Config::vrm_battery_instance;
        Serial.printf("[VRM] Using configured battery instance %d\n", vrmBatteryInstance);
        return true;
    }

    WiFiClientSecure wifiClient;
    wifiClient.setInsecure();

    char url[96];
    snprintf(url, sizeof(url),
             "https://vrmapi.victronenergy.com/v2/installations/%d/system-overview",
             vrmSiteId);

    HTTPClient http;
    http.begin(wifiClient, url);
    http.addHeader("X-Authorization", String("Token ") + Config::vrm_api_token);

    int httpCode = http.GET();
    if (httpCode != 200) {
        Serial.printf("[VRM] system-overview failed, HTTP %d\n", httpCode);
        http.end();
        return false;
    }

    String body = http.getString();
    http.end();

    DynamicJsonDocument respDoc(4096);
    DeserializationError err = deserializeJson(respDoc, body);
    if (err) {
        Serial.printf("[VRM] system-overview JSON parse error: %s\n", err.c_str());
        return false;
    }

    // Walk devices array looking for first "Battery Monitor"
    JsonArray devices = respDoc["records"]["devices"].as<JsonArray>();
    for (JsonObject dev : devices) {
        const char* devName = dev["name"] | "";
        int         inst    = dev["instance"] | -1;

        if (strstr(devName, "Battery Monitor") != nullptr && inst >= 0) {
            vrmBatteryInstance = inst;
            Serial.printf("[VRM] Battery Monitor found, instance=%d\n", inst);
            return true;
        }
    }

    Serial.println("[VRM] No Battery Monitor found in system-overview");
    return false;
}

// ---------------------------------------------------------------------------
// vrmPoll — called from loop(); fetches BatterySummary and updates LED state
// ---------------------------------------------------------------------------
void vrmPoll() {
    if (!Config::vrm_enabled) return;
    if (WiFi.status() != WL_CONNECTED) return;
    if (Config::vrm_api_token[0] == '\0') return;

    uint32_t now = millis();
    if (lastVrmPoll != 0 && now - lastVrmPoll < (uint32_t)Config::vrm_interval * 1000) return;
    lastVrmPoll = now;

    if (!vrmReady) {
        if (!vrmFetchSite())            return;
        if (!vrmFetchBatteryInstance()) return;
        vrmReady = true;
    }

    WiFiClientSecure wifiClient;
    wifiClient.setInsecure();

    char url[128];
    snprintf(url, sizeof(url),
             "https://vrmapi.victronenergy.com/v2/installations/%d/widgets/BatterySummary?instance=%d",
             vrmSiteId, vrmBatteryInstance);

    HTTPClient http;
    http.begin(wifiClient, url);
    http.useHTTP10(true);
    http.addHeader("X-Authorization", String("Token ") + Config::vrm_api_token);

    int httpCode = http.GET();

    if (httpCode == 401) {
        Serial.println("[VRM] Token rejected (401), check API token in config");
        vrmReady    = false;
        lastVrmPoll = 0;
        http.end();
        return;
    }

    if (httpCode != 200) {
        Serial.printf("[VRM] BatterySummary failed, HTTP %d\n", httpCode);
        http.end();
        return;
    }

    String body = http.getString();
    http.end();

    DynamicJsonDocument resp(4096);
    DeserializationError err = deserializeJson(resp, body);
    if (err) {
        Serial.printf("[VRM] BatterySummary JSON parse error: %s\n", err.c_str());
        return;
    }

    float reportedSoc     = -1.0f;
    float reportedCurrent = 0.0f;

    JsonObject data = resp["records"]["data"].as<JsonObject>();
    for (JsonPair kv : data) {
        const char* code = kv.value()["code"] | "";
        float val        = kv.value()["valueFloat"] | -999.0f;
        if (val <= -999.0f) continue; // invalid / no data

        if (strcmp(code, "SOC") == 0) {
            reportedSoc = val;
            ledPercent  = (int)constrain(val, 0.0f, 100.0f);
            ledState    = (ledPercent > 0);
        } else if (strcmp(code, "I") == 0) {
            reportedCurrent = val;
            // Victron BMV: positive current = charging, negative = discharging
            if (val > Config::vrm_charge_threshold)          chargingState = STATE_CHARGING;
            else if (val < Config::vrm_discharge_threshold)  chargingState = STATE_DISCHARGING;
            else                                             chargingState = STATE_IDLE;
        }
    }

    Serial.printf("[VRM] SOC=%.1f%% current=%.2fA\n", reportedSoc, reportedCurrent);

    updateLEDs();
    publishState();
}

// ---------------------------------------------------------------------------
// vrmReset — call when VRM settings are changed via config save
// ---------------------------------------------------------------------------
void vrmReset() {
    vrmUserId          = 0;
    vrmSiteId          = 0;
    vrmBatteryInstance = -1;
    vrmReady           = false;
    lastVrmPoll        = 0;
}
