#ifndef LCD_2_H
#define LCD_2_H

#include <Arduino.h>
#include <stdint.h>
#include <TFT_eSPI.h>
#include <TFT_eWidget.h>

#include "lcd2_game_ball.h"
#include "lcd2_game_dino.h"
#include "system_configuration.h"

class LCD2Dashboard {
public:
    using SystemState = jetson_cfg::SystemState;

    struct MetricsFrame {
        int16_t cpuUsage;  // 0-100 %, negative when invalid
        int16_t gpuUsage;  // 0-100 %, negative when invalid
        int16_t ramUsage;  // 0-100 %, negative when invalid
        float cpuTemp;     // deg C, negative when invalid
        float gpuTemp;     // deg C, negative when invalid
        int32_t powerMw;   // mW, negative when invalid
    };

    static constexpr uint16_t DEFAULT_WIDTH = jetson_cfg::kLcd2Width;
    static constexpr uint16_t DEFAULT_HEIGHT = jetson_cfg::kLcd2Height;
    static constexpr uint8_t DEFAULT_ROTATION = jetson_cfg::kLcd2Rotation;
    static constexpr uint16_t HISTORY_POINTS = jetson_cfg::kLcd2GraphPoints;
    static constexpr uint32_t REFRESH_PERIOD_MS = jetson_cfg::kLcd2RefreshPeriodMs;

    LCD2Dashboard();

    void init(uint16_t width = DEFAULT_WIDTH,
              uint16_t height = DEFAULT_HEIGHT,
              uint8_t rotation = DEFAULT_ROTATION);

    void onStateChange(SystemState newState);
    void onAlertChange(uint8_t alertMask);
    void clearMetrics();
    void pushMetrics(const MetricsFrame& frame);
    void pushBootKernelLine(const char* line);
    void setEnvironment(float boxTemp,
                        float boxHumidity,
                        int16_t fanPercent,
                        int16_t ledBrightnessPercent);
    int16_t getRequestedLedBrightnessPercent() const;
    void update(uint32_t nowMs);

private:
    uint16_t _width;
    uint16_t _height;
    uint8_t _rotation;

    SystemState _state;
    bool _driverReady;
    bool _degradedMode;
    bool _visible;
    bool _layoutDrawn;
    bool _dirty;
    bool _hasMetrics;
    uint8_t _alertMask;
    float _boxTemp;
    float _boxHumidity;
    int16_t _fanPercent;
    int16_t _ledBrightnessPercent;
    bool _touchReady;
    bool _touchDown;
    uint32_t _lastTouchScanMs;
    int16_t _touchSamplesY[3];
    uint8_t _touchSampleCount;
    bool _bootLayoutDrawn;
    uint32_t _lastBootRefreshMs;

    static constexpr size_t kBootLogLineMaxLen = 80;
    static constexpr uint8_t kBootLogRingCapacity = 48;
    char _bootLogLines[kBootLogRingCapacity][kBootLogLineMaxLen + 1];
    uint8_t _bootLogHead;
    uint8_t _bootLogCount;

    enum class ActiveControl : uint8_t {
        NONE = 0,
        LED_SLIDER
    };

    // ---- POWER_OFF screen / game sub-states ----------------------------
    enum class PowerOffMode : uint8_t {
        IDLE      = 0,
        GAME_MENU = 1,
        GAME_DINO = 2,
        GAME_BALL = 3,
    };

    // ---- Power-off screen member variables ----------------------------
    PowerOffMode   _powerOffMode;
    uint32_t       _powerOffLastFrameMs;
    bool           _idleDrawn;
    LCD2DinoGameState _dinoGame;
    LCD2BallGameState _ballGame;
    TFT_eSprite    _gameSprite;
    bool           _gameSpriteReady;
    bool           _gamePrevTouched;
    bool           _gameTouched;
    int16_t        _gameTouchX;
    int16_t        _gameTouchY;
    bool           _powerOffSettingsOpen;
    bool           _powerOffSettingsTouchActive;

    ActiveControl _activeControl;

    uint32_t _lastRefreshMs;

    MetricsFrame _latest;
    int16_t _cpuHistory[HISTORY_POINTS];
    int16_t _gpuHistory[HISTORY_POINTS];
    int16_t _ramHistory[HISTORY_POINTS];
    uint16_t _historyWriteIndex;
    uint16_t _historyCount;

    struct Rect {
        int16_t x;
        int16_t y;
        int16_t w;
        int16_t h;
    };

    struct DashboardLayout {
        Rect cpuFrame;
        Rect gpuFrame;
        Rect ramFrame;
        Rect panelFrame;
        Rect cpuPlot;
        Rect gpuPlot;
        Rect ramPlot;
    };

    bool initGameSprite();
    void deleteGameSprite();
    bool sampleGameTouch(bool& prevTouched);
    void initGameButtons();
    void updatePowerOff(uint32_t nowMs);
    void drawPowerOffIdle();
    void drawPowerOffSettingsButton(bool active);
    void drawPowerOffSettingsPanel();
    Rect makePowerOffSettingsButtonRect() const;
    Rect makePowerOffSettingsPanelRect() const;
    Rect makePowerOffSettingsSliderRect(const Rect& panelRect) const;
    void updatePowerOffSliderFromTouch(const Rect& sliderRect, int16_t touchY, int16_t& targetValue);
    void drawGameMenu();
    void enterDinoGame();
    void tickDinoGame(uint32_t nowMs);
    void renderDinoGame();
    void enterBallGame();
    void tickBallGame(uint32_t nowMs);
    void renderBallGame();

    static int16_t clampUsage(int16_t value);
    void resetHistory();
    bool initSprites();
    void deleteSprites();
    bool initBootLogSprite();
    void deleteBootLogSprite();
    bool initTouchCalibration();
    bool loadTouchCalibration(uint16_t* outCalData, size_t count) const;
    bool saveTouchCalibration(const uint16_t* calData, size_t count) const;
    bool runTouchCalibration(uint16_t* calData, size_t count);

    TFT_eSPI _tft;
    ButtonWidget _btnGames;
    ButtonWidget _btnDino;
    ButtonWidget _btnBall;
    ButtonWidget _btnExit;
    TFT_eSprite _cpuSprite;
    TFT_eSprite _gpuSprite;
    TFT_eSprite _ramSprite;
    TFT_eSprite _panelSprite;
    TFT_eSprite _bootLogSprite;
    bool _spritesReady;
    bool _bootSpriteReady;

    DashboardLayout buildLayout() const;
    void drawLayout();
    void drawDynamic();
    void drawNoData();
    void drawBootLogView();
    void handleTouch(uint32_t nowMs);
    void clearBootLog();
    void appendBootLogLine(const char* line);
    void appendBootLogWrapped(const char* line);
    void drawGraphScaffold(const Rect& frame,
                           const Rect& plot,
                           const char* title,
                           uint16_t accentColor);
    void drawGraphHeader(const Rect& frame,
                         const char* title,
                         int16_t latestUsage,
                         uint16_t accentColor);
    void drawUsageGraph(TFT_eSprite& sprite,
                        int16_t w,
                        int16_t h,
                        const int16_t* history,
                        uint16_t lineColor,
                        uint16_t fillColor);
    void drawNumericPanel(TFT_eSprite& sprite, int16_t w, int16_t h);
    void drawMetricCard(TFT_eSprite& sprite,
                        int16_t x,
                        int16_t y,
                        int16_t w,
                        int16_t h,
                        const char* title,
                        const char* value,
                        uint16_t accentColor);
    void drawSliderControl(TFT_eSprite& sprite,
                           const Rect& area,
                           const char* label,
                           int16_t value,
                           uint16_t accentColor);
    Rect makeLedSliderRect(int16_t panelW, int16_t panelH) const;
    bool pointInRect(int16_t x, int16_t y, const Rect& rect) const;
    void updateSliderFromTouch(const Rect& sliderRect, int16_t touchY, int16_t& targetValue);
    int16_t filterTouchY(int16_t touchY);
    int16_t historyValueAt(const int16_t* history, uint16_t orderedIndex) const;
    void drawDegradedModeNotice(uint32_t nowMs, const char* reason);
};

#endif // LCD_2_H
