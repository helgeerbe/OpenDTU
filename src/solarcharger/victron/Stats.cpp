// SPDX-License-Identifier: GPL-2.0-or-later
#include <Configuration.h>
#include <MqttSettings.h>
#include <MessageOutput.h>
#include <solarcharger/victron/Stats.h>

namespace SolarChargers::Victron {

void Stats::update(const String serial, const std::optional<VeDirectMpptController::data_t> mpptData, uint32_t lastUpdate) const
{
    // serial required as index
    if (serial.isEmpty()) { return; }

    _data[serial] = mpptData;
    _lastUpdate[serial] = lastUpdate;
}

uint32_t Stats::getAgeMillis() const
{
    uint32_t age = 0;
    auto now = millis();

    for (auto const& entry : _data) {
        if (!entry.second) { continue; }
        if (!_lastUpdate[entry.first]) { continue; }

        age = std::max<uint32_t>(age, now - _lastUpdate[entry.first]);
    }

    return age;

}

std::optional<float> Stats::getOutputPowerWatts() const
{
    float sum = 0;
    auto data = false;

    for (auto const& entry : _data) {
        if (!entry.second) { continue; }
        data = true;
        sum += entry.second->batteryOutputPower_W;
    }

    if (!data) { return std::nullopt; }

    return sum;
}

std::optional<float> Stats::getOutputVoltage() const
{
    float min = -1;

    for (auto const& entry : _data) {
        if (!entry.second) { continue; }

        float volts = entry.second->batteryVoltage_V_mV / 1000.0;
        if (min == -1) { min = volts; }
        min = std::min(min, volts);
    }

    if (min == -1) { return std::nullopt; }

    return min;
}

std::optional<uint16_t> Stats::getPanelPowerWatts() const
{
    uint16_t sum = 0;
    auto data = false;

    for (auto const& entry : _data) {
        if (!entry.second) { continue; }
        data = true;

        // if any charge controller is part of a VE.Smart network, and if the
        // charge controller is connected in a way that allows to send
        // requests, we should have the "network total DC input power" available.
        auto networkPower = entry.second->NetworkTotalDcInputPowerMilliWatts;
        if (networkPower.first > 0) {
            return static_cast<int32_t>(networkPower.second / 1000.0);
        }

        sum += entry.second->panelPower_PPV_W;
    }

    if (!data) { return std::nullopt; }

    return sum;
}

std::optional<float> Stats::getYieldTotal() const
{
    float sum = 0;

    for (auto const& entry : _data) {
        if (!entry.second) { continue; }

        sum += entry.second->yieldTotal_H19_Wh / 1000.0;
    }

    return sum;
}

std::optional<float> Stats::getYieldDay() const
{
    float sum = 0;

    for (auto const& entry : _data) {
        if (!entry.second) { continue; }

        sum += entry.second->yieldToday_H20_Wh;
    }

    return sum;
}

std::optional<Stats::StateOfOperation> Stats::getStateOfOperation() const
{
    for (auto const& entry : _data) {
        if (!entry.second) { continue; }
        // see victron protocol documentation for CS values
        switch (entry.second->currentState_CS) {
            case 0: return Stats::StateOfOperation::Off;
            case 3: return Stats::StateOfOperation::Bulk;
            case 246:
            case 4: return Stats::StateOfOperation::Absorption;
            case 5: return Stats::StateOfOperation::Float;
            default: return Stats::StateOfOperation::Various;
        }
    }
    return std::nullopt;
}

std::optional<float> Stats::getFloatVoltage() const
{
    for (auto const& entry : _data) {
        if (!entry.second) { continue; }
        auto voltage = entry.second->BatteryFloatMilliVolt;
        if (voltage.first > 0) { // only return valid and not outdated value
            return voltage.second / 1000.0;
        }
    }
    return std::nullopt;
}

std::optional<float> Stats::getAbsorptionVoltage() const
{
    for (auto const& entry : _data) {
        if (!entry.second) { continue; }
        auto voltage = entry.second->BatteryAbsorptionMilliVolt;
        if (voltage.first > 0) { // only return valid and not outdated value
            return voltage.second / 1000.0;
        }
    }
    return std::nullopt;
}

void Stats::getLiveViewData(JsonVariant& root, const boolean fullUpdate, const uint32_t lastPublish) const
{
    ::SolarChargers::Stats::getLiveViewData(root, fullUpdate, lastPublish);

    auto instances = root["solarcharger"]["instances"].to<JsonObject>();

    for (auto const& entry : _data) {
        if (!entry.second) { continue; }

        auto age = 0;
        if (_lastUpdate[entry.first]) {
            age = millis() - _lastUpdate[entry.first];
        }

        auto hasUpdate = age != 0 && age < millis() - lastPublish;
        if (!fullUpdate && !hasUpdate) { continue; }

        JsonObject instance = instances[entry.first].to<JsonObject>();
        instance["data_age_ms"] = age;
        instance["hide_serial"] = false;
        populateJsonWithInstanceStats(instance, *entry.second);
    }
}

void Stats::populateJsonWithInstanceStats(const JsonObject &root, const VeDirectMpptController::data_t &mpptData) const
{
    root["product_id"] = mpptData.getPidAsString();
    root["firmware_version"] = mpptData.getFwVersionFormatted();

    const JsonObject values = root["values"].to<JsonObject>();

    const JsonObject device = values["device"].to<JsonObject>();

    // LOAD     IL      UI label    result
    // ------------------------------------
    // false    false               Do not display LOAD and IL (device has no physical load output and virtual load is not configured)
    // true     false   "VIRTLOAD"  We display just LOAD (device has no physical load output and virtual load is configured)
    // true     true    "LOAD"      We display LOAD and IL (device has physical load output, regardless if virtual load is configured or not)
    if (mpptData.loadOutputState_LOAD.first > 0) {
        device[(mpptData.loadCurrent_IL_mA.first > 0) ? "LOAD" : "VIRTLOAD"] = mpptData.loadOutputState_LOAD.second ? "ON" : "OFF";
    }
    if (mpptData.loadCurrent_IL_mA.first > 0) {
        device["IL"]["v"] = mpptData.loadCurrent_IL_mA.second / 1000.0;
        device["IL"]["u"] = "A";
        device["IL"]["d"] = 2;
    }
    device["CS"] = mpptData.getCsAsString();
    device["MPPT"] = mpptData.getMpptAsString();
    device["OR"] = mpptData.getOrAsString();
    if (mpptData.relayState_RELAY.first > 0) {
        device["RELAY"] = mpptData.relayState_RELAY.second ? "ON" : "OFF";
    }
    device["ERR"] = mpptData.getErrAsString();
    device["HSDS"]["v"] = mpptData.daySequenceNr_HSDS;
    device["HSDS"]["u"] = "d";
    if (mpptData.MpptTemperatureMilliCelsius.first > 0) {
        device["MpptTemperature"]["v"] = mpptData.MpptTemperatureMilliCelsius.second / 1000.0;
        device["MpptTemperature"]["u"] = "°C";
        device["MpptTemperature"]["d"] = "1";
    }

    const JsonObject output = values["output"].to<JsonObject>();
    output["P"]["v"] = mpptData.batteryOutputPower_W;
    output["P"]["u"] = "W";
    output["P"]["d"] = 0;
    output["V"]["v"] = mpptData.batteryVoltage_V_mV / 1000.0;
    output["V"]["u"] = "V";
    output["V"]["d"] = 2;
    output["I"]["v"] = mpptData.batteryCurrent_I_mA / 1000.0;
    output["I"]["u"] = "A";
    output["I"]["d"] = 2;
    output["E"]["v"] = mpptData.mpptEfficiency_Percent;
    output["E"]["u"] = "%";
    output["E"]["d"] = 1;
    if (mpptData.SmartBatterySenseTemperatureMilliCelsius.first > 0) {
        output["SBSTemperature"]["v"] = mpptData.SmartBatterySenseTemperatureMilliCelsius.second / 1000.0;
        output["SBSTemperature"]["u"] = "°C";
        output["SBSTemperature"]["d"] = "0";
    }
    if (mpptData.BatteryAbsorptionMilliVolt.first > 0) {
        output["AbsorptionVoltage"]["v"] = mpptData.BatteryAbsorptionMilliVolt.second / 1000.0;
        output["AbsorptionVoltage"]["u"] = "V";
        output["AbsorptionVoltage"]["d"] = "2";
    }
    if (mpptData.BatteryFloatMilliVolt.first > 0) {
        output["FloatVoltage"]["v"] = mpptData.BatteryFloatMilliVolt.second / 1000.0;
        output["FloatVoltage"]["u"] = "V";
        output["FloatVoltage"]["d"] = "2";
    }

    const JsonObject input = values["input"].to<JsonObject>();
    if (mpptData.NetworkTotalDcInputPowerMilliWatts.first > 0) {
        input["NetworkPower"]["v"] = mpptData.NetworkTotalDcInputPowerMilliWatts.second / 1000.0;
        input["NetworkPower"]["u"] = "W";
        input["NetworkPower"]["d"] = "0";
    }
    input["PPV"]["v"] = mpptData.panelPower_PPV_W;
    input["PPV"]["u"] = "W";
    input["PPV"]["d"] = 0;
    input["VPV"]["v"] = mpptData.panelVoltage_VPV_mV / 1000.0;
    input["VPV"]["u"] = "V";
    input["VPV"]["d"] = 2;
    input["IPV"]["v"] = mpptData.panelCurrent_mA / 1000.0;
    input["IPV"]["u"] = "A";
    input["IPV"]["d"] = 2;

    if (mpptData.yieldToday_H20_Wh >= 1000.0) {
        input["YieldToday"]["v"] = mpptData.yieldToday_H20_Wh / 1000.0;
        input["YieldToday"]["u"] = "kWh";
        input["YieldToday"]["d"] = 2;
    } else {
        input["YieldToday"]["v"] = mpptData.yieldToday_H20_Wh;
        input["YieldToday"]["u"] = "Wh";
        input["YieldToday"]["d"] = 0;
    }

    if (mpptData.yieldYesterday_H22_Wh >= 1000.0) {
        input["YieldYesterday"]["v"] = mpptData.yieldYesterday_H22_Wh / 1000.0;
        input["YieldYesterday"]["u"] = "kWh";
        input["YieldYesterday"]["d"] = 2;
    } else {
        input["YieldYesterday"]["v"] = mpptData.yieldYesterday_H22_Wh;
        input["YieldYesterday"]["u"] = "Wh";
        input["YieldYesterday"]["d"] = 0;
    }

    input["YieldTotal"]["v"] = mpptData.yieldTotal_H19_Wh / 1000.0;
    input["YieldTotal"]["u"] = "kWh";
    input["YieldTotal"]["d"] = 2;
    input["MaximumPowerToday"]["v"] = mpptData.maxPowerToday_H21_W;
    input["MaximumPowerToday"]["u"] = "W";
    input["MaximumPowerToday"]["d"] = 0;
    input["MaximumPowerYesterday"]["v"] = mpptData.maxPowerYesterday_H23_W;
    input["MaximumPowerYesterday"]["u"] = "W";
    input["MaximumPowerYesterday"]["d"] = 0;
}

void Stats::mqttPublish() const
{
    if ((millis() >= _nextPublishFull) || (millis() >= _nextPublishUpdatesOnly)) {
        auto const& config = Configuration.get();

        // determine if this cycle should publish full values or updates only
        if (_nextPublishFull <= _nextPublishUpdatesOnly) {
            _PublishFull = true;
        } else {
            _PublishFull = !config.SolarCharger.PublishUpdatesOnly;
        }

        for (auto const& entry : _data) {
            auto currentData = entry.second;
            if (!currentData) { continue; }

            auto const& previousData = _previousData[entry.first];
            publishMpptData(*currentData, previousData);

            if (!_PublishFull) {
                _previousData[entry.first] = *currentData;
            }
        }

        // now calculate next points of time to publish
        _nextPublishUpdatesOnly = millis() + ::SolarChargers::Stats::getMqttFullPublishIntervalMs();

        if (_PublishFull) {
            // when Home Assistant MQTT-Auto-Discovery is active,
            // and "enable expiration" is active, all values must be published at
            // least once before the announced expiry interval is reached
            if ((config.SolarCharger.PublishUpdatesOnly) && (config.Mqtt.Hass.Enabled) && (config.Mqtt.Hass.Expire)) {
                _nextPublishFull = millis() + (((config.Mqtt.PublishInterval * 3) - 1) * 1000);

            } else {
                // no future publish full needed
                _nextPublishFull = UINT32_MAX;
            }
        }
    }
}

void Stats::publishMpptData(const VeDirectMpptController::data_t &currentData, const VeDirectMpptController::data_t &previousData) const {
    String value;
    String topic = "victron/";
    topic.concat(currentData.serialNr_SER);
    topic.concat("/");

#define PUBLISH(sm, t, val) \
    if (_PublishFull || currentData.sm != previousData.sm) { \
        MqttSettings.publish(topic + t, String(val)); \
    }

    PUBLISH(productID_PID,           "PID",  currentData.getPidAsString().data());
    PUBLISH(serialNr_SER,            "SER",  currentData.serialNr_SER);
    PUBLISH(firmwareVer_FW,          "FWI",  currentData.getFwVersionAsInteger());
    PUBLISH(firmwareVer_FW,          "FWF",  currentData.getFwVersionFormatted());
    PUBLISH(firmwareVer_FW,           "FW",  currentData.firmwareVer_FW);
    PUBLISH(firmwareVer_FWE,         "FWE",  currentData.firmwareVer_FWE);
    PUBLISH(currentState_CS,          "CS",  currentData.getCsAsString().data());
    PUBLISH(errorCode_ERR,           "ERR",  currentData.getErrAsString().data());
    PUBLISH(offReason_OR,             "OR",  currentData.getOrAsString().data());
    PUBLISH(stateOfTracker_MPPT,    "MPPT",  currentData.getMpptAsString().data());
    PUBLISH(daySequenceNr_HSDS,     "HSDS",  currentData.daySequenceNr_HSDS);
    PUBLISH(batteryVoltage_V_mV,       "V",  currentData.batteryVoltage_V_mV / 1000.0);
    PUBLISH(batteryCurrent_I_mA,       "I",  currentData.batteryCurrent_I_mA / 1000.0);
    PUBLISH(batteryOutputPower_W,      "P",  currentData.batteryOutputPower_W);
    PUBLISH(panelVoltage_VPV_mV,     "VPV",  currentData.panelVoltage_VPV_mV / 1000.0);
    PUBLISH(panelCurrent_mA,         "IPV",  currentData.panelCurrent_mA / 1000.0);
    PUBLISH(panelPower_PPV_W,        "PPV",  currentData.panelPower_PPV_W);
    PUBLISH(mpptEfficiency_Percent,    "E",  currentData.mpptEfficiency_Percent);
    PUBLISH(yieldTotal_H19_Wh,       "H19",  currentData.yieldTotal_H19_Wh / 1000.0);
    PUBLISH(yieldToday_H20_Wh,       "H20",  currentData.yieldToday_H20_Wh / 1000.0);
    PUBLISH(maxPowerToday_H21_W,     "H21",  currentData.maxPowerToday_H21_W);
    PUBLISH(yieldYesterday_H22_Wh,   "H22",  currentData.yieldYesterday_H22_Wh / 1000.0);
    PUBLISH(maxPowerYesterday_H23_W, "H23",  currentData.maxPowerYesterday_H23_W);
#undef PUBLILSH

#define PUBLISH_OPT(sm, t, val) \
    if (currentData.sm.first != 0 && (_PublishFull || currentData.sm.second != previousData.sm.second)) { \
        MqttSettings.publish(topic + t, String(val)); \
    }

    PUBLISH_OPT(relayState_RELAY,                         "RELAY",                        currentData.relayState_RELAY.second ? "ON" : "OFF");
    PUBLISH_OPT(loadOutputState_LOAD,                     "LOAD",                         currentData.loadOutputState_LOAD.second ? "ON" : "OFF");
    PUBLISH_OPT(loadCurrent_IL_mA,                        "IL",                           currentData.loadCurrent_IL_mA.second / 1000.0);
    PUBLISH_OPT(NetworkTotalDcInputPowerMilliWatts,       "NetworkTotalDcInputPower",     currentData.NetworkTotalDcInputPowerMilliWatts.second / 1000.0);
    PUBLISH_OPT(MpptTemperatureMilliCelsius,              "MpptTemperature",              currentData.MpptTemperatureMilliCelsius.second / 1000.0);
    PUBLISH_OPT(BatteryAbsorptionMilliVolt,               "BatteryAbsorption",            currentData.BatteryAbsorptionMilliVolt.second / 1000.0);
    PUBLISH_OPT(BatteryFloatMilliVolt,                    "BatteryFloat",                 currentData.BatteryFloatMilliVolt.second / 1000.0);
    PUBLISH_OPT(SmartBatterySenseTemperatureMilliCelsius, "SmartBatterySenseTemperature", currentData.SmartBatterySenseTemperatureMilliCelsius.second / 1000.0);
#undef PUBLILSH_OPT
}

void Stats::mqttPublishSensors(const boolean forcePublish) const
{
    // TODO(andreasboehm): sensors are only published once, and then never again.
    // This matches the old implementation, but is not ideal. We should publish
    // sensors whenever a new controller is discovered, or when the amount of available
    // datapoints for a controller changed.
    if (!forcePublish) { return; }

    for (auto entry : _data) {
        if (!entry.second) { continue; }
        _hassIntegration.publishSensors(*entry.second);
    }
}

}; // namespace SolarChargers::Victron
