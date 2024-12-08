// SPDX-License-Identifier: GPL-2.0-or-later
#include <algorithm>
#include <string>
#include <vector>
#include <MqttSettings.h>
#include <battery/jbdbms/Stats.h>
#include <battery/jbdbms/DataPoints.h>

namespace BatteryNs::JbdBms {

void Stats::getJsonData(JsonVariant& root, bool verbose) const
{
    ::BatteryNs::Stats::getLiveViewData(root);

    using Label = JbdBms::DataPointLabel;

    auto oCurrent = _dataPoints.get<Label::BatteryCurrentMilliAmps>();
    auto oVoltage = _dataPoints.get<Label::BatteryVoltageMilliVolt>();
    if (oVoltage.has_value() && oCurrent.has_value()) {
        auto current = static_cast<float>(*oCurrent) / 1000;
        auto voltage = static_cast<float>(*oVoltage) / 1000;
        addLiveViewValue(root, "power", current * voltage , "W", 2);
    }

    auto oBatteryChargeEnabled = _dataPoints.get<Label::BatteryChargeEnabled>();
    if (oBatteryChargeEnabled.has_value()) {
        addLiveViewTextValue(root, "chargeEnabled", (*oBatteryChargeEnabled?"yes":"no"));
    }

    auto oBatteryDischargeEnabled = _dataPoints.get<Label::BatteryDischargeEnabled>();
    if (oBatteryDischargeEnabled.has_value()) {
        addLiveViewTextValue(root, "dischargeEnabled", (*oBatteryDischargeEnabled?"yes":"no"));
    }

    auto oTemperatureOne = _dataPoints.get<Label::BatteryTempOneCelsius>();
    if (oTemperatureOne.has_value()) {
        addLiveViewInSection(root, "cells", "batOneTemp", *oTemperatureOne, "°C", 0);
    }

    auto oTemperatureTwo = _dataPoints.get<Label::BatteryTempTwoCelsius>();
    if (oTemperatureTwo.has_value()) {
        addLiveViewInSection(root, "cells", "batTwoTemp", *oTemperatureTwo, "°C", 0);
    }

    if (_cellVoltageTimestamp > 0) {
        addLiveViewInSection(root, "cells", "cellMinVoltage", static_cast<float>(_cellMinMilliVolt)/1000, "V", 3);
        addLiveViewInSection(root, "cells", "cellAvgVoltage", static_cast<float>(_cellAvgMilliVolt)/1000, "V", 3);
        addLiveViewInSection(root, "cells", "cellMaxVoltage", static_cast<float>(_cellMaxMilliVolt)/1000, "V", 3);
        addLiveViewInSection(root, "cells", "cellDiffVoltage", (_cellMaxMilliVolt - _cellMinMilliVolt), "mV", 0);
    }

    auto oBalancingEnabled = _dataPoints.get<Label::BalancingEnabled>();
    if (oBalancingEnabled.has_value()) {
        addLiveViewTextInSection(root, "cells", "balancingActive", (*oBalancingEnabled?"yes":"no"));
    }

    auto oAlarms = _dataPoints.get<Label::AlarmsBitmask>();
    if (oAlarms.has_value()) {
#define ISSUE(t, x) \
        auto x = *oAlarms & static_cast<uint16_t>(JbdBms::AlarmBits::x); \
        addLiveView##t(root, "JbdBmsIssue"#x, x > 0);

        //ISSUE(Warning, LowCapacity);
        ISSUE(Alarm, CellOverVoltage);
        ISSUE(Alarm, CellUnderVoltage);
        ISSUE(Alarm, PackOverVoltage);
        ISSUE(Alarm, PackUnderVoltage);
        ISSUE(Alarm, ChargingOverTemperature);
        ISSUE(Alarm, ChargingLowTemperature);
        ISSUE(Alarm, DischargingOverTemperature);
        ISSUE(Alarm, DischargingLowTemperature);
        ISSUE(Alarm, ChargingOverCurrent);
        ISSUE(Alarm, DischargeOverCurrent);
        ISSUE(Alarm, ShortCircuit);
        ISSUE(Alarm, IcFrontEndError);
        ISSUE(Alarm, MosSotwareLock);
        ISSUE(Alarm, Reserved1);
        ISSUE(Alarm, Reserved2);
        ISSUE(Alarm, Reserved3);
#undef ISSUE
    }
}

void Stats::mqttPublish() const
{
    ::BatteryNs::Stats::mqttPublish();

    using Label = JbdBms::DataPointLabel;

    static std::vector<Label> mqttSkip = {
        Label::CellsMilliVolt, // complex data format
        Label::BatteryVoltageMilliVolt, // already published by base class
        Label::BatterySoCPercent // already published by base class
    };

    // regularly publish all topics regardless of whether or not their value changed
    bool neverFullyPublished = _lastFullMqttPublish == 0;
    bool intervalElapsed = _lastFullMqttPublish + getMqttFullPublishIntervalMs() < millis();
    bool fullPublish = neverFullyPublished || intervalElapsed;

    for (auto iter = _dataPoints.cbegin(); iter != _dataPoints.cend(); ++iter) {
        // skip data points that did not change since last published
        if (!fullPublish && iter->second.getTimestamp() < _lastMqttPublish) { continue; }

        auto skipMatch = std::find(mqttSkip.begin(), mqttSkip.end(), iter->first);
        if (skipMatch != mqttSkip.end()) { continue; }

        String topic((std::string("battery/") + iter->second.getLabelText()).c_str());
        MqttSettings.publish(topic, iter->second.getValueText().c_str());
    }

    auto oCellVoltages = _dataPoints.get<Label::CellsMilliVolt>();
    if (oCellVoltages.has_value() && (fullPublish || _cellVoltageTimestamp > _lastMqttPublish)) {
        unsigned idx = 1;
        for (auto iter = oCellVoltages->cbegin(); iter != oCellVoltages->cend(); ++iter) {
            String topic("battery/Cell");
            topic += String(idx);
            topic += "MilliVolt";

            MqttSettings.publish(topic, String(iter->second));

            ++idx;
        }

        MqttSettings.publish("battery/CellMinMilliVolt", String(_cellMinMilliVolt));
        MqttSettings.publish("battery/CellAvgMilliVolt", String(_cellAvgMilliVolt));
        MqttSettings.publish("battery/CellMaxMilliVolt", String(_cellMaxMilliVolt));
        MqttSettings.publish("battery/CellDiffMilliVolt", String(_cellMaxMilliVolt - _cellMinMilliVolt));
    }

    auto oAlarms = _dataPoints.get<Label::AlarmsBitmask>();
    if (oAlarms.has_value()) {
        for (auto iter = JbdBms::AlarmBitTexts.begin(); iter != JbdBms::AlarmBitTexts.end(); ++iter) {
            auto bit = iter->first;
            String value = (*oAlarms & static_cast<uint16_t>(bit))?"1":"0";
            MqttSettings.publish(String("battery/alarms/") + iter->second.data(), value);
        }
    }

    _lastMqttPublish = millis();
    if (fullPublish) { _lastFullMqttPublish = _lastMqttPublish; }
}

void Stats::updateFrom(JbdBms::DataPointContainer const& dp)
{
    using Label = JbdBms::DataPointLabel;

    setManufacturer("JBDBMS");

    auto oSoCValue = dp.get<Label::BatterySoCPercent>();
    if (oSoCValue.has_value()) {
        auto oSoCDataPoint = dp.getDataPointFor<Label::BatterySoCPercent>();
        ::BatteryNs::Stats::setSoC(*oSoCValue, 0/*precision*/,
                oSoCDataPoint->getTimestamp());
    }

    auto oVoltage = dp.get<Label::BatteryVoltageMilliVolt>();
    if (oVoltage.has_value()) {
        auto oVoltageDataPoint = dp.getDataPointFor<Label::BatteryVoltageMilliVolt>();
        ::BatteryNs::Stats::setVoltage(static_cast<float>(*oVoltage) / 1000,
                oVoltageDataPoint->getTimestamp());
    }

    auto oCurrent = dp.get<Label::BatteryCurrentMilliAmps>();
    if (oCurrent.has_value()) {
        auto oCurrentDataPoint = dp.getDataPointFor<Label::BatteryCurrentMilliAmps>();
        ::BatteryNs::Stats::setCurrent(static_cast<float>(*oCurrent) / 1000, 2/*precision*/,
                oCurrentDataPoint->getTimestamp());
    }

    _dataPoints.updateFrom(dp);

    auto oCellVoltages = _dataPoints.get<Label::CellsMilliVolt>();
    if (oCellVoltages.has_value()) {
        for (auto iter = oCellVoltages->cbegin(); iter != oCellVoltages->cend(); ++iter) {
            if (iter == oCellVoltages->cbegin()) {
                _cellMinMilliVolt = _cellAvgMilliVolt = _cellMaxMilliVolt = iter->second;
                continue;
            }
            _cellMinMilliVolt = std::min(_cellMinMilliVolt, iter->second);
            _cellAvgMilliVolt = (_cellAvgMilliVolt + iter->second) / 2;
            _cellMaxMilliVolt = std::max(_cellMaxMilliVolt, iter->second);
        }
        _cellVoltageTimestamp = millis();
    }

    auto oSoftwareVersion = _dataPoints.get<Label::BmsSoftwareVersion>();
    if (oSoftwareVersion.has_value()) {
        _fwversion = oSoftwareVersion->c_str();
    }

    auto oHardwareVersion = _dataPoints.get<Label::BmsHardwareVersion>();
    if (oHardwareVersion.has_value()) {
        _hwversion = oHardwareVersion->c_str();
    }

    _lastUpdate = millis();
}

} // namespace BatteryNs::JbdBms
