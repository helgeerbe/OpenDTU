// SPDX-License-Identifier: GPL-2.0-or-later
#include <algorithm>
#include <string>
#include <vector>
#include <MqttSettings.h>
#include <battery/jkbms/Stats.h>
#include <battery/jkbms/DataPoints.h>

namespace BatteryNs::JkBms {

void Stats::getJsonData(JsonVariant& root, bool verbose) const
{
    ::BatteryNs::Stats::getLiveViewData(root);

    using Label = JkBms::DataPointLabel;

    auto oCurrent = _dataPoints.get<Label::BatteryCurrentMilliAmps>();
    auto oVoltage = _dataPoints.get<Label::BatteryVoltageMilliVolt>();
    if (oVoltage.has_value() && oCurrent.has_value()) {
        auto current = static_cast<float>(*oCurrent) / 1000;
        auto voltage = static_cast<float>(*oVoltage) / 1000;
        addLiveViewValue(root, "power", current * voltage , "W", 2);
    }

    auto oTemperatureBms = _dataPoints.get<Label::BmsTempCelsius>();
    if (oTemperatureBms.has_value()) {
        addLiveViewValue(root, "bmsTemp", *oTemperatureBms, "°C", 0);
    }

    // labels BatteryChargeEnabled, BatteryDischargeEnabled, and
    // BalancingEnabled refer to the user setting. we want to show the
    // actual MOSFETs' state which control whether charging and discharging
    // is possible and whether the BMS is currently balancing cells.
    auto oStatus = _dataPoints.get<Label::StatusBitmask>();
    if (oStatus.has_value()) {
        using Bits = JkBms::StatusBits;
        auto chargeEnabled = *oStatus & static_cast<uint16_t>(Bits::ChargingActive);
        addLiveViewTextValue(root, "chargeEnabled", (chargeEnabled?"yes":"no"));
        auto dischargeEnabled = *oStatus & static_cast<uint16_t>(Bits::DischargingActive);
        addLiveViewTextValue(root, "dischargeEnabled", (dischargeEnabled?"yes":"no"));
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

    if (oStatus.has_value()) {
        using Bits = JkBms::StatusBits;
        auto balancingActive = *oStatus & static_cast<uint16_t>(Bits::BalancingActive);
        addLiveViewTextInSection(root, "cells", "balancingActive", (balancingActive?"yes":"no"));
    }

    auto oAlarms = _dataPoints.get<Label::AlarmsBitmask>();
    if (oAlarms.has_value()) {
#define ISSUE(t, x) \
        auto x = *oAlarms & static_cast<uint16_t>(JkBms::AlarmBits::x); \
        addLiveView##t(root, "JkBmsIssue"#x, x > 0);

        ISSUE(Warning, LowCapacity);
        ISSUE(Alarm, BmsOvertemperature);
        ISSUE(Alarm, ChargingOvervoltage);
        ISSUE(Alarm, DischargeUndervoltage);
        ISSUE(Alarm, BatteryOvertemperature);
        ISSUE(Alarm, ChargingOvercurrent);
        ISSUE(Alarm, DischargeOvercurrent);
        ISSUE(Alarm, CellVoltageDifference);
        ISSUE(Alarm, BatteryBoxOvertemperature);
        ISSUE(Alarm, BatteryUndertemperature);
        ISSUE(Alarm, CellOvervoltage);
        ISSUE(Alarm, CellUndervoltage);
        ISSUE(Alarm, AProtect);
        ISSUE(Alarm, BProtect);
#undef ISSUE
    }
}

void Stats::mqttPublish() const
{
    ::BatteryNs::Stats::mqttPublish();

    using Label = JkBms::DataPointLabel;

    static std::vector<Label> mqttSkip = {
        Label::CellsMilliVolt, // complex data format
        Label::ModificationPassword, // sensitive data
        Label::BatterySoCPercent // already published by base class
        // NOTE that voltage is also published by the base class, however, we
        // previously published it only from here using the respective topic.
        // to avoid a breaking change, we publish the value again using the
        // "old" topic.
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
        for (auto iter = JkBms::AlarmBitTexts.begin(); iter != JkBms::AlarmBitTexts.end(); ++iter) {
            auto bit = iter->first;
            String value = (*oAlarms & static_cast<uint16_t>(bit))?"1":"0";
            MqttSettings.publish(String("battery/alarms/") + iter->second.data(), value);
        }
    }

    auto oStatus = _dataPoints.get<Label::StatusBitmask>();
    if (oStatus.has_value()) {
        for (auto iter = JkBms::StatusBitTexts.begin(); iter != JkBms::StatusBitTexts.end(); ++iter) {
            auto bit = iter->first;
            String value = (*oStatus & static_cast<uint16_t>(bit))?"1":"0";
            MqttSettings.publish(String("battery/status/") + iter->second.data(), value);
        }
    }

    _lastMqttPublish = millis();
    if (fullPublish) { _lastFullMqttPublish = _lastMqttPublish; }
}

void Stats::updateFrom(JkBms::DataPointContainer const& dp)
{
    using Label = JkBms::DataPointLabel;

    setManufacturer("JKBMS");
    auto oProductId = dp.get<Label::ProductId>();
    if (oProductId.has_value()) {
        // the first twelve chars are expected to be the "User Private Data"
        // setting (see smartphone app). the remainder is expected be the BMS
        // name, which can be changed at will using the smartphone app. so
        // there is not always a "JK" in this string. if there is, we still cut
        // the string there to avoid possible regressions.
        setManufacturer(String(oProductId->substr(12).c_str()));
        auto pos = oProductId->rfind("JK");
        if (pos != std::string::npos) {
            setManufacturer(String(oProductId->substr(pos).c_str()));
        }
    }

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

    auto oVersion = _dataPoints.get<Label::BmsSoftwareVersion>();
    if (oVersion.has_value()) {
        // raw: "11.XW_S11.262H_"
        //   => Hardware "V11.XW" (displayed in Android app)
        //   => Software "V11.262H" (displayed in Android app)
        auto first = oVersion->find('_');
        if (first != std::string::npos) {
            _hwversion = oVersion->substr(0, first).c_str();

            auto second = oVersion->find('_', first + 1);

            // the 'S' seems to be merely an indicator for "software"?
            if (oVersion->at(first + 1) == 'S') { first++; }

            _fwversion = oVersion->substr(first + 1, second - first - 1).c_str();
        }
    }

    _lastUpdate = millis();
}

} // namespace BatteryNs::JkBms
