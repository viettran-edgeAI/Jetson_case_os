#ifndef LCD_1_H
#define LCD_1_H

#include <Arduino.h>
#include <Adafruit_SSD1306.h>
#include "system_configuration.h"

using SystemState = jetson_cfg::SystemState;

class LCD1 {
public:
    static constexpr uint16_t DEFAULT_WIDTH = jetson_cfg::kLcd1Width;
    static constexpr uint16_t DEFAULT_HEIGHT = jetson_cfg::kLcd1Height;
    static constexpr uint8_t DEFAULT_I2C_ADDRESS = jetson_cfg::kLcd1PrimaryI2CAddress;
    static constexpr int8_t DEFAULT_RESET_PIN = jetson_cfg::kLcd1ResetPin;

    LCD1();
    ~LCD1();

    void init(uint16_t width = DEFAULT_WIDTH,
              uint16_t height = DEFAULT_HEIGHT,
              uint8_t i2cAddress = DEFAULT_I2C_ADDRESS,
              int8_t resetPin = DEFAULT_RESET_PIN);

    void onStateChange(SystemState newState);
    void onAlertChange(uint8_t alertMask);
    void startWelcomeEffect();
    bool isWelcomeEffectActive() const;
    void showKernelLine(const char* line);
    void showBootMessage(const char* msg);
    void showPowerOffEffect(const char* label, uint32_t nowMs);
    void showRunningIdle(uint32_t nowMs);
    void update(uint32_t nowMs);

private:
    struct RainDrop {
        int16_t x;
        int16_t y;
        uint8_t length;
        uint8_t speed;
    };

    static constexpr size_t kKernelLineMaxLen = 95;
    static constexpr size_t kBootMessageMaxLen = 63;
    static constexpr size_t kStatusLabelMaxLen = 23;
    static constexpr uint32_t kPowerOffFrameIntervalMs = 45;
    static constexpr uint32_t kRunningIdleFrameIntervalMs = 50;
    static constexpr uint32_t kBootLogoDurationMs = 800;
    static constexpr uint32_t kWelcomeBootCompleteHoldMs = 800;
    static constexpr uint32_t kWelcomeNameScrollInMs = 144;
    static constexpr uint32_t kWelcomeNameHoldMs = 1000;
    static constexpr uint32_t kWelcomeNameScrollOutMs = 144;
    static constexpr uint32_t kWelcomeTaglineScrollInMs = 120;
    static constexpr uint32_t kWelcomeTaglineHoldMs = 1000;
    static constexpr uint32_t kWelcomeTaglineScrollOutMs = 120;
    static constexpr uint32_t kWelcomeFinalPauseMs = 500;
    static constexpr uint32_t kWelcomeDurationMs = kWelcomeBootCompleteHoldMs +
                                                   kWelcomeNameScrollInMs +
                                                   kWelcomeNameHoldMs +
                                                   kWelcomeNameScrollOutMs +
                                                   kWelcomeTaglineScrollInMs +
                                                   kWelcomeTaglineHoldMs +
                                                   kWelcomeTaglineScrollOutMs +
                                                   kWelcomeFinalPauseMs;

    Adafruit_SSD1306* _display;
    uint16_t _width;
    uint16_t _height;
    uint8_t _i2cAddress;
    int8_t _resetPin;

    bool _initialized;
    SystemState _state;

    char _kernelLine[kKernelLineMaxLen + 1];
    char _bootMessage[kBootMessageMaxLen + 1];
    char _statusLabel[kStatusLabelMaxLen + 1];

    uint32_t _lastPowerOffFrameMs;
    uint32_t _lastRunningFrameMs;
    uint32_t _welcomeStartMs;
    uint32_t _bootLogoUntilMs;
    uint8_t _idlePhase;
    bool _bootViewDirty;
    bool _welcomeActive;
    uint8_t _alertMask;

    RainDrop _rainDrops[jetson_cfg::kRainDropCount];

    void safeCopy(char* dst, size_t dstSize, const char* src);
    bool isTransitionState(SystemState state) const;
    const char* stateLabel(SystemState state) const;
    void formatKernelLine(const char* src, char* dst, size_t dstSize);

    void drawTransitionView();
    void drawShortTextLine(int16_t x, int16_t y, const char* text, uint8_t maxChars);
    void drawCenteredTextLine(int16_t y, const char* text, uint8_t maxChars);
    void drawCenteredSizedTextLine(int16_t y, const char* text, uint8_t textSize);
    void drawTextBlock(int16_t x, int16_t y, const char* text, uint8_t maxCharsPerLine, uint8_t maxLines);
    void drawNvidiaLogo();
    void drawWelcomeEffect(uint32_t nowMs);
    void drawWelcomeScrollText(const char* text,
                               uint8_t textSize,
                               uint32_t elapsedMs,
                               uint32_t scrollInMs,
                               uint32_t holdMs,
                               uint32_t scrollOutMs);
    int16_t interpolateY(int16_t startY, int16_t endY, uint32_t elapsedMs, uint32_t durationMs) const;

    void initializeRain();
    void stepRain();
};

#endif // LCD_1_H