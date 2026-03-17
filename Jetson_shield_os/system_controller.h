#pragma once

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>
#include <freertos/semphr.h>

#include "system_configuration.h"
#include "fan.h"
#include "jetson_serial.h"
#include "lcd_1.h"
#include "lcd_2.h"
#include "led.h"
#include "sensor.h"

class SystemController {
public:
	SystemController();

	void init();
	void start();
	jetson_cfg::SystemState getState() const;
	uint32_t getQueueDropCount() const;
	uint32_t getRejectedTransitionCount() const;

private:
	struct ControllerMessage {
		jetson_cfg::MessageSource source;
		jetson_cfg::MessageType type;
		uint32_t timestampMs;
		KernelTransitionEvent transition;
		uint8_t alertMask;
		JetsonStatsSnapshot stats;
		float sensorTempC;
		float sensorHumidityPercent;
		bool sensorValid;
		char line[jetson_cfg::kSerial2LineMaxLen + 1];
	};

	Fan _fan;
	Sensor _sensor;
	LEDController _led;
	JetsonSerial _jetson;
	LCD1 _lcd1;
	LCD2Dashboard _lcd2;

	QueueHandle_t _messageQueue;
	TaskHandle_t _taskController;
	TaskHandle_t _taskSerial1;
	TaskHandle_t _taskSerial2;
	TaskHandle_t _taskSensor;
	TaskHandle_t _taskLed;
	TaskHandle_t _taskLcd2;
	SemaphoreHandle_t _lcd2Mutex;
	bool _lcd2StateSyncPending;
	jetson_cfg::SystemState _lcd2PendingState;

	volatile jetson_cfg::SystemState _state;
	bool _tasksStarted;

	JetsonStatsSnapshot _latestStats;
	bool _hasSensorReading;
	float _sensorTempC;
	float _sensorHumidityPercent;
	uint32_t _lastSensorValidMs;
	uint8_t _sensorConsecutiveFailures;
	int16_t _lastLedBrightnessPercent;
	volatile uint32_t _lastSerial2ActivityMs;
	uint32_t _lastSerial1StatsMs;
	uint32_t _lastBootStartMs;
	uint32_t _lastBootCompleteMs;
	uint32_t _lastShutdownIndicatorMs;
	uint32_t _lastPowerOffIndicatorMs;
	uint8_t _currentAlertMask;
	uint32_t _queueDropCount;
	uint32_t _rejectedTransitionCount;
	bool _bootCompletionPending;

	bool _highTemperatureAlert;
	bool _highHumidityAlert;

	bool postMessage(const ControllerMessage& message, bool critical);
	bool discardOldestNonCriticalMessage();
	bool isCriticalMessage(const ControllerMessage& message) const;
	void handleMessage(const ControllerMessage& message);
	bool isValidTransition(KernelTransitionEvent transition) const;
	bool isLikelyValidSerial2Line(const char* line) const;
	bool hasRecentEvidence(uint32_t lastTimestampMs, uint32_t nowMs, uint32_t windowMs) const;
	void reconcileStateFromSerialEvidence(uint32_t nowMs);
	void resetJetsonStats();
	uint8_t buildAlertMask() const;
	void broadcastAlertMask(uint8_t alertMask);
	void handleMetricStaleness(uint32_t nowMs);
	float selectJetsonThermalMetric(bool& hasMetric) const;
	bool hasValidJetsonMetric() const;
	LCD2Dashboard::MetricsFrame makeLcd2MetricsFrame() const;
	void syncLcd2Metrics();
	void syncLcd2Environment();

	void setState(jetson_cfg::SystemState newState, const char* contextText = nullptr);
	void updateAlerts();
	void applyPolicies();
	void updateDisplays(uint32_t nowMs);
	void handleSensorFreshness(uint32_t nowMs);
	void syncPendingLcd2State();
	int16_t readLcd2RequestedLedBrightnessPercent();
	bool tryLockLcd2();
	void unlockLcd2();

	void controllerTaskLoop();
	void serial1TaskLoop();
	void serial2TaskLoop();
	void sensorTaskLoop();
	void ledTaskLoop();
	void lcd2TaskLoop();

	static void taskControllerEntry(void* arg);
	static void taskSerial1Entry(void* arg);
	static void taskSerial2Entry(void* arg);
	static void taskSensorEntry(void* arg);
	static void taskLedEntry(void* arg);
	static void taskLcd2Entry(void* arg);
};
