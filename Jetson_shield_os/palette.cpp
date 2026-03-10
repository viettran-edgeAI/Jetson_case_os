#include "palette.h"

const uint32_t oceanBreeze[PALETTE_SIZE] = {
    0x004080,
    0x0080C0,
    0x00A0A0,
    0x00C8C8,
    0x0080FF,
    0x0050A0
};

const uint32_t sunsetGlow[PALETTE_SIZE] = {
    0xFF6600,
    0xFF9933,
    0xFF6666,
    0xFF5080,
    0xFF3333,
    0xFF8040
};

const uint32_t forestSerenity[PALETTE_SIZE] = {
    0x228B22,
    0x55AA55,
    0x8BC54A,
    0x669966,
    0x336633,
    0xB4D28C
};

const uint32_t softLavender[PALETTE_SIZE] = {
    0x663399,
    0x9966CC,
    0xCC99FF,
    0xFFCCFF,
    0xE6C8FF,
    0xBEAAFF
};

const uint32_t candlelightComfort[PALETTE_SIZE] = {
    0xFF8C00,
    0xFFA532,
    0xFFB450,
    0xFFC878,
    0xFFDC96,
    0xFFF0B4
};

const uint32_t* const palettes[] = { oceanBreeze, sunsetGlow, forestSerenity, softLavender, candlelightComfort };
const size_t PALETTE_COUNT = sizeof(palettes) / sizeof(palettes[0]);
const uint32_t kLedColorOff = 0x000000;
const uint32_t kLedColorAlertRed = 0xFF0000;
const uint32_t kLedColorSignalGreen = 0x76B900;
