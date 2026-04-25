// Animations.ino — Decorative LED animations for LEDPowerPercentage
// Each animation runs as a non-blocking state machine stepped from loop().

#include <math.h>

// ---------------------------------------------------------------------------
// File-level animation state
// ---------------------------------------------------------------------------

static bool     animNeedsReset = false;

// Per-animation state variables
static uint8_t  rainbowHue   = 0;
static int      meteorPos    = 0;
static uint16_t breathePhase = 0;
static bool     twinkleInit  = false;
static bool     lavaInit     = false;
static bool     wDropInit    = false;
static bool     rainInit     = false;
static bool     starInit     = false;
static uint16_t notifPhase   = 0;

struct Sparkle {
    int     pixel;
    uint8_t brightness;
    uint8_t r, g, b;
};
static const int MAX_SPARKLES = 8;
static Sparkle   sparkles[MAX_SPARKLES];

static uint8_t fireHeat[500];

// ---------------------------------------------------------------------------
// Helper: convert a heat value 0-255 to an RGB colour (black → red → yellow → white)
// ---------------------------------------------------------------------------

static uint32_t heatToColor(uint8_t heat) {
    if (heat < 85) {
        return ws2812b.Color(heat * 3, 0, 0);
    } else if (heat < 170) {
        return ws2812b.Color(255, (heat - 85) * 3, 0);
    } else {
        return ws2812b.Color(255, 255, (heat - 170) * 3);
    }
}

// ---------------------------------------------------------------------------
// animModeToStr / strToAnimMode
// ---------------------------------------------------------------------------

const char* animModeToStr(AnimMode mode) {
    switch (mode) {
        case ANIM_RAINBOW:   return "rainbow";
        case ANIM_FIRE:      return "fire";
        case ANIM_METEOR:    return "meteor";
        case ANIM_TWINKLE:   return "twinkle";
        case ANIM_BREATHE:   return "breathe";
        case ANIM_LAVA:      return "lava";
        case ANIM_WATERFALL: return "waterfall";
        case ANIM_GRADIENT:  return "gradient";
        case ANIM_PULSE:     return "pulse";
        case ANIM_RAIN:      return "rain";
        case ANIM_STARFIELD:    return "starfield";
        case ANIM_NOTIFICATION: return "notification";
        default:                return "none";
    }
}

AnimMode strToAnimMode(const char* s) {
    if (s == nullptr)                return ANIM_NONE;
    if (strcmp(s, "rainbow")   == 0) return ANIM_RAINBOW;
    if (strcmp(s, "fire")      == 0) return ANIM_FIRE;
    if (strcmp(s, "meteor")    == 0) return ANIM_METEOR;
    if (strcmp(s, "twinkle")   == 0) return ANIM_TWINKLE;
    if (strcmp(s, "breathe")   == 0) return ANIM_BREATHE;
    if (strcmp(s, "lava")      == 0) return ANIM_LAVA;
    if (strcmp(s, "waterfall") == 0) return ANIM_WATERFALL;
    if (strcmp(s, "gradient")  == 0) return ANIM_GRADIENT;
    if (strcmp(s, "pulse")     == 0) return ANIM_PULSE;
    if (strcmp(s, "rain")      == 0) return ANIM_RAIN;
    if (strcmp(s, "starfield")    == 0) return ANIM_STARFIELD;
    if (strcmp(s, "notification") == 0) return ANIM_NOTIFICATION;
    return ANIM_NONE;
}

// ---------------------------------------------------------------------------
// setAnimMode — change animation; ANIM_NONE restores normal LED rendering
// ---------------------------------------------------------------------------

void setAnimMode(AnimMode mode) {
    animMode       = mode;
    animNeedsReset = true;
    // Reset all per-animation init flags so every animation reinitialises cleanly
    twinkleInit = false;
    lavaInit    = false;
    wDropInit   = false;
    rainInit    = false;
    starInit    = false;
    notifPhase  = 0;

    if (mode == ANIM_NONE) {
        updateLEDs();
    } else {
        // Animations take full control of the strip; kill color override
        colorOverrideActive = false;
        publishAnimationState();
    }
}

// ---------------------------------------------------------------------------
// Individual animation step functions
// Each is called at its own interval; animNeedsReset is checked inside
// ---------------------------------------------------------------------------

// --- Rainbow: hue cycles across all pixels and rotates over time (20 ms/step) ---
static void stepRainbow() {
    static uint32_t lastStep = 0;
    if (animNeedsReset) {
        rainbowHue = 0;
    }
    uint32_t now = millis();
    if (now - lastStep < 20) return;
    lastStep = now;

    int n = Config::num_pixels;
    ws2812b.setBrightness(currentBrightness);
    for (int i = 0; i < n; i++) {
        uint8_t hue = rainbowHue + (uint8_t)((long)i * 256 / n);
        ws2812b.setPixelColor(i, ws2812b.gamma32(ws2812b.ColorHSV((uint16_t)hue * 257)));
    }
    ws2812b.show();
    rainbowHue++;
}

// --- Fire: bottom-up heat simulation (50 ms/step, pixel 0 = base) ---
static void stepFire() {
    static uint32_t lastStep = 0;
    int n = Config::num_pixels;
    if (n > 500) n = 500;

    if (animNeedsReset) {
        memset(fireHeat, 0, (size_t)n * sizeof(fireHeat[0]));
    }
    uint32_t now = millis();
    if (now - lastStep < 50) return;
    lastStep = now;

    // Step 1: Cool every cell a little
    for (int i = 0; i < n; i++) {
        int cool = random(0, (55 * 10 / n) + 2);
        fireHeat[i] = (uint8_t)max(0, (int)fireHeat[i] - cool);
    }

    // Step 2: Drift heat upward (from top down so we don't overwrite values we still need)
    for (int i = n - 1; i >= 2; i--) {
        fireHeat[i] = (uint8_t)(((int)fireHeat[i - 1] + fireHeat[i - 2] + fireHeat[i - 2]) / 3);
    }

    // Step 3: Randomly ignite at the base
    int sparks = min(6, n - 1);
    for (int i = 0; i <= sparks; i++) {
        if (random(0, 3) == 0) {
            int heat = (int)fireHeat[i] + random(160, 255);
            fireHeat[i] = (uint8_t)min(heat, 255);
        }
    }

    // Step 4: Render heat array to pixels (pixel 0 = base)
    ws2812b.setBrightness(currentBrightness);
    for (int i = 0; i < n; i++) {
        ws2812b.setPixelColor(i, heatToColor(fireHeat[i]));
    }
    ws2812b.show();
}

// --- Meteor: blue-white comet bottom → top with fading tail (30 ms/step) ---
static void stepMeteor() {
    static uint32_t lastStep = 0;
    const int tailLen = 8;

    if (animNeedsReset) {
        meteorPos = -tailLen;
    }
    uint32_t now = millis();
    if (now - lastStep < 30) return;
    lastStep = now;

    int n = Config::num_pixels;

    ws2812b.setBrightness(currentBrightness);
    ws2812b.clear();

    // Draw tail pixels
    for (int t = 0; t < tailLen; t++) {
        int px = meteorPos - t;
        if (px >= 0 && px < n) {
            uint8_t fade = (uint8_t)(255 - (long)t * 255 / tailLen);
            // Blue-white comet: full intensity head fades toward pure blue
            ws2812b.setPixelColor(px, ws2812b.Color(fade / 2, fade / 2, fade));
        }
    }

    // Draw head (fully white-blue)
    if (meteorPos >= 0 && meteorPos < n) {
        ws2812b.setPixelColor(meteorPos, ws2812b.Color(128, 128, 255));
    }

    ws2812b.show();

    meteorPos++;
    if (meteorPos > n + tailLen) {
        meteorPos = -tailLen;
    }
}

// --- Twinkle: random sparkles that fade out (40 ms/step) ---
static void stepTwinkle() {
    static uint32_t lastStep = 0;

    if (animNeedsReset || !twinkleInit) {
        for (int i = 0; i < MAX_SPARKLES; i++) {
            sparkles[i].pixel = -1; // inactive
        }
        twinkleInit = true;
    }
    uint32_t now = millis();
    if (now - lastStep < 40) return;
    lastStep = now;

    int n = Config::num_pixels;
    ws2812b.setBrightness(currentBrightness);
    ws2812b.clear();

    for (int i = 0; i < MAX_SPARKLES; i++) {
        if (sparkles[i].pixel < 0) {
            // Possibly spawn a new sparkle
            if (random(0, 4) == 0) {
                sparkles[i].pixel      = random(0, n);
                sparkles[i].brightness = 255;
                // Random colour: warm gold, cool blue, or white
                int choice = random(0, 3);
                if (choice == 0) {
                    sparkles[i].r = 255; sparkles[i].g = 180; sparkles[i].b = 30;  // warm gold
                } else if (choice == 1) {
                    sparkles[i].r = 80;  sparkles[i].g = 140; sparkles[i].b = 255; // cool blue
                } else {
                    sparkles[i].r = 255; sparkles[i].g = 255; sparkles[i].b = 255; // white
                }
            }
        } else {
            // Draw and fade
            uint8_t bri = sparkles[i].brightness;
            uint8_t r = (uint8_t)((uint32_t)sparkles[i].r * bri / 255);
            uint8_t g = (uint8_t)((uint32_t)sparkles[i].g * bri / 255);
            uint8_t b = (uint8_t)((uint32_t)sparkles[i].b * bri / 255);
            ws2812b.setPixelColor(sparkles[i].pixel, ws2812b.Color(r, g, b));

            if (bri <= 15) {
                sparkles[i].pixel = -1; // kill sparkle
            } else {
                sparkles[i].brightness = bri - 15;
            }
        }
    }

    ws2812b.show();
}

// --- Breathe: whole strip slowly pulses (20 ms/step) ---
static void stepBreathe() {
    static uint32_t lastStep = 0;

    if (animNeedsReset) {
        breathePhase = 0;
    }
    uint32_t now = millis();
    if (now - lastStep < 20) return;
    lastStep = now;

    // Determine base colour; fall back to soft white when all-zero
    uint8_t br = Config::middle_r;
    uint8_t bg = Config::middle_g;
    uint8_t bb = Config::middle_b;
    if (br == 0 && bg == 0 && bb == 0) {
        br = 200; bg = 200; bb = 200;
    }

    // phase 0-511, sinusoidal brightness
    float bright = (sinf((float)breathePhase * 2.0f * M_PI / 512.0f) + 1.0f) * 127.5f;
    uint8_t scale = (uint8_t)(bright + 0.5f);

    uint8_t r = (uint8_t)((uint32_t)br * scale / 255);
    uint8_t g = (uint8_t)((uint32_t)bg * scale / 255);
    uint8_t b = (uint8_t)((uint32_t)bb * scale / 255);

    ws2812b.setBrightness(currentBrightness);
    ws2812b.fill(ws2812b.Color(r, g, b));
    ws2812b.show();

    breathePhase = (breathePhase + 1) % 512;
}

// ---------------------------------------------------------------------------
// New animation step functions (6 additions)
// ---------------------------------------------------------------------------

// --- Lava Lamp: warm blobs drifting upward (50 ms/step) ---
struct Blob { float pos; float speed; uint8_t r, g, b; };
static Blob lavaBlobs[4];

static void stepLava() {
    static uint32_t lastStep = 0;
    int n = Config::num_pixels;
    if (n > 500) n = 500;

    if (animNeedsReset || !lavaInit) {
        lavaBlobs[0] = { (float)(n / 4),     0.06f, 255, 80,  0   };
        lavaBlobs[1] = { (float)(n / 2),     0.04f, 220, 60,  120 };
        lavaBlobs[2] = { (float)(3 * n / 4), 0.08f, 240, 150, 0   };
        lavaBlobs[3] = { (float)(n - 1),     0.05f, 150, 0,   180 };
        lavaInit = true;
    }
    uint32_t now = millis();
    if (now - lastStep < 50) return;
    lastStep = now;

    // Move blobs upward and wrap
    for (int b = 0; b < 4; b++) {
        lavaBlobs[b].pos += lavaBlobs[b].speed;
        if (lavaBlobs[b].pos >= (float)n) lavaBlobs[b].pos = 0.0f;
    }

    // Fill background
    ws2812b.setBrightness(currentBrightness);
    for (int i = 0; i < n; i++) {
        ws2812b.setPixelColor(i, ws2812b.Color(4, 2, 0));
    }

    // Additive blend blobs
    const int radius = 5;
    for (int b = 0; b < 4; b++) {
        int center = (int)lavaBlobs[b].pos;
        for (int i = center - radius; i <= center + radius; i++) {
            if (i < 0 || i >= n) continue;
            int dist = abs(i - center);
            long diff = radius - dist;
            uint8_t fade = (uint8_t)((255L * diff * diff) / ((long)radius * radius));
            uint32_t existing = ws2812b.getPixelColor(i);
            uint8_t er = (existing >> 16) & 0xFF;
            uint8_t eg = (existing >> 8)  & 0xFF;
            uint8_t eb =  existing        & 0xFF;
            uint8_t nr = (uint8_t)min(255, (int)er + ((int)lavaBlobs[b].r * (int)fade) / 255);
            uint8_t ng = (uint8_t)min(255, (int)eg + ((int)lavaBlobs[b].g * (int)fade) / 255);
            uint8_t nb = (uint8_t)min(255, (int)eb + ((int)lavaBlobs[b].b * (int)fade) / 255);
            ws2812b.setPixelColor(i, ws2812b.Color(nr, ng, nb));
        }
    }
    ws2812b.show();
}

// --- Waterfall: cool blues cascading downward (25 ms/step) ---
struct WaterfallDrop { float pos; float speed; };
static WaterfallDrop wDrops[5];
static uint8_t  wSparkleBright[500];

static void stepWaterfall() {
    static uint32_t lastStep = 0;
    int n = Config::num_pixels;
    if (n > 500) n = 500;

    if (animNeedsReset || !wDropInit) {
        for (int i = 0; i < 5; i++) {
            wDrops[i].pos   = (float)random(0, n);
            wDrops[i].speed = 0.3f + (float)random(0, 40) / 100.0f;
        }
        memset(wSparkleBright, 0, sizeof(wSparkleBright));
        wDropInit = true;
    }
    uint32_t now = millis();
    if (now - lastStep < 25) return;
    lastStep = now;

    ws2812b.setBrightness(currentBrightness);
    ws2812b.clear();

    // Decay and draw sparkles
    for (int i = 0; i < n; i++) {
        if (wSparkleBright[i] > 0) {
            uint8_t bri = wSparkleBright[i];
            ws2812b.setPixelColor(i, ws2812b.Color(
                (uint8_t)(200u * bri / 255u),
                (uint8_t)(220u * bri / 255u),
                255));
            wSparkleBright[i] = (bri > 40) ? (bri - 40) : 0;
        }
    }

    // Move drops downward and draw
    for (int d = 0; d < 5; d++) {
        wDrops[d].pos -= wDrops[d].speed;
        if (wDrops[d].pos < 0.0f) {
            wDrops[d].pos   = (float)(n - 1);
            wDrops[d].speed = 0.3f + (float)random(0, 40) / 100.0f;
        }
        int head = (int)wDrops[d].pos;
        // Draw tail (pixels above head = higher pixel indices)
        for (int t = 1; t <= 4; t++) {
            int tp = head + t;
            if (tp < n) {
                uint8_t fade = (uint8_t)(255 - (long)t * 255 / 4);
                uint8_t tr = (uint8_t)(5u  * fade / 255u);
                uint8_t tg = (uint8_t)(30u * fade / 255u);
                uint8_t tb = (uint8_t)(60u * fade / 255u);
                ws2812b.setPixelColor(tp, ws2812b.Color(tr, tg, tb));
            }
        }
        // Draw head
        if (head >= 0 && head < n) {
            ws2812b.setPixelColor(head, ws2812b.Color(30, 130, 220));
        }
    }

    // Random sparkle chance (1% per pixel)
    for (int i = 0; i < n; i++) {
        if (random(0, 100) == 0) {
            wSparkleBright[i] = 200;
        }
    }

    ws2812b.show();
}

// --- Breathing Gradient: 3 color-pair waypoints (30 ms/step) ---
static float gradPhase = 0.0f;

static void stepGradient() {
    static uint32_t lastStep = 0;

    if (animNeedsReset) {
        gradPhase = 0.0f;
    }
    uint32_t now = millis();
    if (now - lastStep < 30) return;
    lastStep = now;

    // Waypoint pairs: (bottomR,bottomG,bottomB, topR,topG,topB)
    static const uint8_t waypoints[3][6] = {
        {   0, 200, 180, 140,   0, 220 },   // A: teal → violet
        { 140,   0, 220, 230, 110,   0 },   // B: violet → amber
        { 230, 110,   0,   0, 200, 180 },   // C: amber → teal
    };

    int   seg    = (int)gradPhase % 3;
    float blend  = gradPhase - (int)gradPhase;  // 0..1 within segment
    // Interpolate bottom and top colors between current and next waypoint
    int   next   = (seg + 1) % 3;
    uint8_t bR = (uint8_t)(waypoints[seg][0] + blend * ((int)waypoints[next][0] - waypoints[seg][0]));
    uint8_t bG = (uint8_t)(waypoints[seg][1] + blend * ((int)waypoints[next][1] - waypoints[seg][1]));
    uint8_t bB = (uint8_t)(waypoints[seg][2] + blend * ((int)waypoints[next][2] - waypoints[seg][2]));
    uint8_t tR = (uint8_t)(waypoints[seg][3] + blend * ((int)waypoints[next][3] - waypoints[seg][3]));
    uint8_t tG = (uint8_t)(waypoints[seg][4] + blend * ((int)waypoints[next][4] - waypoints[seg][4]));
    uint8_t tB = (uint8_t)(waypoints[seg][5] + blend * ((int)waypoints[next][5] - waypoints[seg][5]));

    // Separate breathe multiplier: 60%..100% brightness
    float breatheT = (sinf((float)now / 2000.0f * 2.0f * (float)M_PI) + 1.0f) * 0.2f + 0.6f;

    int n = Config::num_pixels;
    ws2812b.setBrightness(currentBrightness);
    for (int i = 0; i < n; i++) {
        float t = (n > 1) ? (float)i / (float)(n - 1) : 0.0f;  // 0 (bottom) .. 1 (top)
        uint8_t r = (uint8_t)((bR + t * (tR - bR)) * breatheT);
        uint8_t g = (uint8_t)((bG + t * (tG - bG)) * breatheT);
        uint8_t b = (uint8_t)((bB + t * (tB - bB)) * breatheT);
        ws2812b.setPixelColor(i, ws2812b.Color(r, g, b));
    }
    ws2812b.show();

    gradPhase += 0.01f;
    if (gradPhase >= 3.0f) gradPhase -= 3.0f;
}

// --- Energy Pulse: bright shot from bottom to top (20 ms/step) ---
static float    pulsePos        = -1.0f;
static float    pulseSpeed      = 2.0f;
static uint32_t pulseWaitUntil  = 0;
static float    trailBright[500];

static void stepPulse() {
    static uint32_t lastStep = 0;
    int n = Config::num_pixels;
    if (n > 500) n = 500;

    if (animNeedsReset) {
        pulsePos       = -1.0f;
        pulseWaitUntil = 0;
        memset(trailBright, 0, sizeof(trailBright));
    }
    uint32_t now = millis();
    if (now - lastStep < 20) return;
    lastStep = now;

    // Decay trail
    for (int i = 0; i < n; i++) {
        trailBright[i] *= 0.88f;
    }

    if (pulsePos < 0.0f) {
        // Waiting to launch
        if (now >= pulseWaitUntil) {
            pulsePos   = 0.0f;
            pulseSpeed = 1.5f + (float)random(0, 200) / 100.0f;  // 1.5..3.5
        }
    } else {
        // Advance pulse
        pulsePos += pulseSpeed;
        if (pulsePos < (float)n) {
            int px = (int)pulsePos;
            if (px < n) trailBright[px] = 255.0f;
        } else {
            // Reached top — wait random 500-2000ms
            pulsePos       = -1.0f;
            pulseWaitUntil = now + (uint32_t)random(500, 2001);
        }
    }

    ws2812b.setBrightness(currentBrightness);
    ws2812b.clear();

    // Draw trail
    for (int i = 0; i < n; i++) {
        if (trailBright[i] > 1.0f) {
            uint8_t bri = (uint8_t)trailBright[i];
            ws2812b.setPixelColor(i, ws2812b.Color(
                (uint8_t)(200u * bri / 255u),
                (uint8_t)(160u * bri / 255u),
                0));
        }
    }

    // Draw head
    if (pulsePos >= 0.0f) {
        int px = (int)pulsePos;
        if (px >= 0 && px < n) {
            ws2812b.setPixelColor(px, ws2812b.Color(255, 255, 200));
        }
    }

    ws2812b.show();
}

// --- Rain: blue drops falling downward (25 ms/step) ---
struct RainDrop { float pos; float speed; int splashFrames; };
static RainDrop rainDrops[6];

static void stepRain() {
    static uint32_t lastStep = 0;
    int n = Config::num_pixels;
    if (n > 500) n = 500;

    if (animNeedsReset || !rainInit) {
        for (int i = 0; i < 6; i++) {
            rainDrops[i].pos         = (float)random(n / 2, n);
            rainDrops[i].speed       = 0.4f + (float)random(0, 50) / 100.0f;
            rainDrops[i].splashFrames = 0;
        }
        rainInit = true;
    }
    uint32_t now = millis();
    if (now - lastStep < 25) return;
    lastStep = now;

    // Background
    ws2812b.setBrightness(currentBrightness);
    for (int i = 0; i < n; i++) {
        ws2812b.setPixelColor(i, ws2812b.Color(0, 0, 4));
    }

    for (int d = 0; d < 6; d++) {
        rainDrops[d].pos -= rainDrops[d].speed;

        if (rainDrops[d].pos <= 0.0f) {
            rainDrops[d].splashFrames = 2;
            rainDrops[d].pos          = (float)random(n / 2, n);
            rainDrops[d].speed        = 0.4f + (float)random(0, 50) / 100.0f;
        }

        // Draw splash at pixel 0
        if (rainDrops[d].splashFrames > 0) {
            ws2812b.setPixelColor(0, ws2812b.Color(180, 220, 255));
            rainDrops[d].splashFrames--;
        }

        int head = (int)rainDrops[d].pos;
        // Draw tail (2 pixels above = higher indices)
        for (int t = 1; t <= 2; t++) {
            int tp = head + t;
            if (tp >= 0 && tp < n) {
                ws2812b.setPixelColor(tp, ws2812b.Color(6, 18, 54));
            }
        }
        // Draw head
        if (head >= 0 && head < n) {
            ws2812b.setPixelColor(head, ws2812b.Color(20, 60, 180));
        }
    }

    ws2812b.show();
}

// --- Starfield: stars slowly drifting upward with twinkle (40 ms/step) ---
struct Star { float pos; float speed; float phase; float phaseSpeed; uint8_t r, g, b; };
static Star stars[12];

static void stepStarfield() {
    static uint32_t lastStep = 0;
    int n = Config::num_pixels;
    if (n > 500) n = 500;

    if (animNeedsReset || !starInit) {
        for (int i = 0; i < 12; i++) {
            stars[i].pos        = (float)random(0, n);
            stars[i].speed      = 0.015f + (float)random(0, 30) / 1000.0f;
            stars[i].phase      = (float)random(0, 628) / 100.0f;  // 0..2π
            stars[i].phaseSpeed = 0.04f + (float)random(0, 80) / 1000.0f;
            int choice = random(0, 3);
            if (choice == 0) {
                stars[i].r = 255; stars[i].g = 245; stars[i].b = 200;  // warm white
            } else if (choice == 1) {
                stars[i].r = 200; stars[i].g = 220; stars[i].b = 255;  // cool white
            } else {
                stars[i].r = 255; stars[i].g = 255; stars[i].b = 255;  // pure white
            }
        }
        starInit = true;
    }
    uint32_t now = millis();
    if (now - lastStep < 40) return;
    lastStep = now;

    ws2812b.setBrightness(currentBrightness);
    ws2812b.clear();

    for (int i = 0; i < 12; i++) {
        stars[i].pos += stars[i].speed;
        if (stars[i].pos >= (float)n) stars[i].pos -= (float)n;

        stars[i].phase += stars[i].phaseSpeed;
        if (stars[i].phase >= 2.0f * (float)M_PI) stars[i].phase -= 2.0f * (float)M_PI;

        float bright = (sinf(stars[i].phase) + 1.0f) * 127.5f;
        uint8_t bri = (uint8_t)bright;
        ws2812b.setPixelColor((int)stars[i].pos, ws2812b.Color(
            (uint8_t)((uint32_t)stars[i].r * bri / 255u),
            (uint8_t)((uint32_t)stars[i].g * bri / 255u),
            (uint8_t)((uint32_t)stars[i].b * bri / 255u)));
    }

    ws2812b.show();
}

// --- Notification: two quick amber pulses then a pause — mimics Alexa notification ring (25 ms/step) ---
static void stepNotification() {
    static uint32_t lastStep = 0;
    uint32_t now = millis();
    if (now - lastStep < 25) return;
    lastStep = now;

    if (animNeedsReset) notifPhase = 0;

    // Cycle: 0-39 up | 40-79 down | 80-119 gap | 120-159 up | 160-199 down | 200-319 long pause
    // Total: 320 steps × 25 ms = 8 s
    const uint16_t TOTAL = 320;
    uint16_t p = notifPhase;

    uint8_t level = 0;
    if      (p <  40) level = (uint8_t)(p * 255 / 39);
    else if (p <  80) level = (uint8_t)((79 - p) * 255 / 39);
    else if (p < 120) level = 0;
    else if (p < 160) level = (uint8_t)((p - 120) * 255 / 39);
    else if (p < 200) level = (uint8_t)((199 - p) * 255 / 39);
    // 200-319: level stays 0 (pause)

    // Amber: R=255, G=140, B=0 — scale by level
    uint8_t r = (uint8_t)((uint32_t)255 * level / 255u);
    uint8_t g = (uint8_t)((uint32_t)140 * level / 255u);

    int n = Config::num_pixels;
    if (n > 500) n = 500;
    ws2812b.setBrightness(currentBrightness);
    ws2812b.fill(ws2812b.Color(r, g, 0));
    ws2812b.show();

    notifPhase = (notifPhase + 1) % TOTAL;
}

// ---------------------------------------------------------------------------
// handleAnimation — call from loop(); dispatches to the active animation
// ---------------------------------------------------------------------------

void handleAnimation() {
    if (animMode == ANIM_NONE || colorOverrideActive) return;

    switch (animMode) {
        case ANIM_RAINBOW:      stepRainbow();      break;
        case ANIM_FIRE:         stepFire();         break;
        case ANIM_METEOR:       stepMeteor();       break;
        case ANIM_TWINKLE:      stepTwinkle();      break;
        case ANIM_BREATHE:      stepBreathe();      break;
        case ANIM_LAVA:         stepLava();         break;
        case ANIM_WATERFALL:    stepWaterfall();    break;
        case ANIM_GRADIENT:     stepGradient();     break;
        case ANIM_PULSE:        stepPulse();        break;
        case ANIM_RAIN:         stepRain();         break;
        case ANIM_STARFIELD:    stepStarfield();    break;
        case ANIM_NOTIFICATION: stepNotification(); break;
        default: break;
    }

    // Clear reset flag AFTER the step function has had a chance to use it
    animNeedsReset = false;
}
