// ModbusClient.ino — Victron GX Modbus TCP integration (com.victronenergy.battery)
//
// Registers (CCGX Modbus TCP register list):
//   266  State of charge  uint16  ÷10  %
//   261  Current          int16   ÷10  A  (positive = charging, negative = discharging)
//
// Unit ID = Config::vrm_battery_instance when >= 0, otherwise 0.
// Charging state derived from current vs. shared charge/discharge thresholds.

static WiFiClient modbusClient;
static uint32_t   lastModbusPoll = 0;
static uint16_t   modbusTxId     = 0;

// ---------------------------------------------------------------------------
// modbusReadReg — FC03 read of a single holding register.
// Returns raw uint16 value, or -1 on timeout / connection error / exception.
// ---------------------------------------------------------------------------
static int32_t modbusReadReg(uint8_t unitId, uint16_t address) {
    modbusTxId++;
    uint8_t req[12] = {
        (uint8_t)(modbusTxId >> 8), (uint8_t)(modbusTxId & 0xFF),  // Transaction ID
        0x00, 0x00,                                                   // Protocol ID
        0x00, 0x06,                                                   // PDU length
        unitId,                                                        // Unit ID
        0x03,                                                          // FC: Read Holding Registers
        (uint8_t)(address >> 8), (uint8_t)(address & 0xFF),          // Start address
        0x00, 0x01                                                     // Quantity: 1 register
    };
    if (modbusClient.write(req, sizeof(req)) != sizeof(req)) return -1;

    // Response: MBAP(7) + FC(1) + byte count(1) + 2 data bytes = 11 bytes
    uint32_t deadline = millis() + 1000;
    while (modbusClient.available() < 11 && millis() < deadline) delay(1);

    if (modbusClient.available() < 11) {
        Serial.printf("[Modbus] Timeout reading reg %d\n", address);
        return -1;
    }

    uint8_t resp[11];
    modbusClient.readBytes(resp, 11);

    if (resp[7] & 0x80) {
        Serial.printf("[Modbus] Exception 0x%02X for reg %d\n", resp[8], address);
        return -1;
    }
    return (uint16_t)((resp[9] << 8) | resp[10]);
}

// ---------------------------------------------------------------------------
// modbusPoll — called from loop(); reads SOC + current and updates LED state
// ---------------------------------------------------------------------------
void modbusPoll() {
    if (Config::victron_source != VICTRON_MODBUS) return;
    if (WiFi.status() != WL_CONNECTED) return;
    if (Config::modbus_host[0] == '\0') {
        Serial.println("[Modbus] Skipping poll — host IP not configured");
        return;
    }

    uint32_t now = millis();
    if (lastModbusPoll != 0 && now - lastModbusPoll < (uint32_t)Config::vrm_interval * 1000) return;
    lastModbusPoll = now;

    Serial.printf("[Modbus] Connecting to %s:502...\n", Config::modbus_host);
    if (!modbusClient.connect(Config::modbus_host, 502)) {
        Serial.println("[Modbus] Connection failed");
        return;
    }
    Serial.println("[Modbus] Connected");

    float soc     = 0.0f;
    float current = 0.0f;

    if (Config::vrm_battery_instance < 0) {
        // Unit 100 = com.victronenergy.system — always available, no instance config needed
        // Reg 843: SOC uint16 ÷1 %, Reg 841: current int16 ÷10 A, Reg 844: state 0/1/2
        int32_t rawSoc = modbusReadReg(100, 843);
        if (rawSoc < 0) { modbusClient.stop(); return; }
        int32_t rawCur = modbusReadReg(100, 841);
        modbusClient.stop();
        if (rawCur < 0) return;

        soc     = (float)rawSoc;
        current = (float)(int16_t)(rawCur & 0xFFFF) / 10.0f;
        Serial.printf("[Modbus] unit=100 SOC=%.0f%% current=%.2fA\n", soc, current);
    } else {
        // com.victronenergy.battery — unit ID = configured battery instance
        // Reg 266: SOC uint16 ÷10 %, Reg 261: current int16 ÷10 A
        uint8_t unitId = (uint8_t)Config::vrm_battery_instance;
        int32_t rawSoc = modbusReadReg(unitId, 266);
        if (rawSoc < 0) { modbusClient.stop(); return; }
        int32_t rawCur = modbusReadReg(unitId, 261);
        modbusClient.stop();
        if (rawCur < 0) return;

        soc     = (float)rawSoc / 10.0f;
        current = (float)(int16_t)(rawCur & 0xFFFF) / 10.0f;

        Serial.printf("[Modbus] unit=%d SOC=%.1f%% current=%.2fA\n", unitId, soc, current);
    }

    ledPercent    = (int)constrain(soc, 0.0f, 100.0f);
    ledState      = (ledPercent > 0);
    chargingState = (current > Config::vrm_charge_threshold)   ? STATE_CHARGING :
                    (current < Config::vrm_discharge_threshold) ? STATE_DISCHARGING : STATE_IDLE;

    updateLEDs();
    publishState();
}

// ---------------------------------------------------------------------------
// modbusReset — call when Modbus settings change via config save
// ---------------------------------------------------------------------------
void modbusReset() {
    lastModbusPoll = 0;
}
