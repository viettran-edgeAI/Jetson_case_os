#include "jetson_serial.h"

#include <stdio.h>
#include <string.h>

JetsonSerial::JetsonSerial()
    : _serial1Index(0),
      _serial2Index(0),
    _serial1DiscardUntilNewline(false),
    _serial2DiscardUntilNewline(false),
    _serial1OverflowCount(0),
    _serial2OverflowCount(0),
      _lastTransitionMs(0),
      _lastTransition(KernelTransitionEvent::NONE) {
    _serial1Buffer[0] = '\0';
    _serial2Buffer[0] = '\0';
}

void JetsonSerial::init(uint32_t serial1Baud,
                        uint32_t serial2Baud,
                        int serial1RxPin,
                        int serial1TxPin,
                        int serial2RxPin,
                        int serial2TxPin) {
    Serial1.begin(serial1Baud, SERIAL_8N1, serial1RxPin, serial1TxPin);
    Serial2.begin(serial2Baud, SERIAL_8N1, serial2RxPin, serial2TxPin);
    _serial1Index = 0;
    _serial2Index = 0;
    _serial1DiscardUntilNewline = false;
    _serial2DiscardUntilNewline = false;
    _serial1OverflowCount = 0;
    _serial2OverflowCount = 0;
}

bool JetsonSerial::readSerial1Line(char* outLine, size_t outSize) {
    return readLineFromPort(Serial1,
                            _serial1Buffer,
                            sizeof(_serial1Buffer),
                            _serial1Index,
                            _serial1DiscardUntilNewline,
                            _serial1OverflowCount,
                            outLine,
                            outSize);
}

bool JetsonSerial::readSerial2Line(char* outLine, size_t outSize) {
    return readLineFromPort(Serial2,
                            _serial2Buffer,
                            sizeof(_serial2Buffer),
                            _serial2Index,
                            _serial2DiscardUntilNewline,
                            _serial2OverflowCount,
                            outLine,
                            outSize);
}

bool JetsonSerial::pollStats(JetsonStatsSnapshot& inOutStats, char* outLine, size_t outLineSize) {
    char line[jetson_cfg::kSerial1LineMaxLen + 1] = {0};
    if (!readSerial1Line(line, sizeof(line))) {
        return false;
    }

    if (outLine != nullptr && outLineSize > 0) {
        strncpy(outLine, line, outLineSize - 1);
        outLine[outLineSize - 1] = '\0';
    }

    return parseStatsLine(line, inOutStats);
}

bool JetsonSerial::pollKernelTransition(KernelTransitionEvent& outEvent, char* outLine, size_t outLineSize) {
    char line[jetson_cfg::kSerial2LineMaxLen + 1] = {0};
    if (!readSerial2Line(line, sizeof(line))) {
        return false;
    }

    if (outLine != nullptr && outLineSize > 0) {
        strncpy(outLine, line, outLineSize - 1);
        outLine[outLineSize - 1] = '\0';
    }

    const KernelTransitionEvent event = detectKernelTransition(line);
    if (event == KernelTransitionEvent::NONE) {
        return false;
    }

    const uint32_t now = millis();
    if (event == _lastTransition && (now - _lastTransitionMs) < jetson_cfg::kSerial2TransitionDebounceMs) {
        return false;
    }

    _lastTransition = event;
    _lastTransitionMs = now;
    outEvent = event;
    return true;
}

bool JetsonSerial::parseStatsLine(const char* line, JetsonStatsSnapshot& inOutStats) {
    if (line == nullptr || line[0] == '\0') {
        return false;
    }

    char lineCopy[jetson_cfg::kSerial1LineMaxLen + 1] = {0};
    strncpy(lineCopy, line, sizeof(lineCopy) - 1);

    bool parsedAny = false;
    char* savePtr = nullptr;
    char* token = strtok_r(lineCopy, " \t,", &savePtr);
    while (token != nullptr) {
        int intValue = 0;
        float floatValue = 0.0f;

        if (sscanf(token, "RAM:%d", &intValue) == 1) {
            inOutStats.ramPercent = constrain(intValue, 0, 100);
            parsedAny = true;
        } else if (sscanf(token, "CPU:%d", &intValue) == 1) {
            inOutStats.cpuPercent = constrain(intValue, 0, 100);
            parsedAny = true;
        } else if (sscanf(token, "GPU:%d", &intValue) == 1) {
            inOutStats.gpuPercent = constrain(intValue, 0, 100);
            parsedAny = true;
        } else if (sscanf(token, "CT:%f", &floatValue) == 1) {
            inOutStats.cpuTempC = floatValue;
            parsedAny = true;
        } else if (sscanf(token, "GT:%f", &floatValue) == 1) {
            inOutStats.gpuTempC = floatValue;
            parsedAny = true;
        } else if (sscanf(token, "P:%dmW", &intValue) == 1) {
            inOutStats.powerMilliWatt = (intValue < 0) ? 0 : intValue;
            parsedAny = true;
        }

        token = strtok_r(nullptr, " \t,", &savePtr);
    }

    return parsedAny;
}

KernelTransitionEvent JetsonSerial::detectKernelTransition(const char* line) {
    if (line == nullptr || line[0] == '\0') {
        return KernelTransitionEvent::NONE;
    }

    if (strstr(line, jetson_cfg::kTokenBootStart) != nullptr) {
        return KernelTransitionEvent::BOOT_START;
    }

    // Boot complete can be signaled by either login prompt or Ubuntu banner line.
    if (strstr(line, jetson_cfg::kTokenBootComplete) != nullptr ||
        ((strstr(line, "Ubuntu") != nullptr) &&
         ((strstr(line, "jetson") != nullptr) || (strstr(line, "Jetson") != nullptr)))) {
        return KernelTransitionEvent::BOOT_COMPLETE;
    }

    if (strstr(line, jetson_cfg::kTokenPowerOff) != nullptr ||
        strstr(line, jetson_cfg::kTokenPowerOffAlt) != nullptr) {
        return KernelTransitionEvent::POWER_OFF_INDICATOR;
    }

    if (strstr(line, jetson_cfg::kTokenShutdown) != nullptr ||
        strstr(line, jetson_cfg::kTokenSuspend) != nullptr) {
        return KernelTransitionEvent::SHUTDOWN_OR_SUSPEND;
    }

    return KernelTransitionEvent::NONE;
}

bool JetsonSerial::readLineFromPort(HardwareSerial& port,
                                    char* buffer,
                                    size_t bufferSize,
                                    size_t& writeIndex,
                                    bool& discardUntilNewline,
                                    uint32_t& overflowCount,
                                    char* outLine,
                                    size_t outSize) {
    while (port.available() > 0) {
        const int raw = port.read();
        if (raw < 0) {
            break;
        }

        const char c = static_cast<char>(raw);

        if (c == '\r') {
            continue;
        }

        if (discardUntilNewline) {
            if (c == '\n') {
                discardUntilNewline = false;
                writeIndex = 0;
            }
            continue;
        }

        if (c == '\n') {
            if (writeIndex == 0) {
                continue;
            }

            buffer[writeIndex] = '\0';

            if (outLine != nullptr && outSize > 0) {
                strncpy(outLine, buffer, outSize - 1);
                outLine[outSize - 1] = '\0';
            }

            writeIndex = 0;
            return true;
        }

        if (writeIndex < (bufferSize - 1)) {
            buffer[writeIndex++] = c;
        } else {
            // Frame exceeded buffer capacity: count it and discard until newline.
            ++overflowCount;
            writeIndex = 0;
            discardUntilNewline = true;
        }
    }

    return false;
}