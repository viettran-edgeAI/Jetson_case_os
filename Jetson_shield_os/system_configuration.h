#pragma once

#include <Arduino.h>
#include <DHT.h>
#include <stdint.h>

namespace jetson_cfg {

enum class SystemState : uint8_t {
	POWER_OFF = 0,
	BOOTING_ON,
	RUNNING,
	SHUTTING_DOWN
};

enum class AlertType : uint8_t {
	NONE = 0,
	HIGH_TEMPERATURE = 1,
	HIGH_HUMIDITY = 2
};

enum class MessageSource : uint8_t {
	SERIAL1 = 0,
	SERIAL2,
	SENSOR,
	CONTROLLER
};

enum class MessageType : uint8_t {
	JETSON_STATS = 0,
	KERNEL_LOG,
	SENSOR_READING,
	STATE_EVENT,
	ALERT_EVENT
};

static constexpr uint8_t kAlertMaskHighTemperature = 0x01;
static constexpr uint8_t kAlertMaskHighHumidity = 0x02;

// ---------------------------- UART / Serial ----------------------------
static constexpr uint32_t kSerialDebugBaud = 115200;
static constexpr uint32_t kSerial1Baud = 115200;
static constexpr uint32_t kSerial2Baud = 115200;

static constexpr int kSerial1RxPin = 25;
static constexpr int kSerial1TxPin = 32;
static constexpr int kSerial2RxPin = 16;
static constexpr int kSerial2TxPin = 17;

static constexpr size_t kSerial1LineMaxLen = 128;
static constexpr size_t kSerial2LineMaxLen = 192;
static constexpr uint32_t kSerialPollDelayMs = 10;
static constexpr uint32_t kSerialIdleDelayMs = 30;
static constexpr uint32_t kSerial2ProbeIntervalMs = 50;
static constexpr uint32_t kSerial2TransitionDebounceMs = 50;
static constexpr uint32_t kSerial2EvidenceWindowMs = 2500;
static constexpr uint32_t kSerial2ShutdownQuietMs = 1500;
static constexpr uint32_t kSerial2LogThrottleMs = 50;
static constexpr uint32_t kSerial2TransitionLogThrottleMs = 8;
static constexpr uint32_t kSerial1StaleMs = 2500;
static constexpr size_t kSerialNoiseMinLineLength = 6;
static constexpr uint8_t kSerialNoiseMinAlphaChars = 2;

static constexpr const char* kTokenBootStart = "Boot-mode";
static constexpr const char* kTokenBootComplete = "login:";
static constexpr const char* kTokenShutdown = "Shutdown";
static constexpr const char* kTokenSuspend = "Suspend";
static constexpr const char* kTokenPowerOff = "ivc channel driver missing";
static constexpr const char* kTokenPowerOffAlt = "Shutdown state requested";

// ---------------------------- Message Queue ----------------------------
static constexpr size_t kMessageQueueLength = 24;
static constexpr uint32_t kControllerTickMs = 50;
static constexpr uint32_t kControllerPowerOffTickMs = 16;

// ---------------------------- Fan Policy -------------------------------
static constexpr int kFanPwmPin = 26;
static constexpr int kFanTachPin = 27;
static constexpr uint32_t kFanPwmFrequencyHz = 25000;

static constexpr float kTempLowC = 40.0f;
static constexpr float kTempHighC = 85.0f;
static constexpr float kTempFanOffHysteresisC = 37.0f;
static constexpr uint32_t kFanMinOnDwellMs = 3000;
static constexpr uint32_t kFanMinOffDwellMs = 2000;

static constexpr uint8_t kFanDutyMinPercent = 20;
static constexpr uint8_t kFanDutyMaxPercent = 100;

// ---------------------------- LED Policy -------------------------------
static constexpr uint8_t kLedPin = 14;
static constexpr uint16_t kLedCount = 8;
static constexpr uint8_t kLedBrightness = 51;

static constexpr uint32_t kLedChangeIntervalMs = 10000;
static constexpr uint32_t kLedTransitionTimeMs = 1500;
static constexpr uint32_t kLedPaletteIntervalMs = 80000;
static constexpr uint32_t kLedBlinkTransitionMs = 120;
static constexpr uint8_t kLedPowerSignalBlinkCount = 2;
static constexpr uint32_t kLedUpdatePeriodMs = 20;

// ---------------------------- Sensor Policy ----------------------------
static constexpr uint8_t kDhtPin = 13;
static constexpr uint8_t kDhtType = DHT11;

static constexpr uint32_t kSensorSamplePeriodMs = 2000;
static constexpr uint32_t kSensorFreshnessTimeoutMs = 15000;
static constexpr uint8_t kSensorMaxConsecutiveFailures = 5;
static constexpr float kHumidityHighPercent = 88.0f;	// Threshold to trigger high humidity alert
static constexpr float kHumidityRecoverPercent = 85.0f; 	// Threshold to clear high humidity alert

// ---------------------------- LCD_1 (OLED) ----------------------------
static constexpr uint16_t kLcd1Width = 128;
static constexpr uint16_t kLcd1Height = 32;
static constexpr int kLcd1ResetPin = -1;
static constexpr uint8_t kLcd1PrimaryI2CAddress = 0x3C;

static constexpr size_t kEepromSize = 32;
static constexpr size_t kStatusAddress = 0;
static constexpr uint8_t kRainDropCount = 10;

// ---------------------------- LCD_2 (TFT) -----------------------------
static constexpr uint16_t kLcd2Width = 320;
static constexpr uint16_t kLcd2Height = 240;
static constexpr uint8_t kLcd2Rotation = 2; // vertical 
static constexpr uint16_t kLcd2GraphPoints = 60;
static constexpr uint32_t kLcd2RefreshPeriodMs = 1000;
static constexpr uint32_t kLcd2MutexTimeoutMs = 4;
static constexpr bool kLcd2ForceTouchCalibration = false;
static constexpr const char* kLcd2TouchCalibrationFile = "/jsos_touch_cal.bin";

// ---------------------------- Runtime Tasks ----------------------------
static constexpr uint8_t kTaskPriorityController = 4;
static constexpr uint8_t kTaskPrioritySerial2 = 3;
static constexpr uint8_t kTaskPrioritySerial1 = 3;
static constexpr uint8_t kTaskPrioritySensor = 2;
static constexpr uint8_t kTaskPriorityLed = 1;

static constexpr uint16_t kTaskStackController = 4096;
static constexpr uint16_t kTaskStackSerial2 = 3072;
static constexpr uint16_t kTaskStackSerial1 = 3072;
static constexpr uint16_t kTaskStackSensor = 2048;
static constexpr uint16_t kTaskStackLed = 2048;

// Dedicated LCD2 rendering task (Core 0, highest priority for smooth 60fps game loop)
static constexpr uint8_t  kTaskPriorityLcd2 = 5;
static constexpr uint16_t kTaskStackLcd2    = 6144;

} // namespace jetson_cfg

// --------------------- Legacy compatibility aliases --------------------
#define RX1 jetson_cfg::kSerial1RxPin
#define TX1 jetson_cfg::kSerial1TxPin
#define RX2 jetson_cfg::kSerial2RxPin
#define TX2 jetson_cfg::kSerial2TxPin

#define FAN_PWM_PIN jetson_cfg::kFanPwmPin
#define FAN_TACH_PIN jetson_cfg::kFanTachPin
#define TEMP_LOW jetson_cfg::kTempLowC
#define TEMP_HIGH jetson_cfg::kTempHighC

#define LED_PIN jetson_cfg::kLedPin
#define NUM_LEDS jetson_cfg::kLedCount
#define BRIGHTNESS jetson_cfg::kLedBrightness
#define CHANGE_INTERVAL jetson_cfg::kLedChangeIntervalMs
#define TRANSITION_TIME jetson_cfg::kLedTransitionTimeMs
#define PALETTE_INTERVAL jetson_cfg::kLedPaletteIntervalMs

#define DHTPIN jetson_cfg::kDhtPin

#define LCD1_WIDTH jetson_cfg::kLcd1Width
#define LCD1_HEIGHT jetson_cfg::kLcd1Height
#define OLED_RESET jetson_cfg::kLcd1ResetPin
#define PRIMARY_I2C_ADDRESS jetson_cfg::kLcd1PrimaryI2CAddress
#define EEPROM_SIZE jetson_cfg::kEepromSize
#define STATUS_ADDRESS jetson_cfg::kStatusAddress
#define NUM_RAIN_DROPS jetson_cfg::kRainDropCount

#define LCD2_WIDTH jetson_cfg::kLcd2Width
#define LCD2_HEIGHT jetson_cfg::kLcd2Height
#define SCREEN_ROTATION jetson_cfg::kLcd2Rotation
