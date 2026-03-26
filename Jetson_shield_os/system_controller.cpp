#include "system_controller.h"

#include <ctype.h>
#include <math.h>
#include <string.h>

namespace {

constexpr TickType_t kSerialPollDelayTicks = pdMS_TO_TICKS(jetson_cfg::kSerialPollDelayMs);
constexpr TickType_t kSerialIdleDelayTicks = pdMS_TO_TICKS(jetson_cfg::kSerialIdleDelayMs);
constexpr TickType_t kLedTaskDelayTicks = pdMS_TO_TICKS(jetson_cfg::kLedUpdatePeriodMs);
constexpr TickType_t kLcd2MutexTimeoutTicks = pdMS_TO_TICKS(jetson_cfg::kLcd2MutexTimeoutMs);

constexpr BaseType_t kIoCore = 0;
constexpr BaseType_t kControlCore = 1;

TickType_t controllerQueueWaitTicks(jetson_cfg::SystemState state) {
    const uint32_t tickMs = (state == jetson_cfg::SystemState::POWER_OFF)
                                ? jetson_cfg::kControllerPowerOffTickMs
                                : jetson_cfg::kControllerTickMs;
    return pdMS_TO_TICKS(tickMs);
}

void safeCopyLine(char* dst, size_t dstSize, const char* src) {
    if (dst == nullptr || dstSize == 0) {
        return;
    }

    if (src == nullptr) {
        dst[0] = '\0';
        return;
    }

    strncpy(dst, src, dstSize - 1);
    dst[dstSize - 1] = '\0';
}

bool containsAlphaChars(const char* line, uint8_t minAlphaChars) {
    if (line == nullptr) {
        return false;
    }

    uint8_t alphaCount = 0;
    for (size_t i = 0; line[i] != '\0'; ++i) {
        if (isalpha(static_cast<unsigned char>(line[i])) != 0) {
            ++alphaCount;
            if (alphaCount >= minAlphaChars) {
                return true;
            }
        }
    }

    return false;
}

} // namespace

SystemController::SystemController()
    : _fan(jetson_cfg::kFanPwmPin),
      _sensor(jetson_cfg::kDhtPin, jetson_cfg::kDhtType),
      _led(jetson_cfg::kLedPin,
           jetson_cfg::kLedCount,
           jetson_cfg::kLedBrightness,
           jetson_cfg::kLedChangeIntervalMs,
           jetson_cfg::kLedTransitionTimeMs,
           jetson_cfg::kLedPaletteIntervalMs),
      _messageQueue(nullptr),
      _taskController(nullptr),
      _taskSerial1(nullptr),
      _taskSerial2(nullptr),
      _taskSensor(nullptr),
      _taskLed(nullptr),
        _taskLcd2(nullptr),
        _lcd2Mutex(nullptr),
            _lcd2StateSyncPending(false),
            _lcd2PendingState(jetson_cfg::SystemState::POWER_OFF),
      _state(jetson_cfg::SystemState::POWER_OFF),
      _tasksStarted(false),
      _latestStats(),
      _hasSensorReading(false),
      _sensorTempC(NAN),
      _sensorHumidityPercent(NAN),
    _lastSensorValidMs(0),
    _sensorConsecutiveFailures(0),
    _lastLedBrightnessPercent(map(jetson_cfg::kLedBrightness, 0, 255, 0, 100)),
            _lastSerial2ActivityMs(0),
            _lastSerial1StatsMs(0),
            _lastBootStartMs(0),
            _lastBootCompleteMs(0),
            _lastShutdownIndicatorMs(0),
            _lastPowerOffIndicatorMs(0),
            _currentAlertMask(0),
            _queueDropCount(0),
            _rejectedTransitionCount(0),
            _bootCompletionPending(false),
      _highTemperatureAlert(false),
      _highHumidityAlert(false) {
}

void SystemController::init() {
    Serial.begin(jetson_cfg::kSerialDebugBaud);

    _fan.init(jetson_cfg::kFanPwmPin, jetson_cfg::kFanPwmFrequencyHz, 0.0f);
    _sensor.init(jetson_cfg::kDhtPin, jetson_cfg::kDhtType);
    _led.init();
    _jetson.init(jetson_cfg::kSerial1Baud,
                 jetson_cfg::kSerial2Baud,
                 jetson_cfg::kSerial1RxPin,
                 jetson_cfg::kSerial1TxPin,
                 jetson_cfg::kSerial2RxPin,
                 jetson_cfg::kSerial2TxPin);

    _lcd1.init(jetson_cfg::kLcd1Width,
               jetson_cfg::kLcd1Height,
               jetson_cfg::kLcd1PrimaryI2CAddress,
               jetson_cfg::kLcd1ResetPin);
    _lcd2.init(jetson_cfg::kLcd2Width,
               jetson_cfg::kLcd2Height,
               jetson_cfg::kLcd2Rotation);

    _messageQueue = xQueueCreate(jetson_cfg::kMessageQueueLength, sizeof(ControllerMessage));

    _lcd2Mutex = xSemaphoreCreateMutex();

    _latestStats.ramPercent = -1;
    _latestStats.cpuPercent = -1;
    _latestStats.gpuPercent = -1;
    _latestStats.cpuTempC = -1.0f;
    _latestStats.gpuTempC = -1.0f;
    _latestStats.powerMilliWatt = -1;

    _lastSerial2ActivityMs = 0;
    _lastSerial1StatsMs = 0;
    _lastSensorValidMs = 0;
    _sensorConsecutiveFailures = 0;
    _lcd2StateSyncPending = false;
    _lcd2PendingState = jetson_cfg::SystemState::POWER_OFF;
    _lastBootStartMs = 0;
    _lastBootCompleteMs = 0;
    _lastShutdownIndicatorMs = 0;
    _lastPowerOffIndicatorMs = 0;
    _bootCompletionPending = false;

    setState(jetson_cfg::SystemState::POWER_OFF, nullptr);
    updateAlerts();
    applyPolicies();
    updateDisplays(millis());
}

void SystemController::start() {
    if (_tasksStarted || _messageQueue == nullptr) {
        return;
    }

    xTaskCreatePinnedToCore(taskControllerEntry,
                            "ctrl_task",
                            jetson_cfg::kTaskStackController,
                            this,
                            jetson_cfg::kTaskPriorityController,
                            &_taskController,
                            kControlCore);

    xTaskCreatePinnedToCore(taskSerial1Entry,
                            "serial1_task",
                            jetson_cfg::kTaskStackSerial1,
                            this,
                            jetson_cfg::kTaskPrioritySerial1,
                            &_taskSerial1,
                            kIoCore);

    xTaskCreatePinnedToCore(taskSerial2Entry,
                            "serial2_task",
                            jetson_cfg::kTaskStackSerial2,
                            this,
                            jetson_cfg::kTaskPrioritySerial2,
                            &_taskSerial2,
                            kIoCore);

    xTaskCreatePinnedToCore(taskSensorEntry,
                            "sensor_task",
                            jetson_cfg::kTaskStackSensor,
                            this,
                            jetson_cfg::kTaskPrioritySensor,
                            &_taskSensor,
                            kIoCore);

    xTaskCreatePinnedToCore(taskLedEntry,
                            "led_task",
                            jetson_cfg::kTaskStackLed,
                            this,
                            jetson_cfg::kTaskPriorityLed,
                            &_taskLed,
                            kControlCore);

        // Dedicated LCD2 rendering task on Core 0 for smooth 60fps game loop
        xTaskCreatePinnedToCore(taskLcd2Entry,
                                "lcd2_task",
                                jetson_cfg::kTaskStackLcd2,
                                this,
                                jetson_cfg::kTaskPriorityLcd2,
                                &_taskLcd2,
                                kIoCore);

    _tasksStarted = true;
}

jetson_cfg::SystemState SystemController::getState() const {
    return _state;
}

uint32_t SystemController::getQueueDropCount() const {
    return _queueDropCount;
}

uint32_t SystemController::getRejectedTransitionCount() const {
    return _rejectedTransitionCount;
}

bool SystemController::isCriticalMessage(const ControllerMessage& message) const {
    return (message.type == jetson_cfg::MessageType::STATE_EVENT ||
            message.type == jetson_cfg::MessageType::ALERT_EVENT);
}

bool SystemController::discardOldestNonCriticalMessage() {
    if (_messageQueue == nullptr) {
        return false;
    }

    const UBaseType_t queuedCount = uxQueueMessagesWaiting(_messageQueue);
    if (queuedCount == 0) {
        return false;
    }

    static ControllerMessage staged[jetson_cfg::kMessageQueueLength];
    UBaseType_t keptCount = 0;
    bool dropped = false;

    for (UBaseType_t i = 0; i < queuedCount; ++i) {
        ControllerMessage current = {};
        if (xQueueReceive(_messageQueue, &current, 0) != pdTRUE) {
            break;
        }

        if (!dropped && !isCriticalMessage(current)) {
            dropped = true;
            ++_queueDropCount;
            continue;
        }

        if (keptCount < jetson_cfg::kMessageQueueLength) {
            staged[keptCount++] = current;
        }
    }

    for (UBaseType_t i = 0; i < keptCount; ++i) {
        xQueueSend(_messageQueue, &staged[i], 0);
    }

    return dropped;
}

bool SystemController::postMessage(const ControllerMessage& message, bool critical) {
    if (_messageQueue == nullptr) {
        return false;
    }

    if (xQueueSend(_messageQueue, &message, 0) == pdTRUE) {
        return true;
    }

    if (!discardOldestNonCriticalMessage()) {
        return false;
    }

    return (xQueueSend(_messageQueue, &message, 0) == pdTRUE);
}

void SystemController::handleMessage(const ControllerMessage& message) {
    switch (message.type) {
        case jetson_cfg::MessageType::JETSON_STATS: {
            _latestStats = message.stats;
            _lastSerial1StatsMs = message.timestampMs;

            if (tryLockLcd2()) {
                _lcd2.pushMetrics(makeLcd2MetricsFrame());
                unlockLcd2();
            }

            reconcileStateFromSerialEvidence(message.timestampMs);
            if (_state == jetson_cfg::SystemState::RUNNING && _bootCompletionPending) {
                _bootCompletionPending = false;
            }
            break;
        }

        case jetson_cfg::MessageType::KERNEL_LOG: {
            _lastSerial2ActivityMs = message.timestampMs;
            reconcileStateFromSerialEvidence(message.timestampMs);
            if (message.line[0] != '\0') {
                _lcd1.showKernelLine(message.line);
                if (tryLockLcd2()) {
                    _lcd2.pushBootKernelLine(message.line);
                    unlockLcd2();
                }
            }
            break;
        }

        case jetson_cfg::MessageType::SENSOR_READING: {
            if (message.sensorValid) {
                _sensorTempC = message.sensorTempC;
                _sensorHumidityPercent = message.sensorHumidityPercent;
                _hasSensorReading = true;
                _sensorConsecutiveFailures = 0;
                _lastSensorValidMs = message.timestampMs;
                syncLcd2Environment();
            } else if (_sensorConsecutiveFailures < 255U) {
                ++_sensorConsecutiveFailures;
            }
            break;
        }

        case jetson_cfg::MessageType::STATE_EVENT: {
            if (!isValidTransition(message.transition)) {
                ++_rejectedTransitionCount;
                break;
            }

            _lastSerial2ActivityMs = message.timestampMs;
            if (message.line[0] != '\0') {
                _lcd1.showKernelLine(message.line);
            }

            switch (message.transition) {
                case KernelTransitionEvent::BOOT_START:
                    _lastBootStartMs = message.timestampMs;
                    _bootCompletionPending = false;
                    _lcd1.showBootMessage("Booting Jetson");
                    break;
                case KernelTransitionEvent::BOOT_COMPLETE:
                    _lastBootCompleteMs = message.timestampMs;
                    _bootCompletionPending = true;
                    _lcd1.showBootMessage("Boot Complete !");
                    _lcd1.startWelcomeEffect();
                    break;
                case KernelTransitionEvent::SHUTDOWN_OR_SUSPEND:
                    _lastShutdownIndicatorMs = message.timestampMs;
                    _bootCompletionPending = false;
                    _lcd1.showBootMessage((strstr(message.line, jetson_cfg::kTokenSuspend) != nullptr)
                                              ? "Suspend mode"
                                              : "Shutting down");
                    break;
                case KernelTransitionEvent::POWER_OFF_INDICATOR:
                    _lastPowerOffIndicatorMs = message.timestampMs;
                    _bootCompletionPending = false;
                    _lcd1.showBootMessage("Shutting down");
                    break;
                case KernelTransitionEvent::NONE:
                default:
                    break;
            }

            reconcileStateFromSerialEvidence(message.timestampMs);
            break;
        }

        case jetson_cfg::MessageType::ALERT_EVENT:
            broadcastAlertMask(message.alertMask);
            break;

        default:
            break;
    }
}

bool SystemController::isValidTransition(KernelTransitionEvent transition) const {
    switch (transition) {
        case KernelTransitionEvent::BOOT_START:
        case KernelTransitionEvent::BOOT_COMPLETE:
        case KernelTransitionEvent::SHUTDOWN_OR_SUSPEND:
        case KernelTransitionEvent::POWER_OFF_INDICATOR:
            return true;

        case KernelTransitionEvent::NONE:
        default:
            return false;
    }
}

bool SystemController::isLikelyValidSerial2Line(const char* line) const {
    if (line == nullptr) {
        return false;
    }

    if (JetsonSerial::detectKernelTransition(line) != KernelTransitionEvent::NONE) {
        return true;
    }

    const size_t length = strlen(line);
    if (length < jetson_cfg::kSerialNoiseMinLineLength) {
        return false;
    }

    return containsAlphaChars(line, jetson_cfg::kSerialNoiseMinAlphaChars);
}

bool SystemController::hasRecentEvidence(uint32_t lastTimestampMs,
                                         uint32_t nowMs,
                                         uint32_t windowMs) const {
    return (lastTimestampMs != 0U) && ((nowMs - lastTimestampMs) < windowMs);
}

void SystemController::reconcileStateFromSerialEvidence(uint32_t nowMs) {
    const bool serial1Active = hasRecentEvidence(_lastSerial1StatsMs, nowMs, jetson_cfg::kSerial1StaleMs);
    const bool serial2Active = hasRecentEvidence(_lastSerial2ActivityMs, nowMs, jetson_cfg::kSerial2EvidenceWindowMs);
    const uint32_t latestShutdownEvidence = max(_lastShutdownIndicatorMs, _lastPowerOffIndicatorMs);
    const bool shutdownActive = hasRecentEvidence(latestShutdownEvidence,
                                                  nowMs,
                                                  jetson_cfg::kSerial2EvidenceWindowMs) &&
                                (latestShutdownEvidence >= _lastBootStartMs);
    const bool shutdownJustObserved = hasRecentEvidence(latestShutdownEvidence,
                                                        nowMs,
                                                        jetson_cfg::kSerial2ShutdownQuietMs);

    jetson_cfg::SystemState desiredState = _state;
    const char* contextText = nullptr;

    if (_state == jetson_cfg::SystemState::SHUTTING_DOWN) {
        if (serial1Active && _lastSerial1StatsMs > latestShutdownEvidence) {
            desiredState = jetson_cfg::SystemState::RUNNING;
            contextText = "System Running";
        } else if (serial2Active || shutdownJustObserved) {
            desiredState = jetson_cfg::SystemState::SHUTTING_DOWN;
            contextText = "Shutting down";
        } else {
            desiredState = jetson_cfg::SystemState::POWER_OFF;
            contextText = "POWER OFF";
        }
    } else if (_state == jetson_cfg::SystemState::RUNNING) {
        if (shutdownActive) {
            desiredState = jetson_cfg::SystemState::SHUTTING_DOWN;
            contextText = "Shutting down";
        } else if (serial1Active || serial2Active) {
            desiredState = jetson_cfg::SystemState::RUNNING;
            contextText = "System Running";
        } else {
            desiredState = jetson_cfg::SystemState::POWER_OFF;
            contextText = "POWER OFF";
        }
    } else if (_state == jetson_cfg::SystemState::BOOTING_ON) {
        if (serial1Active) {
            desiredState = jetson_cfg::SystemState::RUNNING;
            contextText = "System Running";
        } else {
            desiredState = jetson_cfg::SystemState::BOOTING_ON;
            contextText = (_lastBootCompleteMs != 0U && _lastBootCompleteMs >= _lastBootStartMs)
                              ? "Boot Complete !"
                              : "Booting Jetson";
        }
    } else if (serial1Active) {
        desiredState = jetson_cfg::SystemState::RUNNING;
        contextText = "System Running";
    } else if (serial2Active) {
        if (!shutdownActive) {
            desiredState = jetson_cfg::SystemState::BOOTING_ON;
            contextText = (_lastBootCompleteMs != 0U && _lastBootCompleteMs >= _lastBootStartMs)
                              ? "Boot Complete !"
                              : "Booting Jetson";
        } else {
            desiredState = jetson_cfg::SystemState::POWER_OFF;
            contextText = "POWER OFF";
        }
    } else {
        desiredState = jetson_cfg::SystemState::POWER_OFF;
        contextText = "POWER OFF";
    }

    if (desiredState == _state) {
        if (contextText != nullptr &&
            (desiredState == jetson_cfg::SystemState::BOOTING_ON ||
             desiredState == jetson_cfg::SystemState::SHUTTING_DOWN)) {
            _lcd1.showBootMessage(contextText);
        }
        return;
    }

    setState(desiredState, contextText);
}

void SystemController::resetJetsonStats() {
    _lastSerial1StatsMs = 0;
    _latestStats.ramPercent = -1;
    _latestStats.cpuPercent = -1;
    _latestStats.gpuPercent = -1;
    _latestStats.cpuTempC = -1.0f;
    _latestStats.gpuTempC = -1.0f;
    _latestStats.powerMilliWatt = -1;
}

bool SystemController::hasValidJetsonMetric() const {
    return (_latestStats.ramPercent >= 0) ||
           (_latestStats.cpuPercent >= 0) ||
           (_latestStats.gpuPercent >= 0) ||
           (_latestStats.cpuTempC >= 0.0f) ||
           (_latestStats.gpuTempC >= 0.0f) ||
           (_latestStats.powerMilliWatt >= 0);
}

LCD2Dashboard::MetricsFrame SystemController::makeLcd2MetricsFrame() const {
    LCD2Dashboard::MetricsFrame frame = {};
    frame.cpuUsage = static_cast<int16_t>(_latestStats.cpuPercent);
    frame.gpuUsage = static_cast<int16_t>(_latestStats.gpuPercent);
    frame.ramUsage = static_cast<int16_t>(_latestStats.ramPercent);
    frame.cpuTemp = _latestStats.cpuTempC;
    frame.gpuTemp = _latestStats.gpuTempC;
    frame.powerMw = static_cast<int32_t>(_latestStats.powerMilliWatt);
    return frame;
}

bool SystemController::tryLockLcd2() {
    if (_lcd2Mutex == nullptr) {
        return true;
    }

    return (xSemaphoreTake(_lcd2Mutex, kLcd2MutexTimeoutTicks) == pdTRUE);
}

void SystemController::unlockLcd2() {
    if (_lcd2Mutex != nullptr) {
        xSemaphoreGive(_lcd2Mutex);
    }
}

int16_t SystemController::readLcd2RequestedLedBrightnessPercent() {
    if (tryLockLcd2()) {
        _lastLedBrightnessPercent = _lcd2.getRequestedLedBrightnessPercent();
        unlockLcd2();
    }

    return _lastLedBrightnessPercent;
}

void SystemController::syncLcd2Metrics() {
    if (!tryLockLcd2()) {
        return;
    }

    if (!hasValidJetsonMetric()) {
        _lcd2.clearMetrics();
        unlockLcd2();
        return;
    }

    _lcd2.pushMetrics(makeLcd2MetricsFrame());
    unlockLcd2();
}

void SystemController::syncLcd2Environment() {
    const float boxTemp = _hasSensorReading ? _sensorTempC : -1.0f;
    const float boxHumidity = _hasSensorReading ? _sensorHumidityPercent : -1.0f;
    if (!tryLockLcd2()) {
        return;
    }

    const int16_t ledBrightnessPercent = _lcd2.getRequestedLedBrightnessPercent();
    _lastLedBrightnessPercent = ledBrightnessPercent;
    _lcd2.setEnvironment(boxTemp,
                         boxHumidity,
                         static_cast<int16_t>(_fan.getSpeed()),
                         ledBrightnessPercent);
    unlockLcd2();
}

uint8_t SystemController::buildAlertMask() const {
    uint8_t mask = 0;
    if (_highTemperatureAlert) {
        mask |= jetson_cfg::kAlertMaskHighTemperature;
    }
    if (_highHumidityAlert) {
        mask |= jetson_cfg::kAlertMaskHighHumidity;
    }
    return mask;
}

void SystemController::broadcastAlertMask(uint8_t alertMask) {
    _currentAlertMask = alertMask;
    _fan.onAlertChange(alertMask);
    _sensor.onAlertChange(alertMask);
    _led.onAlertChange(alertMask);
    _lcd1.onAlertChange(alertMask);
    if (tryLockLcd2()) {
        _lcd2.onAlertChange(alertMask);
        unlockLcd2();
    }
}

void SystemController::setState(jetson_cfg::SystemState newState, const char* contextText) {
    const jetson_cfg::SystemState oldState = _state;
    const bool stateChanged = (oldState != newState);

    _state = newState;

    if (newState != jetson_cfg::SystemState::RUNNING) {
        resetJetsonStats();
    }

    if (newState == jetson_cfg::SystemState::POWER_OFF ||
        newState == jetson_cfg::SystemState::SHUTTING_DOWN) {
        _bootCompletionPending = false;
    }

    if (newState == jetson_cfg::SystemState::POWER_OFF) {
        _lastSerial2ActivityMs = 0;
        _lastBootStartMs = 0;
        _lastBootCompleteMs = 0;
        _lastShutdownIndicatorMs = 0;
        _lastPowerOffIndicatorMs = 0;
    }

    _fan.onStateChange(newState);
    _sensor.onStateChange(newState);
    _led.onStateChange(newState);
    _lcd1.onStateChange(newState);
    bool lcd2StateApplied = false;
    if (tryLockLcd2()) {
        _lcd2.onStateChange(newState);
        unlockLcd2();
        lcd2StateApplied = true;
        _lcd2StateSyncPending = false;
    } else {
        _lcd2StateSyncPending = true;
        _lcd2PendingState = newState;
    }
    if (lcd2StateApplied && newState == jetson_cfg::SystemState::RUNNING) {
        syncLcd2Environment();
        syncLcd2Metrics();
    }

    if (!stateChanged) {
        if (newState != jetson_cfg::SystemState::RUNNING &&
            contextText != nullptr && contextText[0] != '\0') {
            _lcd1.showBootMessage(contextText);
        }
        return;
    }

    if (newState != jetson_cfg::SystemState::RUNNING &&
        contextText != nullptr && contextText[0] != '\0') {
        _lcd1.showBootMessage(contextText);
    }
}

void SystemController::updateAlerts() {
    bool nextHighTemp = _highTemperatureAlert;
    const float cpuT = _latestStats.cpuTempC;
    const float gpuT = _latestStats.gpuTempC;

    if (!_highTemperatureAlert) {
        if (cpuT >= jetson_cfg::kTempHighC || gpuT >= jetson_cfg::kTempHighC) {
            nextHighTemp = true;
        }
    } else {
        const bool cpuRecovered = (cpuT < 0.0f) || (cpuT <= jetson_cfg::kTempLowC);
        const bool gpuRecovered = (gpuT < 0.0f) || (gpuT <= jetson_cfg::kTempLowC);
        if (cpuRecovered && gpuRecovered) {
            nextHighTemp = false;
        }
    }

    bool nextHighHumidity = _highHumidityAlert;
    if (!_hasSensorReading) {
        nextHighHumidity = false;
    } else if (!_highHumidityAlert) {
        if (_sensorHumidityPercent >= jetson_cfg::kHumidityHighPercent) {
            nextHighHumidity = true;
        }
    } else {
        if (_sensorHumidityPercent <= jetson_cfg::kHumidityRecoverPercent) {
            nextHighHumidity = false;
        }
    }

    const bool alertsChanged = (nextHighTemp != _highTemperatureAlert) ||
                               (nextHighHumidity != _highHumidityAlert);

    _highTemperatureAlert = nextHighTemp;
    _highHumidityAlert = nextHighHumidity;

    if (alertsChanged) {
        ControllerMessage alertMessage = {};
        alertMessage.source = jetson_cfg::MessageSource::CONTROLLER;
        alertMessage.type = jetson_cfg::MessageType::ALERT_EVENT;
        alertMessage.timestampMs = millis();
        alertMessage.alertMask = buildAlertMask();
        postMessage(alertMessage, true);
    }
}

void SystemController::applyPolicies() {
    const int16_t requestedBrightness = readLcd2RequestedLedBrightnessPercent();
    _led.setBrightness(static_cast<uint8_t>(map(requestedBrightness, 0, 100, 0, 255)));

    if (_state == jetson_cfg::SystemState::POWER_OFF ||
        _state == jetson_cfg::SystemState::SHUTTING_DOWN) {
        _fan.stop();
    } else {
        bool hasJetsonThermalMetric = false;
        const float refTemp = selectJetsonThermalMetric(hasJetsonThermalMetric);
        _fan.update(refTemp, hasJetsonThermalMetric);
    }

    syncLcd2Environment();
}

void SystemController::updateDisplays(uint32_t nowMs) {
    _sensor.update(nowMs);
    _lcd1.update(nowMs);
}

void SystemController::handleMetricStaleness(uint32_t nowMs) {
    if (_state != jetson_cfg::SystemState::RUNNING || _lastSerial1StatsMs == 0) {
        return;
    }

    if ((nowMs - _lastSerial1StatsMs) < jetson_cfg::kSerial1StaleMs) {
        return;
    }

    _lastSerial1StatsMs = 0;
    _latestStats.ramPercent = -1;
    _latestStats.cpuPercent = -1;
    _latestStats.gpuPercent = -1;
    _latestStats.cpuTempC = -1.0f;
    _latestStats.gpuTempC = -1.0f;
    _latestStats.powerMilliWatt = -1;
    if (tryLockLcd2()) {
        _lcd2.clearMetrics();
        unlockLcd2();
    }
    _lcd1.showBootMessage("Waiting stats");
}

void SystemController::handleSensorFreshness(uint32_t nowMs) {
    if (!_hasSensorReading) {
        return;
    }

    const bool staleByAge = (_lastSensorValidMs == 0U) ||
                            ((nowMs - _lastSensorValidMs) >= jetson_cfg::kSensorFreshnessTimeoutMs);
    const bool staleByFailures =
        (_sensorConsecutiveFailures >= jetson_cfg::kSensorMaxConsecutiveFailures);
    if (!staleByAge && !staleByFailures) {
        return;
    }

    _hasSensorReading = false;
    _sensorTempC = NAN;
    _sensorHumidityPercent = NAN;
    syncLcd2Environment();
}

void SystemController::syncPendingLcd2State() {
    if (!_lcd2StateSyncPending) {
        return;
    }

    if (!tryLockLcd2()) {
        return;
    }

    const jetson_cfg::SystemState pendingState = _lcd2PendingState;
    _lcd2.onStateChange(pendingState);
    unlockLcd2();
    _lcd2StateSyncPending = false;

    if (pendingState == jetson_cfg::SystemState::RUNNING) {
        syncLcd2Environment();
        syncLcd2Metrics();
    }
}

float SystemController::selectJetsonThermalMetric(bool& hasMetric) const {
    hasMetric = false;
    float refTemp = NAN;

    if (_latestStats.cpuTempC >= 0.0f) {
        refTemp = _latestStats.cpuTempC;
        hasMetric = true;
    }

    if (_latestStats.gpuTempC >= 0.0f) {
        refTemp = hasMetric ? max(refTemp, _latestStats.gpuTempC) : _latestStats.gpuTempC;
        hasMetric = true;
    }

    return refTemp;
}

void SystemController::controllerTaskLoop() {
    for (;;) {
        ControllerMessage message = {};
        const TickType_t queueWaitTicks = controllerQueueWaitTicks(_state);
        const bool hasMessage = (xQueueReceive(_messageQueue, &message, queueWaitTicks) == pdTRUE);
        if (hasMessage) {
            handleMessage(message);

            while (xQueueReceive(_messageQueue, &message, 0) == pdTRUE) {
                handleMessage(message);
            }
        }

        const uint32_t nowMs = millis();
        reconcileStateFromSerialEvidence(nowMs);
        handleMetricStaleness(nowMs);
        handleSensorFreshness(nowMs);
        syncPendingLcd2State();
        updateAlerts();
        applyPolicies();
        updateDisplays(nowMs);
    }
}

void SystemController::serial1TaskLoop() {
    char line[jetson_cfg::kSerial1LineMaxLen + 1] = {0};

    for (;;) {
        if (!_jetson.readSerial1Line(line, sizeof(line))) {
            vTaskDelay((_state == jetson_cfg::SystemState::RUNNING)
                           ? kSerialPollDelayTicks
                           : kSerialIdleDelayTicks);
            continue;
        }

        JetsonStatsSnapshot parsedStats = _latestStats;
        if (!JetsonSerial::parseStatsLine(line, parsedStats)) {
            continue;
        }

        ControllerMessage message = {};
        message.source = jetson_cfg::MessageSource::SERIAL1;
        message.type = jetson_cfg::MessageType::JETSON_STATS;
        message.timestampMs = millis();
        message.stats = parsedStats;
        safeCopyLine(message.line, sizeof(message.line), line);

        postMessage(message, false);
    }
}

void SystemController::serial2TaskLoop() {
    char line[jetson_cfg::kSerial2LineMaxLen + 1] = {0};
    uint32_t lastTransitionMs = 0;
    uint32_t lastKernelLogPostMs = 0;
    KernelTransitionEvent lastTransition = KernelTransitionEvent::NONE;
    uint8_t consecutiveStoppedLines = 0;

    for (;;) {
        if (!_jetson.readSerial2Line(line, sizeof(line))) {
            if (_state == jetson_cfg::SystemState::BOOTING_ON ||
                _state == jetson_cfg::SystemState::SHUTTING_DOWN) {
                vTaskDelay(kSerialPollDelayTicks);
            } else if (_state == jetson_cfg::SystemState::RUNNING) {
                vTaskDelay(kSerialPollDelayTicks);
            } else {
                vTaskDelay(pdMS_TO_TICKS(jetson_cfg::kSerial2ProbeIntervalMs));
            }
            continue;
        }

        const uint32_t now = millis();
        if (!isLikelyValidSerial2Line(line)) {
            continue;
        }
        _lastSerial2ActivityMs = now;

        KernelTransitionEvent transition = JetsonSerial::detectKernelTransition(line);
        if (strstr(line, "Stopped") != nullptr) {
            if (consecutiveStoppedLines < 255U) {
                ++consecutiveStoppedLines;
            }
            if (consecutiveStoppedLines >= 2U && transition == KernelTransitionEvent::NONE) {
                transition = KernelTransitionEvent::SHUTDOWN_OR_SUSPEND;
            }
        } else {
            consecutiveStoppedLines = 0;
        }

        const bool isTransitionState = (_state == jetson_cfg::SystemState::BOOTING_ON ||
                                        _state == jetson_cfg::SystemState::SHUTTING_DOWN);
        const uint32_t logThrottleMs = isTransitionState
                                           ? jetson_cfg::kSerial2TransitionLogThrottleMs
                                           : jetson_cfg::kSerial2LogThrottleMs;
        const bool shouldPostKernelLog = ((now - lastKernelLogPostMs) >= logThrottleMs) ||
                                         (transition != KernelTransitionEvent::NONE);
        if (shouldPostKernelLog) {
            ControllerMessage logMsg = {};
            logMsg.source = jetson_cfg::MessageSource::SERIAL2;
            logMsg.type = jetson_cfg::MessageType::KERNEL_LOG;
            logMsg.timestampMs = now;
            safeCopyLine(logMsg.line, sizeof(logMsg.line), line);
            postMessage(logMsg, false);
            lastKernelLogPostMs = now;
        }

        if (transition == KernelTransitionEvent::NONE) {
            continue;
        }

        if (transition == lastTransition &&
            (now - lastTransitionMs) < jetson_cfg::kSerial2TransitionDebounceMs) {
            continue;
        }

        lastTransition = transition;
        lastTransitionMs = now;

        ControllerMessage stateMsg = {};
        stateMsg.source = jetson_cfg::MessageSource::SERIAL2;
        stateMsg.type = jetson_cfg::MessageType::STATE_EVENT;
        stateMsg.timestampMs = now;
        stateMsg.transition = transition;
        safeCopyLine(stateMsg.line, sizeof(stateMsg.line), line);

        postMessage(stateMsg, true);
    }
}

void SystemController::sensorTaskLoop() {
    for (;;) {
        ControllerMessage message = {};
        message.source = jetson_cfg::MessageSource::SENSOR;
        message.type = jetson_cfg::MessageType::SENSOR_READING;
        message.timestampMs = millis();

        float temp = NAN;
        float humidity = NAN;
        message.sensorValid = _sensor.readSnapshot(temp, humidity);
        message.sensorTempC = temp;
        message.sensorHumidityPercent = humidity;

        postMessage(message, false);
        vTaskDelay(pdMS_TO_TICKS(jetson_cfg::kSensorSamplePeriodMs));
    }
}

void SystemController::ledTaskLoop() {
    for (;;) {
        _led.update();
        vTaskDelay(kLedTaskDelayTicks);
    }
}

void SystemController::taskControllerEntry(void* arg) {
    if (arg != nullptr) {
        static_cast<SystemController*>(arg)->controllerTaskLoop();
    }
    vTaskDelete(nullptr);
}

void SystemController::taskSerial1Entry(void* arg) {
    if (arg != nullptr) {
        static_cast<SystemController*>(arg)->serial1TaskLoop();
    }
    vTaskDelete(nullptr);
}

void SystemController::taskSerial2Entry(void* arg) {
    if (arg != nullptr) {
        static_cast<SystemController*>(arg)->serial2TaskLoop();
    }
    vTaskDelete(nullptr);
}

void SystemController::taskSensorEntry(void* arg) {
    if (arg != nullptr) {
        static_cast<SystemController*>(arg)->sensorTaskLoop();
    }
    vTaskDelete(nullptr);
}

void SystemController::taskLedEntry(void* arg) {
    if (arg != nullptr) {
        static_cast<SystemController*>(arg)->ledTaskLoop();
    }
    vTaskDelete(nullptr);
}

    void SystemController::lcd2TaskLoop() {
        TickType_t xLastWakeTime = xTaskGetTickCount();
        constexpr TickType_t kPeriod = pdMS_TO_TICKS(16); // ~60 fps
        for (;;) {
            if (_lcd2Mutex != nullptr &&
                xSemaphoreTake(_lcd2Mutex, kLcd2MutexTimeoutTicks) == pdTRUE) {
                _lcd2.update(millis());
                xSemaphoreGive(_lcd2Mutex);
            }
            vTaskDelayUntil(&xLastWakeTime, kPeriod);
        }
    }

    void SystemController::taskLcd2Entry(void* arg) {
        if (arg != nullptr) {
            static_cast<SystemController*>(arg)->lcd2TaskLoop();
        }
        vTaskDelete(nullptr);
    }