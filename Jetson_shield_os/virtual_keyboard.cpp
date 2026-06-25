#include "virtual_keyboard.h"

#include <string.h>

namespace {

constexpr uint16_t kBg = TFT_BLACK;
constexpr uint16_t kFg = TFT_WHITE;
constexpr uint16_t kMuted = TFT_LIGHTGREY;
constexpr uint16_t kFrame = TFT_DARKGREY;
constexpr uint16_t kAccent = TFT_CYAN;
constexpr uint16_t kDanger = TFT_RED;
constexpr uint8_t kKeyRows = 4;
constexpr uint8_t kKeyCols = 10;
constexpr int16_t kKeyGap = 3;
constexpr int16_t kKeyboardTop = 78;
constexpr int16_t kKeyH = 24;
constexpr int16_t kKeyRadius = 5;
constexpr int16_t kInputX = 10;
constexpr int16_t kInputY = 34;
constexpr int16_t kInputH = 30;
constexpr int16_t kEyeW = 30;

const char* alphaRow(uint8_t row) {
    switch (row) {
        case 0: return "qwertyuiop";
        case 1: return "asdfghjkl";
        case 2: return "zxcvbnm";
        default: return "";
    }
}

const char* symbolRow(uint8_t row) {
    switch (row) {
        case 0: return "1234567890";
        case 1: return "!@#$%^&*";
        case 2: return "_-+=.,?";
        default: return "";
    }
}

bool inRect(int16_t x, int16_t y, int16_t rx, int16_t ry, int16_t rw, int16_t rh) {
    return x >= rx && y >= ry && x < rx + rw && y < ry + rh;
}

} // namespace

VirtualKeyboard::VirtualKeyboard()
    : _length(0),
      _shift(false),
      _symbols(false),
      _showText(false),
      _keysDirty(true) {
    _text[0] = '\0';
}

void VirtualKeyboard::reset() {
    _length = 0;
    _shift = false;
    _symbols = false;
    _showText = false;
    _keysDirty = true;
    _text[0] = '\0';
}

void VirtualKeyboard::appendChar(char c) {
    if (_length >= MAX_TEXT) {
        return;
    }
    _text[_length++] = c;
    _text[_length] = '\0';
    if (_shift && !_symbols) {
        _shift = false;
        _keysDirty = true;
    }
}

void VirtualKeyboard::backspace() {
    if (_length == 0) {
        return;
    }
    _text[--_length] = '\0';
}

char VirtualKeyboard::keyForCell(uint8_t row, uint8_t col) const {
    const char* chars = _symbols ? symbolRow(row) : alphaRow(row);
    const size_t len = strlen(chars);
    if (col >= len) {
        return '\0';
    }
    char c = chars[col];
    if (_shift && !_symbols && c >= 'a' && c <= 'z') {
        c = static_cast<char>(c - 'a' + 'A');
    }
    return c;
}

void VirtualKeyboard::drawInputField(TFT_eSPI& tft, uint16_t width, TFT_eSprite* inputSprite) {
    const int16_t inputW = static_cast<int16_t>(width - 20);
    const int16_t eyeX = static_cast<int16_t>(inputW - kEyeW - 4);
    const int16_t eyeY = 4;
    const bool spriteReady = (inputSprite != nullptr && inputSprite->created() &&
                              inputSprite->width() == inputW && inputSprite->height() == kInputH);

    char display[MAX_TEXT + 1];
    if (_showText) {
        strncpy(display, _text, sizeof(display) - 1);
        display[sizeof(display) - 1] = '\0';
    } else {
        for (size_t i = 0; i < _length && i < MAX_TEXT; ++i) {
            display[i] = '*';
        }
        display[_length] = '\0';
    }

    const size_t maxChars = 25;
    const char* visibleText = display;
    if (strlen(display) > maxChars) {
        visibleText = display + (strlen(display) - maxChars);
    }

    if (spriteReady) {
        inputSprite->fillSprite(kBg);
        inputSprite->drawRoundRect(0, 0, inputW, kInputH, 6, kFrame);
        inputSprite->setTextColor(kFg, kBg);
        inputSprite->setTextDatum(TL_DATUM);
        inputSprite->drawString(visibleText, 6, 9, 2);
        inputSprite->fillRoundRect(eyeX, eyeY, kEyeW, static_cast<int16_t>(kInputH - 8), 5, TFT_DARKGREY);
        inputSprite->drawRoundRect(eyeX, eyeY, kEyeW, static_cast<int16_t>(kInputH - 8), 5, kFrame);
        const int16_t eyeCx = static_cast<int16_t>(eyeX + (kEyeW / 2));
        const int16_t eyeCy = static_cast<int16_t>(eyeY + ((kInputH - 8) / 2));
        inputSprite->drawEllipse(eyeCx, eyeCy, 9, 5, _showText ? kAccent : kMuted);
        inputSprite->fillCircle(eyeCx, eyeCy, 2, _showText ? kAccent : kMuted);
        if (!_showText) {
            inputSprite->drawLine(static_cast<int16_t>(eyeX + 7),
                                  static_cast<int16_t>(eyeY + 17),
                                  static_cast<int16_t>(eyeX + 23),
                                  static_cast<int16_t>(eyeY + 5),
                                  kDanger);
        }
        inputSprite->pushSprite(kInputX, kInputY);
        return;
    }

    const int16_t screenEyeX = static_cast<int16_t>(kInputX + eyeX);
    const int16_t screenEyeY = static_cast<int16_t>(kInputY + eyeY);
    tft.fillRoundRect(kInputX, kInputY, inputW, kInputH, 6, kBg);
    tft.drawRoundRect(kInputX, kInputY, inputW, kInputH, 6, kFrame);
    tft.setTextColor(kFg, kBg);
    tft.drawString(visibleText, 16, 43, 2);
    tft.fillRoundRect(screenEyeX, screenEyeY, kEyeW, static_cast<int16_t>(kInputH - 8), 5, TFT_DARKGREY);
    tft.drawRoundRect(screenEyeX, screenEyeY, kEyeW, static_cast<int16_t>(kInputH - 8), 5, kFrame);
    const int16_t eyeCx = static_cast<int16_t>(screenEyeX + (kEyeW / 2));
    const int16_t eyeCy = static_cast<int16_t>(screenEyeY + ((kInputH - 8) / 2));
    tft.drawEllipse(eyeCx, eyeCy, 9, 5, _showText ? kAccent : kMuted);
    tft.fillCircle(eyeCx, eyeCy, 2, _showText ? kAccent : kMuted);
    if (!_showText) {
        tft.drawLine(static_cast<int16_t>(screenEyeX + 7),
                     static_cast<int16_t>(screenEyeY + 17),
                     static_cast<int16_t>(screenEyeX + 23),
                     static_cast<int16_t>(screenEyeY + 5),
                     kDanger);
    }
}

void VirtualKeyboard::drawKey(TFT_eSPI& tft,
                              int16_t x,
                              int16_t y,
                              int16_t w,
                              int16_t h,
                              const char* label,
                              uint16_t color) {
    const uint16_t fill = (color == kDanger) ? TFT_MAROON : ((color == kAccent) ? TFT_DARKCYAN : TFT_DARKGREY);
    tft.fillRoundRect(x, y, w, h, kKeyRadius, fill);
    tft.drawRoundRect(x, y, w, h, kKeyRadius, kFrame);
    tft.setTextColor(color, fill);
    tft.setTextDatum(MC_DATUM);
    tft.drawString(label, static_cast<int16_t>(x + (w / 2)), static_cast<int16_t>(y + (h / 2)), 1);
    tft.setTextDatum(TL_DATUM);
}

void VirtualKeyboard::draw(TFT_eSPI& tft,
                           uint16_t width,
                           uint16_t height,
                           bool fullRedraw,
                           TFT_eSprite* inputSprite) {
    (void)height;
    if (fullRedraw) {
        tft.fillScreen(kBg);
        tft.drawRoundRect(0, 0, width, height, 8, kFrame);
        tft.setTextColor(kFg, kBg);
        tft.setTextDatum(TL_DATUM);
        tft.drawString("Wi-Fi Password", 10, 8, 2);
    }

    drawInputField(tft, width, inputSprite);

    if (fullRedraw || _keysDirty) {
        const int16_t keyW = static_cast<int16_t>((width - 12 - ((kKeyCols - 1) * kKeyGap)) / kKeyCols);
        for (uint8_t row = 0; row < 3; ++row) {
            const char* chars = _symbols ? symbolRow(row) : alphaRow(row);
            const uint8_t count = static_cast<uint8_t>(strlen(chars));
            const int16_t rowW = static_cast<int16_t>((count * keyW) + ((count - 1) * kKeyGap));
            const int16_t startX = static_cast<int16_t>((width - rowW) / 2);
            const int16_t y = static_cast<int16_t>(kKeyboardTop + (row * (kKeyH + kKeyGap)));
            for (uint8_t col = 0; col < count; ++col) {
                char label[2] = {keyForCell(row, col), '\0'};
                drawKey(tft,
                        static_cast<int16_t>(startX + (col * (keyW + kKeyGap))),
                        y,
                        keyW,
                        kKeyH,
                        label,
                        kFg);
            }
        }

        const int16_t bottomY = static_cast<int16_t>(kKeyboardTop + (3 * (kKeyH + kKeyGap)) + 2);
        drawKey(tft, 8, bottomY, 44, 26, _symbols ? "ABC" : "123", kAccent);
        drawKey(tft, 56, bottomY, 44, 26, _shift ? "CAP" : "SHIFT", kAccent);
        drawKey(tft, 104, bottomY, 58, 26, "SPACE", kFg);
        drawKey(tft, 166, bottomY, 42, 26, "DEL", kDanger);
        drawKey(tft, 212, bottomY, 46, 26, "CANCEL", kDanger);
        drawKey(tft, 262, bottomY, 50, 26, "CONNECT", TFT_GREEN);
        _keysDirty = false;
    }
}

VirtualKeyboard::Event VirtualKeyboard::handleTouch(int16_t x, int16_t y, uint16_t width, uint16_t height) {
    (void)height;
    const int16_t inputW = static_cast<int16_t>(width - 20);
    const int16_t eyeX = static_cast<int16_t>(kInputX + inputW - kEyeW - 4);
    const int16_t eyeY = static_cast<int16_t>(kInputY + 4);
    if (inRect(x, y, eyeX, eyeY, kEyeW, static_cast<int16_t>(kInputH - 8))) {
        _showText = !_showText;
        return Event::CHANGED;
    }

    const int16_t keyW = static_cast<int16_t>((width - 12 - ((kKeyCols - 1) * kKeyGap)) / kKeyCols);

    for (uint8_t row = 0; row < 3; ++row) {
        const char* chars = _symbols ? symbolRow(row) : alphaRow(row);
        const uint8_t count = static_cast<uint8_t>(strlen(chars));
        const int16_t rowW = static_cast<int16_t>((count * keyW) + ((count - 1) * kKeyGap));
        const int16_t startX = static_cast<int16_t>((width - rowW) / 2);
        const int16_t rowY = static_cast<int16_t>(kKeyboardTop + (row * (kKeyH + kKeyGap)));
        for (uint8_t col = 0; col < count; ++col) {
            const int16_t keyX = static_cast<int16_t>(startX + (col * (keyW + kKeyGap)));
            if (inRect(x, y, keyX, rowY, keyW, kKeyH)) {
                appendChar(keyForCell(row, col));
                return Event::CHANGED;
            }
        }
    }

    const int16_t bottomY = static_cast<int16_t>(kKeyboardTop + (3 * (kKeyH + kKeyGap)) + 2);
    if (inRect(x, y, 8, bottomY, 44, 26)) {
        _symbols = !_symbols;
        _keysDirty = true;
        return Event::CHANGED;
    }
    if (inRect(x, y, 56, bottomY, 44, 26)) {
        _shift = !_shift;
        _keysDirty = true;
        return Event::CHANGED;
    }
    if (inRect(x, y, 104, bottomY, 58, 26)) {
        appendChar(' ');
        return Event::CHANGED;
    }
    if (inRect(x, y, 166, bottomY, 42, 26)) {
        backspace();
        return Event::CHANGED;
    }
    if (inRect(x, y, 212, bottomY, 46, 26)) {
        return Event::CANCEL;
    }
    if (inRect(x, y, 262, bottomY, 50, 26)) {
        return Event::SUBMIT;
    }

    return Event::NONE;
}
