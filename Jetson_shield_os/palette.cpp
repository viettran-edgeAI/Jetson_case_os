#include "palette.h"

const uint32_t oceanBreeze[PALETTE_SIZE] = {
    0x004080,
    0x0080C0,
    0x00A0A0,
    0x00C8C8,
    0x0080FF,
    0x0050A0,
    0x2E8BFF,
    0x00B4D8
};

const uint32_t sunsetGlow[PALETTE_SIZE] = {
    0xFF6600,
    0xFF9933,
    0xFF6666,
    0xFF5080,
    0xFF3333,
    0xFF8040,
    0xFFC04D,
    0xFF5A36
};

const uint32_t forestSerenity[PALETTE_SIZE] = {
    0x228B22,
    0x55AA55,
    0x8BC54A,
    0x669966,
    0x336633,
    0xB4D28C,
    0x7FB069,
    0x4E7D4E
};

const uint32_t softLavender[PALETTE_SIZE] = {
    0x663399,
    0x9966CC,
    0xCC99FF,
    0xFFCCFF,
    0xE6C8FF,
    0xBEAAFF,
    0xA77BFF,
    0xD6B8FF
};

const uint32_t candlelightComfort[PALETTE_SIZE] = {
    0xFF8C00,
    0xFFA532,
    0xFFB450,
    0xFFC878,
    0xFFDC96,
    0xFFF0B4,
    0xFFE39A,
    0xFFB86B
};

const uint32_t polarNight[PALETTE_SIZE] = {
    0x0A1A2A,
    0x123A5A,
    0x1E5F8A,
    0x2F7FB0,
    0x49A6D9,
    0x6BC3EA,
    0x8EDAF6,
    0x5E9FD1
};

const uint32_t neonCircuit[PALETTE_SIZE] = {
    0x00FF9C,
    0x00E5FF,
    0x00B3FF,
    0x7A5CFF,
    0xD34BFF,
    0xFF4FA3,
    0xFF7C4D,
    0x5CFF6B
};

const uint32_t* const palettes[] = {
    oceanBreeze,
    sunsetGlow,
    forestSerenity,
    softLavender,
    candlelightComfort,
    polarNight,
    neonCircuit
};
const size_t PALETTE_COUNT = sizeof(palettes) / sizeof(palettes[0]);
const uint32_t kLedColorOff = 0x000000;
const uint32_t kLedColorAlertRed = 0xFF0000;
const uint32_t kLedColorSignalGreen = 0x76B900;
