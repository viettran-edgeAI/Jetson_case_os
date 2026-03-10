#include "sensor.h"

#include <math.h>

Sensor::Sensor(uint8_t pin, uint8_t type)
        : _pin(pin),
            _type(type),
            _dht(pin, type),
            _state(jetson_cfg::SystemState::POWER_OFF),
            _alertMask(0) {}

void Sensor::init(uint8_t pin, uint8_t type) {
    // allow runtime override
    _pin = pin;
    _type = type;
    _dht = DHT(_pin, _type);
    _dht.begin();
}

float Sensor::readTemperature() {
    return _dht.readTemperature();
}

float Sensor::readHumidity() {
    return _dht.readHumidity();
}

void Sensor::onStateChange(jetson_cfg::SystemState newState) {
    _state = newState;
}

void Sensor::onAlertChange(uint8_t alertMask) {
    _alertMask = alertMask;
}

void Sensor::update(uint32_t nowMs) {
    (void)nowMs;
}

bool Sensor::readSnapshot(float& outTempC, float& outHumidityPercent) {
    const float temp = readTemperature();
    const float hum = readHumidity();

    if (isnan(temp) || isnan(hum)) {
        return false;
    }

    outTempC = temp;
    outHumidityPercent = hum;
    return true;
}
