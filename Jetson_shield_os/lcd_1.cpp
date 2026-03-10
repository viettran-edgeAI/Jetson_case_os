#include "lcd_1.h"

#include "logo.h"

#include <Wire.h>
#include <new>

LCD1::LCD1()
    : _display(nullptr),
      _width(DEFAULT_WIDTH),
      _height(DEFAULT_HEIGHT),
      _i2cAddress(DEFAULT_I2C_ADDRESS),
      _resetPin(DEFAULT_RESET_PIN),
      _initialized(false),
      _state(SystemState::POWER_OFF),
      _lastPowerOffFrameMs(0),
      _lastRunningFrameMs(0),
    _welcomeStartMs(0),
    _bootLogoUntilMs(0),
    _idlePhase(0),
    _bootViewDirty(true),
    _welcomeActive(false),
    _alertMask(0) {
    _kernelLine[0] = '\0';
    _bootMessage[0] = '\0';
    _statusLabel[0] = '\0';
}

LCD1::~LCD1() {
    if (_display != nullptr) {
        delete _display;
        _display = nullptr;
    }
}

void LCD1::init(uint16_t width, uint16_t height, uint8_t i2cAddress, int8_t resetPin) {
    if (_display != nullptr) {
        delete _display;
        _display = nullptr;
    }

    _width = (width == 0) ? DEFAULT_WIDTH : width;
    _height = (height == 0) ? DEFAULT_HEIGHT : height;
    _i2cAddress = i2cAddress;
    _resetPin = resetPin;

    _display = new (std::nothrow) Adafruit_SSD1306(static_cast<uint8_t>(_width), static_cast<uint8_t>(_height), &Wire, _resetPin);
    if (_display == nullptr) {
        _initialized = false;
        return;
    }

    if (!_display->begin(SSD1306_SWITCHCAPVCC, _i2cAddress)) {
        delete _display;
        _display = nullptr;
        _initialized = false;
        return;
    }

    _display->clearDisplay();
    _display->setTextSize(1);
    _display->setTextColor(SSD1306_WHITE);
    _display->setTextWrap(false);
    _display->display();

    initializeRain();
    _idlePhase = 0;
    _lastPowerOffFrameMs = 0;
    _lastRunningFrameMs = 0;
    _welcomeStartMs = 0;
    _bootLogoUntilMs = 0;
    _bootViewDirty = true;
    _welcomeActive = false;
    _initialized = true;

    if (_statusLabel[0] == '\0') {
        safeCopy(_statusLabel, sizeof(_statusLabel), "POWER OFF");
    }

    update(millis());
}

void LCD1::onStateChange(SystemState newState) {
    const SystemState oldState = _state;
    _state = newState;

    _kernelLine[0] = '\0';
    if (_state == SystemState::POWER_OFF || _state == SystemState::SHUTTING_DOWN) {
        _welcomeActive = false;
        _welcomeStartMs = 0;
    }

    if (_state == SystemState::POWER_OFF) {
        initializeRain();
        _bootMessage[0] = '\0';
        _bootLogoUntilMs = 0;
        safeCopy(_statusLabel, sizeof(_statusLabel), "POWER OFF");
    } else if (_state == SystemState::BOOTING_ON) {
        safeCopy(_bootMessage, sizeof(_bootMessage), "Booting Jetson");
        _bootLogoUntilMs = (oldState == SystemState::POWER_OFF)
                               ? (millis() + kBootLogoDurationMs)
                               : 0;
    } else if (_state == SystemState::RUNNING) {
        _bootLogoUntilMs = 0;
        if (_bootMessage[0] == '\0') {
            safeCopy(_bootMessage, sizeof(_bootMessage), "Waiting stats");
        }
    } else if (_state == SystemState::SHUTTING_DOWN) {
        safeCopy(_bootMessage, sizeof(_bootMessage), "Shutting down");
        _bootLogoUntilMs = 0;
    }

    _lastRunningFrameMs = 0;
    _bootViewDirty = true;
}

void LCD1::onAlertChange(uint8_t alertMask) {
    _alertMask = alertMask;
}

void LCD1::startWelcomeEffect() {
    if (!_initialized || _display == nullptr) {
        return;
    }

    _welcomeActive = true;
    _welcomeStartMs = millis();
    _bootLogoUntilMs = 0;
    _lastRunningFrameMs = 0;
}

bool LCD1::isWelcomeEffectActive() const {
    return _welcomeActive;
}

void LCD1::showKernelLine(const char* line) {
    formatKernelLine(line, _kernelLine, sizeof(_kernelLine));
    if (isTransitionState(_state)) {
        _bootViewDirty = true;
    } else {
        _lastRunningFrameMs = 0;
    }
}

void LCD1::showBootMessage(const char* msg) {
    safeCopy(_bootMessage, sizeof(_bootMessage), msg);
    if (_state == SystemState::POWER_OFF && msg != nullptr && msg[0] != '\0') {
        safeCopy(_statusLabel, sizeof(_statusLabel), msg);
    }
    if (isTransitionState(_state)) {
        _bootViewDirty = true;
    } else {
        _lastRunningFrameMs = 0;
    }
}

void LCD1::showPowerOffEffect(const char* label, uint32_t nowMs) {
    if (!_initialized || _display == nullptr) {
        return;
    }

    safeCopy(_statusLabel, sizeof(_statusLabel), label);

    if ((nowMs - _lastPowerOffFrameMs) < kPowerOffFrameIntervalMs) {
        return;
    }
    _lastPowerOffFrameMs = nowMs;

    stepRain();

    _display->clearDisplay();
    for (size_t i = 0; i < jetson_cfg::kRainDropCount; ++i) {
        const RainDrop& drop = _rainDrops[i];
        _display->drawLine(drop.x, drop.y, drop.x, drop.y + drop.length, SSD1306_WHITE);
    }

    drawCenteredTextLine(static_cast<int16_t>(_height > 16 ? (_height - 10) : 0), _statusLabel, 16);
    _display->display();
}

void LCD1::showRunningIdle(uint32_t nowMs) {
    if (!_initialized || _display == nullptr) {
        return;
    }

    if (!_bootViewDirty && _lastRunningFrameMs != 0) {
        return;
    }
    _lastRunningFrameMs = nowMs;
    _bootViewDirty = false;

    _display->clearDisplay();
    drawNvidiaLogo();
    _display->display();
}

void LCD1::update(uint32_t nowMs) {
    if (!_initialized || _display == nullptr) {
        return;
    }

    if (_welcomeActive) {
        drawWelcomeEffect(nowMs);
        return;
    }

    if (_state == SystemState::BOOTING_ON && _bootLogoUntilMs != 0U) {
        if (nowMs < _bootLogoUntilMs) {
            _display->clearDisplay();
            drawNvidiaLogo();
            _display->display();
            return;
        }

        _bootLogoUntilMs = 0;
        _bootViewDirty = true;
    }

    if (isTransitionState(_state)) {
        if (_bootViewDirty) {
            drawTransitionView();
            _bootViewDirty = false;
        }
        return;
    }

    if (_state == SystemState::POWER_OFF) {
        const char* label = (_statusLabel[0] != '\0') ? _statusLabel : "POWER OFF";
        showPowerOffEffect(label, nowMs);
        return;
    }

    showRunningIdle(nowMs);
}

void LCD1::safeCopy(char* dst, size_t dstSize, const char* src) {
    if (dst == nullptr || dstSize == 0) {
        return;
    }

    if (src == nullptr) {
        dst[0] = '\0';
        return;
    }

    size_t i = 0;
    while (i + 1 < dstSize && src[i] != '\0') {
        dst[i] = src[i];
        ++i;
    }
    dst[i] = '\0';
}

bool LCD1::isTransitionState(SystemState state) const {
    return (state == SystemState::BOOTING_ON || state == SystemState::SHUTTING_DOWN);
}

const char* LCD1::stateLabel(SystemState state) const {
    switch (state) {
        case SystemState::BOOTING_ON:
            return "BOOTING";
        case SystemState::RUNNING:
            return "RUNNING";
        case SystemState::SHUTTING_DOWN:
            return "SHUTDOWN";
        case SystemState::POWER_OFF:
        default:
            return "POWER OFF";
    }
}

void LCD1::formatKernelLine(const char* src, char* dst, size_t dstSize) {
    if (dst == nullptr || dstSize == 0) {
        return;
    }

    if (src == nullptr || src[0] == '\0') {
        dst[0] = '\0';
        return;
    }

    const char* payload = src;

    if (src[0] == '[') {
        const char* closing = strstr(src, "] ");
        if (closing != nullptr) {
            payload = closing + 2;
        }
    }

    safeCopy(dst, dstSize, payload);
}

void LCD1::drawTransitionView() {
    if (_display == nullptr) {
        return;
    }

    _display->clearDisplay();

    if (_bootMessage[0] != '\0') {
        drawShortTextLine(0, 0, _bootMessage, 21);
    }

    if (_kernelLine[0] != '\0') {
        drawTextBlock(0, 10, _kernelLine, 21, 2);
    }

    _display->display();
}

void LCD1::drawShortTextLine(int16_t x, int16_t y, const char* text, uint8_t maxChars) {
    if (_display == nullptr || text == nullptr || maxChars == 0) {
        return;
    }

    char line[32];
    const size_t limit = (maxChars < (sizeof(line) - 1)) ? maxChars : (sizeof(line) - 1);
    size_t i = 0;
    while (i < limit && text[i] != '\0') {
        line[i] = text[i];
        ++i;
    }
    line[i] = '\0';

    _display->setCursor(x, y);
    _display->print(line);
}

void LCD1::drawCenteredTextLine(int16_t y, const char* text, uint8_t maxChars) {
    if (_display == nullptr || text == nullptr || maxChars == 0) {
        return;
    }

    char line[32];
    const size_t limit = (maxChars < (sizeof(line) - 1)) ? maxChars : (sizeof(line) - 1);

    size_t len = 0;
    while (len < limit && text[len] != '\0') {
        line[len] = text[len];
        ++len;
    }
    line[len] = '\0';

    const int16_t textWidth = static_cast<int16_t>(len * 6U);
    int16_t x = 0;
    if (_width > static_cast<uint16_t>(textWidth)) {
        x = static_cast<int16_t>((_width - static_cast<uint16_t>(textWidth)) / 2U);
    }

    _display->setCursor(x, y);
    _display->print(line);
}

void LCD1::drawCenteredSizedTextLine(int16_t y, const char* text, uint8_t textSize) {
    if (_display == nullptr || text == nullptr || text[0] == '\0') {
        return;
    }

    const uint8_t size = (textSize == 0U) ? 1U : textSize;
    const size_t len = strlen(text);
    const int16_t textWidth = static_cast<int16_t>(len * 6U * size);
    int16_t x = 0;
    if (_width > static_cast<uint16_t>(textWidth)) {
        x = static_cast<int16_t>((_width - static_cast<uint16_t>(textWidth)) / 2U);
    }

    _display->setTextSize(size);
    _display->setCursor(x, y);
    _display->print(text);
    _display->setTextSize(1);
}

void LCD1::drawTextBlock(int16_t x,
                         int16_t y,
                         const char* text,
                         uint8_t maxCharsPerLine,
                         uint8_t maxLines) {
    if (_display == nullptr || text == nullptr || maxCharsPerLine == 0 || maxLines == 0) {
        return;
    }

    size_t index = 0;
    const size_t textLen = strlen(text);
    for (uint8_t line = 0; line < maxLines && index < textLen; ++line) {
        char buffer[32];
        size_t count = 0;
        while (count < (sizeof(buffer) - 1) && count < maxCharsPerLine && index < textLen) {
            buffer[count++] = text[index++];
        }
        buffer[count] = '\0';
        drawShortTextLine(x, y + static_cast<int16_t>(line * 12), buffer, maxCharsPerLine);
    }
}

void LCD1::drawWelcomeEffect(uint32_t nowMs) {
    if (_display == nullptr) {
        return;
    }

    const uint32_t elapsedMs = nowMs - _welcomeStartMs;
    if (elapsedMs >= kWelcomeDurationMs) {
        _welcomeActive = false;
        _bootViewDirty = true;
        _lastRunningFrameMs = 0;
        showRunningIdle(nowMs);
        return;
    }

    _display->clearDisplay();

    uint32_t phaseStartMs = 0;
    if (elapsedMs < kWelcomeBootCompleteHoldMs) {
        drawCenteredTextLine(10, "Boot Complete !", 21);
    } else {
        phaseStartMs += kWelcomeBootCompleteHoldMs;

        if (elapsedMs < (phaseStartMs + kWelcomeNameScrollInMs + kWelcomeNameHoldMs + kWelcomeNameScrollOutMs)) {
            drawWelcomeScrollText("Viet Tran",
                                  2,
                                  elapsedMs - phaseStartMs,
                                  kWelcomeNameScrollInMs,
                                  kWelcomeNameHoldMs,
                                  kWelcomeNameScrollOutMs);
        } else {
            phaseStartMs += kWelcomeNameScrollInMs + kWelcomeNameHoldMs + kWelcomeNameScrollOutMs;
            if (elapsedMs < (phaseStartMs + kWelcomeTaglineScrollInMs + kWelcomeTaglineHoldMs + kWelcomeTaglineScrollOutMs)) {
                drawWelcomeScrollText("_ Jetson Orin Nano _",
                                      1,
                                      elapsedMs - phaseStartMs,
                                      kWelcomeTaglineScrollInMs,
                                      kWelcomeTaglineHoldMs,
                                      kWelcomeTaglineScrollOutMs);
            }
        }
    }

    _display->display();
}

void LCD1::drawWelcomeScrollText(const char* text,
                                 uint8_t textSize,
                                 uint32_t elapsedMs,
                                 uint32_t scrollInMs,
                                 uint32_t holdMs,
                                 uint32_t scrollOutMs) {
    if (_display == nullptr || text == nullptr || text[0] == '\0') {
        return;
    }

    const uint8_t size = (textSize == 0U) ? 1U : textSize;
    const int16_t textHeight = static_cast<int16_t>(8U * size);
    const int16_t centeredY = static_cast<int16_t>((static_cast<int32_t>(_height) - textHeight) / 2);
    const int16_t startY = static_cast<int16_t>(-textHeight);
    const int16_t endY = static_cast<int16_t>(_height);

    int16_t y = centeredY;
    if (elapsedMs < scrollInMs) {
        y = interpolateY(startY, centeredY, elapsedMs, scrollInMs);
    } else if (elapsedMs < (scrollInMs + holdMs)) {
        y = centeredY;
    } else {
        y = interpolateY(centeredY,
                         endY,
                         elapsedMs - scrollInMs - holdMs,
                         scrollOutMs);
    }

    drawCenteredSizedTextLine(y, text, size);
}

void LCD1::drawNvidiaLogo() {
    if (_display == nullptr) {
        return;
    }

    _display->drawBitmap(0, 0, nvidia_logo, 128, 32, SSD1306_WHITE);
}

int16_t LCD1::interpolateY(int16_t startY,
                           int16_t endY,
                           uint32_t elapsedMs,
                           uint32_t durationMs) const {
    if (durationMs == 0U || elapsedMs >= durationMs) {
        return endY;
    }

    const int32_t delta = static_cast<int32_t>(endY) - static_cast<int32_t>(startY);
    return static_cast<int16_t>(startY + ((delta * static_cast<int32_t>(elapsedMs)) / static_cast<int32_t>(durationMs)));
}

void LCD1::initializeRain() {
    for (size_t i = 0; i < jetson_cfg::kRainDropCount; ++i) {
        _rainDrops[i].x = static_cast<int16_t>(random(0, _width));
        _rainDrops[i].y = static_cast<int16_t>(random(-static_cast<int32_t>(_height), 0));
        _rainDrops[i].length = static_cast<uint8_t>(random(3, 8));
        _rainDrops[i].speed = static_cast<uint8_t>(random(1, 4));
    }
}

void LCD1::stepRain() {
    for (size_t i = 0; i < jetson_cfg::kRainDropCount; ++i) {
        RainDrop& drop = _rainDrops[i];
        drop.y = static_cast<int16_t>(drop.y + drop.speed);
        if (drop.y > static_cast<int16_t>(_height)) {
            drop.x = static_cast<int16_t>(random(0, _width));
            drop.y = static_cast<int16_t>(random(-static_cast<int32_t>(_height), 0));
            drop.length = static_cast<uint8_t>(random(3, 8));
            drop.speed = static_cast<uint8_t>(random(1, 4));
        }
    }
}
