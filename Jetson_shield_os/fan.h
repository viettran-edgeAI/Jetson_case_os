#ifndef FAN_H
#define FAN_H

#include "driver/mcpwm.h"
#include <Arduino.h>
#include "system_configuration.h"

/**
 * @brief Simple fan controller using the ESP32 MCPWM unit.
 *
 * The class encapsulates PWM initialization and provides a
 * single public API to set the speed in percent (0–100).
 * It is intentionally lightweight so it can be reused by
 * multiple sketches or components in the Jetson project.
 */
class Fan {
public:
    static constexpr int DEFAULT_PWM_PIN = jetson_cfg::kFanPwmPin;

    explicit Fan(int pwmPin = DEFAULT_PWM_PIN);

    void init(int pwmPin = DEFAULT_PWM_PIN,
              uint32_t frequency = jetson_cfg::kFanPwmFrequencyHz,
              float initialDuty = 0.0f);
    inline void begin(int pwmPin = DEFAULT_PWM_PIN,
                      uint32_t frequency = jetson_cfg::kFanPwmFrequencyHz,
                      float initialDuty = 0.0f) { init(pwmPin, frequency, initialDuty); }

    void onStateChange(jetson_cfg::SystemState newState);
    void onAlertChange(uint8_t alertMask);
    void update(float jetsonTempC, bool hasJetsonThermalMetric);

    void setSpeed(uint8_t percent);
    void stop();
    uint8_t computeAutoDuty(float tempC,
                            bool hasJetsonThermalMetric) const;

    uint8_t getSpeed() const { return _currentSpeed; }

private:
    static uint8_t clampDuty(int value);

    int      _pwmPin;
    uint32_t _frequency;
    uint8_t  _currentSpeed;
    bool _isRunning;
    uint32_t _lastToggleMs;
    jetson_cfg::SystemState _state;
    uint8_t _alertMask;
};

#endif // FAN_H
