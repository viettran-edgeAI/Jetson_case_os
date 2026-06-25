#ifndef VIRTUAL_KEYBOARD_H
#define VIRTUAL_KEYBOARD_H

#include <Arduino.h>
#include <TFT_eSPI.h>
#include <stdint.h>
#include <stddef.h>

class VirtualKeyboard {
public:
    enum class Event : uint8_t {
        NONE = 0,
        CHANGED,
        SUBMIT,
        CANCEL
    };

    static constexpr size_t MAX_TEXT = 64;

    VirtualKeyboard();

    void reset();
    void draw(TFT_eSPI& tft,
              uint16_t width,
              uint16_t height,
              bool fullRedraw = true,
              TFT_eSprite* inputSprite = nullptr);
    Event handleTouch(int16_t x, int16_t y, uint16_t width, uint16_t height);
    const char* text() const { return _text; }

private:
    char _text[MAX_TEXT + 1];
    size_t _length;
    bool _shift;
    bool _symbols;
    bool _showText;
    bool _keysDirty;

    void appendChar(char c);
    void backspace();
    char keyForCell(uint8_t row, uint8_t col) const;
    void drawInputField(TFT_eSPI& tft, uint16_t width, TFT_eSprite* inputSprite);
    void drawKey(TFT_eSPI& tft,
                 int16_t x,
                 int16_t y,
                 int16_t w,
                 int16_t h,
                 const char* label,
                 uint16_t color);
};

#endif // VIRTUAL_KEYBOARD_H
