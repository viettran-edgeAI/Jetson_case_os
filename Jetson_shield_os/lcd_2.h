#ifndef LCD_2_H
#define LCD_2_H

#include <Arduino.h>
#include <stdint.h>
#include <TFT_eSPI.h>
#include <TFT_eWidget.h>

#include "lcd2_game_ball.h"
#include "lcd2_game_dino.h"
#include "system_configuration.h"
#include "virtual_keyboard.h"

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
        int8_t powerModeId; // nvpmodel ID, negative when invalid
        int32_t powerLimitMw; // mW, negative when invalid
        int32_t netDownloadKbps; // KB/s, negative when invalid
        int32_t netUploadKbps;   // KB/s, negative when invalid
        int32_t diskReadKbps;    // KB/s, negative when invalid
        int32_t diskWriteKbps;   // KB/s, negative when invalid
        int32_t swapUsedMb;      // used swap MB, negative when invalid
        int32_t swapTotalMb;     // total swap MB, negative when invalid
        int16_t diskUsage;       // 0-100 %, negative when invalid
        int32_t diskUsedMb;      // used disk MB, negative when invalid
        int32_t diskTotalMb;     // total disk MB, negative when invalid
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
    bool popPendingJetsonCommand(char* outCommand, size_t outSize);
    void processJetsonResponseLine(const char* line);
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

    enum class DashboardPage : uint8_t {
        PERFORMANCE = 0,
        IO = 1
    };

    DashboardPage _dashboardPage;
    bool _dashboardTouchActive;
    int16_t _touchStartX;
    int16_t _touchStartY;
    int16_t _touchLastX;
    int16_t _touchLastY;
    bool _settingsOpen;

    enum class SettingsScreen : uint8_t {
        MAIN = 0,
        WIFI_LIST,
        KEYBOARD,
        IP_VIEW,
        CONNECT_RESULT,
        NETWORK,
        SSH,
        NGROK,
        SYSTEM,
        HEADLESS,
        ABOUT,
        NETWORK_HELP,
        SYSTEM_HELP,
        POWER_MODE,
        CONFIRM
    };

    enum class SettingsConfirmAction : uint8_t {
        NONE = 0,
        SSH_STOP,
        NGROK_STOP,
        HEADLESS_APPLY_NOW,
        HEADLESS_REBOOT,
        MONITOR_RESTART,
        JETSON_REBOOT,
        POWER_MODE_SET_0,
        POWER_MODE_SET_1,
        POWER_MODE_SET_2,
        POWER_MODE_SET_3
    };

    enum class NgrokActionState : uint8_t {
        NONE = 0,
        STARTING,
        STOPPING
    };

    struct WifiEntry {
        char ssid[33];
        char security[17];
        uint8_t signal;
        bool current;
    };

    enum class SettingsGameMode : uint8_t {
        NONE = 0,
        MENU,
        DINO,
        BALL
    };

    static constexpr uint8_t kWifiEntryMax = 6;
    static constexpr size_t kJetsonCommandMaxLen = 128;
    SettingsScreen _settingsScreen;
    bool _settingsDrawn;
    bool _settingsFullRedraw;
    SettingsScreen _lastSettingsScreen;
    WifiEntry _wifiEntries[kWifiEntryMax];
    uint8_t _wifiEntryCount;
    int8_t _selectedWifiIndex;
    bool _wifiScanInProgress;
    bool _wifiConnectInProgress;
    bool _wifiConnectOk;
    uint32_t _wifiScanRequestMs;
    uint32_t _wifiConnectRequestMs;
    uint32_t _ipRequestMs;
    char _wifiStatus[40];
    char _connectStatus[40];
    char _ipInterface[17];
    char _ipSsid[33];
    char _ipAddress[24];
    char _ipStatus[17];
    bool _ipRequestInProgress;
    bool _aboutRequestInProgress;
    bool _sshRequestInProgress;
    bool _ngrokRequestInProgress;
    NgrokActionState _ngrokActionState;
    bool _headlessRequestInProgress;
    bool _powerModeRequestInProgress;
    uint32_t _aboutRequestMs;
    uint32_t _sshRequestMs;
    uint32_t _ngrokRequestMs;
    uint32_t _ngrokPollMs;
    uint32_t _headlessRequestMs;
    uint32_t _powerModeRequestMs;
    char _aboutHostname[33];
    char _aboutVersion[24];
    char _sshStatus[16];
    char _sshEnabled[8];
    char _sshService[33];
    char _ngrokStatus[16];
    char _ngrokEnabled[8];
    char _ngrokEndpoint[64];
    char _ngrokApi[16];
    char _ngrokService[33];
    char _headlessDefault[16];
    char _headlessActive[16];
    char _powerModeLabel[40];
    int8_t _powerModeId;
    int8_t _pendingPowerModeId;
    int32_t _powerModeMaxMw;
    char _systemStatus[48];
    SettingsConfirmAction _confirmAction;
    SettingsScreen _confirmReturnScreen;
    char _confirmTitle[28];
    char _confirmBody[64];
    char _pendingJetsonCommand[kJetsonCommandMaxLen];
    bool _hasPendingJetsonCommand;
    VirtualKeyboard _keyboard;

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
    bool           _settingsGameActive;
    bool           _gameMenuDrawn;
    SettingsGameMode _settingsGameMode;

    ActiveControl _activeControl;

    uint32_t _lastRefreshMs;
    uint32_t _lastMetricsMs;

    MetricsFrame _latest;
    int16_t _cpuHistory[HISTORY_POINTS];
    int16_t _gpuHistory[HISTORY_POINTS];
    int16_t _ramHistory[HISTORY_POINTS];
    int16_t _netHistory[HISTORY_POINTS];
    int16_t _netUploadHistory[HISTORY_POINTS];
    int16_t _diskHistory[HISTORY_POINTS];
    int16_t _diskWriteHistory[HISTORY_POINTS];
    int16_t _swapHistory[HISTORY_POINTS];
    int32_t _netGraphScaleKbps;
    int32_t _diskGraphScaleKbps;
    int32_t _swapGraphScaleMb;
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
    void drawSettingsGameMenu();
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
    bool initDynamicSprites();
    void deleteDynamicSprites();
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
    TFT_eSprite _graphHeaderSprite;
    TFT_eSprite _graphValueSprite;
    TFT_eSprite _settingsCompactRowSprite;
    TFT_eSprite _settingsRowSprite;
    TFT_eSprite _wifiStatusSprite;
    TFT_eSprite _wifiRowSprite;
    TFT_eSprite _keyboardInputSprite;
    TFT_eSprite _powerOffSettingsPanelSprite;
    TFT_eSprite _powerOffEnvSprite;
    TFT_eSprite _humidityBannerSprite;
    TFT_eSprite _bootLogSprite;
    bool _spritesReady;
    bool _dynamicSpritesReady;
    bool _bootSpriteReady;
    bool _humidityBannerVisible;

    DashboardLayout buildLayout() const;
    void drawLayout();
    void drawDynamic(uint32_t nowMs, bool updatePanel);
    void drawNoData();
    void drawBootLogView();
    void handleTouch(uint32_t nowMs);
    void clearBootLog();
    void appendBootLogLine(const char* line);
    void appendBootLogWrapped(const char* line);
    void drawGraphScaffold(const Rect& frame,
                           const Rect& plot,
                           const char* title,
                           const char* valueLabel,
                           uint16_t accentColor);
    void drawGraphDynamicLabels(const Rect& frame,
                                const Rect& plot,
                                const char* title,
                                const char* valueLabel,
                                uint16_t accentColor);
    void drawUsageGraph(TFT_eSprite& sprite,
                        int16_t w,
                        int16_t h,
                        const int16_t* history,
                        uint16_t lineColor,
                        uint16_t scrollPhasePermille = 0);
    void drawDualUsageGraph(TFT_eSprite& sprite,
                            int16_t w,
                            int16_t h,
                            const int16_t* firstHistory,
                            const int16_t* secondHistory,
                            uint16_t firstColor,
                            uint16_t secondColor,
                            uint16_t scrollPhasePermille = 0);
    void drawNumericPanel(TFT_eSprite& sprite, int16_t w, int16_t h);
    void drawPerformancePanel(TFT_eSprite& sprite, int16_t w, int16_t h);
    void drawIoPanel(TFT_eSprite& sprite, int16_t w, int16_t h);
    bool handleDashboardSwipe(int16_t endX, int16_t endY);
    void drawSettingsScreen();
    void drawSettingsMainScreen();
    void drawNetworkScreen();
    void drawSshScreen();
    void drawNgrokScreen();
    void drawSystemScreen();
    void drawHeadlessScreen();
    void drawPowerModeScreen();
    void drawAboutScreen();
    void drawSettingsHelpScreen(const char* title, const char* const* lines, uint8_t lineCount);
    void drawNetworkHelpScreen();
    void drawSystemHelpScreen();
    void drawConfirmScreen();
    void drawWifiListScreen();
    void drawWifiStatusRow(const Rect& statusPanel);
    void drawWifiEmptyMessage(const char* message);
    void drawIpViewScreen();
    void drawConnectResultScreen();
    void drawSettingsPanelButton(TFT_eSprite& sprite, int16_t panelW);
    void drawSettingsTftButton(const Rect& rect, const char* label, uint16_t accentColor, bool pressed = false);
    void drawSettingsHelpButton(bool pressed = false);
    void drawSettingsTopBar(const char* title, bool showBack);
    void drawSettingsValueRow(int16_t y, const char* label, const char* value, uint16_t valueColor = TFT_WHITE);
    void drawSettingsUptimeRow();
    void drawSettingsNavButton(const Rect& rect, const char* label, const char* sublabel, uint16_t accentColor, bool leftAligned = false, bool reverseColors = false, bool pressed = false);
    void drawSettingsHorizontalSlider(const Rect& rect);
    void drawWifiEntryRow(uint8_t index);
    void drawPowerOffEnvironment();
    void drawHumidityWarningBanner(bool visible);
    void handleSettingsTouch(int16_t touchX, int16_t touchY);
    void handleSettingsMainTouch(int16_t touchX, int16_t touchY);
    void handleNetworkTouch(int16_t touchX, int16_t touchY);
    void handleSshTouch(int16_t touchX, int16_t touchY);
    void handleNgrokTouch(int16_t touchX, int16_t touchY);
    void handleSystemTouch(int16_t touchX, int16_t touchY);
    void handlePowerModeTouch(int16_t touchX, int16_t touchY);
    void handleHeadlessTouch(int16_t touchX, int16_t touchY);
    void handleAboutTouch(int16_t touchX, int16_t touchY);
    void handleSettingsHelpTouch(SettingsScreen returnScreen, int16_t touchX, int16_t touchY);
    void handleConfirmTouch(int16_t touchX, int16_t touchY);
    void handleWifiListTouch(int16_t touchX, int16_t touchY);
    void handleKeyboardTouch(int16_t touchX, int16_t touchY);
    void handleConnectResultTouch(int16_t touchX, int16_t touchY);
    void requestWifiScan();
    void requestIpInfo();
    void requestAboutInfo();
    void requestSshStatus();
    void requestNgrokStatus();
    void startNgrokAction(NgrokActionState action);
    void pollNgrokAction(uint32_t nowMs);
    void requestHeadlessStatus();
    void requestPowerModeStatus();
    bool requestSimpleCommand(const char* command, const char* busyStatus, const char* queuedStatus);
    void requestWifiConnect();
    void openConfirm(SettingsConfirmAction action, SettingsScreen returnScreen, const char* title, const char* body);
    void executeConfirmAction();
    void updateSettingsTimeouts(uint32_t nowMs);
    bool queueJetsonCommand(const char* command);
    void resetWifiEntries();
    void parseWifiLine(const char* line);
    void sortWifiEntries();
    void parseIpLine(const char* line);
    void parseConnectLine(const char* line);
    void parseAboutLine(const char* line);
    void parseServiceStatusLine(const char* line, bool ngrok);
    void parseHeadlessLine(const char* line);
    void parsePowerModeLine(const char* line);
    void parseResultLine(const char* line);
    void escapeCommandField(const char* src, char* dst, size_t dstSize) const;
    void unescapeResponseField(const char* src, char* dst, size_t dstSize) const;
    const char* tokenValue(const char* line, const char* key, char* out, size_t outSize) const;
    Rect makeSettingsPanelButtonRect(int16_t panelW) const;
    Rect makeSettingsExitButtonRect() const;
    Rect makeSettingsBackButtonRect() const;
    Rect makeSettingsSliderRect() const;
    Rect makeSettingsWifiButtonRect() const;
    Rect makeSettingsIpButtonRect() const;
    Rect makeSettingsNetworkButtonRect() const;
    Rect makeSettingsSystemButtonRect() const;
    Rect makeSettingsAboutButtonRect() const;
    Rect makeSettingsGamesButtonRect() const;
    Rect makeSettingsButtonRect(uint8_t index, uint8_t columns = 1) const;
    Rect makeSettingsHelpButtonRect() const;
    Rect makeSettingsRefreshButtonRect() const;
    Rect makeSettingsConfirmCancelRect() const;
    Rect makeSettingsConfirmOkRect() const;
    Rect makeWifiEntryRect(uint8_t index) const;
    Rect makeRetryButtonRect() const;
    void updateHorizontalSliderFromTouch(const Rect& sliderRect, int16_t touchX, int16_t& targetValue);
    void formatEspFilesystem(char* out, size_t outSize) const;
    void formatEspHeap(char* out, size_t outSize) const;
    void formatEspPsram(char* out, size_t outSize) const;
    void formatEspUptime(char* out, size_t outSize) const;
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
    bool isSettingsButtonPressed(const Rect& rect) const;
    void updateSliderFromTouch(const Rect& sliderRect, int16_t touchY, int16_t& targetValue);
    int16_t filterTouchY(int16_t touchY);
    int16_t historyValueAt(const int16_t* history, uint16_t orderedIndex) const;
    uint16_t graphScrollPhasePermille(uint32_t nowMs) const;
    void drawDegradedModeNotice(uint32_t nowMs, const char* reason);
};

#endif // LCD_2_H
