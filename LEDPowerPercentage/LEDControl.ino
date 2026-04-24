// LEDControl.ino — LED strip control logic for LEDPowerPercentage


uint32_t blendColors(uint32_t c1, uint32_t c2, float t) {
    uint8_t r1 = (c1 >> 16) & 0xFF, g1 = (c1 >> 8) & 0xFF, b1 = c1 & 0xFF;
    uint8_t r2 = (c2 >> 16) & 0xFF, g2 = (c2 >> 8) & 0xFF, b2 = c2 & 0xFF;
    return ws2812b.Color(
        (uint8_t)(r1 + t * (r2 - r1)),
        (uint8_t)(g1 + t * (g2 - g1)),
        (uint8_t)(b1 + t * (b2 - b1))
    );
}

uint32_t getZoneColor(int pixelIndex) {
    int pixelPct = (int)(((long)pixelIndex * 100) / Config::num_pixels);

    uint32_t RED    = ws2812b.Color(255, 0, 0);
    uint32_t GREEN  = ws2812b.Color(0, 255, 0);
    uint32_t MIDDLE = ws2812b.Color(Config::middle_r, Config::middle_g, Config::middle_b);

    int fp = Config::fade_pixels;

    // Fade zone around threshold_1 (RED <-> MIDDLE)
    if (fp > 0 && pixelPct >= Config::threshold_1 - fp && pixelPct <= Config::threshold_1 + fp) {
        float blend = (float)(pixelPct - (Config::threshold_1 - fp)) / (2.0f * fp);
        if (blend < 0.0f) blend = 0.0f;
        if (blend > 1.0f) blend = 1.0f;
        return blendColors(RED, MIDDLE, blend);
    }

    // Fade zone around threshold_2 (MIDDLE <-> GREEN)
    else if (fp > 0 && pixelPct >= Config::threshold_2 - fp && pixelPct <= Config::threshold_2 + fp) {
        float blend = (float)(pixelPct - (Config::threshold_2 - fp)) / (2.0f * fp);
        if (blend < 0.0f) blend = 0.0f;
        if (blend > 1.0f) blend = 1.0f;
        return blendColors(MIDDLE, GREEN, blend);
    }

    if (pixelPct < Config::threshold_1) return RED;
    if (pixelPct >= Config::threshold_2)  return GREEN;
    return MIDDLE;
}

void updateLEDs() {
    ws2812b.setBrightness(currentBrightness);
    ws2812b.clear();
    if (!ledState) {
        ws2812b.show();
        return;
    }
    int litCount = (ledPercent * Config::num_pixels) / 100;
    for (int i = 0; i < litCount; i++) {
        ws2812b.setPixelColor(i, getZoneColor(i));
    }
    ws2812b.show();
}

void handleStateAnimation() {
    static uint32_t lastAnimStart = 0;
    static uint32_t lastAnimStep  = 0;
    static int      animPixel     = -1;
    static bool     initialised   = false;
    static ChargingState lastState = STATE_IDLE;

    if (chargingState == STATE_IDLE || !ledState || !Config::charge_anim) {
        animPixel   = -1;
        initialised = false;
        lastState   = STATE_IDLE;
        return;
    }

    // Reset animation if state changed (e.g. charging → discharging)
    if (chargingState != lastState) {
        animPixel   = -1;
        initialised = false;
        lastState   = chargingState;
    }

    uint32_t now = millis();

    if (!initialised) {
        // Start the first sweep immediately; interval only applies between repeats
        animPixel     = (chargingState == STATE_CHARGING) ? 0 : (Config::num_pixels - 1);
        lastAnimStart = now;
        initialised   = true;
    }

    if (animPixel == -1 && now - lastAnimStart >= (uint32_t)Config::charge_anim_interval * 1000) {
        animPixel = (chargingState == STATE_CHARGING) ? 0 : (Config::num_pixels - 1);
    }

    if (animPixel >= 0 && animPixel < Config::num_pixels && now - lastAnimStep >= 30) {
        lastAnimStep = now;
        ws2812b.setBrightness(currentBrightness);
        ws2812b.clear();
        int litCount = (ledPercent * Config::num_pixels) / 100;
        for (int i = 0; i < litCount; i++) {
            ws2812b.setPixelColor(i, getZoneColor(i));
        }
        ws2812b.setPixelColor(animPixel, ws2812b.Color(255, 255, 255));
        ws2812b.show();

        if (chargingState == STATE_CHARGING) {
            animPixel++;
            if (animPixel >= Config::num_pixels) {
                animPixel     = -1;
                lastAnimStart = now;
                updateLEDs();
            }
        } else { // STATE_DISCHARGING
            animPixel--;
            if (animPixel < 0) {
                animPixel     = -1;
                lastAnimStart = now;
                updateLEDs();
            }
        }
    }
}

void blinkConnected() {
    int animPixels = max(1, Config::num_pixels / 10);
    for (int b = 0; b < 2; b++) {
        ws2812b.clear();
        for (int i = 0; i < animPixels; i++)
            ws2812b.setPixelColor(i, ws2812b.Color(0, 255, 0));
        ws2812b.setBrightness(currentBrightness);
        ws2812b.show();
        delay(200);
        ws2812b.clear();
        ws2812b.show();
        delay(200);
    }
}

void showAPModeAnimation() {
    static uint32_t apAnimLast = 0;
    uint32_t now = millis();
    if (now - apAnimLast < 20) return;
    apAnimLast = now;

    int animPixels = max(1, Config::num_pixels / 10);
    uint32_t t = now % 2000;
    uint8_t brightness = (t < 1000) ? (uint8_t)(t * 255 / 1000) : (uint8_t)((2000 - t) * 255 / 1000);
    ws2812b.clear();
    for (int i = 0; i < animPixels; i++)
        ws2812b.setPixelColor(i, ws2812b.Color(0, 0, brightness));
    ws2812b.setBrightness(currentBrightness);
    ws2812b.show();
}

void showConnectingAnimation() {
    static uint32_t connAnimLast = 0;
    static uint16_t connPos      = 0;
    uint32_t now = millis();
    if (now - connAnimLast < 80) return;
    connAnimLast = now;

    int animPixels = max(1, Config::num_pixels / 10);
    ws2812b.clear();
    ws2812b.setPixelColor(connPos % animPixels, ws2812b.Color(255, 165, 0));
    ws2812b.setBrightness(currentBrightness);
    ws2812b.show();
    connPos++;
}
