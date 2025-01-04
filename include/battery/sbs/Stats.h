// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <battery/Stats.h>

namespace BatteryNs::SBS {

class Stats : public ::BatteryNs::Stats {
friend class Provider;

public:
    void getLiveViewData(JsonVariant& root) const final;
    void mqttPublish() const final;
    float getChargeCurrent() const { return _current; } ;
    float getChargeCurrentLimitation() const { return _chargeCurrentLimitation; } ;

private:
    void setLastUpdate(uint32_t ts) { _lastUpdate = ts; }

    float _chargeVoltage;
    float _chargeCurrentLimitation;
    uint16_t _stateOfHealth;
    float _current;
    float _temperature;

    bool _alarmUnderTemperature;
    bool _alarmOverTemperature;
    bool _alarmUnderVoltage;
    bool _alarmOverVoltage;
    bool _alarmBmsInternal;

    bool _warningHighCurrentDischarge;
    bool _warningHighCurrentCharge;

    bool _chargeEnabled;
    bool _dischargeEnabled;
};

} // namespace BatteryNs::SBS
