#ifndef JETSON_SERIAL_H
#define JETSON_SERIAL_H

#include <Arduino.h>
#include <stdint.h>
#include <stddef.h>

#include "system_configuration.h"

struct JetsonStatsSnapshot {
    int ramPercent = -1;
    int cpuPercent = -1;
    int gpuPercent = -1;
    float cpuTempC = -1.0f;
    float gpuTempC = -1.0f;
    int powerMilliWatt = -1;
};

enum class KernelTransitionEvent : uint8_t {
    NONE = 0,
    BOOT_START,
    BOOT_COMPLETE,
    SHUTDOWN_OR_SUSPEND,
    POWER_OFF_INDICATOR
};

class JetsonSerial {
public:
    JetsonSerial();

    void init(uint32_t serial1Baud = jetson_cfg::kSerial1Baud,
              uint32_t serial2Baud = jetson_cfg::kSerial2Baud,
              int serial1RxPin = jetson_cfg::kSerial1RxPin,
              int serial1TxPin = jetson_cfg::kSerial1TxPin,
              int serial2RxPin = jetson_cfg::kSerial2RxPin,
              int serial2TxPin = jetson_cfg::kSerial2TxPin);

    inline void begin(uint32_t serial1Baud = jetson_cfg::kSerial1Baud,
                      uint32_t serial2Baud = jetson_cfg::kSerial2Baud,
                      int serial1RxPin = jetson_cfg::kSerial1RxPin,
                      int serial1TxPin = jetson_cfg::kSerial1TxPin,
                      int serial2RxPin = jetson_cfg::kSerial2RxPin,
                      int serial2TxPin = jetson_cfg::kSerial2TxPin) {
        init(serial1Baud, serial2Baud, serial1RxPin, serial1TxPin, serial2RxPin, serial2TxPin);
    }

    bool readSerial1Line(char* outLine, size_t outSize);
    bool readSerial2Line(char* outLine, size_t outSize);

    uint32_t getSerial1OverflowCount() const { return _serial1OverflowCount; }
    uint32_t getSerial2OverflowCount() const { return _serial2OverflowCount; }

    bool pollStats(JetsonStatsSnapshot& inOutStats, char* outLine = nullptr, size_t outLineSize = 0);
    bool pollKernelTransition(KernelTransitionEvent& outEvent, char* outLine = nullptr, size_t outLineSize = 0);

    static bool parseStatsLine(const char* line, JetsonStatsSnapshot& inOutStats);
    static KernelTransitionEvent detectKernelTransition(const char* line);

private:
    bool readLineFromPort(HardwareSerial& port,
                          char* buffer,
                          size_t bufferSize,
                          size_t& writeIndex,
                          bool& discardUntilNewline,
                          uint32_t& overflowCount,
                          char* outLine,
                          size_t outSize);

    char _serial1Buffer[jetson_cfg::kSerial1LineMaxLen + 1];
    char _serial2Buffer[jetson_cfg::kSerial2LineMaxLen + 1];
    size_t _serial1Index;
    size_t _serial2Index;
    bool _serial1DiscardUntilNewline;
    bool _serial2DiscardUntilNewline;
    uint32_t _serial1OverflowCount;
    uint32_t _serial2OverflowCount;
    uint32_t _lastTransitionMs;
    KernelTransitionEvent _lastTransition;
};

#endif // JETSON_SERIAL_H