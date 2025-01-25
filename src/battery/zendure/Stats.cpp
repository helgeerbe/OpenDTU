// SPDX-License-Identifier: GPL-2.0-or-later
#include <MqttSettings.h>
#include <battery/zendure/Stats.h>
#include <Utils.h>

namespace BatteryNs::Zendure {

void Stats::getLiveViewData(JsonVariant& root) const
{
    ::BatteryNs::Stats::getLiveViewData(root);

    auto addRemainingTime = [this](auto root, auto section, const char* name, int16_t value, bool charge = false) {
        bool notInScope = charge ? !isCharging(this->_state) : !isDischarging(this->_state);
        if (value < 0 || notInScope) {
            addLiveViewTextInSection(root, section, name, "unavail");
        }else{
            addLiveViewInSection(root, section, name, value, "min", 0);
        }
    };

    // values go into the "Status" card of the web application
    std::string section("status");
    addLiveViewInSection(root, section, "totalInputPower", _input_power, "W", 0);
    addLiveViewInSection(root, section, "chargePower", _charge_power, "W", 0);
    addLiveViewInSection(root, section, "dischargePower", _discharge_power, "W", 0);
    addLiveViewInSection(root, section, "totalOutputPower", _output_power, "W", 0);
    addLiveViewInSection(root, section, "efficiency", _efficiency, "%", 3);
    addLiveViewInSection(root, section, "batteries", _num_batteries, "", 0);
    addLiveViewInSection(root, section, "capacity", _capacity, "Wh", 0);
    addLiveViewInSection(root, section, "availableCapacity", _capacity_avail, "Wh", 0);
    addLiveViewInSection(root, section, "useableCapacity", getUseableCapacity(), "Wh", 0);
    addLiveViewTextInSection(root, section, "state", stateToString<std::string>(_state));
    addLiveViewBooleanInSection(root, section, "heatState", _heat_state);
    addLiveViewBooleanInSection(root, section, "bypassState", _bypass_state);
    addLiveViewBooleanInSection(root, section, "chargethrough", _charge_through_state);
    addLiveViewInSection(root, section, "lastFullCharge", _last_full_charge_hours, "h", 0);
    addRemainingTime(root, section, "remainOutTime", _remain_out_time, false);
    addRemainingTime(root, section, "remainInTime", _remain_in_time, true);

    // values go into the "Settings" card of the web application
    section = "settings";
    addLiveViewInSection(root, section, "maxInversePower", _inverse_max, "W", 0);
    addLiveViewInSection(root, section, "outputLimit", _output_limit, "W", 0);
    addLiveViewInSection(root, section, "inputLimit", _output_limit, "W", 0);
    addLiveViewInSection(root, section, "minSoC", _soc_min, "%", 1);
    addLiveViewInSection(root, section, "maxSoC", _soc_max, "%", 1);
    addLiveViewBooleanInSection(root, section, "autoRecover", _auto_recover);
    addLiveViewBooleanInSection(root, section, "autoShutdown", _auto_shutdown);
    addLiveViewTextInSection(root, section, "bypassMode", bypassModeToString<std::string>(_bypass_mode));
    addLiveViewBooleanInSection(root, section, "buzzer", _buzzer);

    // values go into the "Solar Panels" card of the web application
    section = "panels";
    addLiveViewInSection(root, section, "solarInputPower1", _solar_power_1, "W", 0);
    addLiveViewInSection(root, section, "solarInputPower2", _solar_power_2, "W", 0);

    // pack data goes to dedicated cards of the web application
    char buff[30];
    for (const auto& [index, value] : _packData) {
        snprintf(buff, sizeof(buff), "_%s [%s]", value->getName().c_str(), value->getSerial().c_str());
        section = std::string(buff);
        addLiveViewTextInSection(root, section, "state", stateToString<std::string>(value->_state));
        addLiveViewInSection(root, section, "cellMinVoltage", value->_cell_voltage_min, "mV", 0);
        addLiveViewInSection(root, section, "cellAvgVoltage", value->_cell_voltage_avg, "mV", 0);
        addLiveViewInSection(root, section, "cellMaxVoltage", value->_cell_voltage_max, "mV", 0);
        addLiveViewInSection(root, section, "cellDiffVoltage", value->_cell_voltage_spread, "mV", 0);
        addLiveViewInSection(root, section, "cellMaxTemperature", value->_cell_temperature_max, "°C", 1);
        addLiveViewInSection(root, section, "voltage", value->_voltage_total, "V", 2);
        addLiveViewInSection(root, section, "power", value->_power, "W", 0);
        addLiveViewInSection(root, section, "current", value->_current, "A", 2);
        addLiveViewInSection(root, section, "SoC", value->_soc_level, "%", 1);
        addLiveViewInSection(root, section, "stateOfHealth", value->_state_of_health, "%", 1);
        addLiveViewInSection(root, section, "capacity", value->_capacity, "Wh", 0);
        addLiveViewInSection(root, section, "availableCapacity", value->_capacity_avail, "Wh", 0);
        addLiveViewTextInSection(root, section, "FwVersion", std::string(value->_fwversion.c_str()), false);
    }
}

void Stats::mqttPublish() const
{
    ::BatteryNs::Stats::mqttPublish();

    MqttSettings.publish("battery/cellMinMilliVolt", String(_cellMinMilliVolt));
    MqttSettings.publish("battery/cellAvgMilliVolt", String(_cellAvgMilliVolt));
    MqttSettings.publish("battery/cellMaxMilliVolt", String(_cellMaxMilliVolt));
    MqttSettings.publish("battery/cellDiffMilliVolt", String(_cellDeltaMilliVolt));
    MqttSettings.publish("battery/cellMaxTemperature", String(_cellTemperature));
    MqttSettings.publish("battery/chargePower", String(_charge_power));
    MqttSettings.publish("battery/dischargePower", String(_discharge_power));
    MqttSettings.publish("battery/heating", String(static_cast<uint8_t>(_heat_state)));
    MqttSettings.publish("battery/state", String(static_cast<uint8_t>(_state)));
    MqttSettings.publish("battery/numPacks", String(_num_batteries));
    MqttSettings.publish("battery/efficiency", String(_efficiency));
    MqttSettings.publish("battery/serial", _serial);

    for (const auto& [index, value] : _packData) {
        auto sn = String(index);
        MqttSettings.publish("battery/" + sn + "/cellMinMilliVolt", String(value->_cell_voltage_min));
        MqttSettings.publish("battery/" + sn + "/cellMaxMilliVolt", String(value->_cell_voltage_max));
        MqttSettings.publish("battery/" + sn + "/cellDiffMilliVolt", String(value->_cell_voltage_spread));
        MqttSettings.publish("battery/" + sn + "/cellAvgMilliVolt", String(value->_cell_voltage_avg));
        MqttSettings.publish("battery/" + sn + "/cellMaxTemperature", String(value->_cell_temperature_max));
        MqttSettings.publish("battery/" + sn + "/voltage", String(value->_voltage_total));
        MqttSettings.publish("battery/" + sn + "/power", String(value->_power));
        MqttSettings.publish("battery/" + sn + "/current", String(value->_current));
        MqttSettings.publish("battery/" + sn + "/stateOfCharge", String(value->_soc_level, 1));
        MqttSettings.publish("battery/" + sn + "/stateOfHealth", String(value->_state_of_health, 1));
        MqttSettings.publish("battery/" + sn + "/state", String(static_cast<uint8_t>(value->_state)));
        MqttSettings.publish("battery/" + sn + "/serial", value->getSerial());
        MqttSettings.publish("battery/" + sn + "/name", value->getName());
        MqttSettings.publish("battery/" + sn + "/capacity", String(value->_capacity));
    }

    MqttSettings.publish("battery/solarPowerMppt1", String(_solar_power_1));
    MqttSettings.publish("battery/solarPowerMppt2", String(_solar_power_2));
    MqttSettings.publish("battery/outputPower", String(_output_power));
    MqttSettings.publish("battery/inputPower", String(_input_power));
    MqttSettings.publish("battery/bypass", String(static_cast<uint8_t>(_bypass_state)));
    if (_last_full_charge_hours.has_value()) {
        MqttSettings.publish("battery/lastFullCharge", String(*_last_full_charge_hours));
    }

    MqttSettings.publish("battery/settings/outputLimitPower", String(_output_limit));
    MqttSettings.publish("battery/settings/inputLimitPower", String(_input_limit));
    MqttSettings.publish("battery/settings/stateOfChargeMin", String(_soc_min));
    MqttSettings.publish("battery/settings/stateOfChargeMax", String(_soc_max));
    MqttSettings.publish("battery/settings/bypassModeString", bypassModeToString<String>(_bypass_mode));
    MqttSettings.publish("battery/settings/bypassMode", String(static_cast<uint8_t>(_bypass_mode)));
}

std::shared_ptr<PackStats> Stats::getPackData(size_t index) const {
    try
    {
        return _packData.at(index);
    }
    catch(const std::out_of_range& ex)
    {
        return nullptr;
    }
}

std::shared_ptr<PackStats> Stats::addPackData(size_t index, String serial) {
    std::shared_ptr<PackStats> pack;
    try
    {
        pack = _packData.at(index);
        pack->setSerial(serial);
    }
    catch(const std::out_of_range& ex)
    {
        pack = PackStats::fromSerial(serial);

        if (pack != nullptr) {
            _packData[index] = pack;
        }
    }
    return pack;
}

} // namespace BatteryNs::Zendure
