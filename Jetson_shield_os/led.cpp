#include "led.h"

LEDController::LEDController(uint8_t pin,
                                                         uint16_t count,
                                                         uint8_t bright,
                                                         uint32_t changeInt,
                                                         uint32_t transTime,
                                                         uint32_t palInt)
    : strip(count, pin, NEO_GRB + NEO_KHZ800),
      ledPin(pin),
      ledCount(count),
      currentPalette(nullptr),
      paletteSize(0),
      currentColor(0),
      targetColor(0),
      currentColorIndex(0),
      lastTransitionStart(0),
      lastPaletteChange(0),
      changeInterval(changeInt),
      transitionTime(transTime),
      paletteInterval(palInt),
      brightness(bright),
      mode(LedMode::NORMAL),
      modeBeforeSignal(LedMode::NORMAL),
      alertActive(false),
      signalLedOn(false),
      signalToggleCount(0),
      signalToggleTarget(0),
    lastSignalToggle(0),
    state(jetson_cfg::SystemState::POWER_OFF),
    alertMask(0) {}

void LEDController::init() {
    strip.begin();
    strip.setBrightness(brightness);
    strip.show();

    if (PALETTE_COUNT > 0) {
        setPalette(palettes[0], PALETTE_SIZE);
    }

    const unsigned long now = millis();
    resetPaletteState(now);
    lastTransitionStart = now;
    lastPaletteChange = now;
}

void LEDController::update() {
    const unsigned long now = millis();

    if (alertActive || mode == LedMode::ALERT_RED) {
        applySolid(kLedColorAlertRed);
        return;
    }

    if (mode == LedMode::OFF) {
        applySolid(kLedColorOff);
        return;
    }

    if (mode == LedMode::POWER_SIGNAL) {
        updatePowerSignal(now);
        return;
    }

    updateNormal(now);
}

void LEDController::setMode(LedMode newMode) {
    if (newMode == LedMode::POWER_SIGNAL) {
        signalPowerTransition();
        return;
    }

    mode = newMode;

    if (mode == LedMode::OFF) {
        applySolid(kLedColorOff);
        return;
    }

    if (mode == LedMode::ALERT_RED) {
        applySolid(kLedColorAlertRed);
        return;
    }

    resetPaletteState(millis());
}

void LEDController::onStateChange(jetson_cfg::SystemState newState) {
    const jetson_cfg::SystemState previous = state;
    state = newState;

    setMode(LedMode::NORMAL);

    const bool enteringPowerTransition =
        (newState == jetson_cfg::SystemState::BOOTING_ON ||
         newState == jetson_cfg::SystemState::SHUTTING_DOWN);
    const bool boundaryTransition =
        (previous == jetson_cfg::SystemState::POWER_OFF && newState != jetson_cfg::SystemState::POWER_OFF) ||
        (previous != jetson_cfg::SystemState::POWER_OFF && newState == jetson_cfg::SystemState::POWER_OFF);

    if (enteringPowerTransition || boundaryTransition) {
        signalPowerTransition();
    }
}

void LEDController::onAlertChange(uint8_t nextAlertMask) {
    alertMask = nextAlertMask;
    setAlertActive(alertMask != 0);
}

void LEDController::signalPowerTransition() {
    modeBeforeSignal = mode;
    mode = LedMode::POWER_SIGNAL;
    signalLedOn = false;
    signalToggleCount = 0;
    signalToggleTarget = static_cast<uint8_t>(jetson_cfg::kLedPowerSignalBlinkCount * 2);
    lastSignalToggle = millis();
    applySolid(kLedColorOff);
}

void LEDController::setAlertActive(bool active) {
    alertActive = active;

    if (alertActive) {
        applySolid(kLedColorAlertRed);
        return;
    }

    if (mode == LedMode::ALERT_RED) {
        mode = LedMode::NORMAL;
    }

    if (mode == LedMode::OFF) {
        applySolid(kLedColorOff);
        return;
    }

    resetPaletteState(millis());
}

void LEDController::updateNormal(unsigned long now) {
    if (paletteSize == 0) return;

    if (paletteSize == 1) {
        applySolid(currentPalette[0]);
        return;
    }

    const unsigned long segmentDuration =
        (changeInterval > 0) ? changeInterval : 1UL;
    const unsigned long blendDuration =
        (transitionTime > segmentDuration) ? segmentDuration : transitionTime;
    const unsigned long holdDuration = segmentDuration - blendDuration;

    uint32_t currentRenderedColor = currentColor;
    const unsigned long elapsedSinceSegmentStart = now - lastTransitionStart;
    if (blendDuration > 0 && elapsedSinceSegmentStart >= holdDuration) {
        const unsigned long blendElapsed = elapsedSinceSegmentStart - holdDuration;
        if (blendElapsed < blendDuration) {
            const float fraction = float(blendElapsed) / float(blendDuration);
            currentRenderedColor = blendColor(currentColor, targetColor, fraction);
        } else {
            currentRenderedColor = targetColor;
        }
    }

    if (paletteInterval > 0 && now - lastPaletteChange >= paletteInterval) {
        lastPaletteChange = now;
        size_t idx = 0;
        for (size_t i = 0; i < PALETTE_COUNT; i++) {
            if (palettes[i] == currentPalette) {
                idx = i;
                break;
            }
        }
        idx = (idx + 1) % PALETTE_COUNT;
        setPalette(palettes[idx], PALETTE_SIZE);

        // Continue smoothly into the next palette without skipping index 1.
        currentColorIndex = static_cast<int>((paletteSize > 0) ? (paletteSize - 1) : 0);
        currentColor = currentRenderedColor;
        targetColor = currentPalette[0];
        lastTransitionStart = now;
    }

    unsigned long elapsed = now - lastTransitionStart;

    while (elapsed >= segmentDuration) {
        currentColor = targetColor;
        currentColorIndex = (currentColorIndex + 1) % paletteSize;
        targetColor = currentPalette[(currentColorIndex + 1) % paletteSize];
        lastTransitionStart += segmentDuration;
        elapsed -= segmentDuration;
    }

    if (blendDuration == 0 || elapsed < holdDuration) {
        applySolid(currentColor);
    } else {
        const unsigned long blendElapsed = elapsed - holdDuration;
        const float fraction = float(blendElapsed) / float(blendDuration);
        uint32_t blended = blendColor(currentColor, targetColor, fraction);
        applySolid(blended);
    }
}

void LEDController::setPalette(const uint32_t* palette, size_t size) {
    currentPalette = palette;
    paletteSize = size;
}

void LEDController::nextPalette() {
    if (PALETTE_COUNT == 0) return;
    size_t idx = 0;
    for (size_t i = 0; i < PALETTE_COUNT; i++) {
        if (palettes[i] == currentPalette) {
            idx = i;
            break;
        }
    }
    idx = (idx + 1) % PALETTE_COUNT;
    setPalette(palettes[idx], PALETTE_SIZE);
    resetPaletteState(millis());
}

uint32_t LEDController::blendColor(uint32_t c1, uint32_t c2, float fraction) {
    uint8_t r1 = (c1 >> 16) & 0xFF;
    uint8_t g1 = (c1 >> 8) & 0xFF;
    uint8_t b1 = c1 & 0xFF;

    uint8_t r2 = (c2 >> 16) & 0xFF;
    uint8_t g2 = (c2 >> 8) & 0xFF;
    uint8_t b2 = c2 & 0xFF;

    uint8_t r = r1 + (r2 - r1) * fraction;
    uint8_t g = g1 + (g2 - g1) * fraction;
    uint8_t b = b1 + (b2 - b1) * fraction;
    return strip.Color(r, g, b);
}

uint32_t LEDController::signalColorForState() const {
    return (state == jetson_cfg::SystemState::BOOTING_ON)
               ? kLedColorSignalGreen
               : kLedColorAlertRed;
}

void LEDController::applySolid(uint32_t color) {
    for (uint16_t i = 0; i < ledCount; i++) {
        strip.setPixelColor(i, color);
    }
    strip.show();
}

void LEDController::updatePowerSignal(unsigned long now) {
    if (signalToggleCount >= signalToggleTarget) {
        mode = (modeBeforeSignal == LedMode::OFF) ? LedMode::OFF : LedMode::NORMAL;
        if (mode == LedMode::OFF) {
            applySolid(kLedColorOff);
        } else {
            resetPaletteState(now);
        }
        return;
    }

    if (now - lastSignalToggle >= jetson_cfg::kLedBlinkTransitionMs) {
        lastSignalToggle = now;
        signalLedOn = !signalLedOn;
        signalToggleCount++;
        applySolid(signalLedOn ? signalColorForState() : kLedColorOff);

        if (signalToggleCount >= signalToggleTarget) {
            mode = (modeBeforeSignal == LedMode::OFF) ? LedMode::OFF : LedMode::NORMAL;
            if (mode == LedMode::OFF) {
                applySolid(kLedColorOff);
            } else {
                resetPaletteState(now);
            }
        }
    }
}

void LEDController::resetPaletteState(unsigned long now) {
    if (PALETTE_COUNT == 0) {
        paletteSize = 0;
        currentPalette = nullptr;
        currentColor = kLedColorOff;
        targetColor = kLedColorOff;
        applySolid(kLedColorOff);
        return;
    }

    if (currentPalette == nullptr || paletteSize == 0) {
        setPalette(palettes[0], PALETTE_SIZE);
    }

    currentColorIndex = 0;
    currentColor = currentPalette[0];
    targetColor = currentPalette[1 % paletteSize];
    lastTransitionStart = now;
    lastPaletteChange = now;
    applySolid(currentColor);
}
