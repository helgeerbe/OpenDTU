// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <battery/Stats.h>

namespace BatteryNs::Pylontech {

class Stats : public ::BatteryNs::Stats {
friend class Provider;

public:
    void getLiveViewData(JsonVariant& root) const final;
    void mqttPublish() const final;
    bool getImmediateChargingRequest() const { return _chargeImmediately; } ;
    float getChargeCurrentLimitation() const { return _chargeCurrentLimitation; } ;

private:
    void setLastUpdate(uint32_t ts) { _lastUpdate = ts; }

    float _chargeVoltage;
    float _chargeCurrentLimitation;
    float _dischargeVoltageLimitation;
    uint16_t _stateOfHealth;
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

    uint8_t _moduleCount;
};

} // namespace BatteryNs::Pylontech
