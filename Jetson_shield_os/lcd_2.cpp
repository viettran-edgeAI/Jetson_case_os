#include "lcd_2.h"

#include <FS.h>
#include <LittleFS.h>

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#ifdef ESP32
#include <esp_timer.h>
#endif

#ifdef SMOOTH_FONT
#include "NotoSansBold10.h"
#include "NotoSansBold12.h"
#include "NotoSansBold15.h"
#include "NotoSansBold36.h"
#endif

namespace {

constexpr uint16_t kBackgroundColor = TFT_BLACK;
constexpr uint16_t kFrameColor = TFT_DARKGREY;
constexpr uint16_t kGridColor = TFT_DARKGREY;
constexpr uint16_t kTextColor = TFT_WHITE;
constexpr uint16_t kMutedTextColor = TFT_LIGHTGREY;
constexpr uint16_t kCpuLineColor = TFT_CYAN;
constexpr uint16_t kCpuFillColor = TFT_DARKCYAN;
constexpr uint16_t kGpuLineColor = TFT_ORANGE;
constexpr uint16_t kGpuFillColor = TFT_MAROON;
constexpr uint16_t kRamLineColor = TFT_GREEN;
constexpr uint16_t kRamFillColor = TFT_DARKGREEN;
constexpr uint16_t kLedAccentColor = TFT_YELLOW;
constexpr uint16_t kGpuAccentColor = TFT_ORANGE;
constexpr uint16_t kPowerAccentColor = TFT_MAGENTA;
constexpr uint16_t kPowerGraphColor = TFT_YELLOW;
constexpr uint16_t kSelectedModeColor = TFT_BLUE;
constexpr uint16_t kStatusOkColor = TFT_GREEN;
constexpr uint16_t kStatusAlertColor = TFT_RED;

constexpr int16_t kOuterMargin = 6;
constexpr int16_t kSectionGap = 2;
constexpr int16_t kGraphFooterHeight = 14;
constexpr int16_t kGraphHeaderHeight = 12;
constexpr int16_t kGraphAxisLabelWidth = 44;
constexpr int16_t kGraphPlotWidth = 240;
constexpr int16_t kGraphPlotHeight = 33;
constexpr int16_t kGraphPlotShiftX = 16;
constexpr int16_t kGraphFrameThickness = 2;
constexpr int16_t kStatusBarHeight = 46;
constexpr int16_t kMetricCardGap = 4;
constexpr uint32_t kTouchScanPeriodMs = 20;
constexpr uint32_t kTouchUiRefreshPeriodMs = 33;
constexpr uint32_t kGraphAnimationFrameMs = 125;
constexpr uint32_t kWifiScanTimeoutMs = 20000;
constexpr uint32_t kIpRequestTimeoutMs = 10000;
constexpr uint32_t kWifiConnectTimeoutMs = 20000;
constexpr uint32_t kNgrokActionPollMs = 500;
constexpr int16_t kSettingsCornerRadius = 6;
constexpr int16_t kWifiListTop = 62;
constexpr int16_t kWifiRowHeight = 26;
constexpr int16_t kWifiRowGap = 4;
constexpr int16_t kDashboardSwipeMinDistance = 54;
constexpr int16_t kDashboardSwipeMaxVertical = 42;
constexpr int16_t kSettingsPanelButtonW = 30;
constexpr int16_t kSettingsPanelButtonH = 30;
constexpr int32_t kMinRateGraphScaleKbps = 64;
constexpr int32_t kDefaultNetGraphScaleKbps = 1024;
constexpr int32_t kDefaultDiskGraphScaleKbps = 4096;
constexpr int32_t kDefaultSwapGraphScaleMb = 19 * 1024;
constexpr int32_t kDefaultPowerGraphScaleMw = 25000;
constexpr uint32_t kBootLogRefreshPeriodMs = 16;
constexpr uint32_t kTouchCalibrationMagic = 0x4A534F53UL;
constexpr uint8_t kTouchCalibrationVersion = 1;
constexpr uint8_t kBootSpriteInk = 1;
constexpr uint8_t kBootSpritePaper = 0;
constexpr int16_t kBootHeaderHeight = 16;
constexpr int16_t kBootTextInsetX = 4;
constexpr int16_t kBootTextInsetY = 3;
constexpr int16_t kBootLineHeight = 10;

// ---- POWER_OFF screen and game constants --------------------------------
// 1-bit sprite colours
constexpr uint8_t kGameSpriteInk   = 1;
constexpr uint8_t kGameSpritePaper = 0;

// Frame timing
constexpr uint32_t kPowerOffRefreshMs    = 8000UL;  // idle screen env-data refresh
constexpr uint32_t kGameFramePeriodMs    = 16U;     // 60 Hz target when POWER_OFF is active

// "GAMES" button geometry (screen-coordinate-independent fractions are
// computed at runtime using _width/_height)
constexpr int16_t kGameBtnH       = 34;
constexpr int16_t kGameBtnW       = 108;
constexpr int16_t kGameBtnMarginB = 14;   // px from screen bottom
constexpr int16_t kMenuItemH      = 36;
constexpr uint8_t kMenuItemCount  = 2;
constexpr int16_t kExitBtnW       = 34;
constexpr int16_t kExitBtnH       = 22;
constexpr int16_t kExitBtnOffsetX = 36;
constexpr int16_t kExitBtnY       = 2;
constexpr int16_t kGameOverMenuW  = 168;
constexpr int16_t kGameOverMenuH  = 74;
constexpr int16_t kGameOverBtnW   = 72;
constexpr int16_t kGameOverBtnH   = 20;
constexpr int16_t kGameOverBtnPad = 8;
constexpr int16_t kSettingsBtnSize = 28;
constexpr int16_t kSettingsBtnMargin = 6;
constexpr int16_t kSettingsIconSize = 16;
constexpr int16_t kPowerOffSettingsPanelW = 30;
constexpr int16_t kPowerOffSettingsTopGap = 2;
constexpr int16_t kPowerOffSliderTopInset = 2;
constexpr int16_t kPowerOffSliderBottomInset = 3;

// 16x16 monochrome settings-gear icon (MSB first per row), authored for standby UI.
const uint8_t kSettingsIconBitmap[] PROGMEM = {
  0xfe, 0x7f, 0xfc, 0x3f, 0xc4, 0x23, 0xc0, 0x03, 0xc0, 0x03, 0xe3, 0xc7,
  0x87, 0xe1, 0x06, 0x60, 0x06, 0x60, 0x87, 0xe1, 0xe3, 0xc7, 0xc0, 0x03,
  0xc0, 0x03, 0xc4, 0x23, 0xfc, 0x3f, 0xfe, 0x7f
};

void drawSettingsTinyText(TFT_eSPI& tft, const char* text, int16_t x, int16_t y) {
#ifdef SMOOTH_FONT
    tft.drawString(text, x, y);
#else
    tft.drawString(text, x, y, 1);
#endif
}

void drawSettingsHeadingText(TFT_eSPI& tft, const char* text, int16_t x, int16_t y) {
#ifdef SMOOTH_FONT
    tft.unloadFont();
    tft.loadFont(NotoSansBold12);
    tft.drawString(text, x, y);
    tft.unloadFont();
    tft.loadFont(NotoSansBold10);
#else
    tft.drawString(text, x, y, 2);
#endif
}

void drawInvertedBitmap(TFT_eSPI& tft,
                         int16_t x,
                         int16_t y,
                         const uint8_t* bitmap,
                         uint8_t width,
                         uint8_t height,
                         uint16_t color) {
    if (bitmap == nullptr || width == 0 || height == 0) {
        return;
    }

    const uint8_t bytesPerRow = static_cast<uint8_t>((width + 7) / 8);
    for (uint8_t row = 0; row < height; ++row) {
        for (uint8_t col = 0; col < width; ++col) {
            const uint16_t index = static_cast<uint16_t>(row) * bytesPerRow + (col / 8);
            const uint8_t bits = pgm_read_byte(bitmap + index);
            if ((bits & static_cast<uint8_t>(0x80U >> (col % 8))) == 0U) {
                tft.drawPixel(static_cast<int16_t>(x + col), static_cast<int16_t>(y + row), color);
            }
        }
    }
}

void drawInvertedBitmap(TFT_eSprite& sprite,
                         int16_t x,
                         int16_t y,
                         const uint8_t* bitmap,
                         uint8_t width,
                         uint8_t height,
                         uint16_t color) {
    if (bitmap == nullptr || width == 0 || height == 0) {
        return;
    }

    const uint8_t bytesPerRow = static_cast<uint8_t>((width + 7) / 8);
    for (uint8_t row = 0; row < height; ++row) {
        for (uint8_t col = 0; col < width; ++col) {
            const uint16_t index = static_cast<uint16_t>(row) * bytesPerRow + (col / 8);
            const uint8_t bits = pgm_read_byte(bitmap + index);
            if ((bits & static_cast<uint8_t>(0x80U >> (col % 8))) == 0U) {
                sprite.drawPixel(static_cast<int16_t>(x + col), static_cast<int16_t>(y + row), color);
            }
        }
    }
}

void drawPowerOffButton(TFT_eSPI& tft,
                        int16_t x,
                        int16_t y,
                        int16_t w,
                        int16_t h,
                        bool active,
                        const char* line1,
                        const char* line2) {
    const uint16_t borderColor = active ? TFT_LIGHTGREY : TFT_DARKGREY;
    const uint16_t bgColor = TFT_BLACK;
    const int16_t radius = 6;
    const int16_t cx = static_cast<int16_t>(x + (w / 2));
    const int16_t cy = static_cast<int16_t>(y + (h / 2));

    tft.fillRoundRect(x, y, w, h, radius, bgColor);
    tft.drawRoundRect(x, y, w, h, radius, borderColor);
    if (active) {
        tft.drawRoundRect(static_cast<int16_t>(x + 1),
                          static_cast<int16_t>(y + 1),
                          static_cast<int16_t>(w - 2),
                          static_cast<int16_t>(h - 2),
                          static_cast<int16_t>(radius - 1),
                          borderColor);
    }

    tft.setTextColor(TFT_LIGHTGREY, bgColor);
    tft.setTextDatum(MC_DATUM);
#ifdef SMOOTH_FONT
    tft.loadFont(NotoSansBold15);
    if (line2 == nullptr || line2[0] == '\0') {
        tft.drawString(line1, cx, cy);
    } else {
        tft.drawString(line1, cx, static_cast<int16_t>(cy - 8));
        tft.drawString(line2, cx, static_cast<int16_t>(cy + 8));
    }
    tft.unloadFont();
#else
    if (line2 == nullptr || line2[0] == '\0') {
        tft.drawString(line1, cx, cy, 2);
    } else {
        tft.drawString(line1, cx, static_cast<int16_t>(cy - 6), 1);
        tft.drawString(line2, cx, static_cast<int16_t>(cy + 6), 1);
    }
#endif
    tft.setTextDatum(TL_DATUM);
}

void drawExitButtonOnGameSprite(TFT_eSprite& sprite, uint16_t width) {
    const int16_t x = static_cast<int16_t>(width - kExitBtnOffsetX);
    const int16_t y = kExitBtnY;
    const int16_t w = kExitBtnW;
    const int16_t h = kExitBtnH;
    const int16_t cx = static_cast<int16_t>(x + (w / 2));
    const int16_t cy = static_cast<int16_t>(y + (h / 2));

    sprite.fillRect(x, y, w, h, kGameSpritePaper);
    sprite.drawRoundRect(x, y, w, h, 4, kGameSpriteInk);
    sprite.setTextColor(kGameSpriteInk, kGameSpritePaper);
    sprite.setTextDatum(MC_DATUM);
    sprite.drawString("EXIT", cx, cy, 1);
    sprite.setTextDatum(TL_DATUM);
}

struct GameOverMenuLayout {
    int16_t menuX;
    int16_t menuY;
    int16_t menuW;
    int16_t menuH;
    int16_t restartX;
    int16_t restartY;
    int16_t exitX;
    int16_t exitY;
};

GameOverMenuLayout makeGameOverMenuLayout(uint16_t width, uint16_t height) {
    GameOverMenuLayout layout = {};
    layout.menuW = min<int16_t>(kGameOverMenuW, static_cast<int16_t>(width - 12));
    layout.menuH = kGameOverMenuH;
    layout.menuX = static_cast<int16_t>((static_cast<int16_t>(width) - layout.menuW) / 2);
    layout.menuY = static_cast<int16_t>((static_cast<int16_t>(height) - layout.menuH) / 2);
    layout.restartY = static_cast<int16_t>(layout.menuY + layout.menuH - kGameOverBtnH - 8);
    layout.exitY = layout.restartY;
    layout.restartX = static_cast<int16_t>(layout.menuX + kGameOverBtnPad);
    layout.exitX = static_cast<int16_t>(layout.menuX + layout.menuW - kGameOverBtnPad - kGameOverBtnW);
    return layout;
}

bool pointInBox(int16_t x, int16_t y, int16_t boxX, int16_t boxY, int16_t boxW, int16_t boxH) {
    return (x >= boxX) && (x < static_cast<int16_t>(boxX + boxW)) &&
           (y >= boxY) && (y < static_cast<int16_t>(boxY + boxH));
}

void drawMenuButtonOnSprite(TFT_eSprite& sprite,
                            int16_t x,
                            int16_t y,
                            int16_t w,
                            int16_t h,
                            const char* label) {
    sprite.fillRect(x, y, w, h, kGameSpritePaper);
    sprite.drawRoundRect(x, y, w, h, 4, kGameSpriteInk);
    sprite.setTextDatum(MC_DATUM);
    sprite.drawString(label,
                      static_cast<int16_t>(x + (w / 2)),
                      static_cast<int16_t>(y + (h / 2)),
                      1);
    sprite.setTextDatum(TL_DATUM);
}

void drawGameOverMenuOnSprite(TFT_eSprite& sprite, uint16_t width, uint16_t height, uint32_t score) {
    const GameOverMenuLayout layout = makeGameOverMenuLayout(width, height);

    sprite.fillRect(layout.menuX, layout.menuY, layout.menuW, layout.menuH, kGameSpritePaper);
    sprite.drawRoundRect(layout.menuX, layout.menuY, layout.menuW, layout.menuH, 6, kGameSpriteInk);

    sprite.setTextDatum(TC_DATUM);
    sprite.drawString("GAME OVER",
                      static_cast<int16_t>(layout.menuX + (layout.menuW / 2)),
                      static_cast<int16_t>(layout.menuY + 8),
                      2);

    char scoreBuf[24];
    snprintf(scoreBuf, sizeof(scoreBuf), "SCORE %lu", static_cast<unsigned long>(score));
    sprite.drawString(scoreBuf,
                      static_cast<int16_t>(layout.menuX + (layout.menuW / 2)),
                      static_cast<int16_t>(layout.menuY + 28),
                      1);

    drawMenuButtonOnSprite(sprite,
                           layout.restartX,
                           layout.restartY,
                           kGameOverBtnW,
                           kGameOverBtnH,
                           "RESTART");
    drawMenuButtonOnSprite(sprite,
                           layout.exitX,
                           layout.exitY,
                           kGameOverBtnW,
                           kGameOverBtnH,
                           "EXIT");
    sprite.setTextDatum(TL_DATUM);
}

struct TouchCalibrationBlob {
    uint32_t magic;
    uint8_t version;
    uint8_t rotation;
    uint16_t calData[5];
} __attribute__((packed));

void swap(int16_t& a, int16_t& b) {
    int16_t temp = a;
    a = b;
    b = temp;
}

int16_t clampInt16(int16_t value, int16_t minValue, int16_t maxValue) {
    if (value < minValue) {
        return minValue;
    }
    if (value > maxValue) {
        return maxValue;
    }
    return value;
}

bool isUsageValid(int16_t value) {
    return value >= 0 && value <= 100;
}

bool isTempValid(float value) {
    return value >= 0.0f;
}

bool isPowerValid(int32_t value) {
    return value >= 0;
}

bool isHumidityValid(float value) {
    return value >= 0.0f && value <= 100.0f;
}

void setDisplayEnabled(TFT_eSPI& tft, bool enabled) {
#ifdef TFT_DISPON
#ifdef TFT_DISPOFF
    tft.startWrite();
    tft.writecommand(enabled ? TFT_DISPON : TFT_DISPOFF);
    tft.endWrite();
#else
    (void)enabled;
#endif
#else
    (void)tft;
    (void)enabled;
#endif
}

const char* stripKernelPrefix(const char* line) {
    if (line == nullptr) {
        return "";
    }

    if (line[0] != '[') {
        return line;
    }

    const char* closing = strstr(line, "] ");
    if (closing == nullptr) {
        return line;
    }

    return closing + 2;
}

const char* stateName(jetson_cfg::SystemState state) {
    switch (state) {
        case jetson_cfg::SystemState::POWER_OFF:
            return "POWER OFF";
        case jetson_cfg::SystemState::BOOTING_ON:
            return "BOOTING";
        case jetson_cfg::SystemState::RUNNING:
            return "RUNNING";
        case jetson_cfg::SystemState::SHUTTING_DOWN:
            return "SHUTDOWN";
        default:
            return "UNKNOWN";
    }
}

const char* alertName(uint8_t alertMask) {
    const bool highTemp = (alertMask & jetson_cfg::kAlertMaskHighTemperature) != 0;
    const bool highHumidity = (alertMask & jetson_cfg::kAlertMaskHighHumidity) != 0;

    if (highTemp && highHumidity) {
        return "TEMP+HUM";
    }
    if (highTemp) {
        return "TEMP";
    }
    if (highHumidity) {
        return "HUMID";
    }
    return "CLEAR";
}

uint16_t alertColor(uint8_t alertMask) {
    return (alertMask == 0U) ? kStatusOkColor : kStatusAlertColor;
}

bool hasHighHumidityAlert(uint8_t alertMask) {
    return (alertMask & jetson_cfg::kAlertMaskHighHumidity) != 0;
}

void drawHumidityWarningBannerDirect(TFT_eSPI& tft, uint16_t width, uint16_t height) {
    const int16_t bannerHeight = 12;
    const int16_t y = (height > bannerHeight) ? static_cast<int16_t>(height - bannerHeight) : 0;

    tft.fillRect(0, y, width, bannerHeight, TFT_BLACK);
    tft.drawFastHLine(0, y, width, TFT_DARKGREY);
    tft.setTextColor(TFT_RED, TFT_BLACK);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("HIGH HUMIDITY WARNING",
                   static_cast<int16_t>(width / 2),
                   static_cast<int16_t>(y + (bannerHeight / 2)),
                   1);
    tft.setTextDatum(TL_DATUM);
}

void formatUsageValue(int16_t value, char* buffer, size_t bufferSize) {
    if (!isUsageValid(value)) {
        snprintf(buffer, bufferSize, "--%%");
        return;
    }

    snprintf(buffer, bufferSize, "%d%%", static_cast<int>(value));
}

void formatTempValue(float value, char* buffer, size_t bufferSize) {
    if (!isTempValid(value)) {
        snprintf(buffer, bufferSize, "--.- C");
        return;
    }

    snprintf(buffer, bufferSize, "%.1f C", static_cast<double>(value));
}

void formatPowerValue(int32_t powerMw, char* buffer, size_t bufferSize) {
    if (!isPowerValid(powerMw)) {
        snprintf(buffer, bufferSize, "--");
        return;
    }

    if (powerMw >= 1000) {
        snprintf(buffer, bufferSize, "%.2f W", static_cast<double>(powerMw) / 1000.0);
        return;
    }

    snprintf(buffer, bufferSize, "%ld mW", static_cast<long>(powerMw));
}

void formatCapacityValue(int32_t mb, char* buffer, size_t bufferSize) {
    if (mb < 0) {
        snprintf(buffer, bufferSize, "--");
        return;
    }

    if (mb >= 1024) {
        snprintf(buffer, bufferSize, "%.1fGB", static_cast<double>(mb) / 1024.0);
        return;
    }

    snprintf(buffer, bufferSize, "%ldMB", static_cast<long>(mb));
}

void formatRateValue(int32_t kbps, char* buffer, size_t bufferSize) {
    if (kbps < 0) {
        snprintf(buffer, bufferSize, "--");
        return;
    }

    if (kbps >= 1024 * 1024) {
        snprintf(buffer, bufferSize, "%.1fGb/s", static_cast<double>(kbps) / (1024.0 * 1024.0));
        return;
    }

    if (kbps >= 1024) {
        snprintf(buffer, bufferSize, "%.1fMB/s", static_cast<double>(kbps) / 1024.0);
        return;
    }

    snprintf(buffer, bufferSize, "%ldKB/s", static_cast<long>(kbps));
}

void formatRateAxisValue(int32_t kbps, char* buffer, size_t bufferSize) {
    if (kbps <= 0) {
        snprintf(buffer, bufferSize, "0");
        return;
    }
    formatRateValue(kbps, buffer, bufferSize);
}

void formatRateMaxValue(int32_t kbps, char* buffer, size_t bufferSize) {
    formatRateAxisValue(kbps, buffer, bufferSize);
}

void formatGraphUsageLabel(int16_t value, char* buffer, size_t bufferSize) {
    formatUsageValue(value, buffer, bufferSize);
}

int32_t chooseRateGraphScale(int32_t currentScale, int32_t firstKbps, int32_t secondKbps, int32_t fallbackScale) {
    int32_t peak = max<int32_t>(firstKbps, secondKbps);
    if (peak < 0) {
        peak = 0;
    }
    int32_t target = kMinRateGraphScaleKbps;
    while (target < peak && target < (1024L * 1024L)) {
        target *= 2;
    }
    if (target < fallbackScale && peak == 0) {
        target = fallbackScale;
    }
    if (currentScale <= 0) {
        return target;
    }
    if (peak > currentScale || (peak > 0 && peak < (currentScale / 4) && currentScale > kMinRateGraphScaleKbps)) {
        return target;
    }
    return currentScale;
}

int16_t rateToGraphPercent(int32_t kbps, int32_t scaleKbps) {
    if (kbps < 0) {
        return -1;
    }
    if (scaleKbps <= 0) {
        return 0;
    }
    const int32_t scaled = (kbps * 100L + (scaleKbps / 2)) / scaleKbps;
    return clampInt16(static_cast<int16_t>(scaled), 0, 100);
}

int16_t maxValidUsage(int16_t a, int16_t b) {
    if (!isUsageValid(a)) return b;
    if (!isUsageValid(b)) return a;
    return max<int16_t>(a, b);
}

const char* powerModeNameForId(int id) {
    switch (id) {
        case 3: return "7W";
        case 0: return "15W";
        case 1: return "25W";
        case 2: return "MAXN_SUPER";
        default: return "UNKNOWN";
    }
}

int32_t powerModeLimitMwForId(int id) {
    switch (id) {
        case 3: return 7000;
        case 0: return 15000;
        case 1: return 25000;
        case 2: return 30000;
        default: return -1;
    }
}

void formatPowerGraphTitle(int id, char* out, size_t outSize) {
    if (out == nullptr || outSize == 0) {
        return;
    }
    const char* name = powerModeNameForId(id);
    if (strcmp(name, "UNKNOWN") == 0) {
        snprintf(out, outSize, "POWER");
    } else {
        snprintf(out, outSize, "POWER - %s Mode", name);
    }
}

void formatPowerModeUiLabel(int id, const char* name, int32_t maxMw, char* out, size_t outSize) {
    if (out == nullptr || outSize == 0) {
        return;
    }
    const char* labelName = (name != nullptr && name[0] != '\0') ? name : powerModeNameForId(id);
    if (maxMw <= 0) {
        maxMw = powerModeLimitMwForId(id);
    }
    if (maxMw > 0) {
        snprintf(out, outSize, "%s Mode (%ldW)", labelName, static_cast<long>((maxMw + 500) / 1000));
    } else if (strcmp(labelName, "UNKNOWN") != 0) {
        snprintf(out, outSize, "%s Mode", labelName);
    } else {
        snprintf(out, outSize, "UNKNOWN");
    }
}

void formatHumidityValue(float value, char* buffer, size_t bufferSize) {
    if (!isHumidityValid(value)) {
        snprintf(buffer, bufferSize, "--%%");
        return;
    }

    snprintf(buffer, bufferSize, "%d%%", static_cast<int>(value + 0.5f));
}

int16_t plotYForUsage(int16_t value, int16_t graphHeight) {
    const int16_t topY = 0;
    const int16_t bottomY = max<int16_t>(topY, static_cast<int16_t>(graphHeight - 1));
    const int16_t clampedValue = clampInt16(value, 0, 99);
    const int16_t pixelStep = clampInt16(static_cast<int16_t>((clampedValue + 2) / 3), 0, bottomY);
    return static_cast<int16_t>(bottomY - pixelStep);
}

void drawGraphGrid(TFT_eSprite& sprite, int16_t w, int16_t h) {
    if (w < 2 || h < 2) {
        return;
    }

    const int16_t midY = 1 + ((h - 3) / 2);
    sprite.drawFastHLine(1, midY, w - 2, kGridColor);
    for (uint8_t step = 1; step < 4; ++step) {
        const int16_t gridX = 1 + ((step * (w - 3)) / 4);
        sprite.drawFastVLine(gridX, 1, h - 2, kGridColor);
    }
}


} // namespace

LCD2Dashboard::LCD2Dashboard()
    : _width(DEFAULT_WIDTH),
      _height(DEFAULT_HEIGHT),
      _rotation(DEFAULT_ROTATION),
      _state(SystemState::POWER_OFF),
      _driverReady(false),
    _degradedMode(false),
      _visible(false),
      _layoutDrawn(false),
      _dirty(true),
      _hasMetrics(false),
      _alertMask(0),
    _boxTemp(-1.0f),
    _boxHumidity(-1.0f),
    _fanPercent(0),
    _ledBrightnessPercent(map(jetson_cfg::kLedBrightness, 0, 255, 0, 100)),
    _touchReady(false),
    _touchDown(false),
    _lastTouchScanMs(0),
    _touchSamplesY{0, 0, 0},
    _touchSampleCount(0),
    _bootLayoutDrawn(false),
    _lastBootRefreshMs(0),
    _dashboardPage(DashboardPage::PERFORMANCE),
    _dashboardTouchActive(false),
    _touchStartX(0),
    _touchStartY(0),
    _touchLastX(0),
    _touchLastY(0),
    _settingsOpen(false),
    _settingsScreen(SettingsScreen::MAIN),
    _settingsDrawn(false),
    _settingsFullRedraw(true),
    _lastSettingsScreen(SettingsScreen::MAIN),
    _wifiEntryCount(0),
    _selectedWifiIndex(-1),
    _wifiScanInProgress(false),
    _wifiConnectInProgress(false),
    _wifiConnectOk(false),
    _wifiScanRequestMs(0),
    _wifiConnectRequestMs(0),
    _ipRequestMs(0),
    _wifiStatus{0},
    _connectStatus{0},
    _ipInterface{"N/A"},
    _ipSsid{"N/A"},
    _ipAddress{"N/A"},
    _ipStatus{"UNKNOWN"},
    _ipRequestInProgress(false),
    _aboutRequestInProgress(false),
    _sshRequestInProgress(false),
    _ngrokRequestInProgress(false),
    _ngrokActionState(NgrokActionState::NONE),
    _headlessRequestInProgress(false),
    _powerModeRequestInProgress(false),
    _aboutRequestMs(0),
    _sshRequestMs(0),
    _ngrokRequestMs(0),
    _ngrokPollMs(0),
    _headlessRequestMs(0),
    _powerModeRequestMs(0),
    _aboutHostname{"N/A"},
    _aboutVersion{"1.5.1"},
    _sshStatus{"UNKNOWN"},
    _sshEnabled{"N/A"},
    _sshService{"ssh.service"},
    _ngrokStatus{"UNKNOWN"},
    _ngrokEnabled{"N/A"},
    _ngrokEndpoint{"N/A"},
    _ngrokApi{"UNKNOWN"},
    _ngrokService{"ngrok.service"},
    _headlessDefault{"UNKNOWN"},
    _headlessActive{"UNKNOWN"},
    _powerModeLabel{"UNKNOWN"},
    _powerModeId(-1),
    _pendingPowerModeId(-1),
    _powerModeMaxMw(-1),
    _systemStatus{"Ready"},
    _confirmAction(SettingsConfirmAction::NONE),
    _confirmReturnScreen(SettingsScreen::MAIN),
    _confirmTitle{0},
    _confirmBody{0},
    _pendingJetsonCommand{0},
    _hasPendingJetsonCommand(false),
    _keyboard(),
    _bootLogHead(0),
    _bootLogCount(0),
    _activeControl(ActiveControl::NONE),
      _lastRefreshMs(0),
    _lastMetricsMs(0),
    _latest{-1, -1, -1, -1.0f, -1.0f, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
      _historyWriteIndex(0),
      _historyCount(0),
    _netGraphScaleKbps(kDefaultNetGraphScaleKbps),
    _diskGraphScaleKbps(kDefaultDiskGraphScaleKbps),
    _swapGraphScaleMb(kDefaultPowerGraphScaleMw),
    _tft(DEFAULT_WIDTH, DEFAULT_HEIGHT),
    _btnGames(&_tft),
    _btnDino(&_tft),
    _btnBall(&_tft),
    _btnExit(&_tft),
    _cpuSprite(&_tft),
    _gpuSprite(&_tft),
      _ramSprite(&_tft),
      _panelSprite(&_tft),
    _graphHeaderSprite(&_tft),
    _graphValueSprite(&_tft),
    _settingsCompactRowSprite(&_tft),
    _settingsRowSprite(&_tft),
    _wifiStatusSprite(&_tft),
    _wifiRowSprite(&_tft),
    _keyboardInputSprite(&_tft),
    _powerOffSettingsPanelSprite(&_tft),
    _powerOffEnvSprite(&_tft),
    _humidityBannerSprite(&_tft),
    _bootLogSprite(&_tft),
    _spritesReady(false),
    _dynamicSpritesReady(false),
    _bootSpriteReady(false),
    _humidityBannerVisible(false),
    _powerOffMode(PowerOffMode::IDLE),
    _powerOffLastFrameMs(0),
    _idleDrawn(false),
    _dinoGame{},
    _ballGame{},
    _gameSprite(&_tft),
    _gameSpriteReady(false),
    _gamePrevTouched(false),
    _gameTouched(false),
    _gameTouchX(0),
    _gameTouchY(0),
    _powerOffSettingsOpen(false),
    _powerOffSettingsTouchActive(false),
    _settingsGameActive(false),
    _gameMenuDrawn(false),
    _settingsGameMode(SettingsGameMode::NONE) {
    for (uint16_t i = 0; i < HISTORY_POINTS; ++i) {
        _cpuHistory[i] = -1;
        _gpuHistory[i] = -1;
        _ramHistory[i] = -1;
        _netHistory[i] = -1;
        _netUploadHistory[i] = -1;
        _diskHistory[i] = -1;
        _diskWriteHistory[i] = -1;
        _swapHistory[i] = -1;
    }

    resetWifiEntries();
    clearBootLog();
}

int16_t LCD2Dashboard::clampUsage(int16_t value) {
    if (value < 0) {
        return -1;
    }
    return clampInt16(value, 0, 100);
}

LCD2Dashboard::DashboardLayout LCD2Dashboard::buildLayout() const {
    DashboardLayout layout = {};

    const int16_t screenW = static_cast<int16_t>(_width);
    const int16_t screenH = static_cast<int16_t>(_height);
    const int16_t graphX = kOuterMargin;
    const int16_t graphWidth = static_cast<int16_t>(screenW - (kOuterMargin * 2));
    const int16_t graphTop = static_cast<int16_t>(kOuterMargin + kStatusBarHeight + kSectionGap + 4);
    const int16_t graphHeight = max<int16_t>(34, static_cast<int16_t>((screenH - graphTop - kOuterMargin - (kSectionGap * 2)) / 3));

    layout.panelFrame = {kOuterMargin, kOuterMargin, graphWidth, kStatusBarHeight};
    layout.cpuFrame = {graphX, graphTop, graphWidth, graphHeight};
    layout.gpuFrame = {graphX, static_cast<int16_t>(graphTop + graphHeight + kSectionGap), graphWidth, graphHeight};
    layout.ramFrame = {graphX, static_cast<int16_t>(graphTop + ((graphHeight + kSectionGap) * 2)), graphWidth, graphHeight};

    const int16_t plotWidth = min<int16_t>(kGraphPlotWidth, static_cast<int16_t>(graphWidth - kGraphAxisLabelWidth - kGraphPlotShiftX - 8));
    const int16_t plotHeight = kGraphPlotHeight;
    const int16_t plotX = static_cast<int16_t>(layout.cpuFrame.x + 3 + kGraphPlotShiftX);

    layout.cpuPlot = {plotX,
                      static_cast<int16_t>(layout.cpuFrame.y + kGraphHeaderHeight + 1),
                      plotWidth,
                      plotHeight};
    layout.gpuPlot = {plotX,
                      static_cast<int16_t>(layout.gpuFrame.y + kGraphHeaderHeight + 1),
                      plotWidth,
                      plotHeight};
    layout.ramPlot = {plotX,
                      static_cast<int16_t>(layout.ramFrame.y + kGraphHeaderHeight + 1),
                      plotWidth,
                      plotHeight};

    return layout;
}

bool LCD2Dashboard::initSprites() {
    deleteSprites();

    const DashboardLayout layout = buildLayout();
    if (layout.cpuPlot.w <= 0 || layout.cpuPlot.h <= 0 ||
        layout.gpuPlot.w <= 0 || layout.gpuPlot.h <= 0 ||
        layout.ramPlot.w <= 0 || layout.ramPlot.h <= 0 ||
        layout.panelFrame.w <= 2 || layout.panelFrame.h <= 2) {
        return false;
    }

    _cpuSprite.setColorDepth(8);
    _gpuSprite.setColorDepth(8);
    _ramSprite.setColorDepth(8);
    _panelSprite.setColorDepth(8);

    if (_cpuSprite.createSprite(layout.cpuPlot.w, layout.cpuPlot.h) == nullptr) {
        deleteSprites();
        return false;
    }

    if (_gpuSprite.createSprite(layout.gpuPlot.w, layout.gpuPlot.h) == nullptr) {
        deleteSprites();
        return false;
    }

    if (_ramSprite.createSprite(layout.ramPlot.w, layout.ramPlot.h) == nullptr) {
        deleteSprites();
        return false;
    }

    if (_panelSprite.createSprite(layout.panelFrame.w - 2, layout.panelFrame.h - 2) == nullptr) {
        deleteSprites();
        return false;
    }

    _cpuSprite.setTextColor(kTextColor, kBackgroundColor);
    _gpuSprite.setTextColor(kTextColor, kBackgroundColor);
    _ramSprite.setTextColor(kTextColor, kBackgroundColor);
    _panelSprite.setTextColor(kTextColor, kBackgroundColor);
    _cpuSprite.setTextFont(1);
    _gpuSprite.setTextFont(1);
    _ramSprite.setTextFont(1);
    _panelSprite.setTextFont(1);
    _spritesReady = true;
    return true;
}

void LCD2Dashboard::deleteSprites() {
    if (_cpuSprite.created()) {
        _cpuSprite.deleteSprite();
    }
    if (_gpuSprite.created()) {
        _gpuSprite.deleteSprite();
    }
    if (_ramSprite.created()) {
        _ramSprite.deleteSprite();
    }
    if (_panelSprite.created()) {
        _panelSprite.deleteSprite();
    }
    _spritesReady = false;
}

bool LCD2Dashboard::initDynamicSprites() {
    deleteDynamicSprites();

    const DashboardLayout layout = buildLayout();
    const int16_t graphValueLeft = static_cast<int16_t>(layout.cpuPlot.x + layout.cpuPlot.w + 2);
    const int16_t graphValueRight = static_cast<int16_t>(layout.cpuFrame.x + layout.cpuFrame.w - 2);
    const int16_t graphValueW = max<int16_t>(1, static_cast<int16_t>(graphValueRight - graphValueLeft + 1));
    const int16_t graphValueH = static_cast<int16_t>(layout.cpuPlot.h + (kGraphFrameThickness * 2));
    const int16_t settingsCompactW = max<int16_t>(1, static_cast<int16_t>(_width - 118));
    const int16_t settingsRowW = max<int16_t>(1, static_cast<int16_t>(_width - 24));
    const Rect powerPanel = makePowerOffSettingsPanelRect();
    const int16_t powerEnvW = min<int16_t>(180, max<int16_t>(80, static_cast<int16_t>(_width - 24)));

    _graphHeaderSprite.setColorDepth(8);
    _graphValueSprite.setColorDepth(8);
    _settingsCompactRowSprite.setColorDepth(8);
    _settingsRowSprite.setColorDepth(8);
    _wifiStatusSprite.setColorDepth(8);
    _wifiRowSprite.setColorDepth(8);
    _keyboardInputSprite.setColorDepth(8);
    _powerOffSettingsPanelSprite.setColorDepth(8);
    _powerOffEnvSprite.setColorDepth(8);
    _humidityBannerSprite.setColorDepth(8);

    if (_graphHeaderSprite.createSprite(layout.cpuFrame.w, kGraphHeaderHeight) == nullptr ||
        _graphValueSprite.createSprite(graphValueW, graphValueH) == nullptr ||
        _settingsCompactRowSprite.createSprite(settingsCompactW, 22) == nullptr ||
        _settingsRowSprite.createSprite(settingsRowW, 22) == nullptr ||
        _wifiStatusSprite.createSprite(static_cast<int16_t>(_width - 16), 18) == nullptr ||
        _wifiRowSprite.createSprite(static_cast<int16_t>(_width - 20), kWifiRowHeight) == nullptr ||
        _keyboardInputSprite.createSprite(static_cast<int16_t>(_width - 20), 30) == nullptr ||
        _powerOffSettingsPanelSprite.createSprite(powerPanel.w, powerPanel.h) == nullptr ||
        _powerOffEnvSprite.createSprite(powerEnvW, 24) == nullptr ||
        _humidityBannerSprite.createSprite(_width, 12) == nullptr) {
        deleteDynamicSprites();
        return false;
    }

    _graphHeaderSprite.setTextDatum(TL_DATUM);
    _graphValueSprite.setTextDatum(TL_DATUM);
    _settingsCompactRowSprite.setTextDatum(TL_DATUM);
    _settingsRowSprite.setTextDatum(TL_DATUM);
    _wifiStatusSprite.setTextDatum(TL_DATUM);
    _wifiRowSprite.setTextDatum(TL_DATUM);
    _keyboardInputSprite.setTextDatum(TL_DATUM);
    _powerOffSettingsPanelSprite.setTextDatum(TL_DATUM);
    _powerOffEnvSprite.setTextDatum(TL_DATUM);
    _humidityBannerSprite.setTextDatum(TL_DATUM);
    _humidityBannerSprite.setTextColor(TFT_RED, kBackgroundColor);
    _dynamicSpritesReady = true;
    return true;
}

void LCD2Dashboard::deleteDynamicSprites() {
    if (_graphHeaderSprite.created()) {
        _graphHeaderSprite.deleteSprite();
    }
    if (_graphValueSprite.created()) {
        _graphValueSprite.deleteSprite();
    }
    if (_settingsCompactRowSprite.created()) {
        _settingsCompactRowSprite.deleteSprite();
    }
    if (_settingsRowSprite.created()) {
        _settingsRowSprite.deleteSprite();
    }
    if (_wifiStatusSprite.created()) {
        _wifiStatusSprite.deleteSprite();
    }
    if (_wifiRowSprite.created()) {
        _wifiRowSprite.deleteSprite();
    }
    if (_keyboardInputSprite.created()) {
        _keyboardInputSprite.deleteSprite();
    }
    if (_powerOffSettingsPanelSprite.created()) {
        _powerOffSettingsPanelSprite.deleteSprite();
    }
    if (_powerOffEnvSprite.created()) {
        _powerOffEnvSprite.deleteSprite();
    }
    if (_humidityBannerSprite.created()) {
        _humidityBannerSprite.deleteSprite();
    }
    _dynamicSpritesReady = false;
    _humidityBannerVisible = false;
}

bool LCD2Dashboard::initBootLogSprite() {
    deleteBootLogSprite();

    _bootLogSprite.setColorDepth(1);
    if (_bootLogSprite.createSprite(_width, _height) == nullptr) {
        _bootSpriteReady = false;
        return false;
    }

    _bootLogSprite.setTextColor(kBootSpriteInk, kBootSpritePaper);
    _bootLogSprite.setTextFont(1);
    _bootLogSprite.setTextDatum(TL_DATUM);
    _bootLogSprite.setBitmapColor(kTextColor, kBackgroundColor);
    _bootSpriteReady = true;
    return true;
}

void LCD2Dashboard::deleteBootLogSprite() {
    if (_bootLogSprite.created()) {
        _bootLogSprite.deleteSprite();
    }
    _bootSpriteReady = false;
}

bool LCD2Dashboard::initTouchCalibration() {
    uint16_t calData[5] = {0};
    bool fileSystemReady = LittleFS.begin();
    if (!fileSystemReady) {
        Serial.println("[LCD2] LittleFS unavailable; touch calibration will not persist");
    }

    bool hasCalibration = false;
    if (fileSystemReady && !jetson_cfg::kLcd2ForceTouchCalibration) {
        hasCalibration = loadTouchCalibration(calData, 5);
    }

    if (!hasCalibration) {
        if (!runTouchCalibration(calData, 5)) {
            return false;
        }

        if (fileSystemReady) {
            saveTouchCalibration(calData, 5);
        }
    }

    _tft.setTouch(calData);
    return true;
}

bool LCD2Dashboard::loadTouchCalibration(uint16_t* outCalData, size_t count) const {
    if (outCalData == nullptr || count < 5 || !LittleFS.exists(jetson_cfg::kLcd2TouchCalibrationFile)) {
        return false;
    }

    fs::File file = LittleFS.open(jetson_cfg::kLcd2TouchCalibrationFile, "r");
    if (!file) {
        return false;
    }

    TouchCalibrationBlob blob = {};
    const size_t bytesRead = file.read(reinterpret_cast<uint8_t*>(&blob), sizeof(blob));
    file.close();
    if (bytesRead != sizeof(blob)) {
        return false;
    }

    if (blob.magic != kTouchCalibrationMagic ||
        blob.version != kTouchCalibrationVersion ||
        blob.rotation != _rotation) {
        return false;
    }

    for (size_t i = 0; i < 5; ++i) {
        outCalData[i] = blob.calData[i];
    }
    return true;
}

bool LCD2Dashboard::saveTouchCalibration(const uint16_t* calData, size_t count) const {
    if (calData == nullptr || count < 5) {
        return false;
    }

    fs::File file = LittleFS.open(jetson_cfg::kLcd2TouchCalibrationFile, "w");
    if (!file) {
        return false;
    }

    TouchCalibrationBlob blob = {};
    blob.magic = kTouchCalibrationMagic;
    blob.version = kTouchCalibrationVersion;
    blob.rotation = _rotation;
    for (size_t i = 0; i < 5; ++i) {
        blob.calData[i] = calData[i];
    }

    const size_t bytesWritten = file.write(reinterpret_cast<const uint8_t*>(&blob), sizeof(blob));
    file.close();
    return bytesWritten == sizeof(blob);
}

bool LCD2Dashboard::runTouchCalibration(uint16_t* calData, size_t count) {
    if (calData == nullptr || count < 5) {
        return false;
    }

    _tft.fillScreen(kBackgroundColor);
    _tft.setTextColor(kTextColor, kBackgroundColor);
    _tft.setTextDatum(TL_DATUM);
#ifdef SMOOTH_FONT
    _tft.loadFont(NotoSansBold15);
    _tft.drawString("Touch calibration", 12, 16);
    _tft.drawString("Touch the markers", 12, 44);
    _tft.drawString("at all corners", 12, 72);
    _tft.unloadFont();
#else
    _tft.drawString("Touch calibration", 12, 16, 2);
    _tft.drawString("Touch the markers", 12, 44, 2);
    _tft.drawString("at all corners", 12, 64, 2);
#endif
    _tft.calibrateTouch(calData, TFT_MAGENTA, kBackgroundColor, 15);
    return true;
}

void LCD2Dashboard::init(uint16_t width, uint16_t height, uint8_t rotation) {
    _width = width;
    _height = height;
    _rotation = rotation;

    _tft.init();
    _tft.setRotation(_rotation);
#ifdef ESP32
    _tft.initDMA();
#endif
    _width = static_cast<uint16_t>(_tft.width());
    _height = static_cast<uint16_t>(_tft.height());
    _tft.fillScreen(kBackgroundColor);
    _tft.setTextSize(1);
    _tft.setTextFont(1);
    _tft.setTextColor(kTextColor, kBackgroundColor);
    _tft.setTextDatum(TL_DATUM);
    setDisplayEnabled(_tft, true);
    _touchReady = initTouchCalibration();

    _driverReady = true;
    const bool dashboardSpriteReady = initSprites();
    const bool dynamicSpriteReady = initDynamicSprites();
    const bool bootLogSpriteReady = initBootLogSprite();
    _degradedMode = !(dashboardSpriteReady && bootLogSpriteReady);
    if (_degradedMode) {
        Serial.println("[LCD2] Entering degraded mode (sprite allocation failure)");
    }
    if (!dynamicSpriteReady) {
        Serial.println("[LCD2] Dynamic value sprites unavailable; using bounded direct fallback");
    }
    initGameButtons();
    _visible = false;
    _layoutDrawn = false;
    _bootLayoutDrawn = false;
    _dirty = true;
    _hasMetrics = false;
    _touchDown = false;
    _touchSampleCount = 0;
    _lastBootRefreshMs = 0;
    _bootLogHead = 0;
    _bootLogCount = 0;
    _activeControl = ActiveControl::NONE;
    _lastTouchScanMs = 0;
    _lastRefreshMs = 0;
    _lastMetricsMs = 0;
    _settingsDrawn = false;
    _settingsFullRedraw = true;
    _lastSettingsScreen = SettingsScreen::MAIN;
    _powerOffMode = PowerOffMode::IDLE;
    _powerOffLastFrameMs = 0;
    _idleDrawn = false;
    _gameSpriteReady = false;
    _gamePrevTouched = false;
    _gameTouched = false;
    _gameTouchX = 0;
    _gameTouchY = 0;
    _powerOffSettingsOpen = false;
    _powerOffSettingsTouchActive = false;
    _settingsGameActive = false;
    _gameMenuDrawn = false;
    _settingsGameMode = SettingsGameMode::NONE;
    _humidityBannerVisible = false;
    resetHistory();
    clearBootLog();
}

void LCD2Dashboard::onStateChange(SystemState newState) {
    if (_state == newState) {
        return;
    }

    const SystemState oldState = _state;
    const bool wasRunning = (_state == SystemState::RUNNING);
    _state = newState;
    const bool isRunning = (_state == SystemState::RUNNING);
    const bool isTransitionState = (_state == SystemState::BOOTING_ON ||
                                    _state == SystemState::SHUTTING_DOWN);

    if (isTransitionState) {
        if (_driverReady) {
            setDisplayEnabled(_tft, true);
        }

        resetHistory();
        _layoutDrawn = false;
        _bootLayoutDrawn = false;
        _dirty = true;
        _touchDown = false;
        _dashboardTouchActive = false;
        _settingsOpen = false;
        _settingsScreen = SettingsScreen::MAIN;
        _settingsDrawn = false;
        _settingsFullRedraw = true;
        _touchSampleCount = 0;
        _activeControl = ActiveControl::NONE;
        _lastTouchScanMs = 0;
        _lastBootRefreshMs = 0;

        if (oldState != _state) {
            clearBootLog();
            if (_state == SystemState::BOOTING_ON) {
                appendBootLogLine("Booting Jetson");
            } else {
                appendBootLogLine("Shutting down");
            }
        }

        if (_driverReady && wasRunning) {
            _tft.fillScreen(kBackgroundColor);
        }

        _visible = false;
        return;
    }

    if (!isRunning) {
        if (_state == SystemState::POWER_OFF && _driverReady && oldState != SystemState::POWER_OFF) {
            // Stop any running game and release its sprite RAM.
            deleteGameSprite();

            // Keep display ON — show the POWER_OFF idle screen with games menu.
            setDisplayEnabled(_tft, true);
            _tft.fillScreen(TFT_BLACK);
            _powerOffMode = PowerOffMode::IDLE;
            _powerOffLastFrameMs = 0;
            _idleDrawn = false;
            _gamePrevTouched = false;
            _gameTouched = false;
            _powerOffSettingsOpen = false;
            _powerOffSettingsTouchActive = false;
        }

        resetHistory();
        _layoutDrawn = false;
        _bootLayoutDrawn = false;
        _dirty = true;
        _touchDown = false;
        _dashboardTouchActive = false;
        _settingsOpen = false;
        _settingsScreen = SettingsScreen::MAIN;
        _settingsDrawn = false;
        _settingsFullRedraw = true;
        _touchSampleCount = 0;
        _activeControl = ActiveControl::NONE;
        _powerOffSettingsOpen = false;
        _powerOffSettingsTouchActive = false;
        _bootLogHead = 0;
        _bootLogCount = 0;
        _lastBootRefreshMs = 0;
        _lastTouchScanMs = 0;
        if (_driverReady && wasRunning) {
            _tft.fillScreen(kBackgroundColor);
        }
        _visible = false;
        return;
    }

    if (_driverReady) {
        setDisplayEnabled(_tft, true);
    }

    resetHistory();
    _layoutDrawn = false;
    _bootLayoutDrawn = false;
    _dirty = true;
    _visible = false;
    _touchDown = false;
    _dashboardTouchActive = false;
    _dashboardPage = DashboardPage::PERFORMANCE;
    _settingsOpen = false;
    _settingsScreen = SettingsScreen::MAIN;
    _settingsDrawn = false;
    _settingsFullRedraw = true;
    _touchSampleCount = 0;
    _activeControl = ActiveControl::NONE;
    _powerOffSettingsOpen = false;
    _powerOffSettingsTouchActive = false;
    _lastBootRefreshMs = 0;
    _lastRefreshMs = 0;
}

void LCD2Dashboard::onAlertChange(uint8_t alertMask) {
    _alertMask = alertMask;
    _dirty = true;
}

void LCD2Dashboard::clearMetrics() {
    if (_settingsOpen) {
        return;
    }
    resetHistory();
    _dirty = true;
    _lastRefreshMs = 0;
}

void LCD2Dashboard::setEnvironment(float boxTemp,
                                   float boxHumidity,
                                   int16_t fanPercent,
                                   int16_t ledBrightnessPercent) {
    const float nextBoxTemp = (boxTemp >= 0.0f) ? boxTemp : -1.0f;
    const float nextBoxHumidity = isHumidityValid(boxHumidity) ? boxHumidity : -1.0f;
    const int16_t nextFanPercent = clampUsage(fanPercent);
    const int16_t nextLedBrightnessPercent = clampInt16(ledBrightnessPercent, 0, 100);

    if (_boxTemp == nextBoxTemp &&
        _boxHumidity == nextBoxHumidity &&
        _fanPercent == nextFanPercent &&
        _ledBrightnessPercent == nextLedBrightnessPercent) {
        return;
    }

    _boxTemp = nextBoxTemp;
    _boxHumidity = nextBoxHumidity;
    _fanPercent = nextFanPercent;
    _ledBrightnessPercent = nextLedBrightnessPercent;
    if (!_settingsOpen) {
        _dirty = true;
    }
}

int16_t LCD2Dashboard::getRequestedLedBrightnessPercent() const {
    return clampInt16(_ledBrightnessPercent, 0, 100);
}

bool LCD2Dashboard::popPendingJetsonCommand(char* outCommand, size_t outSize) {
    if (!_hasPendingJetsonCommand || outCommand == nullptr || outSize == 0) {
        return false;
    }

    strncpy(outCommand, _pendingJetsonCommand, outSize - 1);
    outCommand[outSize - 1] = '\0';
    _pendingJetsonCommand[0] = '\0';
    _hasPendingJetsonCommand = false;
    return true;
}

void LCD2Dashboard::processJetsonResponseLine(const char* line) {
    if (line == nullptr || line[0] == '\0') {
        return;
    }

    Serial.print("[LCD2][JETSON_RX] ");
    Serial.println(line);

    if (strncmp(line, "WIFI_BEGIN", 10) == 0) {
        resetWifiEntries();
        _wifiScanInProgress = true;
        strncpy(_wifiStatus, "Scanning", sizeof(_wifiStatus) - 1);
        _wifiStatus[sizeof(_wifiStatus) - 1] = '\0';
        if (_settingsOpen && _settingsScreen == SettingsScreen::WIFI_LIST) {
            _settingsDrawn = false;
            _dirty = true;
            _lastRefreshMs = 0;
        }
        return;
    }

    if (strncmp(line, "WIFI SSID:", 10) == 0) {
        parseWifiLine(line);
        if (!_wifiScanInProgress) {
            _dirty = true;
            _lastRefreshMs = 0;
        }
        return;
    }

    if (strncmp(line, "WIFI_END", 8) == 0) {
        _wifiScanInProgress = false;
        _wifiScanRequestMs = 0;
        if (_wifiEntryCount == 0) {
            strncpy(_wifiStatus, "No networks found", sizeof(_wifiStatus) - 1);
        } else if (_wifiStatus[0] == '\0' || strcmp(_wifiStatus, "Scanning") == 0) {
            strncpy(_wifiStatus, "Scan complete", sizeof(_wifiStatus) - 1);
        }
        _wifiStatus[sizeof(_wifiStatus) - 1] = '\0';
        if (_settingsOpen && _settingsScreen == SettingsScreen::WIFI_LIST) {
            _dirty = true;
            _lastRefreshMs = 0;
        }
        return;
    }

    if (strncmp(line, "IP ", 3) == 0) {
        parseIpLine(line);
        if (_settingsOpen && _settingsScreen == SettingsScreen::IP_VIEW) {
            _dirty = true;
            _lastRefreshMs = 0;
        }
        return;
    }

    if (strncmp(line, "WIFI_CONNECT ", 13) == 0) {
        parseConnectLine(line);
        if (_settingsOpen && _settingsScreen == SettingsScreen::CONNECT_RESULT) {
            _dirty = true;
            _lastRefreshMs = 0;
        }
        return;
    }

    if (strncmp(line, "WIFI_ERR", 8) == 0) {
        _wifiScanInProgress = false;
        _wifiScanRequestMs = 0;
        strncpy(_wifiStatus, line, sizeof(_wifiStatus) - 1);
        _wifiStatus[sizeof(_wifiStatus) - 1] = '\0';
        if (_settingsOpen && _settingsScreen == SettingsScreen::WIFI_LIST) {
            _dirty = true;
        }
        return;
    }

    if (strncmp(line, "ABOUT ", 6) == 0) {
        parseAboutLine(line);
        if (_settingsOpen && _settingsScreen == SettingsScreen::ABOUT) {
            _dirty = true;
            _lastRefreshMs = 0;
        }
        return;
    }

    if (strncmp(line, "SSH ", 4) == 0) {
        parseServiceStatusLine(line, false);
        if (_settingsOpen && _settingsScreen == SettingsScreen::SSH) {
            _dirty = true;
            _lastRefreshMs = 0;
        }
        return;
    }

    if (strncmp(line, "NGROK ", 6) == 0) {
        parseServiceStatusLine(line, true);
        if (_settingsOpen && _settingsScreen == SettingsScreen::NGROK) {
            _dirty = true;
            _lastRefreshMs = 0;
        }
        return;
    }

    if (strncmp(line, "HEADLESS ", 9) == 0) {
        parseHeadlessLine(line);
        if (_settingsOpen && _settingsScreen == SettingsScreen::HEADLESS) {
            _dirty = true;
            _lastRefreshMs = 0;
        }
        return;
    }

    if (strncmp(line, "POWER_MODE ", 11) == 0 || strncmp(line, "POWER_MODE_RESULT ", 18) == 0) {
        parsePowerModeLine(line);
        if (_settingsOpen && _settingsScreen == SettingsScreen::POWER_MODE) {
            _dirty = true;
            _lastRefreshMs = 0;
        }
        return;
    }

    if (strstr(line, "_RESULT ") != nullptr || strncmp(line, "SYSTEM_RESULT ", 14) == 0) {
        parseResultLine(line);
        if (_settingsOpen) {
            _dirty = true;
            _lastRefreshMs = 0;
        }
        return;
    }

    if (strncmp(line, "ERR ", 4) == 0) {
        _wifiScanInProgress = false;
        _wifiScanRequestMs = 0;
        _wifiConnectInProgress = false;
        _wifiConnectRequestMs = 0;
        _aboutRequestInProgress = false;
        _sshRequestInProgress = false;
        _ngrokRequestInProgress = false;
        _headlessRequestInProgress = false;
        _ngrokActionState = NgrokActionState::NONE;
        _ngrokPollMs = 0;
        _wifiConnectOk = false;
        strncpy(_connectStatus, line, sizeof(_connectStatus) - 1);
        _connectStatus[sizeof(_connectStatus) - 1] = '\0';
        strncpy(_systemStatus, line, sizeof(_systemStatus) - 1);
        _systemStatus[sizeof(_systemStatus) - 1] = '\0';
        if (_settingsOpen && _settingsScreen == SettingsScreen::CONNECT_RESULT) {
            _dirty = true;
        }
    }
}

void LCD2Dashboard::pushMetrics(const MetricsFrame& frame) {
    _latest.cpuUsage = clampUsage(frame.cpuUsage);
    _latest.gpuUsage = clampUsage(frame.gpuUsage);
    _latest.ramUsage = clampUsage(frame.ramUsage);
    _latest.cpuTemp = frame.cpuTemp;
    _latest.gpuTemp = frame.gpuTemp;
    _latest.powerMw = frame.powerMw;
    const bool stalePendingMode = (_pendingPowerModeId >= 0 &&
                                   frame.powerModeId >= 0 &&
                                   frame.powerModeId != _pendingPowerModeId);
    _latest.powerModeId = stalePendingMode ? _powerModeId : frame.powerModeId;
    const int32_t reportedPowerLimit = stalePendingMode ? powerModeLimitMwForId(_powerModeId) : frame.powerLimitMw;
    _latest.powerLimitMw = (reportedPowerLimit < 0) ? powerModeLimitMwForId(_latest.powerModeId) : reportedPowerLimit;
    if (!stalePendingMode && _latest.powerModeId >= 0) {
        _powerModeId = _latest.powerModeId;
    }
    if (_pendingPowerModeId >= 0 && frame.powerModeId == _pendingPowerModeId) {
        _pendingPowerModeId = -1;
        _powerModeRequestInProgress = false;
        _powerModeRequestMs = 0;
    }
    if (_latest.powerLimitMw > 0) {
        _powerModeMaxMw = _latest.powerLimitMw;
        formatPowerModeUiLabel(_powerModeId, nullptr, _powerModeMaxMw, _powerModeLabel, sizeof(_powerModeLabel));
    }
    _latest.netDownloadKbps = frame.netDownloadKbps;
    _latest.netUploadKbps = frame.netUploadKbps;
    _latest.diskReadKbps = frame.diskReadKbps;
    _latest.diskWriteKbps = frame.diskWriteKbps;
    _latest.swapUsedMb = (frame.swapUsedMb < 0) ? -1 : frame.swapUsedMb;
    _latest.swapTotalMb = (frame.swapTotalMb < 0) ? -1 : frame.swapTotalMb;
    _latest.diskUsage = clampUsage(frame.diskUsage);
    _latest.diskUsedMb = (frame.diskUsedMb < 0) ? -1 : frame.diskUsedMb;
    _latest.diskTotalMb = (frame.diskTotalMb < 0) ? -1 : frame.diskTotalMb;

    _netGraphScaleKbps = chooseRateGraphScale(_netGraphScaleKbps,
                                               _latest.netDownloadKbps,
                                               _latest.netUploadKbps,
                                               kDefaultNetGraphScaleKbps);
    _diskGraphScaleKbps = chooseRateGraphScale(_diskGraphScaleKbps,
                                                _latest.diskReadKbps,
                                                _latest.diskWriteKbps,
                                                kDefaultDiskGraphScaleKbps);
    _swapGraphScaleMb = (_latest.powerLimitMw > 0) ? _latest.powerLimitMw : kDefaultPowerGraphScaleMw;

    const int16_t netDownGraph = rateToGraphPercent(_latest.netDownloadKbps, _netGraphScaleKbps);
    const int16_t netUpGraph = rateToGraphPercent(_latest.netUploadKbps, _netGraphScaleKbps);
    const int16_t diskReadGraph = rateToGraphPercent(_latest.diskReadKbps, _diskGraphScaleKbps);
    const int16_t diskWriteGraph = rateToGraphPercent(_latest.diskWriteKbps, _diskGraphScaleKbps);
    const int16_t powerGraph = rateToGraphPercent(_latest.powerMw, _swapGraphScaleMb);

    _cpuHistory[_historyWriteIndex] = _latest.cpuUsage;
    _gpuHistory[_historyWriteIndex] = _latest.gpuUsage;
    _ramHistory[_historyWriteIndex] = _latest.ramUsage;
    _netHistory[_historyWriteIndex] = netDownGraph;
    _netUploadHistory[_historyWriteIndex] = netUpGraph;
    _diskHistory[_historyWriteIndex] = diskReadGraph;
    _diskWriteHistory[_historyWriteIndex] = diskWriteGraph;
    _swapHistory[_historyWriteIndex] = powerGraph;

    _historyWriteIndex = (_historyWriteIndex + 1) % HISTORY_POINTS;
    if (_historyCount < HISTORY_POINTS) {
        ++_historyCount;
    }

    _hasMetrics = isUsageValid(_latest.cpuUsage) ||
                  isUsageValid(_latest.gpuUsage) ||
                  isUsageValid(_latest.ramUsage) ||
                  isTempValid(_latest.cpuTemp) ||
                  isTempValid(_latest.gpuTemp) ||
                  isPowerValid(_latest.powerMw) ||
                  (_latest.powerLimitMw >= 0) ||
                  (_latest.netDownloadKbps >= 0) ||
                  (_latest.netUploadKbps >= 0) ||
                  (_latest.diskReadKbps >= 0) ||
                  (_latest.diskWriteKbps >= 0) ||
                  (_latest.swapUsedMb >= 0) ||
                  (_latest.swapTotalMb >= 0) ||
                  isUsageValid(_latest.diskUsage) ||
                  (_latest.diskUsedMb >= 0) ||
                  (_latest.diskTotalMb >= 0);
    _lastMetricsMs = millis();
    if (!_settingsOpen) {
        _dirty = true;
    }
}

void LCD2Dashboard::pushBootKernelLine(const char* line) {
    const bool isTransitionState = (_state == SystemState::BOOTING_ON ||
                                    _state == SystemState::SHUTTING_DOWN);
    if (line == nullptr || line[0] == '\0' || !isTransitionState) {
        return;
    }

    appendBootLogWrapped(line);
    _dirty = true;
    _lastBootRefreshMs = 0;
}

void LCD2Dashboard::resetHistory() {
    _latest = {-1, -1, -1, -1.0f, -1.0f, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1};
    _historyWriteIndex = 0;
    _historyCount = 0;
    _hasMetrics = false;
    _lastMetricsMs = 0;
    _netGraphScaleKbps = kDefaultNetGraphScaleKbps;
    _diskGraphScaleKbps = kDefaultDiskGraphScaleKbps;
    _swapGraphScaleMb = kDefaultPowerGraphScaleMw;

    for (uint16_t i = 0; i < HISTORY_POINTS; ++i) {
        _cpuHistory[i] = -1;
        _gpuHistory[i] = -1;
        _ramHistory[i] = -1;
        _netHistory[i] = -1;
        _netUploadHistory[i] = -1;
        _diskHistory[i] = -1;
        _diskWriteHistory[i] = -1;
        _swapHistory[i] = -1;
    }
}

void LCD2Dashboard::clearBootLog() {
    _bootLogHead = 0;
    _bootLogCount = 0;
    for (uint8_t i = 0; i < kBootLogRingCapacity; ++i) {
        _bootLogLines[i][0] = '\0';
    }
}

void LCD2Dashboard::appendBootLogLine(const char* line) {
    if (line == nullptr || line[0] == '\0') {
        return;
    }

    uint8_t slot = 0;
    if (_bootLogCount < kBootLogRingCapacity) {
        slot = static_cast<uint8_t>((_bootLogHead + _bootLogCount) % kBootLogRingCapacity);
        ++_bootLogCount;
    } else {
        slot = _bootLogHead;
        _bootLogHead = static_cast<uint8_t>((_bootLogHead + 1) % kBootLogRingCapacity);
    }

    strncpy(_bootLogLines[slot], line, kBootLogLineMaxLen);
    _bootLogLines[slot][kBootLogLineMaxLen] = '\0';
}

void LCD2Dashboard::appendBootLogWrapped(const char* line) {
    const char* payload = stripKernelPrefix(line);
    if (payload == nullptr || payload[0] == '\0') {
        return;
    }

    char cleaned[kBootLogLineMaxLen + 1];
    size_t cleanLen = 0;
    for (size_t i = 0; payload[i] != '\0' && cleanLen < kBootLogLineMaxLen; ++i) {
        const char c = payload[i];
        cleaned[cleanLen++] = (c >= 32 && c <= 126) ? c : ' ';
    }
    cleaned[cleanLen] = '\0';

    if (cleaned[0] == '\0') {
        return;
    }

    int16_t charsPerLine = static_cast<int16_t>((_width > (kBootTextInsetX * 2))
                                                     ? ((_width - (kBootTextInsetX * 2)) / 6)
                                                     : 1);
    charsPerLine = clampInt16(charsPerLine, 12, static_cast<int16_t>(kBootLogLineMaxLen));

    const char* cursor = cleaned;
    while (*cursor != '\0') {
        while (*cursor == ' ') {
            ++cursor;
        }
        if (*cursor == '\0') {
            break;
        }

        size_t take = strlen(cursor);
        if (take > static_cast<size_t>(charsPerLine)) {
            take = static_cast<size_t>(charsPerLine);
            while (take > 8 && cursor[take] != '\0' && cursor[take] != ' ') {
                --take;
            }
            if (take == 0) {
                take = static_cast<size_t>(charsPerLine);
            }
        }

        char segment[kBootLogLineMaxLen + 1];
        memcpy(segment, cursor, take);
        segment[take] = '\0';
        appendBootLogLine(segment);
        cursor += take;
    }
}

void LCD2Dashboard::drawBootLogView() {
    if (!_bootSpriteReady) {
        return;
    }

    _bootLogSprite.fillSprite(kBootSpritePaper);
    _bootLogSprite.setTextColor(kBootSpriteInk, kBootSpritePaper);
    _bootLogSprite.setTextDatum(TL_DATUM);

    const char* header = (_state == SystemState::SHUTTING_DOWN)
                             ? "Jetson Serial2 Shutdown"
                             : "Jetson Serial2 Boot";

    _bootLogSprite.drawRect(0, 0, _width, _height, kBootSpriteInk);
    _bootLogSprite.drawString(header, kBootTextInsetX, kBootTextInsetY, 1);
    _bootLogSprite.drawFastHLine(1,
                                 static_cast<int16_t>(kBootHeaderHeight - 1),
                                 static_cast<int16_t>(_width - 2),
                                 kBootSpriteInk);

    const int16_t bodyTop = kBootHeaderHeight + 2;
    const int16_t bodyBottom = static_cast<int16_t>(_height) - 3;
    const int16_t lineCapacity = max<int16_t>(1, (bodyBottom - bodyTop + 1) / kBootLineHeight);

    if (_bootLogCount == 0) {
        _bootLogSprite.drawString("Waiting for kernel lines...", kBootTextInsetX, bodyTop, 1);
    } else {
        const uint8_t visibleCount = static_cast<uint8_t>(min<int16_t>(lineCapacity, _bootLogCount));
        const uint8_t skipCount = static_cast<uint8_t>(_bootLogCount - visibleCount);

        for (uint8_t i = 0; i < visibleCount; ++i) {
            const uint8_t logicalIndex = static_cast<uint8_t>(skipCount + i);
            const uint8_t ringIndex = static_cast<uint8_t>((_bootLogHead + logicalIndex) % kBootLogRingCapacity);
            const int16_t y = static_cast<int16_t>(bodyTop + (i * kBootLineHeight));
            _bootLogSprite.drawString(_bootLogLines[ringIndex], kBootTextInsetX, y, 1);
        }
    }

    _bootLogSprite.setBitmapColor(kTextColor, kBackgroundColor);
    _bootLogSprite.pushSprite(0, 0);
}

void LCD2Dashboard::drawHumidityWarningBanner(bool visible) {
    const int16_t bannerHeight = 12;
    const int16_t y = (_height > bannerHeight) ? static_cast<int16_t>(_height - bannerHeight) : 0;

    if (!visible && !_humidityBannerVisible) {
        return;
    }

    if (_humidityBannerSprite.created() &&
        _humidityBannerSprite.width() == _width &&
        _humidityBannerSprite.height() == bannerHeight) {
        _humidityBannerSprite.fillSprite(kBackgroundColor);
        if (visible) {
            _humidityBannerSprite.drawFastHLine(0, 0, _width, TFT_DARKGREY);
            _humidityBannerSprite.setTextColor(TFT_RED, kBackgroundColor);
            _humidityBannerSprite.setTextDatum(MC_DATUM);
            _humidityBannerSprite.drawString("HIGH HUMIDITY WARNING",
                                             static_cast<int16_t>(_width / 2),
                                             static_cast<int16_t>(bannerHeight / 2),
                                             1);
            _humidityBannerSprite.setTextDatum(TL_DATUM);
        }
        _humidityBannerSprite.pushSprite(0, y);
        _humidityBannerVisible = visible;
        return;
    }

    if (visible) {
        drawHumidityWarningBannerDirect(_tft, _width, _height);
    } else {
        _tft.fillRect(0, y, _width, bannerHeight, kBackgroundColor);
    }
    _humidityBannerVisible = visible;
}

void LCD2Dashboard::drawDegradedModeNotice(uint32_t nowMs, const char* reason) {
    if (!_driverReady || !_degradedMode) {
        return;
    }

    if (!_dirty && _lastRefreshMs != 0U && (nowMs - _lastRefreshMs) < 500U) {
        return;
    }

    _tft.fillScreen(kBackgroundColor);
    _tft.setTextColor(TFT_RED, kBackgroundColor);
    _tft.setTextDatum(TL_DATUM);
    _tft.drawString("LCD2 DEGRADED MODE", 8, 8, 2);

    if (reason != nullptr && reason[0] != '\0') {
        _tft.setTextColor(kMutedTextColor, kBackgroundColor);
        _tft.drawString(reason, 8, 34, 1);
    }

    _tft.setTextColor(kTextColor, kBackgroundColor);
    _lastRefreshMs = nowMs;
    _dirty = false;
}

void LCD2Dashboard::update(uint32_t nowMs) {
    if (!_driverReady) {
        return;
    }

    if (_settingsGameActive) {
        updatePowerOff(nowMs);
        if (_powerOffMode == PowerOffMode::IDLE) {
            if (_settingsGameMode == SettingsGameMode::DINO || _settingsGameMode == SettingsGameMode::BALL) {
                _settingsGameMode = SettingsGameMode::MENU;
                _powerOffMode = PowerOffMode::GAME_MENU;
                _powerOffLastFrameMs = 0;
                drawSettingsGameMenu();
            } else {
                _settingsGameActive = false;
                _settingsGameMode = SettingsGameMode::NONE;
                _settingsOpen = true;
                _settingsScreen = SettingsScreen::MAIN;
                _settingsDrawn = false;
                _settingsFullRedraw = true;
                _layoutDrawn = false;
                _dirty = true;
                _lastRefreshMs = 0;
            }
        }
        return;
    }

    // POWER_OFF: show idle screen / game menu / active game
    if (_state == SystemState::POWER_OFF) {
        updatePowerOff(nowMs);
        return;
    }

    if (_state == SystemState::BOOTING_ON || _state == SystemState::SHUTTING_DOWN) {
        if (!_bootSpriteReady) {
            drawDegradedModeNotice(nowMs, "BOOT LOG RENDER OFFLINE");
            return;
        }

        if (!_bootLayoutDrawn) {
            _tft.fillScreen(kBackgroundColor);
            _bootLayoutDrawn = true;
            _visible = true;
            _dirty = true;
        }

        if (!_dirty) {
            return;
        }

        if (_lastBootRefreshMs != 0U && (nowMs - _lastBootRefreshMs) < kBootLogRefreshPeriodMs) {
            return;
        }

        _lastBootRefreshMs = nowMs;
        drawBootLogView();
        drawHumidityWarningBanner(hasHighHumidityAlert(_alertMask));
        _dirty = false;
        return;
    }

    if (_state != SystemState::RUNNING || !_spritesReady) {
        if (_state == SystemState::RUNNING && !_spritesReady) {
            drawDegradedModeNotice(nowMs, "DASHBOARD RENDER OFFLINE");
        }
        return;
    }

    handleTouch(nowMs);

    if (_settingsGameActive) {
        return;
    }

    if (_settingsOpen) {
        const uint32_t settingsNowMs = millis();
        updateSettingsTimeouts(settingsNowMs);
        if (_settingsScreen == SettingsScreen::MAIN && !_dirty &&
            (_lastRefreshMs == 0U || (settingsNowMs - _lastRefreshMs) >= 1000U)) {
            _lastRefreshMs = settingsNowMs;
            drawSettingsUptimeRow();
            return;
        }
        if (!_dirty) {
            return;
        }

        const uint32_t refreshPeriodMs = kTouchUiRefreshPeriodMs;
        if (_lastRefreshMs != 0U && (settingsNowMs - _lastRefreshMs) < refreshPeriodMs) {
            return;
        }

        _lastRefreshMs = settingsNowMs;
        drawSettingsScreen();
        _dirty = false;
        return;
    }

    if (!_layoutDrawn) {
        drawLayout();
        _layoutDrawn = true;
        _dirty = true;
        _visible = true;
        _lastRefreshMs = 0;
    }

    const bool animateGraph = _hasMetrics && (_lastMetricsMs != 0U);
    if (!_dirty && !animateGraph) {
        return;
    }

    const uint32_t refreshPeriodMs = (_touchDown || _activeControl != ActiveControl::NONE || animateGraph)
                                         ? kGraphAnimationFrameMs
                                         : REFRESH_PERIOD_MS;
    if (_lastRefreshMs != 0U && (nowMs - _lastRefreshMs) < refreshPeriodMs) {
        return;
    }

    _lastRefreshMs = nowMs;
    const bool updatePanel = _dirty;

    if (_hasMetrics) {
        drawDynamic(nowMs, updatePanel);
    } else {
        drawNoData();
    }

    drawHumidityWarningBanner(hasHighHumidityAlert(_alertMask));

    _dirty = false;
}

void LCD2Dashboard::handleTouch(uint32_t nowMs) {
    if (!_touchReady) {
        return;
    }

    if ((nowMs - _lastTouchScanMs) < kTouchScanPeriodMs) {
        return;
    }
    _lastTouchScanMs = nowMs;

    uint16_t touchX = 0;
    uint16_t touchY = 0;
    const bool touched = _tft.getTouch(&touchX, &touchY, jetson_cfg::kLcd2TouchPressureThreshold);
    if (!touched) {
        if (!_settingsOpen && _dashboardTouchActive && handleDashboardSwipe(_touchLastX, _touchLastY)) {
            _layoutDrawn = false;
            _dirty = true;
            _lastRefreshMs = 0;
        }
        if (_settingsOpen && _touchDown) {
            _dirty = true;
        }
        _touchDown = false;
        _dashboardTouchActive = false;
        _touchSampleCount = 0;
        _activeControl = ActiveControl::NONE;
        return;
    }

    if (_settingsOpen) {
        const bool firstPress = !_touchDown;
        _touchDown = true;
        _touchLastX = static_cast<int16_t>(touchX);
        _touchLastY = static_cast<int16_t>(touchY);
        _dirty = true;
        if (firstPress || (_settingsScreen == SettingsScreen::MAIN && _activeControl == ActiveControl::LED_SLIDER)) {
            handleSettingsTouch(static_cast<int16_t>(touchX), static_cast<int16_t>(touchY));
        }
        return;
    }

    const DashboardLayout layout = buildLayout();
    const int16_t rawX = static_cast<int16_t>(touchX);
    const int16_t rawY = static_cast<int16_t>(touchY);

    if (!_touchDown) {
        _touchDown = true;
        _touchStartX = rawX;
        _touchStartY = rawY;
        _touchLastX = rawX;
        _touchLastY = rawY;
        _dashboardTouchActive = !pointInRect(rawX, rawY, layout.panelFrame);
    }

    _touchLastX = rawX;
    _touchLastY = rawY;

    if (_dashboardTouchActive) {
        return;
    }

    const int16_t panelX = layout.panelFrame.x + 1;
    const int16_t panelY = layout.panelFrame.y + 1;
    const int16_t panelW = layout.panelFrame.w - 2;
    const int16_t panelH = layout.panelFrame.h - 2;
    const int16_t localX = rawX - panelX;
    const int16_t localY = rawY - panelY;

    const Rect panelRect = {0, 0, panelW, panelH};
    if (!pointInRect(localX, localY, panelRect)) {
        _touchDown = false;
        _touchSampleCount = 0;
        _activeControl = ActiveControl::NONE;
        return;
    }

    if (pointInRect(localX, localY, makeSettingsPanelButtonRect(panelW))) {
        _settingsOpen = true;
        _settingsDrawn = false;
        _settingsFullRedraw = true;
        _layoutDrawn = false;
        _dirty = true;
        _touchSampleCount = 0;
        _activeControl = ActiveControl::NONE;
        return;
    }

    (void)panelH;
    return;
}

bool LCD2Dashboard::handleDashboardSwipe(int16_t endX, int16_t endY) {
    const int16_t dx = static_cast<int16_t>(endX - _touchStartX);
    const int16_t dy = static_cast<int16_t>(endY - _touchStartY);
    const int16_t absDx = abs(dx);
    const int16_t absDy = abs(dy);

    if (absDx < kDashboardSwipeMinDistance || absDy > kDashboardSwipeMaxVertical || absDx <= (absDy * 2)) {
        return false;
    }

    if (dx < 0 && _dashboardPage == DashboardPage::PERFORMANCE) {
        _dashboardPage = DashboardPage::IO;
        return true;
    }

    if (dx > 0 && _dashboardPage == DashboardPage::IO) {
        _dashboardPage = DashboardPage::PERFORMANCE;
        return true;
    }

    return false;
}

bool LCD2Dashboard::queueJetsonCommand(const char* command) {
    if (command == nullptr || command[0] == '\0') {
        Serial.println("[LCD2][JETSON_CMD] reject empty command");
        return false;
    }
    if (_hasPendingJetsonCommand) {
        Serial.print("[LCD2][JETSON_CMD] busy, drop: ");
        Serial.println(command);
        return false;
    }

    strncpy(_pendingJetsonCommand, command, sizeof(_pendingJetsonCommand) - 1);
    _pendingJetsonCommand[sizeof(_pendingJetsonCommand) - 1] = '\0';
    _hasPendingJetsonCommand = true;
    Serial.print("[LCD2][JETSON_CMD] queued at ");
    Serial.print(millis());
    Serial.print(": ");
    Serial.println(_pendingJetsonCommand);
    return true;
}

void LCD2Dashboard::resetWifiEntries() {
    _wifiEntryCount = 0;
    _selectedWifiIndex = -1;
    for (uint8_t i = 0; i < kWifiEntryMax; ++i) {
        _wifiEntries[i].ssid[0] = '\0';
        _wifiEntries[i].security[0] = '\0';
        _wifiEntries[i].signal = 0;
        _wifiEntries[i].current = false;
    }
}

void LCD2Dashboard::requestWifiScan() {
    const uint32_t now = millis();
    Serial.print("[LCD2][WIFI] request scan at ");
    Serial.println(now);

    if (queueJetsonCommand("REQ:WIFI_SCAN")) {
        resetWifiEntries();
        _wifiScanInProgress = true;
        _wifiScanRequestMs = now;
        strncpy(_wifiStatus, "Scanning", sizeof(_wifiStatus) - 1);
    } else {
        _wifiScanInProgress = false;
        strncpy(_wifiStatus, "Command busy", sizeof(_wifiStatus) - 1);
    }

    _wifiStatus[sizeof(_wifiStatus) - 1] = '\0';
    _settingsScreen = SettingsScreen::WIFI_LIST;
    _settingsDrawn = false;
    _dirty = true;
    _lastRefreshMs = 0;
}

void LCD2Dashboard::requestIpInfo() {
    const uint32_t now = millis();
    Serial.print("[LCD2][IP] request at ");
    Serial.println(now);

    strncpy(_ipInterface, "N/A", sizeof(_ipInterface) - 1);
    _ipInterface[sizeof(_ipInterface) - 1] = '\0';
    strncpy(_ipSsid, "N/A", sizeof(_ipSsid) - 1);
    _ipSsid[sizeof(_ipSsid) - 1] = '\0';
    strncpy(_ipAddress, "N/A", sizeof(_ipAddress) - 1);
    _ipAddress[sizeof(_ipAddress) - 1] = '\0';

    if (queueJetsonCommand("REQ:IP")) {
        _ipRequestInProgress = true;
        _ipRequestMs = now;
        strncpy(_ipStatus, "LOADING", sizeof(_ipStatus) - 1);
    } else {
        _ipRequestInProgress = false;
        strncpy(_ipStatus, "BUSY", sizeof(_ipStatus) - 1);
    }

    _ipStatus[sizeof(_ipStatus) - 1] = '\0';
    _settingsScreen = SettingsScreen::IP_VIEW;
    _dirty = true;
    _lastRefreshMs = 0;
}
void LCD2Dashboard::requestAboutInfo() {
    const uint32_t now = millis();
    if (queueJetsonCommand("REQ:ABOUT")) {
        _aboutRequestInProgress = true;
        _aboutRequestMs = now;
    } else {
        strncpy(_systemStatus, "Command busy", sizeof(_systemStatus) - 1);
        _systemStatus[sizeof(_systemStatus) - 1] = '\0';
    }
    _settingsScreen = SettingsScreen::ABOUT;
    _dirty = true;
    _lastRefreshMs = 0;
}

void LCD2Dashboard::requestSshStatus() {
    const uint32_t now = millis();
    if (queueJetsonCommand("REQ:SSH_STATUS")) {
        _sshRequestInProgress = true;
        _sshRequestMs = now;
    } else {
        strncpy(_systemStatus, "Command busy", sizeof(_systemStatus) - 1);
        _systemStatus[sizeof(_systemStatus) - 1] = '\0';
    }
    _settingsScreen = SettingsScreen::SSH;
    _dirty = true;
    _lastRefreshMs = 0;
}

void LCD2Dashboard::requestNgrokStatus() {
    const uint32_t now = millis();
    if (queueJetsonCommand("REQ:NGROK_STATUS")) {
        _ngrokRequestInProgress = true;
        _ngrokRequestMs = now;
    } else {
        strncpy(_systemStatus, "Command busy", sizeof(_systemStatus) - 1);
        _systemStatus[sizeof(_systemStatus) - 1] = '\0';
    }
    _settingsScreen = SettingsScreen::NGROK;
    _dirty = true;
    _lastRefreshMs = 0;
}

void LCD2Dashboard::startNgrokAction(NgrokActionState action) {
    if (action == NgrokActionState::NONE) {
        return;
    }

    const uint32_t now = millis();
    const bool starting = (action == NgrokActionState::STARTING);
    if (requestSimpleCommand(starting ? "NGROK_START" : "NGROK_STOP",
                             "Ngrok busy",
                             starting ? "Ngrok start requested" : "Ngrok stop requested")) {
        _ngrokActionState = action;
        _ngrokRequestInProgress = true;
        _ngrokRequestMs = now;
        _ngrokPollMs = 0;
    }
    _settingsScreen = SettingsScreen::NGROK;
    _dirty = true;
    _lastRefreshMs = 0;
}

void LCD2Dashboard::pollNgrokAction(uint32_t nowMs) {
    if (_ngrokActionState == NgrokActionState::NONE) {
        return;
    }
    if (_ngrokPollMs != 0U && (nowMs - _ngrokPollMs) < kNgrokActionPollMs) {
        return;
    }

    _ngrokPollMs = nowMs;
    if (queueJetsonCommand("REQ:NGROK_STATUS")) {
        _ngrokRequestInProgress = true;
        _ngrokRequestMs = nowMs;
    }
}

void LCD2Dashboard::requestHeadlessStatus() {
    const uint32_t now = millis();
    if (queueJetsonCommand("REQ:HEADLESS_STATUS")) {
        _headlessRequestInProgress = true;
        _headlessRequestMs = now;
    } else {
        strncpy(_systemStatus, "Command busy", sizeof(_systemStatus) - 1);
        _systemStatus[sizeof(_systemStatus) - 1] = '\0';
    }
    _settingsScreen = SettingsScreen::HEADLESS;
    _dirty = true;
    _lastRefreshMs = 0;
}

void LCD2Dashboard::requestPowerModeStatus() {
    const uint32_t now = millis();
    if (queueJetsonCommand("REQ:POWER_MODE")) {
        _pendingPowerModeId = -1;
        _powerModeRequestInProgress = true;
        _powerModeRequestMs = now;
    } else {
        strncpy(_systemStatus, "Command busy", sizeof(_systemStatus) - 1);
        _systemStatus[sizeof(_systemStatus) - 1] = '\0';
    }
    _settingsScreen = SettingsScreen::POWER_MODE;
    _dirty = true;
    _lastRefreshMs = 0;
}

bool LCD2Dashboard::requestSimpleCommand(const char* command, const char* busyStatus, const char* queuedStatus) {
    const bool queued = queueJetsonCommand(command);
    strncpy(_systemStatus, queued ? queuedStatus : busyStatus, sizeof(_systemStatus) - 1);
    _systemStatus[sizeof(_systemStatus) - 1] = '\0';
    _dirty = true;
    _lastRefreshMs = 0;
    return queued;
}

void LCD2Dashboard::openConfirm(SettingsConfirmAction action,
                                SettingsScreen returnScreen,
                                const char* title,
                                const char* body) {
    _confirmAction = action;
    _confirmReturnScreen = returnScreen;
    strncpy(_confirmTitle, title, sizeof(_confirmTitle) - 1);
    _confirmTitle[sizeof(_confirmTitle) - 1] = '\0';
    strncpy(_confirmBody, body, sizeof(_confirmBody) - 1);
    _confirmBody[sizeof(_confirmBody) - 1] = '\0';
    _settingsScreen = SettingsScreen::CONFIRM;
    _dirty = true;
    _lastRefreshMs = 0;
}

void LCD2Dashboard::executeConfirmAction() {
    const SettingsConfirmAction action = _confirmAction;
    SettingsScreen returnScreen = _confirmReturnScreen;
    _confirmAction = SettingsConfirmAction::NONE;
    _settingsScreen = returnScreen;

    switch (action) {
        case SettingsConfirmAction::SSH_STOP:
            requestSimpleCommand("SSH_STOP", "SSH busy", "SSH stop requested");
            _sshRequestInProgress = true;
            _sshRequestMs = millis();
            break;
        case SettingsConfirmAction::NGROK_STOP:
            startNgrokAction(NgrokActionState::STOPPING);
            break;
        case SettingsConfirmAction::HEADLESS_APPLY_NOW:
            requestSimpleCommand("HEADLESS_APPLY_NOW", "Headless busy", "Apply now requested");
            _headlessRequestInProgress = true;
            _headlessRequestMs = millis();
            break;
        case SettingsConfirmAction::HEADLESS_REBOOT:
        case SettingsConfirmAction::JETSON_REBOOT:
            requestSimpleCommand("JETSON_REBOOT", "System busy", "Reboot requested");
            break;
        case SettingsConfirmAction::MONITOR_RESTART:
            requestSimpleCommand("MONITOR_RESTART", "System busy", "Monitor restart requested");
            break;
        case SettingsConfirmAction::POWER_MODE_SET_0:
            if (requestSimpleCommand("POWER_MODE_SET 0", "Power busy", "15W mode requested")) {
                _powerModeId = 0;
                _pendingPowerModeId = 0;
                formatPowerModeUiLabel(_powerModeId, nullptr, powerModeLimitMwForId(_powerModeId), _powerModeLabel, sizeof(_powerModeLabel));
                _powerModeRequestInProgress = true;
                _powerModeRequestMs = millis();
            }
            break;
        case SettingsConfirmAction::POWER_MODE_SET_1:
            if (requestSimpleCommand("POWER_MODE_SET 1", "Power busy", "25W mode requested")) {
                _powerModeId = 1;
                _pendingPowerModeId = 1;
                formatPowerModeUiLabel(_powerModeId, nullptr, powerModeLimitMwForId(_powerModeId), _powerModeLabel, sizeof(_powerModeLabel));
                _powerModeRequestInProgress = true;
                _powerModeRequestMs = millis();
            }
            break;
        case SettingsConfirmAction::POWER_MODE_SET_2:
            if (requestSimpleCommand("POWER_MODE_SET 2", "Power busy", "MAXN_SUPER mode requested")) {
                _powerModeId = 2;
                _pendingPowerModeId = 2;
                formatPowerModeUiLabel(_powerModeId, nullptr, powerModeLimitMwForId(_powerModeId), _powerModeLabel, sizeof(_powerModeLabel));
                _powerModeRequestInProgress = true;
                _powerModeRequestMs = millis();
            }
            break;
        case SettingsConfirmAction::POWER_MODE_SET_3:
            if (requestSimpleCommand("POWER_MODE_SET 3", "Power busy", "7W mode requested")) {
                _powerModeId = 3;
                _pendingPowerModeId = 3;
                formatPowerModeUiLabel(_powerModeId, nullptr, powerModeLimitMwForId(_powerModeId), _powerModeLabel, sizeof(_powerModeLabel));
                _powerModeRequestInProgress = true;
                _powerModeRequestMs = millis();
            }
            break;
        case SettingsConfirmAction::NONE:
        default:
            _dirty = true;
            break;
    }
}

void LCD2Dashboard::requestWifiConnect() {
    if (_selectedWifiIndex < 0 || _selectedWifiIndex >= static_cast<int8_t>(_wifiEntryCount)) {
        return;
    }

    char ssidEsc[96];
    char pskEsc[96];
    char command[kJetsonCommandMaxLen];
    escapeCommandField(_wifiEntries[_selectedWifiIndex].ssid, ssidEsc, sizeof(ssidEsc));
    escapeCommandField(_keyboard.text(), pskEsc, sizeof(pskEsc));
    snprintf(command, sizeof(command), "WIFI_CONNECT SSID:%s PSK:%s", ssidEsc, pskEsc);

    if (queueJetsonCommand(command)) {
        _wifiConnectInProgress = true;
        _wifiConnectOk = false;
        _wifiConnectRequestMs = millis();
        strncpy(_connectStatus, "Connecting", sizeof(_connectStatus) - 1);
    } else {
        _wifiConnectInProgress = false;
        _wifiConnectOk = false;
        strncpy(_connectStatus, "Command busy", sizeof(_connectStatus) - 1);
    }

    _connectStatus[sizeof(_connectStatus) - 1] = '\0';
    _settingsScreen = SettingsScreen::CONNECT_RESULT;
    _dirty = true;
    _lastRefreshMs = 0;
}
void LCD2Dashboard::updateSettingsTimeouts(uint32_t nowMs) {
    if (_wifiScanInProgress && _wifiScanRequestMs != 0U &&
        (nowMs - _wifiScanRequestMs) >= kWifiScanTimeoutMs) {
        Serial.print("[LCD2][WIFI] timeout now=");
        Serial.print(nowMs);
        Serial.print(" req=");
        Serial.print(_wifiScanRequestMs);
        Serial.print(" delta=");
        Serial.println(nowMs - _wifiScanRequestMs);
        _wifiScanInProgress = false;
        _wifiScanRequestMs = 0;
        strncpy(_wifiStatus, (_wifiEntryCount == 0) ? "Scan timeout" : "Partial scan", sizeof(_wifiStatus) - 1);
        _wifiStatus[sizeof(_wifiStatus) - 1] = '\0';
        _dirty = true;
        _lastRefreshMs = 0;
    }

    if (_ipRequestInProgress && _ipRequestMs != 0U &&
        (nowMs - _ipRequestMs) >= kIpRequestTimeoutMs) {
        Serial.print("[LCD2][IP] timeout now=");
        Serial.print(nowMs);
        Serial.print(" req=");
        Serial.print(_ipRequestMs);
        Serial.print(" delta=");
        Serial.println(nowMs - _ipRequestMs);
        _ipRequestInProgress = false;
        _ipRequestMs = 0;
        strncpy(_ipStatus, "TIMEOUT", sizeof(_ipStatus) - 1);
        _ipStatus[sizeof(_ipStatus) - 1] = '\0';
        _dirty = true;
        _lastRefreshMs = 0;
    }

    if (_wifiConnectInProgress && _wifiConnectRequestMs != 0U &&
        (nowMs - _wifiConnectRequestMs) >= kWifiConnectTimeoutMs) {
        _wifiConnectInProgress = false;
        _wifiConnectRequestMs = 0;
        _wifiConnectOk = false;
        strncpy(_connectStatus, "Connection timeout", sizeof(_connectStatus) - 1);
        _connectStatus[sizeof(_connectStatus) - 1] = '\0';
        _dirty = true;
    }

    if (_aboutRequestInProgress && _aboutRequestMs != 0U && (nowMs - _aboutRequestMs) >= kIpRequestTimeoutMs) {
        _aboutRequestInProgress = false;
        _aboutRequestMs = 0;
        strncpy(_aboutVersion, "TIMEOUT", sizeof(_aboutVersion) - 1);
        _aboutVersion[sizeof(_aboutVersion) - 1] = '\0';
        _dirty = true;
    }
    if (_sshRequestInProgress && _sshRequestMs != 0U && (nowMs - _sshRequestMs) >= kIpRequestTimeoutMs) {
        _sshRequestInProgress = false;
        _sshRequestMs = 0;
        strncpy(_sshStatus, "TIMEOUT", sizeof(_sshStatus) - 1);
        _sshStatus[sizeof(_sshStatus) - 1] = '\0';
        _dirty = true;
    }
    if (_ngrokRequestInProgress && _ngrokRequestMs != 0U && (nowMs - _ngrokRequestMs) >= kIpRequestTimeoutMs) {
        _ngrokRequestInProgress = false;
        _ngrokRequestMs = 0;
        strncpy(_ngrokStatus, "TIMEOUT", sizeof(_ngrokStatus) - 1);
        _ngrokStatus[sizeof(_ngrokStatus) - 1] = '\0';
        _dirty = true;
    }
    if (_headlessRequestInProgress && _headlessRequestMs != 0U && (nowMs - _headlessRequestMs) >= kIpRequestTimeoutMs) {
        _headlessRequestInProgress = false;
        _headlessRequestMs = 0;
        strncpy(_headlessDefault, "TIMEOUT", sizeof(_headlessDefault) - 1);
        _headlessDefault[sizeof(_headlessDefault) - 1] = '\0';
        _dirty = true;
    }
    if (_powerModeRequestInProgress && _powerModeRequestMs != 0U && (nowMs - _powerModeRequestMs) >= kIpRequestTimeoutMs) {
        _powerModeRequestInProgress = false;
        _powerModeRequestMs = 0;
        _pendingPowerModeId = -1;
        strncpy(_powerModeLabel, "TIMEOUT", sizeof(_powerModeLabel) - 1);
        _powerModeLabel[sizeof(_powerModeLabel) - 1] = '\0';
        _dirty = true;
    }

    pollNgrokAction(nowMs);
}

void LCD2Dashboard::escapeCommandField(const char* src, char* dst, size_t dstSize) const {
    static const char hex[] = "0123456789ABCDEF";
    size_t pos = 0;
    if (dst == nullptr || dstSize == 0) {
        return;
    }

    for (size_t i = 0; src != nullptr && src[i] != '\0' && pos + 1 < dstSize; ++i) {
        const uint8_t c = static_cast<uint8_t>(src[i]);
        if (isalnum(c) || c == '_' || c == '-' || c == '.') {
            dst[pos++] = static_cast<char>(c);
        } else {
            if (pos + 3 >= dstSize) {
                break;
            }
            dst[pos++] = '%';
            dst[pos++] = hex[c >> 4];
            dst[pos++] = hex[c & 0x0F];
        }
    }
    dst[pos] = '\0';
}

void LCD2Dashboard::unescapeResponseField(const char* src, char* dst, size_t dstSize) const {
    size_t pos = 0;
    if (dst == nullptr || dstSize == 0) {
        return;
    }

    for (size_t i = 0; src != nullptr && src[i] != '\0' && pos + 1 < dstSize; ++i) {
        if (src[i] == '%' && isxdigit(static_cast<unsigned char>(src[i + 1])) && isxdigit(static_cast<unsigned char>(src[i + 2]))) {
            const char hiChar = src[i + 1];
            const char loChar = src[i + 2];
            const uint8_t hi = static_cast<uint8_t>(isdigit(hiChar) ? (hiChar - '0') : (toupper(hiChar) - 'A' + 10));
            const uint8_t lo = static_cast<uint8_t>(isdigit(loChar) ? (loChar - '0') : (toupper(loChar) - 'A' + 10));
            dst[pos++] = static_cast<char>((hi << 4) | lo);
            i += 2;
        } else {
            dst[pos++] = src[i];
        }
    }
    dst[pos] = '\0';
}

const char* LCD2Dashboard::tokenValue(const char* line, const char* key, char* out, size_t outSize) const {
    if (line == nullptr || key == nullptr || out == nullptr || outSize == 0) {
        return nullptr;
    }

    const char* start = strstr(line, key);
    if (start == nullptr) {
        out[0] = '\0';
        return nullptr;
    }

    start += strlen(key);
    size_t pos = 0;
    while (start[pos] != '\0' && !isspace(static_cast<unsigned char>(start[pos])) && pos + 1 < outSize) {
        out[pos] = start[pos];
        ++pos;
    }
    out[pos] = '\0';
    return out;
}

void LCD2Dashboard::parseWifiLine(const char* line) {
    char encoded[96] = {0};
    char signal[8] = {0};
    char security[32] = {0};
    char current[8] = {0};

    if (tokenValue(line, "SSID:", encoded, sizeof(encoded)) == nullptr) {
        return;
    }

    WifiEntry incoming = {};
    unescapeResponseField(encoded, incoming.ssid, sizeof(incoming.ssid));
    tokenValue(line, "SIG:", signal, sizeof(signal));
    tokenValue(line, "SEC:", security, sizeof(security));
    tokenValue(line, "CUR:", current, sizeof(current));
    unescapeResponseField(security, incoming.security, sizeof(incoming.security));
    incoming.signal = static_cast<uint8_t>(constrain(atoi(signal), 0, 100));
    incoming.current = (atoi(current) != 0);

    if (incoming.ssid[0] == '\0') {
        return;
    }

    for (uint8_t i = 0; i < _wifiEntryCount; ++i) {
        if (strcmp(_wifiEntries[i].ssid, incoming.ssid) != 0) {
            continue;
        }
        if (incoming.signal >= _wifiEntries[i].signal || incoming.current) {
            _wifiEntries[i] = incoming;
        }
        sortWifiEntries();
        return;
    }

    if (_wifiEntryCount < kWifiEntryMax) {
        _wifiEntries[_wifiEntryCount++] = incoming;
    } else if (incoming.signal > _wifiEntries[_wifiEntryCount - 1].signal) {
        _wifiEntries[_wifiEntryCount - 1] = incoming;
    } else {
        return;
    }

    sortWifiEntries();
    if (_wifiScanInProgress && strcmp(_wifiStatus, "Scanning") == 0) {
        strncpy(_wifiStatus, "Scanning", sizeof(_wifiStatus) - 1);
        _wifiStatus[sizeof(_wifiStatus) - 1] = '\0';
    }
}

void LCD2Dashboard::sortWifiEntries() {
    for (uint8_t i = 1; i < _wifiEntryCount; ++i) {
        WifiEntry key = _wifiEntries[i];
        int8_t j = static_cast<int8_t>(i) - 1;
        while (j >= 0 && _wifiEntries[j].signal < key.signal) {
            _wifiEntries[j + 1] = _wifiEntries[j];
            --j;
        }
        _wifiEntries[j + 1] = key;
    }
}

void LCD2Dashboard::parseIpLine(const char* line) {
    char encodedSsid[96] = {0};
    char value[40] = {0};

    if (tokenValue(line, "IF:", value, sizeof(value)) != nullptr) {
        strncpy(_ipInterface, value, sizeof(_ipInterface) - 1);
        _ipInterface[sizeof(_ipInterface) - 1] = '\0';
    }
    if (tokenValue(line, "SSID:", encodedSsid, sizeof(encodedSsid)) != nullptr) {
        unescapeResponseField(encodedSsid, _ipSsid, sizeof(_ipSsid));
    }
    if (tokenValue(line, "ADDR:", value, sizeof(value)) != nullptr) {
        strncpy(_ipAddress, value, sizeof(_ipAddress) - 1);
        _ipAddress[sizeof(_ipAddress) - 1] = '\0';
    }
    if (tokenValue(line, "STAT:", value, sizeof(value)) != nullptr) {
        strncpy(_ipStatus, value, sizeof(_ipStatus) - 1);
        _ipStatus[sizeof(_ipStatus) - 1] = '\0';
    }
    if (tokenValue(line, "HOST:", value, sizeof(value)) != nullptr) {
        unescapeResponseField(value, _aboutHostname, sizeof(_aboutHostname));
    }
    _ipRequestInProgress = false;
    _ipRequestMs = 0;
}

void LCD2Dashboard::parseConnectLine(const char* line) {
    _wifiConnectInProgress = false;
    _wifiConnectRequestMs = 0;
    _wifiConnectOk = (strstr(line, "WIFI_CONNECT OK") != nullptr);
    strncpy(_connectStatus, _wifiConnectOk ? "Connected" : "Connection failed", sizeof(_connectStatus) - 1);
    _connectStatus[sizeof(_connectStatus) - 1] = '\0';
}


void LCD2Dashboard::parseAboutLine(const char* line) {
    char value[80] = {0};
    if (tokenValue(line, "HOST:", value, sizeof(value)) != nullptr) {
        unescapeResponseField(value, _aboutHostname, sizeof(_aboutHostname));
    }
    if (tokenValue(line, "IP:", value, sizeof(value)) != nullptr) {
        strncpy(_ipAddress, value, sizeof(_ipAddress) - 1);
        _ipAddress[sizeof(_ipAddress) - 1] = '\0';
    }
    if (tokenValue(line, "VER:", value, sizeof(value)) != nullptr) {
        unescapeResponseField(value, _aboutVersion, sizeof(_aboutVersion));
    }
    _aboutRequestInProgress = false;
    _aboutRequestMs = 0;
}

void LCD2Dashboard::parseServiceStatusLine(const char* line, bool ngrok) {
    char value[96] = {0};
    if (tokenValue(line, "STAT:", value, sizeof(value)) != nullptr) {
        strncpy(ngrok ? _ngrokStatus : _sshStatus, value, ngrok ? sizeof(_ngrokStatus) - 1 : sizeof(_sshStatus) - 1);
        if (ngrok) _ngrokStatus[sizeof(_ngrokStatus) - 1] = '\0';
        else _sshStatus[sizeof(_sshStatus) - 1] = '\0';
    }
    if (tokenValue(line, "EN:", value, sizeof(value)) != nullptr) {
        strncpy(ngrok ? _ngrokEnabled : _sshEnabled, strcmp(value, "1") == 0 ? "YES" : "NO", ngrok ? sizeof(_ngrokEnabled) - 1 : sizeof(_sshEnabled) - 1);
        if (ngrok) _ngrokEnabled[sizeof(_ngrokEnabled) - 1] = '\0';
        else _sshEnabled[sizeof(_sshEnabled) - 1] = '\0';
    }
    if (tokenValue(line, "SVC:", value, sizeof(value)) != nullptr) {
        if (ngrok) unescapeResponseField(value, _ngrokService, sizeof(_ngrokService));
        else unescapeResponseField(value, _sshService, sizeof(_sshService));
    }
    if (ngrok && tokenValue(line, "END:", value, sizeof(value)) != nullptr) {
        unescapeResponseField(value, _ngrokEndpoint, sizeof(_ngrokEndpoint));
    }
    if (ngrok && tokenValue(line, "API:", value, sizeof(value)) != nullptr) {
        strncpy(_ngrokApi, value, sizeof(_ngrokApi) - 1);
        _ngrokApi[sizeof(_ngrokApi) - 1] = '\0';
    }
    if (ngrok) {
        _ngrokRequestInProgress = false;
        _ngrokRequestMs = 0;
        if (_ngrokActionState == NgrokActionState::STARTING &&
            strcmp(_ngrokStatus, "RUNNING") == 0 && strcmp(_ngrokApi, "OK") == 0) {
            _ngrokActionState = NgrokActionState::NONE;
            _ngrokPollMs = 0;
            strncpy(_systemStatus, "Ngrok started", sizeof(_systemStatus) - 1);
            _systemStatus[sizeof(_systemStatus) - 1] = '\0';
            _dirty = true;
        } else if (_ngrokActionState == NgrokActionState::STOPPING && strcmp(_ngrokStatus, "STOPPED") == 0) {
            _ngrokActionState = NgrokActionState::NONE;
            _ngrokPollMs = 0;
            strncpy(_systemStatus, "Ngrok stopped", sizeof(_systemStatus) - 1);
            _systemStatus[sizeof(_systemStatus) - 1] = '\0';
            _dirty = true;
        }
    } else {
        _sshRequestInProgress = false;
        _sshRequestMs = 0;
    }
}

void LCD2Dashboard::parseHeadlessLine(const char* line) {
    char value[32] = {0};
    if (tokenValue(line, "DEF:", value, sizeof(value)) != nullptr) {
        strncpy(_headlessDefault, value, sizeof(_headlessDefault) - 1);
        _headlessDefault[sizeof(_headlessDefault) - 1] = '\0';
    }
    if (tokenValue(line, "ACT:", value, sizeof(value)) != nullptr) {
        strncpy(_headlessActive, value, sizeof(_headlessActive) - 1);
        _headlessActive[sizeof(_headlessActive) - 1] = '\0';
    }
    _headlessRequestInProgress = false;
    _headlessRequestMs = 0;
}

void LCD2Dashboard::parsePowerModeLine(const char* line) {
    char value[48] = {0};
    char name[48] = {0};
    int id = _powerModeId;
    int32_t maxMw = _powerModeMaxMw;
    const bool isSetResult = (strncmp(line, "POWER_MODE_RESULT ", 18) == 0);
    const bool hasId = (tokenValue(line, "ID:", value, sizeof(value)) != nullptr);
    if (hasId) {
        id = atoi(value);
    }
    if (tokenValue(line, "MAX:", value, sizeof(value)) != nullptr) {
        maxMw = static_cast<int32_t>(atol(value));
    }
    if (tokenValue(line, "NAME:", value, sizeof(value)) != nullptr) {
        unescapeResponseField(value, name, sizeof(name));
    }
    if (_pendingPowerModeId >= 0 && (!hasId || id != _pendingPowerModeId)) {
        return;
    }
    _powerModeId = static_cast<int8_t>(id);
    _powerModeMaxMw = maxMw;
    if (name[0] == '\0') {
        strncpy(name, powerModeNameForId(id), sizeof(name) - 1);
        name[sizeof(name) - 1] = '\0';
    }
    if (maxMw <= 0) {
        maxMw = powerModeLimitMwForId(id);
        _powerModeMaxMw = maxMw;
    }
    formatPowerModeUiLabel(id, name, maxMw, _powerModeLabel, sizeof(_powerModeLabel));
    if (!isSetResult || _pendingPowerModeId < 0) {
        _powerModeRequestInProgress = false;
        _powerModeRequestMs = 0;
        _pendingPowerModeId = -1;
    }
    _settingsDrawn = false;
}

void LCD2Dashboard::parseResultLine(const char* line) {
    strncpy(_systemStatus, line, sizeof(_systemStatus) - 1);
    _systemStatus[sizeof(_systemStatus) - 1] = '\0';
    _sshRequestInProgress = false;
    _ngrokRequestInProgress = false;
    _headlessRequestInProgress = false;

    if (strncmp(line, "NGROK_RESULT ", 13) == 0 && strstr(line, "STAT:FAIL") != nullptr) {
        _ngrokActionState = NgrokActionState::NONE;
        _ngrokPollMs = 0;
    }
}

LCD2Dashboard::Rect LCD2Dashboard::makeSettingsPanelButtonRect(int16_t panelW) const {
    return {static_cast<int16_t>(panelW - kSettingsPanelButtonW - 2),
            8,
            kSettingsPanelButtonW,
            kSettingsPanelButtonH};
}

LCD2Dashboard::Rect LCD2Dashboard::makeSettingsExitButtonRect() const {
    return {static_cast<int16_t>(_width - 56), 4, 52, 26};
}

LCD2Dashboard::Rect LCD2Dashboard::makeSettingsBackButtonRect() const {
    return {4, 4, 56, 28};
}

LCD2Dashboard::Rect LCD2Dashboard::makeSettingsSliderRect() const {
    return {12, 42, static_cast<int16_t>(_width - 118), 44};
}

LCD2Dashboard::Rect LCD2Dashboard::makeSettingsWifiButtonRect() const {
    return {static_cast<int16_t>(_width - 94), 48, 82, 34};
}

LCD2Dashboard::Rect LCD2Dashboard::makeSettingsIpButtonRect() const {
    return makeSettingsButtonRect(0);
}

LCD2Dashboard::Rect LCD2Dashboard::makeSettingsNetworkButtonRect() const {
    return {static_cast<int16_t>(_width - 94), 73, 82, 34};
}

LCD2Dashboard::Rect LCD2Dashboard::makeSettingsSystemButtonRect() const {
    return {static_cast<int16_t>(_width - 94), 113, 82, 34};
}

LCD2Dashboard::Rect LCD2Dashboard::makeSettingsAboutButtonRect() const {
    return {static_cast<int16_t>(_width - 94), 153, 82, 34};
}

LCD2Dashboard::Rect LCD2Dashboard::makeSettingsGamesButtonRect() const {
    return {static_cast<int16_t>(_width - 94), 189, 82, 34};
}

LCD2Dashboard::Rect LCD2Dashboard::makeSettingsButtonRect(uint8_t index, uint8_t columns) const {
    const int16_t left = 12;
    const int16_t top = 44;
    const int16_t gap = 8;
    const int16_t h = 40;
    columns = max<uint8_t>(1, columns);
    const int16_t usableW = static_cast<int16_t>(_width - 24 - ((columns - 1) * gap));
    const int16_t w = static_cast<int16_t>(usableW / columns);
    const int16_t row = static_cast<int16_t>(index / columns);
    const int16_t col = static_cast<int16_t>(index % columns);
    return {static_cast<int16_t>(left + col * (w + gap)),
            static_cast<int16_t>(top + row * (h + gap)),
            w,
            h};
}

LCD2Dashboard::Rect LCD2Dashboard::makeSettingsHelpButtonRect() const {
    return {static_cast<int16_t>(_width - 38), 4, 28, 28};
}

LCD2Dashboard::Rect LCD2Dashboard::makeSettingsRefreshButtonRect() const {
    return {12, static_cast<int16_t>(_height - 42), static_cast<int16_t>(_width - 24), 32};
}

LCD2Dashboard::Rect LCD2Dashboard::makeSettingsConfirmCancelRect() const {
    return {26, static_cast<int16_t>(_height - 48), 110, 34};
}

LCD2Dashboard::Rect LCD2Dashboard::makeSettingsConfirmOkRect() const {
    return {static_cast<int16_t>(_width - 136), static_cast<int16_t>(_height - 48), 110, 34};
}

LCD2Dashboard::Rect LCD2Dashboard::makeWifiEntryRect(uint8_t index) const {
    return {10, static_cast<int16_t>(kWifiListTop + (index * (kWifiRowHeight + kWifiRowGap))), static_cast<int16_t>(_width - 20), kWifiRowHeight};
}

LCD2Dashboard::Rect LCD2Dashboard::makeRetryButtonRect() const {
    return {28, 178, 86, 32};
}

void LCD2Dashboard::updateHorizontalSliderFromTouch(const Rect& sliderRect, int16_t touchX, int16_t& targetValue) {
    const int16_t slotX = sliderRect.x + 52;
    const int16_t slotW = max<int16_t>(1, static_cast<int16_t>(sliderRect.w - 98));
    const int16_t clampedX = clampInt16(touchX, slotX, static_cast<int16_t>(slotX + slotW));
    const int32_t numerator = static_cast<int32_t>(clampedX - slotX) * 100L;
    targetValue = clampInt16(static_cast<int16_t>((numerator + (slotW / 2)) / slotW), 0, 100);
}

void LCD2Dashboard::formatEspFilesystem(char* out, size_t outSize) const {
    if (out == nullptr || outSize == 0) return;
    const size_t total = LittleFS.totalBytes();
    const size_t used = LittleFS.usedBytes();
    if (total == 0) snprintf(out, outSize, "N/A");
    else snprintf(out, outSize, "%u/%u KB", static_cast<unsigned>(used / 1024U), static_cast<unsigned>(total / 1024U));
}

void LCD2Dashboard::formatEspHeap(char* out, size_t outSize) const {
    if (out == nullptr || outSize == 0) return;
    const uint32_t freeHeap = ESP.getFreeHeap();
    const uint32_t totalHeap = ESP.getHeapSize();
    const uint32_t usedHeap = (totalHeap > freeHeap) ? (totalHeap - freeHeap) : 0;
    snprintf(out, outSize, "%u/%u KB", static_cast<unsigned>(usedHeap / 1024U), static_cast<unsigned>(totalHeap / 1024U));
}

void LCD2Dashboard::formatEspPsram(char* out, size_t outSize) const {
    if (out == nullptr || outSize == 0) return;
    const uint32_t total = ESP.getPsramSize();
    if (total == 0) {
        snprintf(out, outSize, "N/A");
        return;
    }
    const uint32_t free = ESP.getFreePsram();
    const uint32_t used = (total > free) ? (total - free) : 0;
    snprintf(out, outSize, "%u/%u KB", static_cast<unsigned>(used / 1024U), static_cast<unsigned>(total / 1024U));
}

void LCD2Dashboard::formatEspUptime(char* out, size_t outSize) const {
    if (out == nullptr || outSize == 0) return;
#ifdef ESP32
    uint64_t seconds = static_cast<uint64_t>(esp_timer_get_time() / 1000000LL);
#else
    uint64_t seconds = static_cast<uint64_t>(millis()) / 1000ULL;
#endif
    uint64_t days = seconds / 86400ULL;
    seconds %= 86400ULL;
    const uint32_t hours = static_cast<uint32_t>(seconds / 3600ULL);
    seconds %= 3600ULL;
    const uint32_t minutes = static_cast<uint32_t>(seconds / 60ULL);
    seconds %= 60ULL;
    const uint64_t years = days / 365ULL;
    days %= 365ULL;

    if (years > 0ULL) {
        snprintf(out, outSize, "%lluy %llud %02u:%02u:%02u",
                 static_cast<unsigned long long>(years),
                 static_cast<unsigned long long>(days),
                 static_cast<unsigned>(hours),
                 static_cast<unsigned>(minutes),
                 static_cast<unsigned>(seconds));
    } else if (days > 0ULL) {
        snprintf(out, outSize, "%llud %02u:%02u:%02u",
                 static_cast<unsigned long long>(days),
                 static_cast<unsigned>(hours),
                 static_cast<unsigned>(minutes),
                 static_cast<unsigned>(seconds));
    } else {
        snprintf(out, outSize, "%02u:%02u:%02u",
                 static_cast<unsigned>(hours),
                 static_cast<unsigned>(minutes),
                 static_cast<unsigned>(seconds));
    }
}

void LCD2Dashboard::drawSettingsPanelButton(TFT_eSprite& sprite, int16_t panelW) {
    const Rect rect = makeSettingsPanelButtonRect(panelW);
    sprite.fillRoundRect(rect.x, rect.y, rect.w, rect.h, 6, kBackgroundColor);
    sprite.drawRoundRect(rect.x, rect.y, rect.w, rect.h, 6, kFrameColor);
    const int16_t iconX = static_cast<int16_t>(rect.x + ((rect.w - kSettingsIconSize) / 2));
    const int16_t iconY = static_cast<int16_t>(rect.y + ((rect.h - kSettingsIconSize) / 2));
    drawInvertedBitmap(sprite,
                       iconX,
                       iconY,
                       kSettingsIconBitmap,
                       static_cast<uint8_t>(kSettingsIconSize),
                       static_cast<uint8_t>(kSettingsIconSize),
                       kTextColor);
}

void LCD2Dashboard::drawSettingsTftButton(const Rect& rect, const char* label, uint16_t accentColor, bool pressed) {
    const uint16_t buttonBg = pressed ? kTextColor : kBackgroundColor;
    const uint16_t frameColor = pressed ? kTextColor : kFrameColor;
    const uint16_t barColor = pressed ? kBackgroundColor : accentColor;
    const uint16_t textColor = pressed ? kBackgroundColor : kTextColor;
    _tft.fillSmoothRoundRect(rect.x, rect.y, rect.w, rect.h, kSettingsCornerRadius, buttonBg, buttonBg);
    _tft.drawRoundRect(rect.x, rect.y, rect.w, rect.h, kSettingsCornerRadius, frameColor);
    _tft.fillSmoothRoundRect(rect.x + 2, rect.y + 2, 5, rect.h - 4, 3, barColor, buttonBg);
    _tft.setTextColor(textColor, buttonBg);
    _tft.setTextDatum(MC_DATUM);
#ifdef SMOOTH_FONT
    _tft.loadFont(NotoSansBold12);
    _tft.drawString(label,
                    static_cast<int16_t>(rect.x + (rect.w / 2)),
                    static_cast<int16_t>(rect.y + (rect.h / 2)));
    _tft.unloadFont();
#else
    _tft.drawString(label,
                    static_cast<int16_t>(rect.x + (rect.w / 2)),
                    static_cast<int16_t>(rect.y + (rect.h / 2)),
                    2);
#endif
    _tft.setTextDatum(TL_DATUM);
}

void LCD2Dashboard::drawSettingsScreen() {
    _settingsFullRedraw = !_settingsDrawn || (_lastSettingsScreen != _settingsScreen);
    _lastSettingsScreen = _settingsScreen;

    switch (_settingsScreen) {
        case SettingsScreen::WIFI_LIST:
            drawWifiListScreen();
            break;
        case SettingsScreen::KEYBOARD:
            _keyboard.draw(_tft,
                           _width,
                           _height,
                           _settingsFullRedraw,
                           _keyboardInputSprite.created() ? &_keyboardInputSprite : nullptr);
            break;
        case SettingsScreen::IP_VIEW:
            drawIpViewScreen();
            break;
        case SettingsScreen::CONNECT_RESULT:
            drawConnectResultScreen();
            break;
        case SettingsScreen::NETWORK:
            drawNetworkScreen();
            break;
        case SettingsScreen::SSH:
            drawSshScreen();
            break;
        case SettingsScreen::NGROK:
            drawNgrokScreen();
            break;
        case SettingsScreen::SYSTEM:
            drawSystemScreen();
            break;
        case SettingsScreen::HEADLESS:
            drawHeadlessScreen();
            break;
        case SettingsScreen::POWER_MODE:
            drawPowerModeScreen();
            break;
        case SettingsScreen::ABOUT:
            drawAboutScreen();
            break;
        case SettingsScreen::NETWORK_HELP:
            drawNetworkHelpScreen();
            break;
        case SettingsScreen::SYSTEM_HELP:
            drawSystemHelpScreen();
            break;
        case SettingsScreen::CONFIRM:
            drawConfirmScreen();
            break;
        case SettingsScreen::MAIN:
        default:
            drawSettingsMainScreen();
            break;
    }

    _settingsDrawn = true;
    _settingsFullRedraw = false;
}

void LCD2Dashboard::drawSettingsTopBar(const char* title, bool showBack) {
    constexpr int16_t kTopBarHeight = 36;
    const int16_t barCenterY = static_cast<int16_t>(kTopBarHeight / 2);
    _tft.fillRoundRect(0, 0, _width, 36, 0, kBackgroundColor);
    _tft.drawFastHLine(0, static_cast<int16_t>(kTopBarHeight - 1), _width, kFrameColor);
    if (showBack) {
        drawSettingsTftButton(makeSettingsBackButtonRect(), "BACK", kMutedTextColor, isSettingsButtonPressed(makeSettingsBackButtonRect()));
    }
    const bool wifiTitle = (title != nullptr && strcmp(title, "Wi-Fi") == 0);
    uint16_t titleColor = kTextColor;
    if (title != nullptr) {
        if (wifiTitle) titleColor = TFT_CYAN;
        else if (strcmp(title, "Network") == 0) titleColor = kRamLineColor;
        else if (strcmp(title, "Power Mode") == 0) titleColor = kGpuAccentColor;
        else if (strcmp(title, "System") == 0) titleColor = kPowerAccentColor;
        else if (strcmp(title, "About") == 0) titleColor = kGpuAccentColor;
    }
    const bool centeredTitle = !showBack && title != nullptr;
    const int16_t titleX = wifiTitle ? 70 : static_cast<int16_t>(_width / 2);
    const int16_t titleY = wifiTitle ? static_cast<int16_t>(barCenterY - 8) : barCenterY;
    _tft.setTextDatum((wifiTitle || centeredTitle || showBack) ? MC_DATUM : TL_DATUM);
    _tft.setTextColor(titleColor, kBackgroundColor);
#ifdef SMOOTH_FONT
    _tft.loadFont(wifiTitle ? NotoSansBold12 : NotoSansBold15);
    _tft.drawString(title, titleX, titleY);
    _tft.unloadFont();
#else
    _tft.drawString(title, titleX, titleY, 2);
#endif
    _tft.setTextDatum(TL_DATUM);
}

void LCD2Dashboard::drawSettingsValueRow(int16_t y, const char* label, const char* value, uint16_t valueColor) {
    const bool compactMainRow = (_settingsScreen == SettingsScreen::MAIN);
    const int16_t rowW = compactMainRow ? static_cast<int16_t>(_width - 118) : static_cast<int16_t>(_width - 24);
    const int16_t valueX = compactMainRow ? 96 : 116;
    const int16_t valueLocalX = static_cast<int16_t>(valueX - 12);
    TFT_eSprite* rowSprite = compactMainRow ? &_settingsCompactRowSprite : &_settingsRowSprite;
    const bool spriteReady = (rowSprite->created() && rowSprite->width() == rowW && rowSprite->height() == 22);

    if (label == nullptr) {
        label = "";
    }
    if (value == nullptr) {
        value = "";
    }

    char clippedValue[48];
    const int16_t valueAreaW = max<int16_t>(1, static_cast<int16_t>(rowW - valueLocalX - 4));
    const size_t maxChars = max<size_t>(1, min<size_t>(sizeof(clippedValue) - 1, static_cast<size_t>(valueAreaW / 6)));
    strncpy(clippedValue, value, maxChars);
    clippedValue[maxChars] = '\0';
    if (strlen(value) > maxChars && maxChars >= 2) {
        clippedValue[maxChars - 1] = '.';
    }

    if (spriteReady) {
        rowSprite->fillSprite(kBackgroundColor);
        rowSprite->setTextDatum(TL_DATUM);
#ifdef SMOOTH_FONT
        rowSprite->loadFont(NotoSansBold10);
        rowSprite->setTextColor(kMutedTextColor, kBackgroundColor);
        rowSprite->drawString(label, 2, 3);
        rowSprite->unloadFont();
        rowSprite->loadFont(NotoSansBold12);
        rowSprite->setTextColor(valueColor, kBackgroundColor);
        rowSprite->drawString(clippedValue, valueLocalX, 1);
        rowSprite->unloadFont();
#else
        rowSprite->setTextColor(kMutedTextColor, kBackgroundColor);
        rowSprite->drawString(label, 2, 3, 1);
        rowSprite->setTextColor(valueColor, kBackgroundColor);
        rowSprite->drawString(clippedValue, valueLocalX, 1, 2);
#endif
        rowSprite->pushSprite(12, y);
        return;
    }

    _tft.fillRect(12, y, rowW, 22, kBackgroundColor);
    _tft.setTextDatum(TL_DATUM);
#ifdef SMOOTH_FONT
    _tft.loadFont(NotoSansBold10);
    _tft.setTextColor(kMutedTextColor, kBackgroundColor);
    _tft.drawString(label, 14, y + 3);
    _tft.unloadFont();
    _tft.loadFont(NotoSansBold12);
    _tft.setTextColor(valueColor, kBackgroundColor);
    _tft.drawString(clippedValue, valueX, y + 1);
    _tft.unloadFont();
#else
    _tft.setTextColor(kMutedTextColor, kBackgroundColor);
    _tft.drawString(label, 14, y + 3, 1);
    _tft.setTextColor(valueColor, kBackgroundColor);
    _tft.drawString(clippedValue, valueX, y + 1, 2);
#endif
}

void LCD2Dashboard::drawSettingsNavButton(const Rect& rect, const char* label, const char* sublabel, uint16_t accentColor, bool leftAligned, bool reverseColors, bool pressed) {
    const uint16_t buttonBg = pressed ? kTextColor : (reverseColors ? accentColor : kBackgroundColor);
    const uint16_t buttonFrame = pressed ? kTextColor : (reverseColors ? accentColor : kFrameColor);
    const uint16_t accentBarColor = pressed ? kBackgroundColor : (reverseColors ? buttonBg : accentColor);
    _tft.fillSmoothRoundRect(rect.x, rect.y, rect.w, rect.h, 6, buttonBg, buttonBg);
    _tft.drawRoundRect(rect.x, rect.y, rect.w, rect.h, 6, buttonFrame);
    _tft.fillSmoothRoundRect(rect.x + 3, rect.y + 4, 5, rect.h - 8, 3, accentBarColor, buttonBg);

    const bool compactMainButton = (rect.w < 110);
    const bool hasSublabel = (!compactMainButton && sublabel != nullptr && sublabel[0] != '\0');
    const bool useLeftAlignment = leftAligned || !compactMainButton;
    const int16_t labelX = useLeftAlignment ? static_cast<int16_t>(rect.x + 16) : static_cast<int16_t>(rect.x + (rect.w / 2));
    const int16_t centerY = static_cast<int16_t>(rect.y + (rect.h / 2));
    const int16_t labelY = useLeftAlignment ? static_cast<int16_t>(centerY - (hasSublabel ? 7 : 0)) : centerY;
    const int16_t sublabelX = labelX;
    const int16_t sublabelY = useLeftAlignment ? static_cast<int16_t>(centerY + 7) : static_cast<int16_t>(centerY + 9);
    _tft.setTextDatum(useLeftAlignment ? ML_DATUM : MC_DATUM);
#ifdef SMOOTH_FONT
    _tft.loadFont(NotoSansBold12);
    _tft.setTextColor(pressed ? kBackgroundColor : (reverseColors ? kBackgroundColor : ((accentColor == kSelectedModeColor) ? kSelectedModeColor : kTextColor)), buttonBg);
    _tft.drawString(label, labelX, labelY);
    _tft.unloadFont();
    if (hasSublabel) {
        _tft.loadFont(NotoSansBold10);
        _tft.setTextDatum(useLeftAlignment ? ML_DATUM : TL_DATUM);
        _tft.setTextColor(pressed ? kBackgroundColor : (reverseColors ? kBackgroundColor : kMutedTextColor), buttonBg);
        _tft.drawString(sublabel, sublabelX, sublabelY);
        _tft.unloadFont();
    }
#else
    _tft.setTextColor(pressed ? kBackgroundColor : (reverseColors ? kBackgroundColor : ((accentColor == kSelectedModeColor) ? kSelectedModeColor : kTextColor)), buttonBg);
    _tft.drawString(label, labelX, labelY, 2);
    if (hasSublabel) {
        _tft.setTextDatum(useLeftAlignment ? ML_DATUM : TL_DATUM);
        _tft.setTextColor(pressed ? kBackgroundColor : (reverseColors ? kBackgroundColor : kMutedTextColor), buttonBg);
        _tft.drawString(sublabel, sublabelX, sublabelY, 1);
    }
#endif
    _tft.setTextDatum(TL_DATUM);
}

void LCD2Dashboard::drawSettingsHelpButton(bool pressed) {
    const Rect rect = makeSettingsHelpButtonRect();
    const uint16_t buttonBg = pressed ? kTextColor : kBackgroundColor;
    const uint16_t frameColor = pressed ? kTextColor : kFrameColor;
    const uint16_t textColor = pressed ? kBackgroundColor : kTextColor;
    _tft.fillSmoothCircle(static_cast<int16_t>(rect.x + rect.w / 2),
                          static_cast<int16_t>(rect.y + rect.h / 2),
                          12,
                          buttonBg,
                          buttonBg);
    _tft.drawCircle(static_cast<int16_t>(rect.x + rect.w / 2),
                    static_cast<int16_t>(rect.y + rect.h / 2),
                    12,
                    frameColor);
    _tft.setTextDatum(MC_DATUM);
    _tft.setTextColor(textColor, buttonBg);
#ifdef SMOOTH_FONT
    _tft.loadFont(NotoSansBold12);
    _tft.drawString("?", static_cast<int16_t>(rect.x + rect.w / 2), static_cast<int16_t>(rect.y + rect.h / 2));
    _tft.unloadFont();
#else
    _tft.drawString("?", static_cast<int16_t>(rect.x + rect.w / 2), static_cast<int16_t>(rect.y + rect.h / 2), 2);
#endif
    _tft.setTextDatum(TL_DATUM);
}

void LCD2Dashboard::drawSettingsHorizontalSlider(const Rect& rect) {
    const int16_t value = getRequestedLedBrightnessPercent();
    const int16_t slotX = rect.x + 52;
    const int16_t slotY = rect.y + 18;
    const int16_t slotW = static_cast<int16_t>(rect.w - 98);
    const int16_t slotH = 6;
    const int16_t knobX = static_cast<int16_t>(slotX + ((value * slotW) / 100));
    char valueLabel[12];
    formatUsageValue(value, valueLabel, sizeof(valueLabel));

    _tft.fillSmoothRoundRect(rect.x, rect.y, rect.w, rect.h, 6, kBackgroundColor, kBackgroundColor);
    _tft.drawRoundRect(rect.x, rect.y, rect.w, rect.h, 6, kFrameColor);
    _tft.setTextDatum(TL_DATUM);
    _tft.setTextColor(kLedAccentColor, kBackgroundColor);
#ifdef SMOOTH_FONT
    _tft.loadFont(NotoSansBold12);
    _tft.drawString("LED", rect.x + 12, rect.y + 13);
    _tft.unloadFont();
#else
    _tft.drawString("LED", rect.x + 12, rect.y + 12, 2);
#endif
    _tft.fillRoundRect(slotX, slotY, slotW, slotH, 4, TFT_DARKGREY);
    _tft.fillRoundRect(slotX, slotY, static_cast<int16_t>(knobX - slotX), slotH, 4, kLedAccentColor);
    _tft.fillSmoothCircle(knobX, static_cast<int16_t>(slotY + slotH / 2), 6, kTextColor, kBackgroundColor);
    _tft.setTextDatum(MR_DATUM);
    _tft.setTextColor(kTextColor, kBackgroundColor);
#ifdef SMOOTH_FONT
    _tft.loadFont(NotoSansBold12);
    _tft.drawString(valueLabel, rect.x + rect.w - 8, static_cast<int16_t>(slotY + (slotH / 2)));
    _tft.unloadFont();
#else
    _tft.drawString(valueLabel, rect.x + rect.w - 8, static_cast<int16_t>(slotY + (slotH / 2)), 2);
#endif
    _tft.setTextDatum(TL_DATUM);
}

void LCD2Dashboard::drawSettingsUptimeRow() {
    char uptime[24];
    formatEspUptime(uptime, sizeof(uptime));
    drawSettingsValueRow(202, "Uptime", uptime, kTextColor);
}

void LCD2Dashboard::drawSettingsMainScreen() {
    if (_settingsFullRedraw) {
        _tft.fillScreen(kBackgroundColor);
        _tft.drawRoundRect(0, 0, _width, _height, 8, kFrameColor);
        drawSettingsTopBar("Settings", false);
    }

    drawSettingsTopBar("Settings", false);
    drawSettingsTftButton(makeSettingsExitButtonRect(), "EXIT", kStatusAlertColor, isSettingsButtonPressed(makeSettingsExitButtonRect()));
    drawSettingsNavButton(makeSettingsNetworkButtonRect(), "Network", "IP, SSH, ngrok", kRamLineColor, true, false, isSettingsButtonPressed(makeSettingsNetworkButtonRect()));
    drawSettingsNavButton(makeSettingsSystemButtonRect(), "System", "headless, power", kPowerAccentColor, true, false, isSettingsButtonPressed(makeSettingsSystemButtonRect()));
    drawSettingsNavButton(makeSettingsAboutButtonRect(), "About", "Jetson info", kGpuAccentColor, true, false, isSettingsButtonPressed(makeSettingsAboutButtonRect()));
    drawSettingsNavButton(makeSettingsGamesButtonRect(), "Games", "", kLedAccentColor, true, false, isSettingsButtonPressed(makeSettingsGamesButtonRect()));

    drawSettingsHorizontalSlider(makeSettingsSliderRect());

    char caseTemp[16];
    char caseHum[16];
    char fs[32];
    char heap[32];
    char psram[32];
    if (_boxTemp >= 0.0f) snprintf(caseTemp, sizeof(caseTemp), "%.1f C", static_cast<double>(_boxTemp));
    else strncpy(caseTemp, "N/A", sizeof(caseTemp));
    if (_boxHumidity >= 0.0f) snprintf(caseHum, sizeof(caseHum), "%d %%", static_cast<int>(_boxHumidity + 0.5f));
    else strncpy(caseHum, "N/A", sizeof(caseHum));
    formatEspFilesystem(fs, sizeof(fs));
    formatEspHeap(heap, sizeof(heap));
    formatEspPsram(psram, sizeof(psram));

    drawSettingsValueRow(92, "Case temp", caseTemp, kCpuLineColor);
    drawSettingsValueRow(114, "Humidity", caseHum, kRamLineColor);
    drawSettingsValueRow(136, "Filesystem", fs, kTextColor);
    drawSettingsValueRow(158, "Heap RAM", heap, kTextColor);
    drawSettingsValueRow(180, "PSRAM", psram, kMutedTextColor);
    drawSettingsUptimeRow();
}

void LCD2Dashboard::drawNetworkScreen() {
    if (_settingsFullRedraw) {
        _tft.fillScreen(kBackgroundColor);
        _tft.drawRoundRect(0, 0, _width, _height, 8, kFrameColor);
        drawSettingsTopBar("Network", true);
    }
    drawSettingsTopBar("Network", true);
    drawSettingsHelpButton(isSettingsButtonPressed(makeSettingsHelpButtonRect()));
    drawSettingsNavButton(makeSettingsButtonRect(0), "Wi-Fi", _wifiStatus[0] != '\0' ? _wifiStatus : "scan, connect", kCpuLineColor, false, false, isSettingsButtonPressed(makeSettingsButtonRect(0)));
    drawSettingsNavButton(makeSettingsButtonRect(1), "Show IP", _ipStatus, kRamLineColor, false, false, isSettingsButtonPressed(makeSettingsButtonRect(1)));
    drawSettingsNavButton(makeSettingsButtonRect(2), "SSH", _sshStatus, kCpuLineColor, false, false, isSettingsButtonPressed(makeSettingsButtonRect(2)));
    drawSettingsNavButton(makeSettingsButtonRect(3), "Ngrok SSH", _ngrokEndpoint, kPowerAccentColor, false, false, isSettingsButtonPressed(makeSettingsButtonRect(3)));
}

void LCD2Dashboard::drawSshScreen() {
    if (_settingsFullRedraw) {
        _tft.fillScreen(kBackgroundColor);
        _tft.drawRoundRect(0, 0, _width, _height, 8, kFrameColor);
        drawSettingsTopBar("SSH", true);
    }
    const Rect startButton = {12, 158, 140, 32};
    const Rect stopButton = {168, 158, 140, 32};
    drawSettingsTopBar("SSH", true);
    drawSettingsTftButton(startButton, "START", kStatusOkColor, isSettingsButtonPressed(startButton));
    drawSettingsTftButton(stopButton, "STOP", kStatusAlertColor, isSettingsButtonPressed(stopButton));
    drawSettingsTftButton(makeSettingsRefreshButtonRect(), "REFRESH", TFT_CYAN, isSettingsButtonPressed(makeSettingsRefreshButtonRect()));
    drawSettingsValueRow(54, "Status", _sshRequestInProgress ? "LOADING" : _sshStatus, kTextColor);
    drawSettingsValueRow(82, "Enabled", _sshEnabled, kTextColor);
    drawSettingsValueRow(110, "Service", _sshService, kMutedTextColor);
}

void LCD2Dashboard::drawNgrokScreen() {
    const char* statusText = _ngrokStatus;
    if (_ngrokActionState == NgrokActionState::STARTING) {
        statusText = "STARTING";
    } else if (_ngrokActionState == NgrokActionState::STOPPING) {
        statusText = "STOPPING";
    } else if (_ngrokRequestInProgress) {
        statusText = "LOADING";
    }

    if (_settingsFullRedraw) {
        _tft.fillScreen(kBackgroundColor);
        _tft.drawRoundRect(0, 0, _width, _height, 8, kFrameColor);
        drawSettingsTopBar("Ngrok SSH", true);
    }
    const Rect startButton = {12, 158, 140, 32};
    const Rect stopButton = {168, 158, 140, 32};
    drawSettingsTopBar("Ngrok SSH", true);
    drawSettingsTftButton(startButton, "START", kStatusOkColor, isSettingsButtonPressed(startButton));
    drawSettingsTftButton(stopButton, "STOP", kStatusAlertColor, isSettingsButtonPressed(stopButton));
    drawSettingsTftButton(makeSettingsRefreshButtonRect(), "REFRESH", TFT_CYAN, isSettingsButtonPressed(makeSettingsRefreshButtonRect()));
    drawSettingsValueRow(52, "Status", statusText, kTextColor);
    drawSettingsValueRow(74, "Endpoint", _ngrokEndpoint, kCpuLineColor);
    drawSettingsValueRow(96, "API", _ngrokApi, kMutedTextColor);
    drawSettingsValueRow(118, "Service", _ngrokService, kMutedTextColor);
}

void LCD2Dashboard::drawSystemScreen() {
    if (_settingsFullRedraw) {
        _tft.fillScreen(kBackgroundColor);
        _tft.drawRoundRect(0, 0, _width, _height, 8, kFrameColor);
        drawSettingsTopBar("System", true);
    }
    drawSettingsTopBar("System", true);
    drawSettingsHelpButton(isSettingsButtonPressed(makeSettingsHelpButtonRect()));
    drawSettingsNavButton(makeSettingsButtonRect(0), "Power mode", _powerModeLabel, kLedAccentColor, false, false, isSettingsButtonPressed(makeSettingsButtonRect(0)));
    drawSettingsNavButton(makeSettingsButtonRect(1), "Headless Mode", _headlessDefault, kCpuLineColor, false, false, isSettingsButtonPressed(makeSettingsButtonRect(1)));
    drawSettingsNavButton(makeSettingsButtonRect(2), "Restart Service", "confirm required", kPowerAccentColor, false, false, isSettingsButtonPressed(makeSettingsButtonRect(2)));
    drawSettingsNavButton(makeSettingsButtonRect(3), "Reboot", "confirm required", kStatusAlertColor, false, false, isSettingsButtonPressed(makeSettingsButtonRect(3)));
}

void LCD2Dashboard::drawHeadlessScreen() {
    const bool guiDefault = (strcmp(_headlessDefault, "GRAPHICAL") == 0);
    const char* targetLabel = guiDefault ? "Switch to Headless" : "Switch to GUI";
    const Rect switchButton = {12, 112, 296, 38};
    const Rect applyButton = {12, 160, 140, 34};
    const Rect restartButton = {168, 160, 140, 34};
    const Rect defaultButton = {12, 202, 296, 28};

    if (_settingsFullRedraw) {
        _tft.fillScreen(kBackgroundColor);
        _tft.drawRoundRect(0, 0, _width, _height, 8, kFrameColor);
        drawSettingsTopBar("Display Mode", true);
    }
    drawSettingsTopBar("Display Mode", true);
    drawSettingsTftButton(switchButton, targetLabel, guiDefault ? kCpuLineColor : kRamLineColor, isSettingsButtonPressed(switchButton));
    drawSettingsTftButton(applyButton, "APPLY NOW", kPowerAccentColor, isSettingsButtonPressed(applyButton));
    drawSettingsTftButton(restartButton, "RESTART", kStatusAlertColor, isSettingsButtonPressed(restartButton));
    drawSettingsTftButton(defaultButton, "SET AS STARTUP DEFAULT", kLedAccentColor, isSettingsButtonPressed(defaultButton));
    drawSettingsValueRow(50, "Startup", _headlessRequestInProgress ? "LOADING" : _headlessDefault, kTextColor);
    drawSettingsValueRow(78, "Current", _headlessActive, kTextColor);
}

void LCD2Dashboard::drawPowerModeScreen() {
    if (_settingsFullRedraw) {
        _tft.fillScreen(kBackgroundColor);
        _tft.drawRoundRect(0, 0, _width, _height, 8, kFrameColor);
    }

    const Rect mode7W = makeSettingsButtonRect(0);
    const Rect mode15W = makeSettingsButtonRect(1);
    const Rect mode25W = makeSettingsButtonRect(2);
    const Rect modeMaxn = makeSettingsButtonRect(3);
    drawSettingsTopBar("Power Mode", true);
    drawSettingsNavButton(mode7W, "7W", "ID 3", kCpuLineColor, true, false, isSettingsButtonPressed(mode7W));
    drawSettingsNavButton(mode15W, "15W", "ID 0", kRamLineColor, true, false, isSettingsButtonPressed(mode15W));
    drawSettingsNavButton(mode25W, "25W", "ID 1", kPowerAccentColor, true, false, isSettingsButtonPressed(mode25W));
    drawSettingsNavButton(modeMaxn, "MAXN_SUPER", "ID 2", kStatusAlertColor, true, false, isSettingsButtonPressed(modeMaxn));

    const int8_t selectedPowerModeId = (_pendingPowerModeId >= 0) ? _pendingPowerModeId : _powerModeId;
    const Rect* selectedRect = nullptr;
    if (selectedPowerModeId == 3) selectedRect = &mode7W;
    else if (selectedPowerModeId == 0) selectedRect = &mode15W;
    else if (selectedPowerModeId == 1) selectedRect = &mode25W;
    else if (selectedPowerModeId == 2) selectedRect = &modeMaxn;

    if (selectedRect != nullptr) {
        const int16_t dotX = static_cast<int16_t>(selectedRect->x + selectedRect->w - 14);
        const int16_t dotY = static_cast<int16_t>(selectedRect->y + (selectedRect->h / 2));
        _tft.drawCircle(dotX, dotY, 5, TFT_DARKGREY);
        _tft.fillCircle(dotX, dotY, 3, kStatusOkColor);
    }
}

void LCD2Dashboard::drawAboutScreen() {
    if (!_settingsFullRedraw) {
        return;
    }

    _tft.fillScreen(kBackgroundColor);
    _tft.drawRoundRect(0, 0, _width, _height, 8, kFrameColor);
    drawSettingsTopBar("About", true);

    char diskLabel[32];
    char swapLabel[32];
    const int32_t diskUsedGb = (_latest.diskUsedMb >= 0) ? ((_latest.diskUsedMb + 512) / 1024) : -1;
    const int32_t diskTotalGb = (_latest.diskTotalMb >= 0) ? ((_latest.diskTotalMb + 512) / 1024) : -1;
    const int32_t swapUsedHundredGb = (_latest.swapUsedMb >= 0) ? ((_latest.swapUsedMb * 100) + 512) / 1024 : -1;
    const int32_t swapTotalHundredGb = (_latest.swapTotalMb > 0) ? ((_latest.swapTotalMb * 100) + 512) / 1024 : 1900;

    if (diskUsedGb >= 0 && diskTotalGb > 0) {
        snprintf(diskLabel, sizeof(diskLabel), "%ld / %ld GB", static_cast<long>(diskUsedGb), static_cast<long>(diskTotalGb));
    } else {
        snprintf(diskLabel, sizeof(diskLabel), "245 / 467 GB");
    }
    if (swapUsedHundredGb >= 0) {
        snprintf(swapLabel, sizeof(swapLabel), "%ld.%02ld / %ld.%02ld GB",
                 static_cast<long>(swapUsedHundredGb / 100),
                 static_cast<long>(swapUsedHundredGb % 100),
                 static_cast<long>(swapTotalHundredGb / 100),
                 static_cast<long>(swapTotalHundredGb % 100));
    } else {
        snprintf(swapLabel, sizeof(swapLabel), "N/A / %ld.%02ld GB",
                 static_cast<long>(swapTotalHundredGb / 100),
                 static_cast<long>(swapTotalHundredGb % 100));
    }

    const int16_t x = 12;
    const int16_t labelX = 20;
    const int16_t valueX = 112;
    const int16_t itemX = 20;
    const int16_t line = 11;
    int16_t y = 42;

    _tft.setTextDatum(TL_DATUM);
#ifdef SMOOTH_FONT
    _tft.loadFont(NotoSansBold10);
#endif

    _tft.setTextColor(kGpuAccentColor, kBackgroundColor);
    drawSettingsHeadingText(_tft, "Jetson Shield OS", x, y);
    y += 15;

    _tft.setTextColor(kMutedTextColor, kBackgroundColor);
    drawSettingsTinyText(_tft, "Service version:", labelX, y);
    _tft.setTextColor(kTextColor, kBackgroundColor);
    drawSettingsTinyText(_tft, _aboutVersion[0] != '\0' ? _aboutVersion : "1.5.1", valueX, y);
    y += line;

    _tft.setTextColor(kMutedTextColor, kBackgroundColor);
    drawSettingsTinyText(_tft, "Author:", labelX, y);
    _tft.setTextColor(kTextColor, kBackgroundColor);
    drawSettingsTinyText(_tft, "Viettran", valueX, y);
    y += line + 2;

    _tft.setTextColor(kCpuLineColor, kBackgroundColor);
    drawSettingsHeadingText(_tft, "Jetson Orin Nano Super", x, y);
    y += 16;

    _tft.setTextColor(kMutedTextColor, kBackgroundColor);
    drawSettingsTinyText(_tft, "RAM:", itemX, y);
    _tft.setTextColor(kTextColor, kBackgroundColor);
    drawSettingsTinyText(_tft, "8 GB", valueX, y);
    y += line;

    _tft.setTextColor(kMutedTextColor, kBackgroundColor);
    drawSettingsTinyText(_tft, "Disk:", itemX, y);
    _tft.setTextColor(kTextColor, kBackgroundColor);
    drawSettingsTinyText(_tft, diskLabel, valueX, y);
    y += line;

    _tft.setTextColor(kMutedTextColor, kBackgroundColor);
    drawSettingsTinyText(_tft, "SWAP:", itemX, y);
    _tft.setTextColor(kTextColor, kBackgroundColor);
    drawSettingsTinyText(_tft, swapLabel, valueX, y);
    y += line + 2;

    _tft.setTextColor(kRamLineColor, kBackgroundColor);
    drawSettingsHeadingText(_tft, "Peripherals", x, y);
    y += 16;

    _tft.setTextColor(kMutedTextColor, kBackgroundColor);
    drawSettingsTinyText(_tft, "MCU:", itemX, y);
    _tft.setTextColor(kTextColor, kBackgroundColor);
    drawSettingsTinyText(_tft, "ESP32 dev module", valueX, y);
    y += line;

    _tft.setTextColor(kMutedTextColor, kBackgroundColor);
    drawSettingsTinyText(_tft, "Fan:", itemX, y);
    _tft.setTextColor(kTextColor, kBackgroundColor);
    drawSettingsTinyText(_tft, "TF-9215-K", valueX, y);
    y += line;

    _tft.setTextColor(kMutedTextColor, kBackgroundColor);
    drawSettingsTinyText(_tft, "LCD1:", itemX, y);
    _tft.setTextColor(kTextColor, kBackgroundColor);
    drawSettingsTinyText(_tft, "OLED SSD1306 0.96 inch", valueX, y);
    y += line;

    _tft.setTextColor(kMutedTextColor, kBackgroundColor);
    drawSettingsTinyText(_tft, "LCD2:", itemX, y);
    _tft.setTextColor(kTextColor, kBackgroundColor);
    drawSettingsTinyText(_tft, "TFT ILI9341 touch 2.8 inch", valueX, y);
    y += line;

    _tft.setTextColor(kMutedTextColor, kBackgroundColor);
    drawSettingsTinyText(_tft, "Sensor:", itemX, y);
    _tft.setTextColor(kTextColor, kBackgroundColor);
    drawSettingsTinyText(_tft, "DHT11", valueX, y);
    y += line;

    _tft.setTextColor(kMutedTextColor, kBackgroundColor);
    drawSettingsTinyText(_tft, "LED:", itemX, y);
    _tft.setTextColor(kTextColor, kBackgroundColor);
    drawSettingsTinyText(_tft, "NeoPixel 8 RGB WS2812", valueX, y);

#ifdef SMOOTH_FONT
    _tft.unloadFont();
#endif
}

void LCD2Dashboard::drawSettingsHelpScreen(const char* title, const char* const* lines, uint8_t lineCount) {
    if (!_settingsFullRedraw) {
        return;
    }

    _tft.fillScreen(kBackgroundColor);
    _tft.drawRoundRect(0, 0, _width, _height, 8, kFrameColor);
    drawSettingsTopBar(title, true);

    _tft.setTextDatum(TL_DATUM);
#ifdef SMOOTH_FONT
    _tft.loadFont(NotoSansBold10);
#endif
    int16_t y = 48;
    for (uint8_t i = 0; i < lineCount; ++i) {
        const char* line = lines[i];
        if (line == nullptr || line[0] == '\0') {
            y += 8;
            continue;
        }
        const bool heading = (line[0] == '#');
        _tft.setTextColor(heading ? kCpuLineColor : kTextColor, kBackgroundColor);
        drawSettingsTinyText(_tft, heading ? line + 1 : line, 14, y);
        y += heading ? 13 : 11;
    }
#ifdef SMOOTH_FONT
    _tft.unloadFont();
#endif
}

void LCD2Dashboard::drawNetworkHelpScreen() {
    static const char* const lines[] = {
        "#Show IP",
        "Tap to request interface, hostname,",
        "SSID, address, and link status.",
        "",
        "#SSH",
        "Tap to view SSH service state.",
        "Use START or STOP on the SSH page.",
        "",
        "#Ngrok SSH",
        "Tap to view tunnel endpoint and API.",
        "Use START or STOP on its detail page."
    };
    drawSettingsHelpScreen("Network Help", lines, static_cast<uint8_t>(sizeof(lines) / sizeof(lines[0])));
}

void LCD2Dashboard::drawSystemHelpScreen() {
    static const char* const lines[] = {
        "#Headless Mode",
        "View default and active boot target.",
        "Open it to enable, disable, apply,",
        "or reboot with confirmation.",
        "",
        "#Restart Jetson Shield Service",
        "Restarts the Jetson monitor service.",
        "",
        "#Reboot",
        "Restarts the Jetson after",
        "confirmation.",
        "",
        "#Power mode",
        "View and set nvpmodel mode."
    };
    drawSettingsHelpScreen("System Help", lines, static_cast<uint8_t>(sizeof(lines) / sizeof(lines[0])));
}

void LCD2Dashboard::drawConfirmScreen() {
    if (_settingsFullRedraw) {
        _tft.fillScreen(kBackgroundColor);
        _tft.drawRoundRect(0, 0, _width, _height, 8, kFrameColor);
        drawSettingsTopBar(_confirmTitle, false);
        _tft.setTextDatum(TL_DATUM);
        _tft.setTextColor(kTextColor, kBackgroundColor);
#ifdef SMOOTH_FONT
        _tft.loadFont(NotoSansBold12);
        _tft.drawString(_confirmBody, 18, 76);
        _tft.unloadFont();
#else
        _tft.drawString(_confirmBody, 18, 76, 2);
#endif
    }
    drawSettingsTopBar(_confirmTitle, false);
    drawSettingsTftButton(makeSettingsConfirmCancelRect(), "CANCEL", kMutedTextColor, isSettingsButtonPressed(makeSettingsConfirmCancelRect()));
    drawSettingsTftButton(makeSettingsConfirmOkRect(), "CONFIRM", kStatusAlertColor, isSettingsButtonPressed(makeSettingsConfirmOkRect()));
}

void LCD2Dashboard::drawWifiEntryRow(uint8_t index) {
    if (index >= _wifiEntryCount) {
        return;
    }

    const Rect row = makeWifiEntryRect(index);
    const WifiEntry& entry = _wifiEntries[index];
    const uint16_t border = entry.current ? kStatusOkColor : kFrameColor;
    const uint16_t text = entry.current ? kStatusOkColor : kTextColor;
    const uint16_t fill = entry.current ? TFT_DARKGREEN : kBackgroundColor;
    const bool spriteReady = (_wifiRowSprite.created() &&
                              _wifiRowSprite.width() == row.w &&
                              _wifiRowSprite.height() == row.h);

    char ssid[22];
    strncpy(ssid, entry.ssid, sizeof(ssid) - 1);
    ssid[sizeof(ssid) - 1] = '\0';
    if (strlen(entry.ssid) >= sizeof(ssid)) {
        ssid[sizeof(ssid) - 2] = '.';
    }

    char security[9];
    strncpy(security, entry.security[0] == '\0' ? "OPEN" : entry.security, sizeof(security) - 1);
    security[sizeof(security) - 1] = '\0';

    char signalLabel[8];
    snprintf(signalLabel, sizeof(signalLabel), "%u%%", static_cast<unsigned>(entry.signal));

    if (spriteReady) {
        _wifiRowSprite.fillSprite(kBackgroundColor);
        _wifiRowSprite.fillRoundRect(0, 0, row.w, row.h, kSettingsCornerRadius, fill);
        _wifiRowSprite.drawRoundRect(0, 0, row.w, row.h, kSettingsCornerRadius, border);
        if (entry.current) {
            _wifiRowSprite.fillCircle(12, static_cast<int16_t>(row.h / 2), 3, kStatusOkColor);
        }
#ifdef SMOOTH_FONT
        _wifiRowSprite.loadFont(NotoSansBold12);
        _wifiRowSprite.setTextDatum(TL_DATUM);
        _wifiRowSprite.setTextColor(text, fill);
        _wifiRowSprite.drawString(ssid, 22, 7);
        _wifiRowSprite.setTextDatum(TR_DATUM);
        _wifiRowSprite.setTextColor(kMutedTextColor, fill);
        _wifiRowSprite.drawString(signalLabel, static_cast<int16_t>(row.w - 8), 4);
        _wifiRowSprite.loadFont(NotoSansBold10);
        _wifiRowSprite.drawString(security, static_cast<int16_t>(row.w - 8), 16);
        _wifiRowSprite.unloadFont();
#else
        _wifiRowSprite.setTextDatum(TL_DATUM);
        _wifiRowSprite.setTextColor(text, fill);
        _wifiRowSprite.drawString(ssid, 22, 4, 1);
        _wifiRowSprite.setTextDatum(TR_DATUM);
        _wifiRowSprite.setTextColor(kMutedTextColor, fill);
        _wifiRowSprite.drawString(signalLabel, static_cast<int16_t>(row.w - 8), 3, 1);
        _wifiRowSprite.drawString(security, static_cast<int16_t>(row.w - 8), 15, 1);
#endif
        _wifiRowSprite.setTextDatum(TL_DATUM);
        _wifiRowSprite.pushSprite(row.x, row.y);
        return;
    }

    _tft.fillRoundRect(row.x, row.y, row.w, row.h, kSettingsCornerRadius, fill);
    _tft.drawRoundRect(row.x, row.y, row.w, row.h, kSettingsCornerRadius, border);
    if (entry.current) {
        _tft.fillCircle(static_cast<int16_t>(row.x + 12), static_cast<int16_t>(row.y + (row.h / 2)), 3, kStatusOkColor);
    }
#ifdef SMOOTH_FONT
    _tft.loadFont(NotoSansBold12);
    _tft.setTextDatum(TL_DATUM);
    _tft.setTextColor(text, fill);
    _tft.drawString(ssid, row.x + 22, row.y + 7);
    _tft.setTextDatum(TR_DATUM);
    _tft.setTextColor(kMutedTextColor, fill);
    _tft.drawString(signalLabel, static_cast<int16_t>(row.x + row.w - 8), row.y + 4);
    _tft.loadFont(NotoSansBold10);
    _tft.drawString(security, static_cast<int16_t>(row.x + row.w - 8), row.y + 16);
    _tft.unloadFont();
#else
    _tft.setTextDatum(TL_DATUM);
    _tft.setTextColor(text, fill);
    _tft.drawString(ssid, row.x + 22, row.y + 4, 1);
    _tft.setTextDatum(TR_DATUM);
    _tft.setTextColor(kMutedTextColor, fill);
    _tft.drawString(signalLabel, static_cast<int16_t>(row.x + row.w - 8), row.y + 3, 1);
    _tft.drawString(security, static_cast<int16_t>(row.x + row.w - 8), row.y + 15, 1);
#endif
    _tft.setTextDatum(TL_DATUM);
}

void LCD2Dashboard::drawWifiStatusRow(const Rect& statusPanel) {
    const bool spriteReady = (_wifiStatusSprite.created() &&
                              _wifiStatusSprite.width() == statusPanel.w &&
                              _wifiStatusSprite.height() == statusPanel.h);
    const uint16_t statusColor = _wifiScanInProgress ? kMutedTextColor : kTextColor;

    if (spriteReady) {
        _wifiStatusSprite.fillSprite(kBackgroundColor);
        _wifiStatusSprite.drawRoundRect(0, 0, statusPanel.w, statusPanel.h, 5, kFrameColor);
        if (_wifiStatus[0] != '\0') {
            _wifiStatusSprite.setTextColor(statusColor, kBackgroundColor);
            _wifiStatusSprite.setTextDatum(TL_DATUM);
#ifdef SMOOTH_FONT
            _wifiStatusSprite.loadFont(NotoSansBold10);
            _wifiStatusSprite.drawString(_wifiStatus, 6, 4);
            _wifiStatusSprite.unloadFont();
#else
            _wifiStatusSprite.drawString(_wifiStatus, 6, 4, 1);
#endif
        }
        _wifiStatusSprite.pushSprite(statusPanel.x, statusPanel.y);
        return;
    }

    _tft.fillRoundRect(statusPanel.x, statusPanel.y, statusPanel.w, statusPanel.h, 5, kBackgroundColor);
    _tft.drawRoundRect(statusPanel.x, statusPanel.y, statusPanel.w, statusPanel.h, 5, kFrameColor);
    if (_wifiStatus[0] != '\0') {
        _tft.setTextColor(statusColor, kBackgroundColor);
        _tft.setTextDatum(TL_DATUM);
#ifdef SMOOTH_FONT
        _tft.loadFont(NotoSansBold10);
        _tft.drawString(_wifiStatus, statusPanel.x + 6, statusPanel.y + 4);
        _tft.unloadFont();
#else
        _tft.drawString(_wifiStatus, statusPanel.x + 6, statusPanel.y + 4, 1);
#endif
    }
}

void LCD2Dashboard::drawWifiEmptyMessage(const char* message) {
    const Rect row = {10, 113, static_cast<int16_t>(_width - 20), kWifiRowHeight};
    const bool spriteReady = (_wifiRowSprite.created() &&
                              _wifiRowSprite.width() == row.w &&
                              _wifiRowSprite.height() == row.h);

    if (spriteReady) {
        _wifiRowSprite.fillSprite(kBackgroundColor);
        if (message != nullptr && message[0] != '\0') {
            _wifiRowSprite.setTextColor(kMutedTextColor, kBackgroundColor);
            _wifiRowSprite.setTextDatum(MC_DATUM);
#ifdef SMOOTH_FONT
            _wifiRowSprite.loadFont(NotoSansBold12);
            _wifiRowSprite.drawString(message, static_cast<int16_t>(row.w / 2), static_cast<int16_t>(row.h / 2));
            _wifiRowSprite.unloadFont();
#else
            _wifiRowSprite.drawString(message, static_cast<int16_t>(row.w / 2), static_cast<int16_t>(row.h / 2), 2);
#endif
            _wifiRowSprite.setTextDatum(TL_DATUM);
        }
        _wifiRowSprite.pushSprite(row.x, row.y);
        return;
    }

    _tft.fillRect(row.x, row.y, row.w, row.h, kBackgroundColor);
    if (message != nullptr && message[0] != '\0') {
        _tft.setTextColor(kMutedTextColor, kBackgroundColor);
        _tft.setTextDatum(MC_DATUM);
#ifdef SMOOTH_FONT
        _tft.loadFont(NotoSansBold12);
        _tft.drawString(message, static_cast<int16_t>(_width / 2), 126);
        _tft.unloadFont();
#else
        _tft.drawString(message, static_cast<int16_t>(_width / 2), 126, 2);
#endif
        _tft.setTextDatum(TL_DATUM);
    }
}

void LCD2Dashboard::drawWifiListScreen() {
    if (_settingsFullRedraw) {
        _tft.fillScreen(kBackgroundColor);
        _tft.drawRoundRect(0, 0, _width, _height, 8, kFrameColor);
        drawSettingsTopBar("Wi-Fi", true);
        _tft.setTextDatum(TL_DATUM);
        const int16_t helperX = 70;
#ifdef SMOOTH_FONT
        _tft.loadFont(NotoSansBold10);
        _tft.setTextColor(kMutedTextColor, kBackgroundColor);
        _tft.drawString("Tap a network to connect", helperX, 22);
        _tft.unloadFont();
#else
        _tft.setTextColor(kMutedTextColor, kBackgroundColor);
        _tft.drawString("Tap a network to connect", helperX, 22, 1);
#endif
        const int16_t listY = kWifiListTop - 2;
        const int16_t listH = static_cast<int16_t>(_height - listY - 2);
        _tft.fillRect(8, listY, static_cast<int16_t>(_width - 16), listH, kBackgroundColor);
    }

    const Rect statusPanel = {8, 40, static_cast<int16_t>(_width - 16), 18};
    drawWifiStatusRow(statusPanel);

    if (_wifiScanInProgress) {
        drawWifiEmptyMessage("Collecting Wi-Fi data");
        return;
    }

    if (_wifiEntryCount == 0) {
        drawWifiEmptyMessage("No networks found");
        return;
    }

    drawWifiEmptyMessage(nullptr);
    for (uint8_t i = 0; i < _wifiEntryCount; ++i) {
        drawWifiEntryRow(i);
    }
}

void LCD2Dashboard::drawIpViewScreen() {
    if (_settingsFullRedraw) {
        _tft.fillScreen(kBackgroundColor);
        _tft.drawRoundRect(0, 0, _width, _height, 8, kFrameColor);
        _tft.setTextDatum(TL_DATUM);
#ifdef SMOOTH_FONT
        _tft.loadFont(NotoSansBold12);
        _tft.setTextColor(kTextColor, kBackgroundColor);
        _tft.drawString("Jetson Network", 74, 12);
        _tft.unloadFont();
#else
        _tft.setTextColor(kTextColor, kBackgroundColor);
        _tft.drawString("Jetson Network", 74, 12, 2);
#endif
    }

    drawSettingsTftButton(makeSettingsBackButtonRect(), "BACK", kMutedTextColor, isSettingsButtonPressed(makeSettingsBackButtonRect()));
    drawSettingsTftButton(makeSettingsRefreshButtonRect(), "REFRESH", TFT_CYAN, isSettingsButtonPressed(makeSettingsRefreshButtonRect()));
    drawSettingsValueRow(50, _ipRequestInProgress ? "Loading" : "Status", _ipStatus, kTextColor);
    drawSettingsValueRow(79, "Hostname", _aboutHostname, kTextColor);
    drawSettingsValueRow(108, "Interface", _ipInterface, kTextColor);
    drawSettingsValueRow(137, "SSID", _ipSsid, kTextColor);
    drawSettingsValueRow(166, "IP", _ipAddress, kCpuLineColor);
}

void LCD2Dashboard::drawConnectResultScreen() {
    if (_settingsFullRedraw) {
        _tft.fillScreen(kBackgroundColor);
        _tft.drawRoundRect(0, 0, _width, _height, 8, kFrameColor);
    }

    drawSettingsTftButton(makeSettingsBackButtonRect(), "BACK", kMutedTextColor, isSettingsButtonPressed(makeSettingsBackButtonRect()));
    drawSettingsValueRow(72,
                         "Wi-Fi",
                         _wifiConnectInProgress ? "Connecting" : _connectStatus,
                         _wifiConnectOk ? TFT_GREEN : TFT_RED);

    if (!_wifiConnectInProgress && !_wifiConnectOk) {
        drawSettingsTftButton(makeRetryButtonRect(), "RETRY", kCpuLineColor, isSettingsButtonPressed(makeRetryButtonRect()));
    }
}

void LCD2Dashboard::handleSettingsTouch(int16_t touchX, int16_t touchY) {
    switch (_settingsScreen) {
        case SettingsScreen::WIFI_LIST:
            handleWifiListTouch(touchX, touchY);
            return;
        case SettingsScreen::KEYBOARD:
            handleKeyboardTouch(touchX, touchY);
            return;
        case SettingsScreen::IP_VIEW:
            if (pointInRect(touchX, touchY, makeSettingsBackButtonRect())) {
                _settingsScreen = SettingsScreen::NETWORK;
                _dirty = true;
                _lastRefreshMs = 0;
            } else if (pointInRect(touchX, touchY, makeSettingsRefreshButtonRect())) {
                requestIpInfo();
            }
            return;
        case SettingsScreen::CONNECT_RESULT:
            handleConnectResultTouch(touchX, touchY);
            return;
        case SettingsScreen::NETWORK:
            handleNetworkTouch(touchX, touchY);
            return;
        case SettingsScreen::SSH:
            handleSshTouch(touchX, touchY);
            return;
        case SettingsScreen::NGROK:
            handleNgrokTouch(touchX, touchY);
            return;
        case SettingsScreen::SYSTEM:
            handleSystemTouch(touchX, touchY);
            return;
        case SettingsScreen::POWER_MODE:
            handlePowerModeTouch(touchX, touchY);
            return;
        case SettingsScreen::HEADLESS:
            handleHeadlessTouch(touchX, touchY);
            return;
        case SettingsScreen::ABOUT:
            handleAboutTouch(touchX, touchY);
            return;
        case SettingsScreen::NETWORK_HELP:
            handleSettingsHelpTouch(SettingsScreen::NETWORK, touchX, touchY);
            return;
        case SettingsScreen::SYSTEM_HELP:
            handleSettingsHelpTouch(SettingsScreen::SYSTEM, touchX, touchY);
            return;
        case SettingsScreen::CONFIRM:
            handleConfirmTouch(touchX, touchY);
            return;
        case SettingsScreen::MAIN:
        default:
            handleSettingsMainTouch(touchX, touchY);
            return;
    }
}

void LCD2Dashboard::handleSettingsMainTouch(int16_t touchX, int16_t touchY) {
    if (pointInRect(touchX, touchY, makeSettingsExitButtonRect())) {
        _settingsOpen = false;
        _settingsScreen = SettingsScreen::MAIN;
        _settingsDrawn = false;
        _settingsFullRedraw = true;
        _layoutDrawn = false;
        _dirty = true;
        _touchSampleCount = 0;
        _activeControl = ActiveControl::NONE;
        return;
    }

    if (pointInRect(touchX, touchY, makeSettingsNetworkButtonRect())) {
        _settingsScreen = SettingsScreen::NETWORK;
        _dirty = true;
        _lastRefreshMs = 0;
        return;
    }
    if (pointInRect(touchX, touchY, makeSettingsSystemButtonRect())) {
        _settingsScreen = SettingsScreen::SYSTEM;
        _dirty = true;
        _lastRefreshMs = 0;
        return;
    }
    if (pointInRect(touchX, touchY, makeSettingsAboutButtonRect())) {
        _settingsScreen = SettingsScreen::ABOUT;
        _dirty = true;
        _lastRefreshMs = 0;
        return;
    }
    if (pointInRect(touchX, touchY, makeSettingsGamesButtonRect())) {
        _settingsGameActive = true;
        _settingsGameMode = SettingsGameMode::MENU;
        _settingsOpen = false;
        _powerOffMode = PowerOffMode::GAME_MENU;
        _powerOffLastFrameMs = 0;
        _idleDrawn = false;
        _dirty = false;
        drawSettingsGameMenu();
        return;
    }

    const Rect slider = makeSettingsSliderRect();
    if (_activeControl == ActiveControl::NONE) {
        if (!pointInRect(touchX, touchY, slider)) {
            return;
        }
        _activeControl = ActiveControl::LED_SLIDER;
    }

    const int16_t previousValue = _ledBrightnessPercent;
    updateHorizontalSliderFromTouch(slider, touchX, _ledBrightnessPercent);
    if (previousValue != _ledBrightnessPercent) {
        _dirty = true;
    }
}

void LCD2Dashboard::handleNetworkTouch(int16_t touchX, int16_t touchY) {
    if (pointInRect(touchX, touchY, makeSettingsHelpButtonRect())) {
        _settingsScreen = SettingsScreen::NETWORK_HELP;
        _dirty = true;
        return;
    }
    if (pointInRect(touchX, touchY, makeSettingsBackButtonRect())) {
        _settingsScreen = SettingsScreen::MAIN;
        _dirty = true;
        return;
    }
    if (pointInRect(touchX, touchY, makeSettingsButtonRect(0))) {
        requestWifiScan();
        return;
    }
    if (pointInRect(touchX, touchY, makeSettingsButtonRect(1))) {
        requestIpInfo();
        return;
    }
    if (pointInRect(touchX, touchY, makeSettingsButtonRect(2))) {
        requestSshStatus();
        return;
    }
    if (pointInRect(touchX, touchY, makeSettingsButtonRect(3))) {
        requestNgrokStatus();
        return;
    }
}

void LCD2Dashboard::handleSshTouch(int16_t touchX, int16_t touchY) {
    if (pointInRect(touchX, touchY, makeSettingsBackButtonRect())) {
        _settingsScreen = SettingsScreen::NETWORK;
        _dirty = true;
        return;
    }
    const Rect startButton = {12, 158, 140, 32};
    const Rect stopButton = {168, 158, 140, 32};
    if (pointInRect(touchX, touchY, makeSettingsRefreshButtonRect())) {
        requestSshStatus();
        return;
    }
    if (pointInRect(touchX, touchY, startButton)) {
        requestSimpleCommand("SSH_START", "SSH busy", "SSH start requested");
        _sshRequestInProgress = true;
        _sshRequestMs = millis();
        return;
    }
    if (pointInRect(touchX, touchY, stopButton)) {
        openConfirm(SettingsConfirmAction::SSH_STOP, SettingsScreen::SSH, "Stop SSH", "Stop the Jetson SSH service?");
    }
}

void LCD2Dashboard::handleNgrokTouch(int16_t touchX, int16_t touchY) {
    if (pointInRect(touchX, touchY, makeSettingsBackButtonRect())) {
        _settingsScreen = SettingsScreen::NETWORK;
        _dirty = true;
        return;
    }
    const Rect startButton = {12, 158, 140, 32};
    const Rect stopButton = {168, 158, 140, 32};
    if (pointInRect(touchX, touchY, makeSettingsRefreshButtonRect())) {
        requestNgrokStatus();
        return;
    }
    if (pointInRect(touchX, touchY, startButton)) {
        startNgrokAction(NgrokActionState::STARTING);
        return;
    }
    if (pointInRect(touchX, touchY, stopButton)) {
        openConfirm(SettingsConfirmAction::NGROK_STOP, SettingsScreen::NGROK, "Stop Ngrok", "Stop the ngrok SSH tunnel?");
    }
}

void LCD2Dashboard::handleSystemTouch(int16_t touchX, int16_t touchY) {
    if (pointInRect(touchX, touchY, makeSettingsHelpButtonRect())) {
        _settingsScreen = SettingsScreen::SYSTEM_HELP;
        _dirty = true;
        return;
    }
    if (pointInRect(touchX, touchY, makeSettingsBackButtonRect())) {
        _settingsScreen = SettingsScreen::MAIN;
        _dirty = true;
        return;
    }
    if (pointInRect(touchX, touchY, makeSettingsButtonRect(0))) {
        requestPowerModeStatus();
        return;
    }
    if (pointInRect(touchX, touchY, makeSettingsButtonRect(1))) {
        requestHeadlessStatus();
        return;
    }
    if (pointInRect(touchX, touchY, makeSettingsButtonRect(2))) {
        openConfirm(SettingsConfirmAction::MONITOR_RESTART, SettingsScreen::SYSTEM, "Restart Service", "Restart Jetson_shield service?");
        return;
    }
    if (pointInRect(touchX, touchY, makeSettingsButtonRect(3))) {
        openConfirm(SettingsConfirmAction::JETSON_REBOOT, SettingsScreen::SYSTEM, "Reboot Jetson", "Reboot the Jetson now?");
        return;
    }
}

void LCD2Dashboard::handlePowerModeTouch(int16_t touchX, int16_t touchY) {
    if (pointInRect(touchX, touchY, makeSettingsBackButtonRect())) {
        _settingsScreen = SettingsScreen::SYSTEM;
        _dirty = true;
        return;
    }
    if (pointInRect(touchX, touchY, makeSettingsButtonRect(0))) {
        openConfirm(SettingsConfirmAction::POWER_MODE_SET_3, SettingsScreen::POWER_MODE, "7W Mode", "Set Jetson power mode to 7W?");
        return;
    }
    if (pointInRect(touchX, touchY, makeSettingsButtonRect(1))) {
        openConfirm(SettingsConfirmAction::POWER_MODE_SET_0, SettingsScreen::POWER_MODE, "15W Mode", "Set Jetson power mode to 15W?");
        return;
    }
    if (pointInRect(touchX, touchY, makeSettingsButtonRect(2))) {
        openConfirm(SettingsConfirmAction::POWER_MODE_SET_1, SettingsScreen::POWER_MODE, "25W Mode", "Set Jetson power mode to 25W?");
        return;
    }
    if (pointInRect(touchX, touchY, makeSettingsButtonRect(3))) {
        openConfirm(SettingsConfirmAction::POWER_MODE_SET_2, SettingsScreen::POWER_MODE, "MAXN_SUPER", "Set Jetson power mode to MAXN_SUPER?");
        return;
    }
}

void LCD2Dashboard::handleHeadlessTouch(int16_t touchX, int16_t touchY) {
    const bool guiDefault = (strcmp(_headlessDefault, "GRAPHICAL") == 0);
    if (pointInRect(touchX, touchY, makeSettingsBackButtonRect())) {
        _settingsScreen = SettingsScreen::SYSTEM;
        _dirty = true;
        return;
    }
    const Rect switchButton = {12, 112, 296, 38};
    const Rect applyButton = {12, 160, 140, 34};
    const Rect restartButton = {168, 160, 140, 34};
    const Rect defaultButton = {12, 202, 296, 28};
    if (pointInRect(touchX, touchY, switchButton) ||
        pointInRect(touchX, touchY, defaultButton)) {
        requestSimpleCommand(guiDefault ? "HEADLESS_ENABLE_BOOT" : "HEADLESS_DISABLE_BOOT",
                             "Headless busy",
                             guiDefault ? "Headless startup requested" : "GUI startup requested");
        _headlessRequestInProgress = true;
        _headlessRequestMs = millis();
        return;
    }
    if (pointInRect(touchX, touchY, applyButton)) {
        openConfirm(SettingsConfirmAction::HEADLESS_APPLY_NOW, SettingsScreen::HEADLESS, "Apply Mode", "Apply selected display mode now?");
        return;
    }
    if (pointInRect(touchX, touchY, restartButton)) {
        openConfirm(SettingsConfirmAction::HEADLESS_REBOOT, SettingsScreen::HEADLESS, "Restart Jetson", "Restart to apply display mode?");
        return;
    }
}

void LCD2Dashboard::handleAboutTouch(int16_t touchX, int16_t touchY) {
    if (pointInRect(touchX, touchY, makeSettingsBackButtonRect())) {
        _settingsScreen = SettingsScreen::MAIN;
        _dirty = true;
    }
}

void LCD2Dashboard::handleSettingsHelpTouch(SettingsScreen returnScreen, int16_t touchX, int16_t touchY) {
    if (pointInRect(touchX, touchY, makeSettingsBackButtonRect())) {
        _settingsScreen = returnScreen;
        _dirty = true;
    }
}

void LCD2Dashboard::handleConfirmTouch(int16_t touchX, int16_t touchY) {
    if (pointInRect(touchX, touchY, makeSettingsConfirmCancelRect())) {
        _settingsScreen = _confirmReturnScreen;
        _confirmAction = SettingsConfirmAction::NONE;
        _dirty = true;
        return;
    }
    if (pointInRect(touchX, touchY, makeSettingsConfirmOkRect())) {
        executeConfirmAction();
    }
}

void LCD2Dashboard::handleWifiListTouch(int16_t touchX, int16_t touchY) {
    if (pointInRect(touchX, touchY, makeSettingsBackButtonRect())) {
        _settingsScreen = SettingsScreen::NETWORK;
        _dirty = true;
        return;
    }

    for (uint8_t i = 0; i < _wifiEntryCount; ++i) {
        if (!pointInRect(touchX, touchY, makeWifiEntryRect(i))) {
            continue;
        }

        _selectedWifiIndex = static_cast<int8_t>(i);
        if (_wifiEntries[i].current) {
            _wifiConnectOk = true;
            _wifiConnectInProgress = false;
            strncpy(_connectStatus, "Already connected", sizeof(_connectStatus) - 1);
            _connectStatus[sizeof(_connectStatus) - 1] = '\0';
            _settingsScreen = SettingsScreen::CONNECT_RESULT;
        } else {
            _keyboard.reset();
            _settingsScreen = SettingsScreen::KEYBOARD;
        }
        _dirty = true;
        return;
    }
}

void LCD2Dashboard::handleKeyboardTouch(int16_t touchX, int16_t touchY) {
    const VirtualKeyboard::Event event = _keyboard.handleTouch(touchX, touchY, _width, _height);
    if (event == VirtualKeyboard::Event::CANCEL) {
        _settingsScreen = SettingsScreen::WIFI_LIST;
        _dirty = true;
        return;
    }
    if (event == VirtualKeyboard::Event::SUBMIT) {
        requestWifiConnect();
        return;
    }
    if (event == VirtualKeyboard::Event::CHANGED) {
        _dirty = true;
    }
}

void LCD2Dashboard::handleConnectResultTouch(int16_t touchX, int16_t touchY) {
    if (pointInRect(touchX, touchY, makeSettingsBackButtonRect())) {
        _settingsScreen = SettingsScreen::WIFI_LIST;
        _dirty = true;
        return;
    }

    if (!_wifiConnectInProgress && !_wifiConnectOk && pointInRect(touchX, touchY, makeRetryButtonRect())) {
        _keyboard.reset();
        _settingsScreen = SettingsScreen::KEYBOARD;
        _dirty = true;
    }
}

LCD2Dashboard::Rect LCD2Dashboard::makeLedSliderRect(int16_t panelW, int16_t panelH) const {
    const int16_t top = 40;
    const int16_t width = 22;
    const int16_t rightMargin = 4;
    const int16_t bottomMargin = 8;
    return {static_cast<int16_t>(panelW - rightMargin - width),
            top,
            width,
            static_cast<int16_t>(panelH - top - bottomMargin)};
}

bool LCD2Dashboard::pointInRect(int16_t x, int16_t y, const Rect& rect) const {
    return (x >= rect.x) &&
           (y >= rect.y) &&
           (x < (rect.x + rect.w)) &&
           (y < (rect.y + rect.h));
}

bool LCD2Dashboard::isSettingsButtonPressed(const Rect& rect) const {
    return _settingsOpen &&
           _touchDown &&
           _activeControl == ActiveControl::NONE &&
           pointInRect(_touchLastX, _touchLastY, rect);
}

void LCD2Dashboard::updateSliderFromTouch(const Rect& sliderRect,
                                          int16_t touchY,
                                          int16_t& targetValue) {
    const int16_t slotTop = sliderRect.y + 34;
    const int16_t slotBottom = sliderRect.y + sliderRect.h - 30;
    const int16_t clampedY = clampInt16(touchY, slotTop, slotBottom);
    const int16_t span = max<int16_t>(1, slotBottom - slotTop);
    const int32_t numerator = static_cast<int32_t>(slotBottom - clampedY) * 100L;
    targetValue = clampInt16(static_cast<int16_t>((numerator + (span / 2)) / span), 0, 100);
}

int16_t LCD2Dashboard::filterTouchY(int16_t touchY) {
    if (_touchSampleCount < 3) {
        _touchSamplesY[_touchSampleCount++] = touchY;
    } else {
        _touchSamplesY[0] = _touchSamplesY[1];
        _touchSamplesY[1] = _touchSamplesY[2];
        _touchSamplesY[2] = touchY;
    }

    if (_touchSampleCount == 1) {
        return _touchSamplesY[0];
    }

    if (_touchSampleCount == 2) {
        return static_cast<int16_t>((_touchSamplesY[0] + _touchSamplesY[1]) / 2);
    }

    int16_t a = _touchSamplesY[0];
    int16_t b = _touchSamplesY[1];
    int16_t c = _touchSamplesY[2];
    if (a > b) {
        swap(a, b);
    }
    if (b > c) {
        swap(b, c);
    }
    if (a > b) {
        swap(a, b);
    }
    return b;
}

int16_t LCD2Dashboard::historyValueAt(const int16_t* history, uint16_t orderedIndex) const {
    if (_historyCount == 0 || history == nullptr) {
        return -1;
    }

    const uint16_t oldestIndex = (_historyWriteIndex + HISTORY_POINTS - _historyCount) % HISTORY_POINTS;
    const uint16_t ringIndex = (oldestIndex + orderedIndex) % HISTORY_POINTS;
    return history[ringIndex];
}

uint16_t LCD2Dashboard::graphScrollPhasePermille(uint32_t nowMs) const {
    if (_lastMetricsMs == 0U || REFRESH_PERIOD_MS == 0U) {
        return 0;
    }

    const uint32_t elapsed = min<uint32_t>(nowMs - _lastMetricsMs, REFRESH_PERIOD_MS);
    return static_cast<uint16_t>((elapsed * 1000UL) / REFRESH_PERIOD_MS);
}

void LCD2Dashboard::drawGraphScaffold(const Rect& frame,
                                      const Rect& plot,
                                      const char* title,
                                      const char* valueLabel,
                                      uint16_t accentColor) {
    _tft.drawRect(plot.x - kGraphFrameThickness,
                  plot.y - kGraphFrameThickness,
                  static_cast<int16_t>(plot.w + (kGraphFrameThickness * 2)),
                  static_cast<int16_t>(plot.h + (kGraphFrameThickness * 2)),
                  kFrameColor);
    _tft.drawRect(static_cast<int16_t>(plot.x - 1),
                  static_cast<int16_t>(plot.y - 1),
                  static_cast<int16_t>(plot.w + 2),
                  static_cast<int16_t>(plot.h + 2),
                  kFrameColor);

    drawGraphDynamicLabels(frame, plot, title, valueLabel, accentColor);

    uint32_t historySpanSeconds = ((HISTORY_POINTS > 1 ? HISTORY_POINTS - 2 : 0) * REFRESH_PERIOD_MS) / 1000U;
    if (historySpanSeconds == 0U) {
        historySpanSeconds = 1U;
    }
    char leftLabel[12];
    char midLabel[12];
    snprintf(leftLabel, sizeof(leftLabel), "%lus", static_cast<unsigned long>(historySpanSeconds));
    snprintf(midLabel, sizeof(midLabel), "%lus", static_cast<unsigned long>(historySpanSeconds / 2U));
    _tft.setTextColor(kMutedTextColor, kBackgroundColor);
    _tft.setTextDatum(TL_DATUM);
    _tft.drawString(leftLabel, plot.x, frame.y + frame.h - kGraphFooterHeight + 6, 1);
    _tft.setTextDatum(MC_DATUM);
    _tft.drawString(midLabel,
                    plot.x + (plot.w / 2),
                    frame.y + frame.h - (kGraphFooterHeight / 2) + 3,
                    1);
    _tft.setTextDatum(TR_DATUM);
    _tft.drawString("0s", plot.x + plot.w, frame.y + frame.h - kGraphFooterHeight + 6, 1);
    _tft.setTextDatum(TL_DATUM);
}

void LCD2Dashboard::drawGraphDynamicLabels(const Rect& frame,
                                          const Rect& plot,
                                          const char* title,
                                          const char* valueLabel,
                                          uint16_t accentColor) {
    if (title == nullptr) {
        title = "";
    }

    if (_graphHeaderSprite.created() &&
        _graphHeaderSprite.width() == frame.w &&
        _graphHeaderSprite.height() == kGraphHeaderHeight) {
        _graphHeaderSprite.fillSprite(kBackgroundColor);
        _graphHeaderSprite.setTextDatum(TL_DATUM);
        _graphHeaderSprite.setTextColor(accentColor, kBackgroundColor);
        _graphHeaderSprite.drawString(title, 5, 2, 1);
        _graphHeaderSprite.pushSprite(frame.x, frame.y);
    } else {
        _tft.fillRect(frame.x, frame.y, frame.w, kGraphHeaderHeight, kBackgroundColor);
        _tft.setTextDatum(TL_DATUM);
        _tft.setTextColor(accentColor, kBackgroundColor);
        _tft.drawString(title, frame.x + 5, frame.y + 2, 1);
    }

    if (valueLabel == nullptr || valueLabel[0] == '\0') {
        return;
    }

    char number[10] = {0};
    char unit[8] = {0};
    size_t split = 0;
    while (valueLabel[split] != '\0' &&
           (isdigit(static_cast<unsigned char>(valueLabel[split])) ||
            valueLabel[split] == '-' ||
            valueLabel[split] == '.')) {
        ++split;
    }
    if (split == 0) {
        split = min<size_t>(strlen(valueLabel), sizeof(number) - 1);
    }
    strncpy(number, valueLabel, min<size_t>(split, sizeof(number) - 1));
    number[sizeof(number) - 1] = '\0';
    strncpy(unit, valueLabel + split, sizeof(unit) - 1);
    unit[sizeof(unit) - 1] = '\0';

    const int16_t labelLeft = static_cast<int16_t>(plot.x + plot.w + 2);
    const int16_t labelRight = static_cast<int16_t>(frame.x + frame.w - 2);
    const int16_t labelW = static_cast<int16_t>(labelRight - labelLeft + 1);
    const int16_t labelH = static_cast<int16_t>(plot.h + (kGraphFrameThickness * 2));
    const int16_t labelY = static_cast<int16_t>(plot.y - kGraphFrameThickness);

    if (_graphValueSprite.created() &&
        _graphValueSprite.width() == labelW &&
        _graphValueSprite.height() == labelH) {
        _graphValueSprite.fillSprite(kBackgroundColor);
        _graphValueSprite.setTextColor(accentColor, kBackgroundColor);
        _graphValueSprite.setTextDatum(MC_DATUM);
#ifdef SMOOTH_FONT
        _graphValueSprite.loadFont(NotoSansBold12);
        _graphValueSprite.drawString(number, static_cast<int16_t>(labelW / 2), static_cast<int16_t>((labelH / 2) - 7));
        _graphValueSprite.drawString(unit, static_cast<int16_t>(labelW / 2), static_cast<int16_t>((labelH / 2) + 7));
        _graphValueSprite.unloadFont();
#else
        _graphValueSprite.drawString(number, static_cast<int16_t>(labelW / 2), static_cast<int16_t>((labelH / 2) - 7), 2);
        _graphValueSprite.drawString(unit, static_cast<int16_t>(labelW / 2), static_cast<int16_t>((labelH / 2) + 8), 2);
#endif
        _graphValueSprite.setTextDatum(TL_DATUM);
        _graphValueSprite.pushSprite(labelLeft, labelY);
        return;
    }

    const int16_t labelX = static_cast<int16_t>((labelLeft + labelRight) / 2);
    const int16_t centerY = static_cast<int16_t>(plot.y + (plot.h / 2));
    _tft.fillRect(labelLeft, labelY, labelW, labelH, kBackgroundColor);
    _tft.setTextColor(accentColor, kBackgroundColor);
    _tft.setTextDatum(MC_DATUM);
#ifdef SMOOTH_FONT
    _tft.loadFont(NotoSansBold12);
    _tft.drawString(number, labelX, static_cast<int16_t>(centerY - 7));
    _tft.drawString(unit, labelX, static_cast<int16_t>(centerY + 7));
    _tft.unloadFont();
#else
    _tft.drawString(number, labelX, static_cast<int16_t>(centerY - 7), 2);
    _tft.drawString(unit, labelX, static_cast<int16_t>(centerY + 8), 2);
#endif
    _tft.setTextDatum(TL_DATUM);
}

void LCD2Dashboard::drawLayout() {
    const DashboardLayout layout = buildLayout();
    const bool ioPage = (_dashboardPage == DashboardPage::IO);
    _tft.fillScreen(kBackgroundColor);

    char cpuValueLabel[16];
    char gpuValueLabel[16];
    char ramValueLabel[16];
    char powerGraphTitle[32];
    formatPowerGraphTitle(_powerModeId, powerGraphTitle, sizeof(powerGraphTitle));
    if (ioPage) {
        formatRateMaxValue(_netGraphScaleKbps, cpuValueLabel, sizeof(cpuValueLabel));
        formatRateMaxValue(_diskGraphScaleKbps, gpuValueLabel, sizeof(gpuValueLabel));
        formatPowerValue(_latest.powerMw, ramValueLabel, sizeof(ramValueLabel));
    } else {
        formatGraphUsageLabel(_latest.cpuUsage, cpuValueLabel, sizeof(cpuValueLabel));
        formatGraphUsageLabel(_latest.gpuUsage, gpuValueLabel, sizeof(gpuValueLabel));
        formatGraphUsageLabel(_latest.ramUsage, ramValueLabel, sizeof(ramValueLabel));
    }
    drawGraphScaffold(layout.cpuFrame, layout.cpuPlot, ioPage ? "Network" : "CPU", cpuValueLabel, ioPage ? kCpuLineColor : kCpuLineColor);
    drawGraphScaffold(layout.gpuFrame, layout.gpuPlot, ioPage ? "DISK" : "GPU", gpuValueLabel, ioPage ? kPowerAccentColor : kGpuLineColor);
    drawGraphScaffold(layout.ramFrame, layout.ramPlot, ioPage ? powerGraphTitle : "RAM", ramValueLabel, ioPage ? kPowerGraphColor : kRamLineColor);
    const int16_t separatorY = static_cast<int16_t>(layout.panelFrame.y + layout.panelFrame.h + (kSectionGap / 2));
    _tft.drawFastHLine(kOuterMargin, separatorY, static_cast<int16_t>(_width - (kOuterMargin * 2)), kFrameColor);
    _tft.drawRoundRect(0, 0, _width, _height, 8, kFrameColor);
}

void LCD2Dashboard::drawDynamic(uint32_t nowMs, bool updatePanel) {
    if (!_spritesReady) {
        return;
    }

    const DashboardLayout layout = buildLayout();
    const uint16_t scrollPhase = graphScrollPhasePermille(nowMs);

    const bool ioPage = (_dashboardPage == DashboardPage::IO);
    char cpuValueLabel[16];
    char gpuValueLabel[16];
    char ramValueLabel[16];
    char powerGraphTitle[32];
    formatPowerGraphTitle(_powerModeId, powerGraphTitle, sizeof(powerGraphTitle));
    if (ioPage) {
        formatRateMaxValue(_netGraphScaleKbps, cpuValueLabel, sizeof(cpuValueLabel));
        formatRateMaxValue(_diskGraphScaleKbps, gpuValueLabel, sizeof(gpuValueLabel));
        formatPowerValue(_latest.powerMw, ramValueLabel, sizeof(ramValueLabel));
    } else {
        formatGraphUsageLabel(_latest.cpuUsage, cpuValueLabel, sizeof(cpuValueLabel));
        formatGraphUsageLabel(_latest.gpuUsage, gpuValueLabel, sizeof(gpuValueLabel));
        formatGraphUsageLabel(_latest.ramUsage, ramValueLabel, sizeof(ramValueLabel));
    }
    if (updatePanel) {
        drawGraphDynamicLabels(layout.cpuFrame, layout.cpuPlot, ioPage ? "Network" : "CPU", cpuValueLabel, ioPage ? kCpuLineColor : kCpuLineColor);
        drawGraphDynamicLabels(layout.gpuFrame, layout.gpuPlot, ioPage ? "DISK" : "GPU", gpuValueLabel, ioPage ? kPowerAccentColor : kGpuLineColor);
        drawGraphDynamicLabels(layout.ramFrame, layout.ramPlot, ioPage ? powerGraphTitle : "RAM", ramValueLabel, ioPage ? kPowerGraphColor : kRamLineColor);
    }
    if (ioPage) {
        drawDualUsageGraph(_cpuSprite,
                           layout.cpuPlot.w,
                           layout.cpuPlot.h,
                           _netHistory,
                           _netUploadHistory,
                           kCpuLineColor,
                           kGpuLineColor,
                           scrollPhase);
        drawDualUsageGraph(_gpuSprite,
                           layout.gpuPlot.w,
                           layout.gpuPlot.h,
                           _diskHistory,
                           _diskWriteHistory,
                           kPowerAccentColor,
                           kRamLineColor,
                           scrollPhase);
    } else {
        drawUsageGraph(_cpuSprite, layout.cpuPlot.w, layout.cpuPlot.h, _cpuHistory, kCpuLineColor, scrollPhase);
        drawUsageGraph(_gpuSprite, layout.gpuPlot.w, layout.gpuPlot.h, _gpuHistory, kGpuLineColor, scrollPhase);
    }
    drawUsageGraph(_ramSprite,
                   layout.ramPlot.w,
                   layout.ramPlot.h,
                   ioPage ? _swapHistory : _ramHistory,
                   ioPage ? kPowerGraphColor : kRamLineColor,
                   scrollPhase);
    if (updatePanel) {
        drawNumericPanel(_panelSprite, layout.panelFrame.w - 2, layout.panelFrame.h - 2);
    }

    _tft.startWrite();
    _cpuSprite.pushSprite(layout.cpuPlot.x, layout.cpuPlot.y);
    _gpuSprite.pushSprite(layout.gpuPlot.x, layout.gpuPlot.y);
    _ramSprite.pushSprite(layout.ramPlot.x, layout.ramPlot.y);
    if (updatePanel) {
        _panelSprite.pushSprite(layout.panelFrame.x + 1, layout.panelFrame.y + 1);
    }
    _tft.endWrite();
}

void LCD2Dashboard::drawNoData() {
    if (!_spritesReady) {
        return;
    }

    const DashboardLayout layout = buildLayout();

    const bool ioPage = (_dashboardPage == DashboardPage::IO);
    char cpuValueLabel[16];
    char gpuValueLabel[16];
    char ramValueLabel[16];
    char powerGraphTitle[32];
    formatPowerGraphTitle(_powerModeId, powerGraphTitle, sizeof(powerGraphTitle));
    if (ioPage) {
        formatRateMaxValue(_netGraphScaleKbps, cpuValueLabel, sizeof(cpuValueLabel));
        formatRateMaxValue(_diskGraphScaleKbps, gpuValueLabel, sizeof(gpuValueLabel));
        formatPowerValue(_latest.powerMw, ramValueLabel, sizeof(ramValueLabel));
    } else {
        formatGraphUsageLabel(_latest.cpuUsage, cpuValueLabel, sizeof(cpuValueLabel));
        formatGraphUsageLabel(_latest.gpuUsage, gpuValueLabel, sizeof(gpuValueLabel));
        formatGraphUsageLabel(_latest.ramUsage, ramValueLabel, sizeof(ramValueLabel));
    }
    drawGraphScaffold(layout.cpuFrame, layout.cpuPlot, ioPage ? "Network" : "CPU", cpuValueLabel, kCpuLineColor);
    drawGraphScaffold(layout.gpuFrame, layout.gpuPlot, ioPage ? "DISK" : "GPU", gpuValueLabel, ioPage ? kPowerAccentColor : kGpuLineColor);
    drawGraphScaffold(layout.ramFrame, layout.ramPlot, ioPage ? powerGraphTitle : "RAM", ramValueLabel, ioPage ? kPowerGraphColor : kRamLineColor);

    _cpuSprite.fillSprite(kBackgroundColor);
    _gpuSprite.fillSprite(kBackgroundColor);
    _ramSprite.fillSprite(kBackgroundColor);
    _panelSprite.fillSprite(kBackgroundColor);

    _cpuSprite.setTextColor(kMutedTextColor, kBackgroundColor);
    _gpuSprite.setTextColor(kMutedTextColor, kBackgroundColor);
    _ramSprite.setTextColor(kMutedTextColor, kBackgroundColor);
    _cpuSprite.setTextDatum(MC_DATUM);
    _gpuSprite.setTextDatum(MC_DATUM);
    _ramSprite.setTextDatum(MC_DATUM);
    _cpuSprite.drawString("Waiting for", layout.cpuPlot.w / 2, (layout.cpuPlot.h / 2) - 8, 2);
    _cpuSprite.drawString("Serial1 stats", layout.cpuPlot.w / 2, (layout.cpuPlot.h / 2) + 10, 2);
    _gpuSprite.drawString("GPU history", layout.gpuPlot.w / 2, (layout.gpuPlot.h / 2) - 8, 2);
    _gpuSprite.drawString("starts here", layout.gpuPlot.w / 2, (layout.gpuPlot.h / 2) + 10, 2);
    _ramSprite.drawString("Graph resumes", layout.ramPlot.w / 2, (layout.ramPlot.h / 2) - 8, 2);
    _ramSprite.drawString("when metrics arrive", layout.ramPlot.w / 2, (layout.ramPlot.h / 2) + 10, 1);
    _cpuSprite.setTextDatum(TL_DATUM);
    _gpuSprite.setTextDatum(TL_DATUM);
    _ramSprite.setTextDatum(TL_DATUM);

    drawNumericPanel(_panelSprite, layout.panelFrame.w - 2, layout.panelFrame.h - 2);

    _tft.startWrite();
    _cpuSprite.pushSprite(layout.cpuPlot.x, layout.cpuPlot.y);
    _gpuSprite.pushSprite(layout.gpuPlot.x, layout.gpuPlot.y);
    _ramSprite.pushSprite(layout.ramPlot.x, layout.ramPlot.y);
    _panelSprite.pushSprite(layout.panelFrame.x + 1, layout.panelFrame.y + 1);
    _tft.endWrite();
}

namespace {

struct GraphPoint {
    int16_t x;
    int16_t y;
    bool valid;
};

bool clipTraceSegment(int16_t inX0,
                      int16_t inY0,
                      int16_t inX1,
                      int16_t inY1,
                      int16_t leftX,
                      int16_t rightX,
                      int16_t& outX0,
                      int16_t& outY0,
                      int16_t& outX1,
                      int16_t& outY1) {
    if ((inX0 < leftX && inX1 < leftX) || (inX0 > rightX && inX1 > rightX)) {
        return false;
    }

    outX0 = inX0;
    outY0 = inY0;
    outX1 = inX1;
    outY1 = inY1;

    if (inX0 == inX1) {
        return inX0 >= leftX && inX0 <= rightX;
    }

    auto interpolateY = [&](int16_t clipX) -> int16_t {
        const int32_t dx = static_cast<int32_t>(inX1) - static_cast<int32_t>(inX0);
        const int32_t dy = static_cast<int32_t>(inY1) - static_cast<int32_t>(inY0);
        const int32_t step = static_cast<int32_t>(clipX) - static_cast<int32_t>(inX0);
        return static_cast<int16_t>(static_cast<int32_t>(inY0) + ((dy * step) / dx));
    };

    if (outX0 < leftX) {
        outY0 = interpolateY(leftX);
        outX0 = leftX;
    } else if (outX0 > rightX) {
        outY0 = interpolateY(rightX);
        outX0 = rightX;
    }

    if (outX1 < leftX) {
        outY1 = interpolateY(leftX);
        outX1 = leftX;
    } else if (outX1 > rightX) {
        outY1 = interpolateY(rightX);
        outX1 = rightX;
    }

    return true;
}

void drawTraceSegment(TFT_eSprite& sprite,
                      int16_t x0,
                      int16_t y0,
                      int16_t x1,
                      int16_t y1,
                      uint16_t color,
                      uint8_t thickness) {
    sprite.drawLine(x0, y0, x1, y1, color);
    if (thickness < 2U) {
        return;
    }
    if (abs(x1 - x0) >= abs(y1 - y0)) {
        sprite.drawLine(x0, static_cast<int16_t>(y0 + 1), x1, static_cast<int16_t>(y1 + 1), color);
    } else {
        sprite.drawLine(static_cast<int16_t>(x0 + 1), y0, static_cast<int16_t>(x1 + 1), y1, color);
    }
}


void fillTraceArea(TFT_eSprite& sprite,
                   int16_t x0,
                   int16_t y0,
                   int16_t x1,
                   int16_t y1,
                   int16_t bottomY,
                   uint16_t fillColor) {
    if (x1 < x0) {
        swap(x0, x1);
        swap(y0, y1);
    }

    if (x0 == x1) {
        const int16_t topY = min(y0, y1);
        sprite.drawFastVLine(x0, topY, static_cast<int16_t>(bottomY - topY + 1), fillColor);
        return;
    }

    const int32_t dx = static_cast<int32_t>(x1) - static_cast<int32_t>(x0);
    const int32_t dy = static_cast<int32_t>(y1) - static_cast<int32_t>(y0);
    for (int16_t x = x0; x <= x1; ++x) {
        const int32_t step = static_cast<int32_t>(x) - static_cast<int32_t>(x0);
        const int16_t y = static_cast<int16_t>(y0 + ((dy * step) / dx));
        sprite.drawFastVLine(x, y, static_cast<int16_t>(bottomY - y + 1), fillColor);
    }
}

void drawClippedTraceSegment(TFT_eSprite& sprite,
                             int16_t inX0,
                             int16_t inY0,
                             int16_t inX1,
                             int16_t inY1,
                             int16_t leftX,
                             int16_t rightX,
                             uint16_t color,
                             uint8_t thickness) {
    int16_t x0 = 0;
    int16_t y0 = 0;
    int16_t x1 = 0;
    int16_t y1 = 0;
    if (clipTraceSegment(inX0, inY0, inX1, inY1, leftX, rightX, x0, y0, x1, y1)) {
        drawTraceSegment(sprite, x0, y0, x1, y1, color, thickness);
    }
}

void fillClippedTraceSegment(TFT_eSprite& sprite,
                             int16_t inX0,
                             int16_t inY0,
                             int16_t inX1,
                             int16_t inY1,
                             int16_t leftX,
                             int16_t rightX,
                             int16_t bottomY,
                             uint16_t fillColor) {
    int16_t x0 = 0;
    int16_t y0 = 0;
    int16_t x1 = 0;
    int16_t y1 = 0;
    if (clipTraceSegment(inX0, inY0, inX1, inY1, leftX, rightX, x0, y0, x1, y1)) {
        fillTraceArea(sprite, x0, y0, x1, y1, bottomY, fillColor);
    }
}

void drawRoundedTrace(TFT_eSprite& sprite,
                      const GraphPoint* points,
                      uint16_t count,
                      int16_t leftX,
                      int16_t rightX,
                      uint16_t color,
                      uint8_t thickness) {
    if (points == nullptr || count < 2) {
        return;
    }

    GraphPoint validPoints[LCD2Dashboard::HISTORY_POINTS];
    uint16_t validCount = 0;
    for (uint16_t i = 0; i < count && validCount < LCD2Dashboard::HISTORY_POINTS; ++i) {
        if (points[i].valid) {
            validPoints[validCount++] = points[i];
        }
    }

    if (validCount < 2) {
        return;
    }

    if (validCount == 2) {
        drawClippedTraceSegment(sprite,
                                validPoints[0].x,
                                validPoints[0].y,
                                validPoints[1].x,
                                validPoints[1].y,
                                leftX,
                                rightX,
                                color,
                                thickness);
        return;
    }

    const int16_t firstMidX = static_cast<int16_t>((validPoints[0].x + validPoints[1].x) / 2);
    const int16_t firstMidY = static_cast<int16_t>((validPoints[0].y + validPoints[1].y) / 2);
    drawClippedTraceSegment(sprite,
                            validPoints[0].x,
                            validPoints[0].y,
                            firstMidX,
                            firstMidY,
                            leftX,
                            rightX,
                            color,
                            thickness);

    for (uint16_t i = 1; i + 1 < validCount; ++i) {
        const GraphPoint previous = validPoints[i - 1];
        const GraphPoint current = validPoints[i];
        const GraphPoint next = validPoints[i + 1];
        const int16_t startX = static_cast<int16_t>((previous.x + current.x) / 2);
        const int16_t startY = static_cast<int16_t>((previous.y + current.y) / 2);
        const int16_t endX = static_cast<int16_t>((current.x + next.x) / 2);
        const int16_t endY = static_cast<int16_t>((current.y + next.y) / 2);
        int16_t lastX = startX;
        int16_t lastY = startY;

        for (uint8_t step = 1; step <= 6; ++step) {
            const int32_t t = static_cast<int32_t>(step) * 256L / 6L;
            const int32_t inv = 256L - t;
            const int16_t x = static_cast<int16_t>(((inv * inv * startX) + (2L * inv * t * current.x) + (t * t * endX)) / (256L * 256L));
            const int16_t y = static_cast<int16_t>(((inv * inv * startY) + (2L * inv * t * current.y) + (t * t * endY)) / (256L * 256L));
            drawClippedTraceSegment(sprite, lastX, lastY, x, y, leftX, rightX, color, thickness);
            lastX = x;
            lastY = y;
        }
    }

    const GraphPoint beforeLast = validPoints[validCount - 2];
    const GraphPoint last = validPoints[validCount - 1];
    const int16_t lastMidX = static_cast<int16_t>((beforeLast.x + last.x) / 2);
    const int16_t lastMidY = static_cast<int16_t>((beforeLast.y + last.y) / 2);
    drawClippedTraceSegment(sprite,
                            lastMidX,
                            lastMidY,
                            last.x,
                            last.y,
                            leftX,
                            rightX,
                            color,
                            thickness);
}

void fillSingleTrace(TFT_eSprite& sprite,
                     const GraphPoint* points,
                     uint16_t count,
                     int16_t leftX,
                     int16_t rightX,
                     int16_t bottomY,
                     uint16_t fillColor) {
    if (points == nullptr || count < 2) {
        return;
    }

    GraphPoint validPoints[LCD2Dashboard::HISTORY_POINTS];
    uint16_t validCount = 0;
    for (uint16_t i = 0; i < count && validCount < LCD2Dashboard::HISTORY_POINTS; ++i) {
        if (points[i].valid) {
            validPoints[validCount++] = points[i];
        }
    }

    if (validCount < 2) {
        return;
    }

    if (validCount == 2) {
        fillClippedTraceSegment(sprite,
                                validPoints[0].x,
                                validPoints[0].y,
                                validPoints[1].x,
                                validPoints[1].y,
                                leftX,
                                rightX,
                                bottomY,
                                fillColor);
        return;
    }

    const int16_t firstMidX = static_cast<int16_t>((validPoints[0].x + validPoints[1].x) / 2);
    const int16_t firstMidY = static_cast<int16_t>((validPoints[0].y + validPoints[1].y) / 2);
    fillClippedTraceSegment(sprite,
                            validPoints[0].x,
                            validPoints[0].y,
                            firstMidX,
                            firstMidY,
                            leftX,
                            rightX,
                            bottomY,
                            fillColor);

    for (uint16_t i = 1; i + 1 < validCount; ++i) {
        const GraphPoint previous = validPoints[i - 1];
        const GraphPoint current = validPoints[i];
        const GraphPoint next = validPoints[i + 1];
        const int16_t startX = static_cast<int16_t>((previous.x + current.x) / 2);
        const int16_t startY = static_cast<int16_t>((previous.y + current.y) / 2);
        const int16_t endX = static_cast<int16_t>((current.x + next.x) / 2);
        const int16_t endY = static_cast<int16_t>((current.y + next.y) / 2);
        int16_t lastX = startX;
        int16_t lastY = startY;

        for (uint8_t step = 1; step <= 6; ++step) {
            const int32_t t = static_cast<int32_t>(step) * 256L / 6L;
            const int32_t inv = 256L - t;
            const int16_t x = static_cast<int16_t>(((inv * inv * startX) + (2L * inv * t * current.x) + (t * t * endX)) / (256L * 256L));
            const int16_t y = static_cast<int16_t>(((inv * inv * startY) + (2L * inv * t * current.y) + (t * t * endY)) / (256L * 256L));
            fillClippedTraceSegment(sprite, lastX, lastY, x, y, leftX, rightX, bottomY, fillColor);
            lastX = x;
            lastY = y;
        }
    }

    const GraphPoint beforeLast = validPoints[validCount - 2];
    const GraphPoint last = validPoints[validCount - 1];
    const int16_t lastMidX = static_cast<int16_t>((beforeLast.x + last.x) / 2);
    const int16_t lastMidY = static_cast<int16_t>((beforeLast.y + last.y) / 2);
    fillClippedTraceSegment(sprite,
                            lastMidX,
                            lastMidY,
                            last.x,
                            last.y,
                            leftX,
                            rightX,
                            bottomY,
                            fillColor);
}

} // namespace

void LCD2Dashboard::drawDualUsageGraph(TFT_eSprite& sprite,
                                       int16_t w,
                                       int16_t h,
                                       const int16_t* firstHistory,
                                       const int16_t* secondHistory,
                                       uint16_t firstColor,
                                       uint16_t secondColor,
                                       uint16_t scrollPhasePermille) {
    sprite.fillSprite(kBackgroundColor);

    if (w < 2 || h < 2) {
        return;
    }

    if (_historyCount == 0 || firstHistory == nullptr) {
        drawGraphGrid(sprite, w, h);
        return;
    }

    uint16_t sampleCount = _historyCount;
    if (sampleCount > HISTORY_POINTS) {
        sampleCount = HISTORY_POINTS;
    }

    const uint16_t startOrderedIndex = _historyCount - sampleCount;
    const int16_t leftX = 0;
    const int16_t rightX = w - 1;
    const int16_t drawableWidth = (rightX > leftX) ? (rightX - leftX) : 1;
    const uint16_t visibleIntervals = (HISTORY_POINTS > 1) ? (HISTORY_POINTS - 2) : 1;
    const int32_t spacing1000 = (visibleIntervals > 0)
                                    ? ((static_cast<int32_t>(drawableWidth) * 1000L) / visibleIntervals)
                                    : static_cast<int32_t>(drawableWidth) * 1000L;
    const int32_t scroll1000 = (spacing1000 * min<uint16_t>(scrollPhasePermille, 1000)) / 1000L;
    const int32_t extendedRightX1000 = (static_cast<int32_t>(rightX) * 1000L) + spacing1000;

    GraphPoint firstPoints[HISTORY_POINTS];
    GraphPoint secondPoints[HISTORY_POINTS];
    for (uint16_t i = 0; i < sampleCount; ++i) {
        const uint16_t ageFromNewest = static_cast<uint16_t>(sampleCount - 1U - i);
        const int32_t x1000 = extendedRightX1000 -
                              (static_cast<int32_t>(ageFromNewest) * spacing1000) -
                              scroll1000;
        const int16_t px = static_cast<int16_t>((x1000 + 500L) / 1000L);
        const int16_t firstValue = historyValueAt(firstHistory, startOrderedIndex + i);
        firstPoints[i] = {px, plotYForUsage(firstValue, h), isUsageValid(firstValue)};
        if (secondHistory != nullptr) {
            const int16_t secondValue = historyValueAt(secondHistory, startOrderedIndex + i);
            secondPoints[i] = {px, plotYForUsage(secondValue, h), isUsageValid(secondValue)};
        } else {
            secondPoints[i] = {px, 0, false};
        }
    }

    const bool singleLineGraph = (secondHistory == nullptr);
    if (singleLineGraph) {
        const uint16_t fillColor = (firstColor == kCpuLineColor) ? kCpuFillColor :
                                   ((firstColor == kGpuLineColor) ? kGpuFillColor :
                                    ((firstColor == kPowerGraphColor) ? kPowerGraphColor : kRamFillColor));
        fillSingleTrace(sprite, firstPoints, sampleCount, leftX, rightX, static_cast<int16_t>(h - 1), fillColor);
    }

    drawGraphGrid(sprite, w, h);
    drawRoundedTrace(sprite, firstPoints, sampleCount, leftX, rightX, firstColor, singleLineGraph ? 1U : 2U);
    if (!singleLineGraph) {
        drawRoundedTrace(sprite, secondPoints, sampleCount, leftX, rightX, secondColor, 2U);
    }

}

void LCD2Dashboard::drawUsageGraph(TFT_eSprite& sprite,
                                   int16_t w,
                                   int16_t h,
                                   const int16_t* history,
                                   uint16_t lineColor,
                                   uint16_t scrollPhasePermille) {
    drawDualUsageGraph(sprite, w, h, history, nullptr, lineColor, lineColor, scrollPhasePermille);
}

void LCD2Dashboard::drawMetricCard(TFT_eSprite& sprite,
                                   int16_t x,
                                   int16_t y,
                                   int16_t w,
                                   int16_t h,
                                   const char* title,
                                   const char* value,
                                   uint16_t accentColor) {
    if (w <= 0 || h <= 0) {
        return;
    }

    sprite.fillRect(x, y, w, h, kBackgroundColor);
    const int16_t dividerW = 3;
    const int16_t textX = static_cast<int16_t>(x + dividerW + 4);
    sprite.fillRect(x, y, dividerW, h, accentColor);
    sprite.setTextDatum(TL_DATUM);
#ifdef SMOOTH_FONT
    sprite.loadFont(NotoSansBold12);
    sprite.setTextColor(accentColor, kBackgroundColor);
    sprite.drawString(title, textX, static_cast<int16_t>(y + 4));
    sprite.setTextColor(kTextColor, kBackgroundColor);
    sprite.drawString(value, textX, static_cast<int16_t>(y + h - 15));
    sprite.unloadFont();
#else
    sprite.setTextColor(accentColor, kBackgroundColor);
    sprite.drawString(title, textX, static_cast<int16_t>(y + 2), 2);
    sprite.setTextColor(kTextColor, kBackgroundColor);
    sprite.drawString(value, textX, static_cast<int16_t>(y + h - 17), 2);
#endif
}

void LCD2Dashboard::drawSliderControl(TFT_eSprite& sprite,
                                      const Rect& area,
                                      const char* label,
                                      int16_t value,
                                      uint16_t accentColor) {
    char valueLabel[12];
    formatUsageValue(value, valueLabel, sizeof(valueLabel));

    const int16_t slotTop = area.y + 20;
    const int16_t slotBottom = area.y + area.h - 26;
    const int16_t slotHeight = max<int16_t>(16, slotBottom - slotTop);
    const int16_t slotWidth = 8;
    const int16_t slotX = area.x + ((area.w - slotWidth) / 2);
    const int16_t knobWidth = max<int16_t>(16, area.w - 8);
    const int16_t knobHeight = 12;
    const int16_t clampedValue = clampInt16(value, 0, 100);
    const int16_t knobCenterY = slotBottom - ((clampedValue * slotHeight) / 100);
    const int16_t knobY = clampInt16(static_cast<int16_t>(knobCenterY - (knobHeight / 2)),
                                     slotTop - 1,
                                     slotBottom - knobHeight + 1);
    const int16_t fillTop = clampInt16(knobCenterY, slotTop, slotBottom);

    sprite.drawRoundRect(area.x, area.y, area.w, area.h, 5, kFrameColor);
    sprite.setTextColor(accentColor, kBackgroundColor);
    sprite.setTextDatum(TC_DATUM);
    sprite.drawString(label, area.x + (area.w / 2), area.y + 3, 2);

    sprite.drawRoundRect(slotX - 3, slotTop - 2, slotWidth + 6, slotHeight + 4, 5, kFrameColor);
    sprite.fillRoundRect(slotX, slotTop, slotWidth, slotHeight, 4, TFT_DARKGREY);
    sprite.fillRoundRect(slotX,
                         fillTop,
                         slotWidth,
                         static_cast<int16_t>(slotBottom - fillTop + 1),
                         4,
                         accentColor);
    sprite.fillRoundRect(static_cast<int16_t>(area.x + ((area.w - knobWidth) / 2)),
                         knobY,
                         knobWidth,
                         knobHeight,
                         4,
                         kTextColor);
    sprite.drawFastHLine(static_cast<int16_t>(area.x + 6),
                         static_cast<int16_t>(knobY + (knobHeight / 2)),
                         static_cast<int16_t>(area.w - 12),
                         accentColor);

    sprite.setTextColor(kTextColor, kBackgroundColor);
    sprite.drawString(valueLabel, area.x + (area.w / 2), area.y + area.h - 17, 2);
    sprite.setTextDatum(TL_DATUM);
}

void LCD2Dashboard::drawNumericPanel(TFT_eSprite& sprite, int16_t w, int16_t h) {
    if (_dashboardPage == DashboardPage::IO) {
        drawIoPanel(sprite, w, h);
        return;
    }

    drawPerformancePanel(sprite, w, h);
}

void LCD2Dashboard::drawPerformancePanel(TFT_eSprite& sprite, int16_t w, int16_t h) {
    char cpuTempLabel[16];
    char gpuTempLabel[16];
    char powerLabel[16];
    char fanLabel[12];

    formatTempValue(_latest.cpuTemp, cpuTempLabel, sizeof(cpuTempLabel));
    formatTempValue(_latest.gpuTemp, gpuTempLabel, sizeof(gpuTempLabel));
    formatPowerValue(_latest.powerMw, powerLabel, sizeof(powerLabel));
    formatUsageValue(_fanPercent, fanLabel, sizeof(fanLabel));

    sprite.fillSprite(kBackgroundColor);

    const Rect settingsRect = makeSettingsPanelButtonRect(w);
    const int16_t cardCount = 4;
    const int16_t cardH = static_cast<int16_t>(h - 8);
    const int16_t statusW = max<int16_t>(4, settingsRect.x);
    const int16_t baseW = static_cast<int16_t>(statusW / cardCount);
    const int16_t card1W = static_cast<int16_t>(baseW + 10);
    const int16_t card2W = static_cast<int16_t>(baseW + 10);
    const int16_t card3W = static_cast<int16_t>(baseW - 5);
    const int16_t card4X = static_cast<int16_t>(card1W + card2W + card3W);
    const int16_t card4W = static_cast<int16_t>(statusW - card4X);
    drawMetricCard(sprite, 0, 4, card1W, cardH, "CPU Temp", cpuTempLabel, kCpuLineColor);
    drawMetricCard(sprite, card1W, 4, card2W, cardH, "GPU Temp", gpuTempLabel, kGpuAccentColor);
    drawMetricCard(sprite, static_cast<int16_t>(card1W + card2W), 4, card3W, cardH, "POWER", powerLabel, kPowerAccentColor);
    drawMetricCard(sprite, card4X, 4, card4W, cardH, "FAN", fanLabel, kStatusOkColor);
    drawSettingsPanelButton(sprite, w);
    sprite.setTextDatum(TL_DATUM);
}

void LCD2Dashboard::drawIoPanel(TFT_eSprite& sprite, int16_t w, int16_t h) {
    char dlLabel[16];
    char ulLabel[16];
    char readLabel[16];
    char writeLabel[16];

    formatRateValue(_latest.netDownloadKbps, dlLabel, sizeof(dlLabel));
    formatRateValue(_latest.netUploadKbps, ulLabel, sizeof(ulLabel));
    formatRateValue(_latest.diskReadKbps, readLabel, sizeof(readLabel));
    formatRateValue(_latest.diskWriteKbps, writeLabel, sizeof(writeLabel));

    sprite.fillSprite(kBackgroundColor);

    const Rect settingsRect = makeSettingsPanelButtonRect(w);
    const int16_t cardCount = 4;
    const int16_t cardH = static_cast<int16_t>(h - 8);
    const int16_t statusW = max<int16_t>(4, settingsRect.x);
    const int16_t baseW = static_cast<int16_t>(statusW / cardCount);
    const int16_t card1W = static_cast<int16_t>(baseW + 10);
    const int16_t card2W = static_cast<int16_t>(baseW + 10);
    const int16_t card3W = static_cast<int16_t>(baseW - 5);
    const int16_t card4X = static_cast<int16_t>(card1W + card2W + card3W);
    const int16_t card4W = static_cast<int16_t>(statusW - card4X);
    drawMetricCard(sprite, 0, 4, card1W, cardH, "DOWNLOAD", dlLabel, kCpuLineColor);
    drawMetricCard(sprite, card1W, 4, card2W, cardH, "UPLOAD", ulLabel, kGpuLineColor);
    drawMetricCard(sprite, static_cast<int16_t>(card1W + card2W), 4, card3W, cardH, "READ", readLabel, kPowerAccentColor);
    drawMetricCard(sprite, card4X, 4, card4W, cardH, "WRITE", writeLabel, kRamLineColor);
    drawSettingsPanelButton(sprite, w);
    sprite.setTextDatum(TL_DATUM);
}

// ============================================================================

// POWER_OFF screen — 1-bit sprite helpers
// ============================================================================

bool LCD2Dashboard::initGameSprite() {
    deleteGameSprite();
    _gameSprite.setColorDepth(1);
    if (_gameSprite.createSprite(_width, _height) == nullptr) {
        _gameSpriteReady = false;
        return false;
    }

    _gameSprite.setTextFont(1);
    _gameSprite.setTextColor(kGameSpriteInk, kGameSpritePaper);
    _gameSprite.setTextDatum(TL_DATUM);
    // Maps 1-bit ink/paper values onto 16-bit colours for the TFT.
    _gameSprite.setBitmapColor(TFT_WHITE, TFT_BLACK);
    _gameSpriteReady = true;
    return true;
}

void LCD2Dashboard::deleteGameSprite() {
    if (_gameSprite.created()) {
        _gameSprite.deleteSprite();
    }
    _gameSpriteReady = false;
}

// Returns true on rising edge (new touch contact). Updates prevTouched,
// _gameTouched, _gameTouchX, _gameTouchY.
bool LCD2Dashboard::sampleGameTouch(bool& prevTouched) {
    bool cur = false;
    if (_touchReady) {
        uint16_t tx = 0, ty = 0;
        cur = _tft.getTouch(&tx, &ty, jetson_cfg::kLcd2TouchPressureThreshold);
        if (cur) {
            _gameTouchX = static_cast<int16_t>(tx);
            _gameTouchY = static_cast<int16_t>(ty);
        }
    }
    const bool risingEdge = cur && !prevTouched;
    prevTouched = cur;
    _gameTouched = cur;
    return risingEdge;
}

// ============================================================================
// ButtonWidget geometry initialisation (call after _width/_height are final)
// ============================================================================

void LCD2Dashboard::initGameButtons() {
    const int16_t btnX = static_cast<int16_t>((_width  - kGameBtnW) / 2);
    const int16_t btnY = static_cast<int16_t>(_height - kGameBtnH - kGameBtnMarginB);

    _btnGames.initButtonUL(btnX, btnY, kGameBtnW, kGameBtnH,
                           TFT_DARKGREY, TFT_BLACK, TFT_LIGHTGREY, "GAMES", 2);

    for (int8_t i = 0; i < kMenuItemCount; ++i) {
        const int16_t itemY = static_cast<int16_t>(
            btnY - 4 - (kMenuItemCount - i) * (kMenuItemH + 2));
        if (i == 0) {
            _btnDino.initButtonUL(btnX, itemY, kGameBtnW, kMenuItemH,
                                  TFT_DARKGREY, TFT_BLACK, TFT_LIGHTGREY, "Dino Runner", 2);
        } else {
            _btnBall.initButtonUL(btnX, itemY, kGameBtnW, kMenuItemH,
                                  TFT_DARKGREY, TFT_BLACK, TFT_LIGHTGREY, "Paddle Ball", 2);
        }
    }

    // EXIT button – top-right corner, shared by both games.
    // Width chosen to fit inside the ball-game right panel (36 px) with a small margin.
    const int16_t exitX = static_cast<int16_t>(_width - kExitBtnOffsetX);
    _btnExit.initButtonUL(exitX, kExitBtnY, kExitBtnW, kExitBtnH,
                          TFT_DARKGREY, TFT_BLACK, TFT_LIGHTGREY, "EXIT", 1);
}

LCD2Dashboard::Rect LCD2Dashboard::makePowerOffSettingsButtonRect() const {
    return {
        static_cast<int16_t>(_width - kSettingsBtnSize - kSettingsBtnMargin),
        kSettingsBtnMargin,
        kSettingsBtnSize,
        kSettingsBtnSize
    };
}

LCD2Dashboard::Rect LCD2Dashboard::makePowerOffSettingsPanelRect() const {
    const Rect buttonRect = makePowerOffSettingsButtonRect();
    const int16_t panelW = kPowerOffSettingsPanelW;
    const int16_t panelX = static_cast<int16_t>(buttonRect.x + ((buttonRect.w - panelW) / 2));
    const int16_t panelY = static_cast<int16_t>(buttonRect.y + buttonRect.h + kPowerOffSettingsTopGap);
    const int16_t panelH = max<int16_t>(24, static_cast<int16_t>(_height - panelY));
    return {panelX, panelY, panelW, panelH};
}

LCD2Dashboard::Rect LCD2Dashboard::makePowerOffSettingsSliderRect(const Rect& panelRect) const {
    const int16_t sliderW = max<int16_t>(16, static_cast<int16_t>(panelRect.w - 4));
    return {
        static_cast<int16_t>(panelRect.x + ((panelRect.w - sliderW) / 2)),
        panelRect.y,
        sliderW,
        panelRect.h
    };
}

void LCD2Dashboard::updatePowerOffSliderFromTouch(const Rect& sliderRect,
                                                  int16_t touchY,
                                                  int16_t& targetValue) {
    const int16_t slotTop = static_cast<int16_t>(sliderRect.y + kPowerOffSliderTopInset);
    const int16_t slotBottom = static_cast<int16_t>(sliderRect.y + sliderRect.h - kPowerOffSliderBottomInset);
    const int16_t clampedY = clampInt16(touchY, slotTop, slotBottom);
    const int16_t span = max<int16_t>(1, slotBottom - slotTop);
    const int32_t numerator = static_cast<int32_t>(slotBottom - clampedY) * 100L;
    targetValue = clampInt16(static_cast<int16_t>((numerator + (span / 2)) / span), 0, 100);
}

void LCD2Dashboard::drawPowerOffSettingsButton(bool active) {
    const Rect button = makePowerOffSettingsButtonRect();
    const uint16_t borderColor = active ? TFT_LIGHTGREY : TFT_DARKGREY;

    _tft.fillRoundRect(button.x, button.y, button.w, button.h, 5, TFT_BLACK);
    _tft.drawRoundRect(button.x, button.y, button.w, button.h, 5, borderColor);
    if (active) {
        _tft.drawRoundRect(static_cast<int16_t>(button.x + 1),
                           static_cast<int16_t>(button.y + 1),
                           static_cast<int16_t>(button.w - 2),
                           static_cast<int16_t>(button.h - 2),
                           4,
                           borderColor);
    }

    const int16_t iconX = static_cast<int16_t>(button.x + ((button.w - kSettingsIconSize) / 2));
    const int16_t iconY = static_cast<int16_t>(button.y + ((button.h - kSettingsIconSize) / 2));
    drawInvertedBitmap(_tft,
                       iconX,
                       iconY,
                       kSettingsIconBitmap,
                       static_cast<uint8_t>(kSettingsIconSize),
                       static_cast<uint8_t>(kSettingsIconSize),
                       TFT_LIGHTGREY);
}

void LCD2Dashboard::drawPowerOffSettingsPanel() {
    const Rect panel = makePowerOffSettingsPanelRect();
    const Rect slider = makePowerOffSettingsSliderRect(panel);
    const bool spriteReady = (_powerOffSettingsPanelSprite.created() &&
                              _powerOffSettingsPanelSprite.width() == panel.w &&
                              _powerOffSettingsPanelSprite.height() == panel.h);

    if (!_powerOffSettingsOpen) {
        if (spriteReady) {
            _powerOffSettingsPanelSprite.fillSprite(TFT_BLACK);
            _powerOffSettingsPanelSprite.pushSprite(panel.x, panel.y);
        } else {
            _tft.fillRect(panel.x, panel.y, panel.w, panel.h, TFT_BLACK);
        }
        return;
    }

    const int16_t value = getRequestedLedBrightnessPercent();
    const int16_t sliderX = static_cast<int16_t>(slider.x - panel.x);
    const int16_t sliderY = static_cast<int16_t>(slider.y - panel.y);
    const int16_t slotTop = static_cast<int16_t>(sliderY + kPowerOffSliderTopInset);
    const int16_t slotBottom = static_cast<int16_t>(sliderY + slider.h - kPowerOffSliderBottomInset);
    const int16_t slotHeight = max<int16_t>(8, slotBottom - slotTop);
    const int16_t slotWidth = 8;
    const int16_t slotX = static_cast<int16_t>(sliderX + ((slider.w - slotWidth) / 2));
    const int16_t knobWidth = max<int16_t>(14, static_cast<int16_t>(slider.w - 2));
    const int16_t knobHeight = 10;
    const int16_t clampedValue = clampInt16(value, 0, 100);
    const int16_t knobCenterY = static_cast<int16_t>(slotBottom - ((clampedValue * slotHeight) / 100));
    const int16_t knobY = clampInt16(static_cast<int16_t>(knobCenterY - (knobHeight / 2)),
                                     static_cast<int16_t>(slotTop - 1),
                                     static_cast<int16_t>(slotBottom - knobHeight + 1));
    const int16_t fillTop = clampInt16(knobCenterY, slotTop, slotBottom);

    if (spriteReady) {
        _powerOffSettingsPanelSprite.fillSprite(TFT_BLACK);
        _powerOffSettingsPanelSprite.drawRoundRect(0, 0, panel.w, panel.h, 6, TFT_DARKGREY);
        _powerOffSettingsPanelSprite.drawRoundRect(static_cast<int16_t>(slotX - 3),
                                                   static_cast<int16_t>(slotTop - 2),
                                                   static_cast<int16_t>(slotWidth + 6),
                                                   static_cast<int16_t>(slotHeight + 4),
                                                   5,
                                                   TFT_DARKGREY);
        _powerOffSettingsPanelSprite.fillRoundRect(slotX, slotTop, slotWidth, slotHeight, 4, TFT_DARKGREY);
        _powerOffSettingsPanelSprite.fillRoundRect(slotX,
                                                   fillTop,
                                                   slotWidth,
                                                   static_cast<int16_t>(slotBottom - fillTop + 1),
                                                   4,
                                                   kLedAccentColor);
        _powerOffSettingsPanelSprite.fillRoundRect(static_cast<int16_t>(sliderX + ((slider.w - knobWidth) / 2)),
                                                   knobY,
                                                   knobWidth,
                                                   knobHeight,
                                                   4,
                                                   kTextColor);
        _powerOffSettingsPanelSprite.drawFastHLine(static_cast<int16_t>(sliderX + 4),
                                                   static_cast<int16_t>(knobY + (knobHeight / 2)),
                                                   static_cast<int16_t>(slider.w - 8),
                                                   kLedAccentColor);
        _powerOffSettingsPanelSprite.pushSprite(panel.x, panel.y);
        return;
    }

    const int16_t screenSlotTop = static_cast<int16_t>(slider.y + kPowerOffSliderTopInset);
    const int16_t screenSlotBottom = static_cast<int16_t>(slider.y + slider.h - kPowerOffSliderBottomInset);
    const int16_t screenSlotHeight = max<int16_t>(8, screenSlotBottom - screenSlotTop);
    const int16_t screenSlotX = static_cast<int16_t>(slider.x + ((slider.w - slotWidth) / 2));
    const int16_t screenKnobCenterY = static_cast<int16_t>(screenSlotBottom - ((clampedValue * screenSlotHeight) / 100));
    const int16_t screenKnobY = clampInt16(static_cast<int16_t>(screenKnobCenterY - (knobHeight / 2)),
                                           static_cast<int16_t>(screenSlotTop - 1),
                                           static_cast<int16_t>(screenSlotBottom - knobHeight + 1));
    const int16_t screenFillTop = clampInt16(screenKnobCenterY, screenSlotTop, screenSlotBottom);

    _tft.fillRoundRect(panel.x, panel.y, panel.w, panel.h, 6, TFT_BLACK);
    _tft.drawRoundRect(panel.x, panel.y, panel.w, panel.h, 6, TFT_DARKGREY);
    _tft.drawRoundRect(static_cast<int16_t>(screenSlotX - 3),
                       static_cast<int16_t>(screenSlotTop - 2),
                       static_cast<int16_t>(slotWidth + 6),
                       static_cast<int16_t>(screenSlotHeight + 4),
                       5,
                       TFT_DARKGREY);
    _tft.fillRoundRect(screenSlotX, screenSlotTop, slotWidth, screenSlotHeight, 4, TFT_DARKGREY);
    _tft.fillRoundRect(screenSlotX,
                       screenFillTop,
                       slotWidth,
                       static_cast<int16_t>(screenSlotBottom - screenFillTop + 1),
                       4,
                       kLedAccentColor);
    _tft.fillRoundRect(static_cast<int16_t>(slider.x + ((slider.w - knobWidth) / 2)),
                       screenKnobY,
                       knobWidth,
                       knobHeight,
                       4,
                       kTextColor);
    _tft.drawFastHLine(static_cast<int16_t>(slider.x + 4),
                       static_cast<int16_t>(screenKnobY + (knobHeight / 2)),
                       static_cast<int16_t>(slider.w - 8),
                       kLedAccentColor);
}

void LCD2Dashboard::drawPowerOffEnvironment() {
    const int16_t envW = _powerOffEnvSprite.created() ? _powerOffEnvSprite.width() : min<int16_t>(180, static_cast<int16_t>(_width - 24));
    const int16_t envH = _powerOffEnvSprite.created() ? _powerOffEnvSprite.height() : 24;
    const int16_t x = static_cast<int16_t>((static_cast<int16_t>(_width) - envW) / 2);
    const int16_t cy = static_cast<int16_t>(_height / 2 - 24);
    const int16_t y = static_cast<int16_t>(cy + 42);
    char envBuf[28] = {0};
    const bool hasTemp = (_boxTemp >= 0.0f);
    const bool hasHumidity = (_boxHumidity >= 0.0f && _boxHumidity <= 100.0f);

    if (hasTemp && hasHumidity) {
        snprintf(envBuf, sizeof(envBuf), "%.0fC  %d%%",
                 static_cast<double>(_boxTemp),
                 static_cast<int>(_boxHumidity + 0.5f));
    } else if (hasTemp) {
        snprintf(envBuf, sizeof(envBuf), "%.0fC", static_cast<double>(_boxTemp));
    } else if (hasHumidity) {
        snprintf(envBuf, sizeof(envBuf), "%d%% RH", static_cast<int>(_boxHumidity + 0.5f));
    }

    if (_powerOffEnvSprite.created()) {
        _powerOffEnvSprite.fillSprite(TFT_BLACK);
        if (envBuf[0] != '\0') {
            _powerOffEnvSprite.setTextColor(TFT_DARKGREY, TFT_BLACK);
            _powerOffEnvSprite.setTextDatum(MC_DATUM);
#ifdef SMOOTH_FONT
            _powerOffEnvSprite.loadFont(NotoSansBold15);
            _powerOffEnvSprite.drawString(envBuf, static_cast<int16_t>(envW / 2), static_cast<int16_t>(envH / 2));
            _powerOffEnvSprite.unloadFont();
#else
            _powerOffEnvSprite.drawString(envBuf, static_cast<int16_t>(envW / 2), static_cast<int16_t>(envH / 2), 2);
#endif
            _powerOffEnvSprite.setTextDatum(TL_DATUM);
        }
        _powerOffEnvSprite.pushSprite(x, y);
        return;
    }

    _tft.fillRect(x, y, envW, envH, TFT_BLACK);
    if (envBuf[0] != '\0') {
        _tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
        _tft.setTextDatum(MC_DATUM);
#ifdef SMOOTH_FONT
        _tft.loadFont(NotoSansBold15);
        _tft.drawString(envBuf, static_cast<int16_t>(_width / 2), static_cast<int16_t>(y + (envH / 2)));
        _tft.unloadFont();
#else
        _tft.drawString(envBuf, static_cast<int16_t>(_width / 2), static_cast<int16_t>(y + (envH / 2)), 2);
#endif
        _tft.setTextDatum(TL_DATUM);
    }
}

// ============================================================================
// POWER_OFF idle screen
// ============================================================================

void LCD2Dashboard::drawPowerOffIdle() {
    _tft.fillScreen(TFT_BLACK);

    const int16_t cx = static_cast<int16_t>(_width / 2);
    const int16_t cy = static_cast<int16_t>(_height / 2 - 24);

    // Title
    _tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
    _tft.setTextDatum(MC_DATUM);
#ifdef SMOOTH_FONT
    _tft.loadFont(NotoSansBold36);
    _tft.drawString("JETSON IS", cx, static_cast<int16_t>(cy - 18));
    _tft.drawString("OFFLINE", cx, static_cast<int16_t>(cy + 22));
    _tft.unloadFont();
#else
    _tft.drawString("JETSON IS", cx, static_cast<int16_t>(cy - 18), 4);
    _tft.drawString("OFFLINE", cx, static_cast<int16_t>(cy + 14), 4);
#endif

    // Thin separator
    _tft.drawFastHLine(static_cast<int16_t>(cx - 54),
                       static_cast<int16_t>(cy + 36),
                       108, TFT_DARKGREY);

    drawPowerOffEnvironment();

    // GAMES button
    const int16_t btnX = static_cast<int16_t>((_width - kGameBtnW) / 2);
    const int16_t btnY = static_cast<int16_t>(_height - kGameBtnH - kGameBtnMarginB);
    drawPowerOffButton(_tft, btnX, btnY, kGameBtnW, kGameBtnH, false, "GAMES", nullptr);

    drawPowerOffSettingsButton(_powerOffSettingsOpen);
    drawPowerOffSettingsPanel();
    drawHumidityWarningBanner(hasHighHumidityAlert(_alertMask));

    _tft.setTextDatum(TL_DATUM);
    _tft.setTextColor(kTextColor, kBackgroundColor);
}

// ============================================================================
// Game menu (dropdown above the GAMES button)
// ============================================================================

void LCD2Dashboard::drawGameMenu() {
    if (!_idleDrawn) {
        drawPowerOffIdle();
        _idleDrawn = true;
    }

    const int16_t cx  = static_cast<int16_t>(_width / 2);
    const int16_t btnY = static_cast<int16_t>(_height - kGameBtnH - kGameBtnMarginB);

    // "GAMES" button drawn in active/inverted state
    const int16_t btnX = static_cast<int16_t>((_width - kGameBtnW) / 2);
    drawPowerOffButton(_tft, btnX, btnY, kGameBtnW, kGameBtnH, true, "GAMES", nullptr);

    // "SELECT GAME" heading above the menu items
    _tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
    _tft.setTextDatum(MC_DATUM);
#ifdef SMOOTH_FONT
    _tft.loadFont(NotoSansBold15);
    _tft.drawString("SELECT GAME", cx,
                    static_cast<int16_t>(btnY - (kMenuItemCount * (kMenuItemH + 2)) - 28));
    _tft.unloadFont();
#else
    _tft.drawString("SELECT GAME", cx,
                    static_cast<int16_t>(btnY - (kMenuItemCount * (kMenuItemH + 2)) - 28), 2);
#endif

    const int16_t dinoY = static_cast<int16_t>(btnY - 4 - (kMenuItemCount - 0) * (kMenuItemH + 2));
    const int16_t ballY = static_cast<int16_t>(btnY - 4 - (kMenuItemCount - 1) * (kMenuItemH + 2));
    drawPowerOffButton(_tft, btnX, dinoY, kGameBtnW, kMenuItemH, false, "Dino", "Running");
    drawPowerOffButton(_tft, btnX, ballY, kGameBtnW, kMenuItemH, false, "Paddle", "Ball");

    if (hasHighHumidityAlert(_alertMask)) {
        drawHumidityWarningBanner(true);
    }

    _tft.setTextDatum(TL_DATUM);
    _tft.setTextColor(kTextColor, kBackgroundColor);
}

void LCD2Dashboard::drawSettingsGameMenu() {
    _tft.fillScreen(kBackgroundColor);
    _tft.drawRoundRect(0, 0, _width, _height, 8, kFrameColor);
    drawSettingsTopBar("Games", true);
    drawSettingsNavButton(makeSettingsButtonRect(0), "Dino", "Running", kCpuLineColor);
    drawSettingsNavButton(makeSettingsButtonRect(1), "Paddle", "Ball", kLedAccentColor);
    _tft.setTextDatum(TL_DATUM);
    _tft.setTextColor(kTextColor, kBackgroundColor);
}

// ============================================================================
// Main POWER_OFF update dispatcher
// ============================================================================

void LCD2Dashboard::updatePowerOff(uint32_t nowMs) {
    if (!_driverReady) {
        return;
    }

    // Sample touch once per call; returns true on new-contact rising edge.
    const bool risingEdge = sampleGameTouch(_gamePrevTouched);
    const bool inDinoGame = (_powerOffMode == PowerOffMode::GAME_DINO);
    const bool inBallGame = (_powerOffMode == PowerOffMode::GAME_BALL);
    const bool inAnyGame = inDinoGame || inBallGame;
    const bool gameFinished = (inDinoGame && _dinoGame.finished) ||
                              (inBallGame && _ballGame.finished);

    if (inAnyGame && gameFinished && risingEdge) {
        const GameOverMenuLayout menu = makeGameOverMenuLayout(_width, _height);
        if (pointInBox(_gameTouchX,
                       _gameTouchY,
                       menu.restartX,
                       menu.restartY,
                       kGameOverBtnW,
                       kGameOverBtnH)) {
            if (inDinoGame) {
                lcd2_game_dino::reset(_dinoGame, _width, _height, nowMs);
            } else {
                lcd2_game_ball::reset(_ballGame, _width, _height);
            }
            _powerOffLastFrameMs = 0;
            return;
        }

        if (pointInBox(_gameTouchX,
                       _gameTouchY,
                       menu.exitX,
                       menu.exitY,
                       kGameOverBtnW,
                       kGameOverBtnH)) {
            deleteGameSprite();
            _tft.fillScreen(TFT_BLACK);
            _powerOffMode = PowerOffMode::IDLE;
            _idleDrawn = false;
            return;
        }
    }

    // --- EXIT zone: top-right corner tap during any game ---
    if ((_powerOffMode == PowerOffMode::GAME_DINO ||
         _powerOffMode == PowerOffMode::GAME_BALL) && risingEdge && !gameFinished) {
        if (_btnExit.contains(_gameTouchX, _gameTouchY)) {
            deleteGameSprite();
            _tft.fillScreen(TFT_BLACK);
            _powerOffMode = PowerOffMode::IDLE;
            _idleDrawn = false;
            return;
        }
        // Jump request for dino game
        if (_powerOffMode == PowerOffMode::GAME_DINO) {
            _dinoGame.jumpRequested = true;
        }
    }

    // --- IDLE ---
    if (_powerOffMode == PowerOffMode::IDLE) {
        if (_powerOffSettingsOpen) {
            const Rect settingsPanelRect = makePowerOffSettingsPanelRect();
            const Rect sliderRect = makePowerOffSettingsSliderRect(settingsPanelRect);
            const Rect sliderTrackingRect = {
                static_cast<int16_t>(sliderRect.x - 10),
                sliderRect.y,
                static_cast<int16_t>(sliderRect.w + 20),
                sliderRect.h
            };

            if (_gameTouched) {
                const bool inSliderTrack = pointInRect(_gameTouchX, _gameTouchY, sliderTrackingRect);
                if (inSliderTrack || _powerOffSettingsTouchActive) {
                    const int16_t previousValue = _ledBrightnessPercent;
                    updatePowerOffSliderFromTouch(sliderRect, filterTouchY(_gameTouchY), _ledBrightnessPercent);
                    _powerOffSettingsTouchActive = inSliderTrack;
                    if (previousValue != _ledBrightnessPercent) {
                        if (_idleDrawn && _powerOffSettingsOpen) {
                            drawPowerOffSettingsButton(true);
                            drawPowerOffSettingsPanel();
                            if (hasHighHumidityAlert(_alertMask)) {
                                drawHumidityWarningBanner(true);
                            }
                        } else {
                            _dirty = true;
                            _powerOffLastFrameMs = 0;
                        }
                    }
                } else {
                    _powerOffSettingsTouchActive = false;
                }
            } else {
                _powerOffSettingsTouchActive = false;
                _touchSampleCount = 0;
            }
        } else {
            _powerOffSettingsTouchActive = false;
            _touchSampleCount = 0;
        }

        if (!_idleDrawn) {
            drawPowerOffIdle();
            _idleDrawn = true;
            _dirty = false;
            _powerOffLastFrameMs = nowMs;
        } else if (_dirty) {
            drawPowerOffEnvironment();
            drawPowerOffSettingsButton(_powerOffSettingsOpen);
            drawPowerOffSettingsPanel();
            drawHumidityWarningBanner(hasHighHumidityAlert(_alertMask));
            _dirty = false;
            _powerOffLastFrameMs = nowMs;
        } else if (nowMs - _powerOffLastFrameMs >= kPowerOffRefreshMs) {
            drawPowerOffEnvironment();
            drawHumidityWarningBanner(hasHighHumidityAlert(_alertMask));
            _powerOffLastFrameMs = nowMs;
        }

        if (risingEdge) {
            const Rect settingsButtonRect = makePowerOffSettingsButtonRect();
            if (pointInRect(_gameTouchX, _gameTouchY, settingsButtonRect)) {
                _powerOffSettingsOpen = !_powerOffSettingsOpen;
                _powerOffSettingsTouchActive = false;
                _touchSampleCount = 0;
                _dirty = true;
                _powerOffLastFrameMs = 0;
                return;
            }

            if (_powerOffSettingsOpen) {
                const Rect settingsPanelRect = makePowerOffSettingsPanelRect();
                if (!pointInRect(_gameTouchX, _gameTouchY, settingsPanelRect) &&
                    !_btnGames.contains(_gameTouchX, _gameTouchY)) {
                    _powerOffSettingsOpen = false;
                    _powerOffSettingsTouchActive = false;
                    _touchSampleCount = 0;
                    _dirty = true;
                    _powerOffLastFrameMs = 0;
                    return;
                }
            }

            if (_btnGames.contains(_gameTouchX, _gameTouchY)) {
                _powerOffSettingsOpen = false;
                _powerOffSettingsTouchActive = false;
                _touchSampleCount = 0;
                _powerOffMode = PowerOffMode::GAME_MENU;
                _gameMenuDrawn = false;
                drawGameMenu();
                _gameMenuDrawn = true;
            }
        }
        return;
    }

    // --- GAME_MENU ---
    if (_powerOffMode == PowerOffMode::GAME_MENU) {
        _powerOffSettingsTouchActive = false;
        _touchSampleCount = 0;

        if (_settingsGameActive) {
            if (_dirty || _settingsGameMode != SettingsGameMode::MENU) {
                _settingsGameMode = SettingsGameMode::MENU;
                drawSettingsGameMenu();
                _dirty = false;
            }

            if (risingEdge) {
                if (pointInRect(_gameTouchX, _gameTouchY, makeSettingsBackButtonRect())) {
                    _powerOffMode = PowerOffMode::IDLE;
                    return;
                }
                if (pointInRect(_gameTouchX, _gameTouchY, makeSettingsButtonRect(0))) {
                    _settingsGameMode = SettingsGameMode::DINO;
                    enterDinoGame();
                    return;
                }
                if (pointInRect(_gameTouchX, _gameTouchY, makeSettingsButtonRect(1))) {
                    _settingsGameMode = SettingsGameMode::BALL;
                    enterBallGame();
                    return;
                }
            }
            return;
        }

        if (!_gameMenuDrawn) {
            drawGameMenu();
            _gameMenuDrawn = true;
            _dirty = false;
        }

        if (risingEdge) {
            bool handled = false;
            if (_btnDino.contains(_gameTouchX, _gameTouchY)) {
                enterDinoGame();
                handled = true;
            } else if (_btnBall.contains(_gameTouchX, _gameTouchY)) {
                enterBallGame();
                handled = true;
            }
            if (!handled) {
                _powerOffMode = PowerOffMode::IDLE;
                _idleDrawn = false;
                _gameMenuDrawn = false;
            }
        }
        return;
    }

    // --- Game frame-rate gate (both games run at kGameFramePeriodMs) ---
    if (nowMs - _powerOffLastFrameMs < kGameFramePeriodMs) {
        return;
    }
    _powerOffLastFrameMs = nowMs;

    if (_powerOffMode == PowerOffMode::GAME_DINO) {
        tickDinoGame(nowMs);
        if (_gameSpriteReady) {
            renderDinoGame();
        }
        return;
    }
    if (_powerOffMode == PowerOffMode::GAME_BALL) {
        tickBallGame(nowMs);
        if (_gameSpriteReady) {
            renderBallGame();
        }
        return;
    }
}

void LCD2Dashboard::enterDinoGame() {
    if (!initGameSprite()) {
        _powerOffMode = PowerOffMode::IDLE;
        _idleDrawn = false;
        return;
    }

    randomSeed(static_cast<unsigned long>(millis()));
    _powerOffMode = PowerOffMode::GAME_DINO;
    _powerOffLastFrameMs = 0;
    lcd2_game_dino::reset(_dinoGame, _width, _height, millis());
}

void LCD2Dashboard::tickDinoGame(uint32_t nowMs) {
    if (_dinoGame.finished) {
        return;
    }

    lcd2_game_dino::tick(_dinoGame, _width, _height, nowMs);
}

void LCD2Dashboard::renderDinoGame() {
    lcd2_game_dino::render(_dinoGame, _gameSprite, _width, _height);
    drawExitButtonOnGameSprite(_gameSprite, _width);
    if (_dinoGame.finished) {
        drawGameOverMenuOnSprite(_gameSprite, _width, _height, _dinoGame.score);
    }
    _gameSprite.setBitmapColor(_dinoGame.isNight ? TFT_WHITE : TFT_BLACK,
                               _dinoGame.isNight ? TFT_BLACK : TFT_WHITE);
    _tft.startWrite();
    _gameSprite.pushSprite(0, 0);
    _tft.endWrite();
    if (hasHighHumidityAlert(_alertMask)) {
        drawHumidityWarningBanner(true);
    }
}

void LCD2Dashboard::enterBallGame() {
    if (!initGameSprite()) {
        _powerOffMode = PowerOffMode::IDLE;
        _idleDrawn = false;
        return;
    }

    randomSeed(static_cast<unsigned long>(millis()));
    _powerOffMode = PowerOffMode::GAME_BALL;
    _powerOffLastFrameMs = 0;
    lcd2_game_ball::reset(_ballGame, _width, _height);
}

void LCD2Dashboard::tickBallGame(uint32_t nowMs) {
    if (_ballGame.finished) {
        return;
    }

    lcd2_game_ball::tick(_ballGame,
                         _width,
                         _height,
                         nowMs,
                         _gameTouched,
                         _gameTouchX,
                         _gameTouchY);
}

void LCD2Dashboard::renderBallGame() {
    lcd2_game_ball::render(_ballGame, _gameSprite, _width, _height);
    drawExitButtonOnGameSprite(_gameSprite, _width);
    if (_ballGame.finished) {
        drawGameOverMenuOnSprite(_gameSprite, _width, _height, _ballGame.score);
    }
    _gameSprite.setBitmapColor(TFT_WHITE, TFT_BLACK);
    _tft.startWrite();
    _gameSprite.pushSprite(0, 0);
    _tft.endWrite();
    if (hasHighHumidityAlert(_alertMask)) {
        drawHumidityWarningBanner(true);
    }
}
