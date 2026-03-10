#include "lcd_2.h"

#include <FS.h>
#include <LittleFS.h>

#include <stdio.h>

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
constexpr uint16_t kStatusOkColor = TFT_GREEN;
constexpr uint16_t kStatusAlertColor = TFT_RED;

constexpr int16_t kOuterMargin = 6;
constexpr int16_t kSectionGap = 8;
constexpr int16_t kGraphFooterHeight = 16;
constexpr int16_t kGraphAxisLabelWidth = 20;
constexpr int16_t kGraphInfoWidth = 42;
constexpr uint32_t kTouchScanPeriodMs = 20;
constexpr uint32_t kTouchUiRefreshPeriodMs = 33;
constexpr uint32_t kTouchCalibrationMagic = 0x4A534F53UL;
constexpr uint8_t kTouchCalibrationVersion = 1;

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

void formatHumidityValue(float value, char* buffer, size_t bufferSize) {
    if (!isHumidityValid(value)) {
        snprintf(buffer, bufferSize, "--%%");
        return;
    }

    snprintf(buffer, bufferSize, "%d%%", static_cast<int>(value + 0.5f));
}

int16_t plotYForUsage(int16_t value, int16_t graphHeight) {
    const int16_t topY = 1;
    const int16_t bottomVisibleY = max<int16_t>(topY, graphHeight - 3);
    const int16_t drawableHeight = max<int16_t>(1, bottomVisibleY - topY);
    const int16_t clampedValue = clampInt16(value, 0, 100);
    return bottomVisibleY - ((clampedValue * drawableHeight) / 100);
}

void drawGraphGrid(TFT_eSprite& sprite, int16_t w, int16_t h) {
    if (w < 2 || h < 2) {
        return;
    }

    for (uint8_t step = 1; step < 4; ++step) {
        const int16_t gridY = 1 + ((step * (h - 3)) / 4);
        const int16_t gridX = 1 + ((step * (w - 3)) / 4);
        sprite.drawFastHLine(1, gridY, w - 2, kGridColor);
        sprite.drawFastVLine(gridX, 1, h - 2, kGridColor);
    }
}

void drawFilledSegment(TFT_eSprite& sprite,
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
        sprite.drawFastVLine(x0, topY, bottomY - topY + 1, fillColor);
        return;
    }

    const int32_t dx = static_cast<int32_t>(x1) - static_cast<int32_t>(x0);
    const int32_t dy = static_cast<int32_t>(y1) - static_cast<int32_t>(y0);
    for (int16_t x = x0; x <= x1; ++x) {
        const int32_t step = static_cast<int32_t>(x) - static_cast<int32_t>(x0);
        const int16_t y = static_cast<int16_t>(y0 + ((dy * step) / dx));
        sprite.drawFastVLine(x, y, bottomY - y + 1, fillColor);
    }
}

} // namespace

LCD2Dashboard::LCD2Dashboard()
    : _width(DEFAULT_WIDTH),
      _height(DEFAULT_HEIGHT),
      _rotation(DEFAULT_ROTATION),
      _state(SystemState::POWER_OFF),
      _driverReady(false),
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
            _activeControl(ActiveControl::NONE),
      _lastRefreshMs(0),
            _latest{-1, -1, -1, -1.0f, -1.0f, -1},
      _historyWriteIndex(0),
      _historyCount(0),
            _tft(DEFAULT_WIDTH, DEFAULT_HEIGHT),
      _cpuSprite(&_tft),
            _gpuSprite(&_tft),
      _ramSprite(&_tft),
      _panelSprite(&_tft),
      _spritesReady(false) {
    for (uint16_t i = 0; i < HISTORY_POINTS; ++i) {
        _cpuHistory[i] = -1;
                _gpuHistory[i] = -1;
        _ramHistory[i] = -1;
    }
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

    int16_t panelWidth = screenW / 3;
    if (panelWidth < 96) {
        panelWidth = 96;
    }
    if (panelWidth > 104) {
        panelWidth = 104;
    }

    int16_t graphWidth = screenW - (kOuterMargin * 2) - kSectionGap - panelWidth;
    if (graphWidth < 150) {
        graphWidth = 150;
        panelWidth = screenW - (kOuterMargin * 2) - kSectionGap - graphWidth;
    }
    if (panelWidth < 80) {
        panelWidth = 80;
    }

    const int16_t graphHeight = (screenH - (kOuterMargin * 2) - (kSectionGap * 2)) / 3;
    const int16_t graphX = kOuterMargin;
    const int16_t panelX = graphX + graphWidth + kSectionGap;

    layout.cpuFrame = {graphX, kOuterMargin, graphWidth, graphHeight};
    layout.gpuFrame = {graphX, static_cast<int16_t>(kOuterMargin + graphHeight + kSectionGap), graphWidth, graphHeight};
    layout.ramFrame = {graphX, static_cast<int16_t>(kOuterMargin + ((graphHeight + kSectionGap) * 2)), graphWidth, graphHeight};
    layout.panelFrame = {panelX, kOuterMargin, panelWidth, static_cast<int16_t>(screenH - (kOuterMargin * 2))};

    const int16_t plotWidth = graphWidth - kGraphAxisLabelWidth - kGraphInfoWidth - 6;
    const int16_t plotHeight = graphHeight - kGraphFooterHeight - 4;

    layout.cpuPlot = {static_cast<int16_t>(layout.cpuFrame.x + kGraphAxisLabelWidth + 4),
                      static_cast<int16_t>(layout.cpuFrame.y + 2),
                      plotWidth,
                      plotHeight};
    layout.gpuPlot = {static_cast<int16_t>(layout.gpuFrame.x + kGraphAxisLabelWidth + 4),
                      static_cast<int16_t>(layout.gpuFrame.y + 2),
                      plotWidth,
                      plotHeight};
    layout.ramPlot = {static_cast<int16_t>(layout.ramFrame.x + kGraphAxisLabelWidth + 4),
                      static_cast<int16_t>(layout.ramFrame.y + 2),
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

bool LCD2Dashboard::initTouchCalibration() {
    uint16_t calData[5] = {0};
    bool fileSystemReady = LittleFS.begin();
    if (!fileSystemReady) {
        LittleFS.format();
        fileSystemReady = LittleFS.begin();
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
    _tft.drawString("Touch calibration", 12, 16, 2);
    _tft.drawString("Touch the markers", 12, 44, 2);
    _tft.drawString("at all corners", 12, 64, 2);
    delay(1200);

    _tft.calibrateTouch(calData, TFT_MAGENTA, kBackgroundColor, 15);
    return true;
}

void LCD2Dashboard::init(uint16_t width, uint16_t height, uint8_t rotation) {
    _width = width;
    _height = height;
    _rotation = rotation;

    _tft.init();
    _tft.setRotation(_rotation);
    _width = static_cast<uint16_t>(_tft.width());
    _height = static_cast<uint16_t>(_tft.height());
    _tft.fillScreen(kBackgroundColor);
    _tft.setTextSize(1);
    _tft.setTextFont(1);
    _tft.setTextColor(kTextColor, kBackgroundColor);
    _tft.setTextDatum(TL_DATUM);
    _touchReady = initTouchCalibration();

    _driverReady = initSprites();
    _visible = false;
    _layoutDrawn = false;
    _dirty = true;
    _hasMetrics = false;
    _touchDown = false;
    _touchSampleCount = 0;
    _activeControl = ActiveControl::NONE;
    _lastTouchScanMs = 0;
    _lastRefreshMs = 0;
    resetHistory();
}

void LCD2Dashboard::onStateChange(SystemState newState) {
    if (_state == newState) {
        return;
    }

    const bool wasRunning = (_state == SystemState::RUNNING);
    _state = newState;
    const bool isRunning = (_state == SystemState::RUNNING);

    if (!isRunning) {
        resetHistory();
        _layoutDrawn = false;
        _dirty = true;
        _touchDown = false;
        _touchSampleCount = 0;
        _activeControl = ActiveControl::NONE;
        _lastTouchScanMs = 0;
        if (_driverReady && wasRunning) {
            _tft.fillScreen(kBackgroundColor);
        }
        _visible = false;
        return;
    }

    resetHistory();
    _layoutDrawn = false;
    _dirty = true;
    _visible = false;
    _touchDown = false;
    _touchSampleCount = 0;
    _activeControl = ActiveControl::NONE;
    _lastRefreshMs = 0;
}

void LCD2Dashboard::onAlertChange(uint8_t alertMask) {
    _alertMask = alertMask;
    _dirty = true;
}

void LCD2Dashboard::clearMetrics() {
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
    _dirty = true;
}

int16_t LCD2Dashboard::getRequestedLedBrightnessPercent() const {
    return clampInt16(_ledBrightnessPercent, 0, 100);
}

void LCD2Dashboard::pushMetrics(const MetricsFrame& frame) {
    _latest.cpuUsage = clampUsage(frame.cpuUsage);
    _latest.gpuUsage = clampUsage(frame.gpuUsage);
    _latest.ramUsage = clampUsage(frame.ramUsage);
    _latest.cpuTemp = frame.cpuTemp;
    _latest.gpuTemp = frame.gpuTemp;
    _latest.powerMw = frame.powerMw;

    _cpuHistory[_historyWriteIndex] = _latest.cpuUsage;
    _gpuHistory[_historyWriteIndex] = _latest.gpuUsage;
    _ramHistory[_historyWriteIndex] = _latest.ramUsage;

    _historyWriteIndex = (_historyWriteIndex + 1) % HISTORY_POINTS;
    if (_historyCount < HISTORY_POINTS) {
        ++_historyCount;
    }

    _hasMetrics = isUsageValid(_latest.cpuUsage) ||
                  isUsageValid(_latest.gpuUsage) ||
                  isUsageValid(_latest.ramUsage) ||
                  isTempValid(_latest.cpuTemp) ||
                  isTempValid(_latest.gpuTemp) ||
                  isPowerValid(_latest.powerMw);
    _dirty = true;
}

void LCD2Dashboard::resetHistory() {
    _latest = {-1, -1, -1, -1.0f, -1.0f, -1};
    _historyWriteIndex = 0;
    _historyCount = 0;
    _hasMetrics = false;

    for (uint16_t i = 0; i < HISTORY_POINTS; ++i) {
        _cpuHistory[i] = -1;
        _gpuHistory[i] = -1;
        _ramHistory[i] = -1;
    }
}

void LCD2Dashboard::update(uint32_t nowMs) {
    if (_state != SystemState::RUNNING || !_driverReady) {
        return;
    }

    handleTouch(nowMs);

    if (!_layoutDrawn) {
        drawLayout();
        _layoutDrawn = true;
        _dirty = true;
        _visible = true;
    }

    if (!_dirty) {
        return;
    }

    const uint32_t refreshPeriodMs = (_touchDown || _activeControl != ActiveControl::NONE)
                                         ? kTouchUiRefreshPeriodMs
                                         : REFRESH_PERIOD_MS;
    if (_lastRefreshMs != 0U && (nowMs - _lastRefreshMs) < refreshPeriodMs) {
        return;
    }

    _lastRefreshMs = nowMs;

    if (_hasMetrics) {
        drawDynamic();
    } else {
        drawNoData();
    }

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
    const bool touched = _tft.getTouch(&touchX, &touchY, 250);
    if (!touched) {
        _touchDown = false;
        _touchSampleCount = 0;
        _activeControl = ActiveControl::NONE;
        return;
    }

    const DashboardLayout layout = buildLayout();
    const int16_t panelX = layout.panelFrame.x + 1;
    const int16_t panelY = layout.panelFrame.y + 1;
    const int16_t panelW = layout.panelFrame.w - 2;
    const int16_t panelH = layout.panelFrame.h - 2;
    const int16_t localX = static_cast<int16_t>(touchX) - panelX;
    const int16_t localY = static_cast<int16_t>(touchY) - panelY;

    const Rect panelRect = {0, 0, panelW, panelH};
    if (!pointInRect(localX, localY, panelRect)) {
        _touchDown = false;
        _touchSampleCount = 0;
        _activeControl = ActiveControl::NONE;
        return;
    }

    const Rect ledSliderRect = makeLedSliderRect(panelW, panelH);
    if (_activeControl == ActiveControl::NONE) {
        _touchDown = true;
        if (pointInRect(localX, localY, ledSliderRect)) {
            _activeControl = ActiveControl::LED_SLIDER;
        } else {
            _touchSampleCount = 0;
            return;
        }
    }

    const Rect ledTrackingRect = {static_cast<int16_t>(ledSliderRect.x - 10),
                                  ledSliderRect.y,
                                  static_cast<int16_t>(ledSliderRect.w + 20),
                                  ledSliderRect.h};
    if (!pointInRect(localX, localY, ledTrackingRect)) {
        _touchDown = false;
        _touchSampleCount = 0;
        _activeControl = ActiveControl::NONE;
        return;
    }

    const int16_t previousValue = _ledBrightnessPercent;
    updateSliderFromTouch(ledSliderRect, filterTouchY(localY), _ledBrightnessPercent);

    if (previousValue != _ledBrightnessPercent) {
        _dirty = true;
        _lastRefreshMs = 0;
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

void LCD2Dashboard::updateSliderFromTouch(const Rect& sliderRect,
                                          int16_t touchY,
                                          int16_t& targetValue) {
    const int16_t slotTop = sliderRect.y + 20;
    const int16_t slotBottom = sliderRect.y + sliderRect.h - 26;
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

void LCD2Dashboard::drawGraphScaffold(const Rect& frame,
                                      const Rect& plot,
                                      const char* title,
                                      uint16_t accentColor) {
    _tft.drawRect(frame.x, frame.y, frame.w, frame.h, kFrameColor);
    _tft.drawRect(plot.x - 1, plot.y - 1, plot.w + 2, plot.h + 2, kFrameColor);

    const int16_t infoSeparatorX = frame.x + frame.w - kGraphInfoWidth;
    _tft.drawFastVLine(infoSeparatorX,
                       frame.y + 2,
                       static_cast<int16_t>(frame.h - kGraphFooterHeight - 4),
                       kFrameColor);

    drawGraphHeader(frame, title, -1, accentColor);

    _tft.setTextColor(kMutedTextColor, kBackgroundColor);
    _tft.setTextDatum(TR_DATUM);
    for (int16_t percent = 0; percent <= 100; percent += 25) {
        const int16_t labelY = plot.y + plot.h - 3 - ((percent * (plot.h - 2)) / 100);
        char label[8];
        snprintf(label, sizeof(label), "%d", static_cast<int>(percent));
        _tft.drawString(label, plot.x - 4, labelY, 1);
    }

    uint32_t historySpanSeconds = (HISTORY_POINTS * REFRESH_PERIOD_MS) / 1000U;
    if (historySpanSeconds == 0U) {
        historySpanSeconds = 1U;
    }
    char leftLabel[12];
    char midLabel[12];
    snprintf(leftLabel, sizeof(leftLabel), "%lus", static_cast<unsigned long>(historySpanSeconds));
    snprintf(midLabel, sizeof(midLabel), "%lus", static_cast<unsigned long>(historySpanSeconds / 2U));
    _tft.setTextDatum(TL_DATUM);
    _tft.drawString(leftLabel, plot.x, frame.y + frame.h - kGraphFooterHeight + 4, 1);
    _tft.setTextDatum(MC_DATUM);
    _tft.drawString(midLabel,
                    plot.x + (plot.w / 2),
                    frame.y + frame.h - (kGraphFooterHeight / 2),
                    1);
    _tft.setTextDatum(TR_DATUM);
    _tft.drawString("0s", plot.x + plot.w, frame.y + frame.h - kGraphFooterHeight + 4, 1);
    _tft.setTextDatum(TL_DATUM);
}

void LCD2Dashboard::drawGraphHeader(const Rect& frame,
                                    const char* title,
                                    int16_t latestUsage,
                                    uint16_t accentColor) {
    char valueLabel[12];
    formatUsageValue(latestUsage, valueLabel, sizeof(valueLabel));

    const int16_t infoX = frame.x + frame.w - kGraphInfoWidth + 3;
    const int16_t infoY = frame.y + 6;
    const int16_t infoH = frame.h - kGraphFooterHeight - 10;

    _tft.fillRect(infoX - 2,
                  frame.y + 1,
                  kGraphInfoWidth - 4,
                  frame.h - kGraphFooterHeight - 2,
                  kBackgroundColor);
    _tft.setTextColor(accentColor, kBackgroundColor);
    _tft.setTextDatum(TL_DATUM);
    _tft.drawString(title, infoX, infoY + max<int16_t>((infoH / 2) - 17, 0), 2);

    _tft.setTextColor(isUsageValid(latestUsage) ? kTextColor : kMutedTextColor, kBackgroundColor);
    _tft.drawString(valueLabel, infoX, infoY + max<int16_t>((infoH / 2) + 4, 0), 2);
    _tft.setTextDatum(TL_DATUM);
}

void LCD2Dashboard::drawLayout() {
    const DashboardLayout layout = buildLayout();
    _tft.fillScreen(kBackgroundColor);

    drawGraphScaffold(layout.cpuFrame, layout.cpuPlot, "CPU", kCpuLineColor);
    drawGraphScaffold(layout.gpuFrame, layout.gpuPlot, "GPU", kGpuLineColor);
    drawGraphScaffold(layout.ramFrame, layout.ramPlot, "RAM", kRamLineColor);
    _tft.drawRect(layout.panelFrame.x, layout.panelFrame.y, layout.panelFrame.w, layout.panelFrame.h, kFrameColor);
}

void LCD2Dashboard::drawDynamic() {
    if (!_spritesReady) {
        return;
    }

    const DashboardLayout layout = buildLayout();

    drawGraphHeader(layout.cpuFrame, "CPU", _latest.cpuUsage, kCpuLineColor);
    drawGraphHeader(layout.gpuFrame, "GPU", _latest.gpuUsage, kGpuLineColor);
    drawGraphHeader(layout.ramFrame, "RAM", _latest.ramUsage, kRamLineColor);
    drawUsageGraph(_cpuSprite,
                   layout.cpuPlot.w,
                   layout.cpuPlot.h,
                   _cpuHistory,
                   kCpuLineColor,
                   kCpuFillColor);
    drawUsageGraph(_gpuSprite,
                   layout.gpuPlot.w,
                   layout.gpuPlot.h,
                   _gpuHistory,
                   kGpuLineColor,
                   kGpuFillColor);
    drawUsageGraph(_ramSprite,
                   layout.ramPlot.w,
                   layout.ramPlot.h,
                   _ramHistory,
                   kRamLineColor,
                   kRamFillColor);
    drawNumericPanel(_panelSprite, layout.panelFrame.w - 2, layout.panelFrame.h - 2);

    _tft.startWrite();
    _cpuSprite.pushSprite(layout.cpuPlot.x, layout.cpuPlot.y);
    _gpuSprite.pushSprite(layout.gpuPlot.x, layout.gpuPlot.y);
    _ramSprite.pushSprite(layout.ramPlot.x, layout.ramPlot.y);
    _panelSprite.pushSprite(layout.panelFrame.x + 1, layout.panelFrame.y + 1);
    _tft.endWrite();
}

void LCD2Dashboard::drawNoData() {
    if (!_spritesReady) {
        return;
    }

    const DashboardLayout layout = buildLayout();

    drawGraphHeader(layout.cpuFrame, "CPU", -1, kCpuLineColor);
    drawGraphHeader(layout.gpuFrame, "GPU", -1, kGpuLineColor);
    drawGraphHeader(layout.ramFrame, "RAM", -1, kRamLineColor);

    _cpuSprite.fillSprite(kBackgroundColor);
    _gpuSprite.fillSprite(kBackgroundColor);
    _ramSprite.fillSprite(kBackgroundColor);
    _panelSprite.fillSprite(kBackgroundColor);

    _cpuSprite.drawRect(0, 0, layout.cpuPlot.w, layout.cpuPlot.h, kFrameColor);
    _gpuSprite.drawRect(0, 0, layout.gpuPlot.w, layout.gpuPlot.h, kFrameColor);
    _ramSprite.drawRect(0, 0, layout.ramPlot.w, layout.ramPlot.h, kFrameColor);

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

void LCD2Dashboard::drawUsageGraph(TFT_eSprite& sprite,
                                   int16_t w,
                                   int16_t h,
                                   const int16_t* history,
                                   uint16_t lineColor,
                                   uint16_t fillColor) {
    sprite.fillSprite(kBackgroundColor);

    if (w < 2 || h < 2) {
        return;
    }

    sprite.drawRect(0, 0, w, h, kFrameColor);

    if (_historyCount == 0 || history == nullptr) {
        drawGraphGrid(sprite, w, h);
        return;
    }

    uint16_t sampleCount = _historyCount;
    if (sampleCount > HISTORY_POINTS) {
        sampleCount = HISTORY_POINTS;
    }

    if (sampleCount == 0) {
        return;
    }

    const uint16_t startOrderedIndex = _historyCount - sampleCount;
    const int16_t leftX = 1;
    const int16_t rightX = w - 2;
    const int16_t bottomY = h - 2;
    const int16_t drawableWidth = (rightX > leftX) ? (rightX - leftX) : 1;

    bool hasPreviousPoint = false;
    int16_t prevX = leftX;
    int16_t prevY = bottomY;

    for (uint16_t i = 0; i < sampleCount; ++i) {
        const int16_t value = historyValueAt(history, startOrderedIndex + i);
        if (!isUsageValid(value)) {
            hasPreviousPoint = false;
            continue;
        }

        const int16_t px = (sampleCount > 1)
                               ? static_cast<int16_t>(leftX + ((i * drawableWidth) / (sampleCount - 1)))
                               : leftX;
        const int16_t py = plotYForUsage(value, h);

        if (!hasPreviousPoint) {
            sprite.drawFastVLine(px, py, bottomY - py + 1, fillColor);
        }

        if (hasPreviousPoint) {
            drawFilledSegment(sprite, prevX, prevY, px, py, bottomY, fillColor);
        }

        prevX = px;
        prevY = py;
        hasPreviousPoint = true;
    }

    drawGraphGrid(sprite, w, h);

    hasPreviousPoint = false;
    prevX = leftX;
    prevY = bottomY;
    for (uint16_t i = 0; i < sampleCount; ++i) {
        const int16_t value = historyValueAt(history, startOrderedIndex + i);
        if (!isUsageValid(value)) {
            hasPreviousPoint = false;
            continue;
        }

        const int16_t px = (sampleCount > 1)
                               ? static_cast<int16_t>(leftX + ((i * drawableWidth) / (sampleCount - 1)))
                               : leftX;
        const int16_t py = plotYForUsage(value, h);
        if (hasPreviousPoint) {
            sprite.drawLine(prevX, prevY, px, py, lineColor);
        }
        sprite.drawPixel(px, py, lineColor);

        prevX = px;
        prevY = py;
        hasPreviousPoint = true;
    }

    sprite.drawRect(0, 0, w, h, kFrameColor);
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

    sprite.drawRect(x, y, w, h, kFrameColor);
    sprite.fillRect(x + 1, y + 1, 3, h - 2, accentColor);
    sprite.setTextColor(accentColor, kBackgroundColor);
    sprite.setTextDatum(TL_DATUM);
    sprite.drawString(title, x + 8, y + 3, 1);
    sprite.setTextColor(kTextColor, kBackgroundColor);
    sprite.setTextDatum(TR_DATUM);
    sprite.drawString(value, x + w - 5, y + 15, 2);
    sprite.setTextDatum(TL_DATUM);
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

    sprite.drawRect(area.x, area.y, area.w, area.h, kFrameColor);
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
    char cpuTempLabel[16];
    char gpuTempLabel[16];
    char powerLabel[16];
    char fanLabel[12];
    char humidityLabel[12];
    char boxTempLabel[16];

    formatTempValue(_latest.cpuTemp, cpuTempLabel, sizeof(cpuTempLabel));
    formatTempValue(_latest.gpuTemp, gpuTempLabel, sizeof(gpuTempLabel));
    formatPowerValue(_latest.powerMw, powerLabel, sizeof(powerLabel));
    formatUsageValue(_fanPercent, fanLabel, sizeof(fanLabel));
    formatHumidityValue(_boxHumidity, humidityLabel, sizeof(humidityLabel));
    formatTempValue(_boxTemp, boxTempLabel, sizeof(boxTempLabel));

    const Rect ledSliderRect = makeLedSliderRect(w, h);
    const int16_t contentLeft = 4;
    const int16_t contentRight = ledSliderRect.x - 4;
    const int16_t contentWidth = max<int16_t>(24, contentRight - contentLeft);

    sprite.fillSprite(kBackgroundColor);
    sprite.setTextColor(kTextColor, kBackgroundColor);
    sprite.drawRect(0, 0, w, h, kFrameColor);

    sprite.setTextColor(kTextColor, kBackgroundColor);
    sprite.setTextDatum(TL_DATUM);
    sprite.drawString("Jetson", 6, 4, 2);
    sprite.fillCircle(60, 12, 4, alertColor(_alertMask));
    sprite.setTextDatum(TL_DATUM);
    sprite.setTextColor(kMutedTextColor, kBackgroundColor);
    sprite.drawString(stateName(_state), 6, 22, 1);
    sprite.drawFastHLine(0, 34, w, kFrameColor);

    drawMetricCard(sprite, contentLeft, 40, contentWidth, 42, "CPU TEMP", cpuTempLabel, kCpuLineColor);
    drawMetricCard(sprite, contentLeft, 88, contentWidth, 42, "GPU TEMP", gpuTempLabel, kGpuAccentColor);
    drawMetricCard(sprite, contentLeft, 136, contentWidth, 44, "POWER", powerLabel, kPowerAccentColor);

    sprite.drawFastHLine(contentLeft, h - 46, contentWidth, kFrameColor);
    sprite.setTextColor(kMutedTextColor, kBackgroundColor);
    sprite.drawString("FAN", contentLeft + 2, h - 40, 1);
    sprite.drawString("HUM", contentLeft + 2, h - 26, 1);
    sprite.drawString("TEMP", contentLeft + 2, h - 12, 1);
    sprite.setTextColor(kTextColor, kBackgroundColor);
    sprite.setTextDatum(TR_DATUM);
    sprite.drawString(fanLabel, contentLeft + contentWidth - 4, h - 40, 1);
    sprite.drawString(humidityLabel, contentLeft + contentWidth - 4, h - 26, 1);
    sprite.drawString(boxTempLabel, contentLeft + contentWidth - 4, h - 12, 1);
    sprite.setTextDatum(TL_DATUM);

    drawSliderControl(sprite, ledSliderRect, "LED", getRequestedLedBrightnessPercent(), kLedAccentColor);
}
