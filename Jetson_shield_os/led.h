#ifndef LED_H
#define LED_H

#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include "palette.h"
#include "system_configuration.h"

enum class LedMode : uint8_t {
    OFF = 0,
    NORMAL,
    ALERT_RED,
    POWER_SIGNAL
};

class LEDController {
public:
    static constexpr uint8_t DEFAULT_PIN        = jetson_cfg::kLedPin;
    static constexpr uint16_t DEFAULT_COUNT     = jetson_cfg::kLedCount;
    static constexpr uint8_t DEFAULT_BRIGHTNESS = jetson_cfg::kLedBrightness;
    static constexpr uint32_t DEFAULT_CHANGE_INTERVAL = jetson_cfg::kLedChangeIntervalMs;
    static constexpr uint32_t DEFAULT_TRANSITION_TIME = jetson_cfg::kLedTransitionTimeMs;
    static constexpr uint32_t DEFAULT_PALETTE_INTERVAL = jetson_cfg::kLedPaletteIntervalMs;

    LEDController(uint8_t pin = DEFAULT_PIN,
                  uint16_t count = DEFAULT_COUNT,
                  uint8_t bright = DEFAULT_BRIGHTNESS,
                  uint32_t changeInt = DEFAULT_CHANGE_INTERVAL,
                  uint32_t transTime = DEFAULT_TRANSITION_TIME,
                  uint32_t palInt = DEFAULT_PALETTE_INTERVAL);

    void init();
    inline void begin() { init(); }

    void update();

    void setChangeInterval(uint32_t ms) { changeInterval = ms; }
    void setTransitionTime(uint32_t ms) { transitionTime = ms; }
    void setPaletteInterval(uint32_t ms) { paletteInterval = ms; }

    void setPalette(const uint32_t* palette, size_t size);
    void nextPalette();
    void onStateChange(jetson_cfg::SystemState newState);
    void onAlertChange(uint8_t alertMask);
    void setMode(LedMode newMode);
    LedMode getMode() const { return mode; }
    void signalPowerTransition();
    void setAlertActive(bool active);

    void setBrightness(uint8_t b) {
        brightness = b;
        strip.setBrightness(brightness);
    }
    uint8_t getBrightness() const { return brightness; }

private:
    Adafruit_NeoPixel strip;
    uint8_t ledPin;
    uint16_t ledCount;
    const uint32_t* currentPalette;
    size_t paletteSize;
    uint32_t currentColor;
    uint32_t targetColor;
    int currentColorIndex;
    unsigned long lastTransitionStart;
    unsigned long lastPaletteChange;

    uint32_t changeInterval;
    uint32_t transitionTime;
    uint32_t paletteInterval;
    uint8_t brightness;
    LedMode mode;
    LedMode modeBeforeSignal;
    bool alertActive;
    bool signalLedOn;
    uint8_t signalToggleCount;
    uint8_t signalToggleTarget;
    unsigned long lastSignalToggle;
    jetson_cfg::SystemState state;
    uint8_t alertMask;

    uint32_t blendColor(uint32_t c1, uint32_t c2, float fraction);
    uint32_t signalColorForState() const;
    void applySolid(uint32_t color);
    void updateNormal(unsigned long now);
    void updatePowerSignal(unsigned long now);
    void resetPaletteState(unsigned long now);
};

#endif // LED_H
