#include "fan.h"

#include <math.h>

Fan::Fan(int pwmPin)
    : _pwmPin(pwmPin),
      _frequency(jetson_cfg::kFanPwmFrequencyHz),
    _currentSpeed(0),
    _state(jetson_cfg::SystemState::POWER_OFF),
    _alertMask(0) {}

void Fan::init(int pwmPin, uint32_t frequency, float initialDuty) {
    _pwmPin = pwmPin;
    _frequency = frequency;
    _currentSpeed = clampDuty(static_cast<int>(initialDuty));

    mcpwm_gpio_init(MCPWM_UNIT_0, MCPWM0A, _pwmPin);
    mcpwm_config_t pwm_config = {};
    pwm_config.frequency = _frequency;
    pwm_config.cmpr_a = static_cast<float>(_currentSpeed);
    pwm_config.counter_mode = MCPWM_UP_COUNTER;
    pwm_config.duty_mode = MCPWM_DUTY_MODE_0;
    mcpwm_init(MCPWM_UNIT_0, MCPWM_TIMER_0, &pwm_config);
}

void Fan::setSpeed(uint8_t percent) {
    _currentSpeed = clampDuty(percent);
    mcpwm_set_duty(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_A, (float)_currentSpeed);
}

void Fan::onStateChange(jetson_cfg::SystemState newState) {
    _state = newState;
    if (_state == jetson_cfg::SystemState::POWER_OFF) {
        stop();
    }
}

void Fan::onAlertChange(uint8_t alertMask) {
    _alertMask = alertMask;
}

void Fan::update(float jetsonTempC, bool hasJetsonThermalMetric) {
    const uint8_t duty = computeAutoDuty(jetsonTempC, hasJetsonThermalMetric);
    if (duty == 0) {
        stop();
        return;
    }

    setSpeed(duty);
}

void Fan::stop() {
    setSpeed(0);
}

uint8_t Fan::computeAutoDuty(float tempC,
                             bool hasJetsonThermalMetric) const {
    const uint8_t minDuty = clampDuty(jetson_cfg::kFanDutyMinPercent);
    const uint8_t maxDuty = clampDuty(jetson_cfg::kFanDutyMaxPercent);
    const bool highTemperatureAlert = (_alertMask & jetson_cfg::kAlertMaskHighTemperature) != 0;
    const bool highHumidityAlert = (_alertMask & jetson_cfg::kAlertMaskHighHumidity) != 0;

    if (highTemperatureAlert) {
        return maxDuty;
    }

    if (_state == jetson_cfg::SystemState::POWER_OFF) {
        return 0;
    }

    if (_state != jetson_cfg::SystemState::RUNNING) {
        if (!highHumidityAlert) {
            return 0;
        }

        return minDuty;
    }

    if (!hasJetsonThermalMetric || isnan(tempC)) {
        return (_currentSpeed > 0) ? minDuty : 0;
    }

    if (tempC <= jetson_cfg::kTempFanOffHysteresisC) {
        return 0;
    }

    if (tempC <= jetson_cfg::kTempLowC) {
        return (_currentSpeed > 0) ? minDuty : 0;
    }

    if (tempC >= jetson_cfg::kTempHighC) {
        return maxDuty;
    }

    const float span = jetson_cfg::kTempHighC - jetson_cfg::kTempLowC;
    if (span <= 0.0f) {
        return maxDuty;
    }

    const float alpha = (tempC - jetson_cfg::kTempLowC) / span;
    const int duty = static_cast<int>(minDuty + alpha * (maxDuty - minDuty));
    return clampDuty(duty);
}

uint8_t Fan::clampDuty(int value) {
    if (value < 0) {
        return 0;
    }
    if (value > 100) {
        return 100;
    }
    return static_cast<uint8_t>(value);
}
