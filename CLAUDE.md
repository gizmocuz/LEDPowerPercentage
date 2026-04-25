# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

Arduino firmware for an **ESP32-C3 Super Mini** that drives a WS2812B/WS2811 LED strip as a battery/power percentage indicator. The percentage comes from MQTT, the Victron VRM Portal, or the built-in REST API.

## Building and Uploading

**IDE:** Arduino IDE with the ESP32 Arduino core installed.

**Board settings:**
- Board: `ESP32C3 Dev Module`
- Partition Scheme: **Minimal SPIFFS (1.9 MB APP with OTA / 190 KB SPIFFS)** — required because `WiFiClientSecure` (used for VRM) adds ~400 KB; the default 1.25 MB partition is too small
- Serial monitor: 115200 baud

**Upload methods:**
- USB (first flash)
- OTA via Arduino IDE (Tools → Port → Network ports) — hostname and password are both the device ID (e.g. `LEDPOWER-AABBCCDDEEFF`)

**Required libraries (Arduino Library Manager):**

| Library | Notes |
|---------|-------|
| Adafruit NeoPixel | LED strip control |
| ArduinoJson | JSON serialisation |
| ArduinoOTA | OTA updates |
| PubSubClient | MQTT client |
| WiFiManager (tzapu) | Captive portal |
| WebServer | Built-in ESP32 library |
| SPIFFS | Built-in ESP32 library |

## Architecture

The sketch is split across several `.ino` files that the Arduino IDE concatenates before compiling. All globals are declared in the main file and shared implicitly.

| File | Responsibility |
|------|---------------|
| `LEDPowerPercentage.ino` | `#include`s, compile-time defines, all globals, `setup()`, `loop()` |
| `Config.h` | `Config` namespace — all persistent settings, `load()` / `save()` via SPIFFS `/config.json` |
| `LEDControl.ino` | Colour zone rendering, fade blending, charge/discharge sweep animation |
| `MQTTAutoDiscovery.ino` | MQTT state publish, HA auto-discovery payloads, `mqttCallback()` |
| `WebServer.ino` | Web UI handlers (`/`, `/set`, `/config`, `/reset`) and REST API (`/api/state`, `/api/brightness`, `/api/color`) |
| `WiFiMqttSetup.ino` | WiFi captive portal (`setupWifi`), OTA (`setupOTA`), MQTT reconnect (`mqttReconnect`) |
| `VRMClient.ino` | Victron VRM HTTPS polling — resolves user ID → site ID → battery instance, then polls `BatterySummary` on a configurable interval |

## Key Design Decisions

- **MQTT and VRM are mutually exclusive.** Only one input source can be active at a time; the config page enforces this.
- **Device identifier** is built from the MAC address in `setup()` and used as the MQTT client ID, OTA hostname/password, and WiFi AP name: `LEDPOWER-AABBCCDDEEFF`.
- **LED strip is reconfigured at runtime** from `Config` values — pin, pixel count, colour order, and kHz are applied via `updateLength()`, `setPin()`, and `updateType()` rather than compile-time constants.
- **`colorOverrideActive`** is a separate display mode (solid colour fill) triggered by the MQTT color entity or `POST /api/color`. It takes priority over the normal percentage display in `updateLEDs()`.
- **VRM init is lazy and rate-limited.** On first poll it resolves site ID and battery instance sequentially; on failure it backs off by `vrm_interval` seconds. Call `vrmReset()` whenever VRM config changes.
- **SPIFFS** stores all config as `/config.json`. If the mount fails, SPIFFS is formatted and all settings reset to defaults.
- **MQTT buffer** is set to 2048 bytes (`setBufferSize`) to accommodate the auto-discovery JSON payloads.

## MQTT Topic Scheme

All topics follow the pattern `esp32-ledpower/<DEVICE_ID>/<suffix>`. The three HA entities are:
1. **Light** — state/command on `…/state` and `…/command`; brightness is 0–100 (not 0–255)
2. **Select** (charging state) — `…/charging_state/set` accepts `idle` / `charging` / `discharging`
3. **RGB Light** (colour override) — `…/color/state` and `…/color/command`
