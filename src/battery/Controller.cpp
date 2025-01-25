// SPDX-License-Identifier: GPL-2.0-or-later
#include <battery/Controller.h>
#include <battery/jbdbms/Provider.h>
#include <battery/jkbms/Provider.h>
#include <battery/mqtt/Provider.h>
#include <battery/pylontech/Provider.h>
#include <battery/pytes/Provider.h>
#include <battery/sbs/Provider.h>
#include <battery/victronsmartshunt/Provider.h>
#include <battery/zendure/Provider.h>
#include <Configuration.h>
#include <MessageOutput.h>
#include <MqttSettings.h>

BatteryNs::Controller Battery;

namespace BatteryNs {

std::shared_ptr<Stats const> Controller::getStats() const
{
    std::lock_guard<std::mutex> lock(_mutex);

    if (!_upProvider) {
        static auto sspDummyStats = std::make_shared<Stats>();
        return sspDummyStats;
    }

    return _upProvider->getStats();
}

void Controller::init(Scheduler& scheduler)
{
    scheduler.addTask(_loopTask);
    _loopTask.setCallback(std::bind(&Controller::loop, this));
    _loopTask.setIterations(TASK_FOREVER);
    _loopTask.enable();

    this->updateSettings();
}

void Controller::updateSettings()
{
    std::lock_guard<std::mutex> lock(_mutex);

    if (_upProvider) {
        _upProvider->deinit();
        _upProvider = nullptr;
    }

    auto const& config = Configuration.get();
    if (!config.Battery.Enabled) { return; }

    bool verboseLogging = config.Battery.VerboseLogging;

    switch (config.Battery.Provider) {
        case 0:
            _upProvider = std::make_unique<Pylontech::Provider>();
            break;
        case 1:
            _upProvider = std::make_unique<JkBms::Provider>();
            break;
        case 2:
            _upProvider = std::make_unique<Mqtt::Provider>();
            break;
        case 3:
            _upProvider = std::make_unique<VictronSmartShunt::Provider>();
            break;
        case 4:
            _upProvider = std::make_unique<Pytes::Provider>();
            break;
        case 5:
            _upProvider = std::make_unique<SBS::Provider>();
            break;
        case 6:
            _upProvider = std::make_unique<JbdBms::Provider>();
            break;
        case 7:
            _upProvider = std::make_unique<Zendure::Provider>();
            break;
        default:
            MessageOutput.printf("[Battery] Unknown provider: %d\r\n", config.Battery.Provider);
            return;
    }

    if (!_upProvider->init(verboseLogging)) { _upProvider = nullptr; }

    _publishSensors = true;
}

void Controller::loop()
{
    std::lock_guard<std::mutex> lock(_mutex);

    if (!_upProvider) { return; }

    _upProvider->loop();

    _upProvider->getStats()->mqttLoop();

    auto const& config = Configuration.get();
    if (!config.Mqtt.Hass.Enabled) { return; }

    // TODO(schlimmchen): this cannot make sure that transient
    // connection problems are actually always noticed.
    if (!MqttSettings.getConnected()) {
        _publishSensors = true;
        return;
    }

    if (!_publishSensors) { return; }

    _upProvider->getHassIntegration().publishSensors();

    _publishSensors = false;
}

float Controller::getDischargeCurrentLimit()
{
    auto const& config = Configuration.get();

    if (!config.Battery.EnableDischargeCurrentLimit) { return FLT_MAX; }

    auto dischargeCurrentLimit = config.Battery.DischargeCurrentLimit;
    auto dischargeCurrentLimitValid = dischargeCurrentLimit > 0.0f;
    auto dischargeCurrentLimitBelowSoc = config.Battery.DischargeCurrentLimitBelowSoc;
    auto dischargeCurrentLimitBelowVoltage = config.Battery.DischargeCurrentLimitBelowVoltage;
    auto statsSoCValid = getStats()->getSoCAgeSeconds() <= 60 && !config.PowerLimiter.IgnoreSoc;
    auto statsSoC = statsSoCValid ? getStats()->getSoC() : 100.0; // fail open so we use voltage instead
    auto statsVoltageValid = getStats()->getVoltageAgeSeconds() <= 60;
    auto statsVoltage = statsVoltageValid ? getStats()->getVoltage() : 0.0; // fail closed
    auto statsCurrentLimit = getStats()->getDischargeCurrentLimit();
    auto statsLimitValid = config.Battery.UseBatteryReportedDischargeCurrentLimit
        && statsCurrentLimit >= 0.0f
        && getStats()->getDischargeCurrentLimitAgeSeconds() <= 60;


    if (statsSoC > dischargeCurrentLimitBelowSoc && statsVoltage > dischargeCurrentLimitBelowVoltage) {
        // Above SoC and Voltage thresholds, ignore custom limit.
        // Battery-provided limit will still be applied.
        dischargeCurrentLimitValid = false;
    }

    if (statsLimitValid && dischargeCurrentLimitValid) {
        // take the lowest limit
        return min(statsCurrentLimit, dischargeCurrentLimit);
    }

    if (statsLimitValid) {
        return statsCurrentLimit;
    }

    if (dischargeCurrentLimitValid) {
        return dischargeCurrentLimit;
    }

    return FLT_MAX;
}

} // namespace BatteryNs
