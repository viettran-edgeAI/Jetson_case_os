#ifndef SENSOR_H
#define SENSOR_H

#include <Arduino.h>
#include "DHT.h"
#include "system_configuration.h"

/**
 * @brief Simple wrapper around the Adafruit DHT library.
 *
 * The class owns a DHT object and exposes a compact API that can be
 * constructed/initialized with a pin and sensor type.  Methods return
 * "nan" on failure exactly like the underlying library, so callers can
 * check isnan() if desired.
 */
class Sensor {
public:
    static constexpr uint8_t DEFAULT_PIN  = jetson_cfg::kDhtPin;
    static constexpr uint8_t DEFAULT_TYPE = jetson_cfg::kDhtType;

    /**
     * Construct the sensor object.  No hardware is touched until init().
     *
     * @param pin  GPIO connected to the sensor's data line.
     * @param type sensor type constant (DHT11, DHT22, etc.)
     */
    Sensor(uint8_t pin = DEFAULT_PIN, uint8_t type = DEFAULT_TYPE);

    /**
     * Initialise the sensor hardware.  Must be called once before
     * readTemperature()/readHumidity().
     *
     * @param pin  overrides the pin stored in the object; if omitted,
     *             the previously supplied pin (or DEFAULT_PIN) is used.
     * @param type optional sensor type override.
     */
    void init(uint8_t pin = DEFAULT_PIN, uint8_t type = DEFAULT_TYPE);
    inline void begin(uint8_t pin = DEFAULT_PIN, uint8_t type = DEFAULT_TYPE) { init(pin, type); }

    /**
     * Read air temperature in °C.  Returns NaN on error.
     */
    float readTemperature();

    /**
     * Read relative humidity in %RH.  Returns NaN on error.
     */
    float readHumidity();
    void onStateChange(jetson_cfg::SystemState newState);
    void onAlertChange(uint8_t alertMask);
    void update(uint32_t nowMs);
    bool readSnapshot(float& outTempC, float& outHumidityPercent);

private:
    uint8_t _pin;
    uint8_t _type;
    DHT     _dht;
    jetson_cfg::SystemState _state;
    uint8_t _alertMask;
};

#endif // SENSOR_H
