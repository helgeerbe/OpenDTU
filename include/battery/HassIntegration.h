// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>

namespace BatteryNs {

class HassIntegration {
public:
    virtual void publishSensors() const;

protected:
    void publish(const String& subtopic, const String& payload) const;
    void publishBinarySensor(const char* caption,
            const char* icon, const char* subTopic,
            const char* payload_on, const char* payload_off) const;
    void publishSensor(const char* caption, const char* icon,
            const char* subTopic, const char* deviceClass = nullptr,
            const char* stateClass = nullptr,
            const char* unitOfMeasurement = nullptr) const;
    void createDeviceInfo(JsonObject& object) const;

    String _serial = "0001"; // pseudo-serial, can be replaced in future with real serialnumber
};

} // namespace BatteryNs
