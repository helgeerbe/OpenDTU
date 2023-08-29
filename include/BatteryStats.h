// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <stdint.h>

#include "AsyncJson.h"
#include "Arduino.h"
#include "JkBmsDataPoints.h"

// mandatory interface for all kinds of batteries
class BatteryStats {
    public:
        String const& getManufacturer() const { return _manufacturer; }

        // the last time *any* datum was updated
        uint32_t getAgeSeconds() const { return (millis() - _lastUpdate) / 1000; }
        bool updateAvailable(uint32_t since) const { return _lastUpdate > since; }

        uint8_t getSoC() const { return _SoC; }
        uint32_t getSoCAgeSeconds() const { return (millis() - _lastUpdateSoC) / 1000; }

        // convert stats to JSON for web application live view
        virtual void getLiveViewData(JsonVariant& root) const;

        virtual void mqttPublish() const;

        bool isValid() const { return _lastUpdateSoC > 0 && _lastUpdate > 0; }

    protected:
        String _manufacturer = "unknown";
        uint8_t _SoC = 0;
        uint32_t _lastUpdateSoC = 0;
        uint32_t _lastUpdate = 0;
};

class PylontechBatteryStats : public BatteryStats {
    friend class PylontechCanReceiver;

    public:
        void getLiveViewData(JsonVariant& root) const final;
        void mqttPublish() const final;

    private:
        void setManufacturer(String&& m) { _manufacturer = std::move(m); }
        void setSoC(uint8_t SoC) { _SoC = SoC; _lastUpdateSoC = millis(); }
        void setLastUpdate(uint32_t ts) { _lastUpdate = ts; }

        float _chargeVoltage;
        float _chargeCurrentLimitation;
        float _dischargeCurrentLimitation;
        uint16_t _stateOfHealth;
        float _voltage; // total voltage of the battery pack
        // total current into (positive) or from (negative)
        // the battery, i.e., the charging current
        float _current;
        float _temperature;

        bool _alarmOverCurrentDischarge;
        bool _alarmOverCurrentCharge;
        bool _alarmUnderTemperature;
        bool _alarmOverTemperature;
        bool _alarmUnderVoltage;
        bool _alarmOverVoltage;
        bool _alarmBmsInternal;

        bool _warningHighCurrentDischarge;
        bool _warningHighCurrentCharge;
        bool _warningLowTemperature;
        bool _warningHighTemperature;
        bool _warningLowVoltage;
        bool _warningHighVoltage;
        bool _warningBmsInternal;

        bool _chargeEnabled;
        bool _dischargeEnabled;
        bool _chargeImmediately;
};

class JkBmsBatteryStats : public BatteryStats {
    public:
        void getLiveViewData(JsonVariant& root) const final;
        void mqttPublish() const final;

        void updateFrom(JkBms::DataPointContainer const& dp);

    private:
        JkBms::DataPointContainer _dataPoints;
};
