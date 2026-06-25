#ifndef LED_PALETTE_H
#define LED_PALETTE_H

#include <Arduino.h>

constexpr size_t PALETTE_SIZE = 8;

extern const uint32_t oceanBreeze[PALETTE_SIZE];
extern const uint32_t sunsetGlow[PALETTE_SIZE];
extern const uint32_t forestSerenity[PALETTE_SIZE];
extern const uint32_t softLavender[PALETTE_SIZE];
extern const uint32_t candlelightComfort[PALETTE_SIZE];
extern const uint32_t polarNight[PALETTE_SIZE];
extern const uint32_t neonCircuit[PALETTE_SIZE];

extern const uint32_t* const palettes[];
extern const size_t PALETTE_COUNT;
extern const uint32_t kLedColorOff;
extern const uint32_t kLedColorAlertRed;
extern const uint32_t kLedColorSignalGreen;

#endif // LED_PALETTE_H
