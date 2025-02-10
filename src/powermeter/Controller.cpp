// SPDX-License-Identifier: GPL-2.0-or-later
#include <powermeter/Controller.h>
#include <Configuration.h>
#include <powermeter/json/http/Provider.h>
#include <powermeter/sml/http/Provider.h>
#include <powermeter/mqtt/Provider.h>
#include <powermeter/sdm/serial/Provider.h>
#include <powermeter/sml/serial/Provider.h>
#include <powermeter/udp/smahm/Provider.h>

PowerMeters::Controller PowerMeter;

namespace PowerMeters {

void Controller::init(Scheduler& scheduler)
{
    scheduler.addTask(_loopTask);
    _loopTask.setCallback(std::bind(&Controller::loop, this));
    _loopTask.setIterations(TASK_FOREVER);
    _loopTask.enable();

    updateSettings();
}

void Controller::updateSettings()
{
    std::lock_guard<std::mutex> l(_mutex);

    if (_upProvider) { _upProvider.reset(); }

    auto const& pmcfg = Configuration.get().PowerMeter;

    if (!pmcfg.Enabled) { return; }

    switch(static_cast<Provider::Type>(pmcfg.Source)) {
        case Provider::Type::MQTT:
            _upProvider = std::make_unique<::PowerMeters::Mqtt::Provider>(pmcfg.Mqtt);
            break;
        case Provider::Type::SDM1PH:
            _upProvider = std::make_unique<::PowerMeters::Sdm::Serial::Provider>(
                    ::PowerMeters::Sdm::Serial::Provider::Phases::One, pmcfg.SerialSdm);
            break;
        case Provider::Type::SDM3PH:
            _upProvider = std::make_unique<::PowerMeters::Sdm::Serial::Provider>(
                    ::PowerMeters::Sdm::Serial::Provider::Phases::Three, pmcfg.SerialSdm);
            break;
        case Provider::Type::HTTP_JSON:
            _upProvider = std::make_unique<::PowerMeters::Json::Http::Provider>(pmcfg.HttpJson);
            break;
        case Provider::Type::SERIAL_SML:
            _upProvider = std::make_unique<::PowerMeters::Sml::Serial::Provider>();
            break;
        case Provider::Type::SMAHM2:
            _upProvider = std::make_unique<::PowerMeters::Udp::SmaHM::Provider>();
            break;
        case Provider::Type::HTTP_SML:
            _upProvider = std::make_unique<::PowerMeters::Sml::Http::Provider>(pmcfg.HttpSml);
            break;
    }

    if (!_upProvider->init()) {
        _upProvider = nullptr;
    }
}

float Controller::getPowerTotal() const
{
    std::lock_guard<std::mutex> l(_mutex);
    if (!_upProvider) { return 0.0; }
    return _upProvider->getPowerTotal();
}

uint32_t Controller::getLastUpdate() const
{
    std::lock_guard<std::mutex> l(_mutex);
    if (!_upProvider) { return 0; }
    return _upProvider->getLastUpdate();
}

bool Controller::isDataValid() const
{
    std::lock_guard<std::mutex> l(_mutex);
    if (!_upProvider) { return false; }
    return _upProvider->isDataValid();
}

void Controller::loop()
{
    std::lock_guard<std::mutex> lock(_mutex);
    if (!_upProvider) { return; }
    _upProvider->loop();

    auto const& pmcfg = Configuration.get().PowerMeter;
    if (pmcfg.Source == static_cast<uint8_t>(Provider::Type::MQTT)) { return; }
    _upProvider->mqttLoop();
}

} // namespace PowerMeters
