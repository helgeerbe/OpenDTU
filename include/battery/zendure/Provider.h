// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <memory>
#include <battery/Provider.h>
#include <battery/zendure/Stats.h>
#include <battery/zendure/HassIntegration.h>
#include <battery/zendure/Constants.h>
#include <espMqttClient.h>
#include <MessageOutput.h>

namespace BatteryNs::Zendure {

class Provider : public ::BatteryNs::Provider {
public:
    bool init(bool verboseLogging) final;
    void deinit() final;
    void loop() final;
    std::shared_ptr<::BatteryNs::Stats> getStats() const final { return _stats; }
    ::BatteryNs::HassIntegration const& getHassIntegration() const final { return _hassIntegration; }

    uint16_t setOutputLimit(uint16_t limit) const;
    uint16_t setInverterMax(uint16_t limit) const;
    void shutdown() const;

    bool checkChargeThrough(uint32_t predictHours = 0U);

protected:
    void timesync();
    static String parseVersion(uint32_t version);
    uint16_t calcOutputLimit(uint16_t limit) const;
    void setTargetSoCs(const float soc_min, const float soc_max);

private:
    bool _verboseLogging = false;
    uint32_t _lastUpdate = 0;
    std::shared_ptr<Stats> _stats = std::make_shared<Stats>();
    HassIntegration _hassIntegration;


    void calculateEfficiency();
    void calculateFullChargeAge();
    void publishProperty(const String& topic, const String& property, const String& value) const;
    template<typename... Arg>
    void publishProperties(const String& topic, Arg&&... args) const;

    void setSoC(const float soc, const uint32_t timestamp = 0, const uint8_t precision = 2);
    bool setChargeThrough(const bool value, const bool publish = true);

    void rescheduleSunCalc() { _nextSunCalc = 0; }
    bool alive() const { return _stats->getAgeSeconds() < ZENDURE_ALIVE_SECONDS; }

    void publishPersistentSettings(const char* subtopic, const String& payload);

    template <typename... Args>
    void log(char const* format, Args&&... args) const {
        if (_verboseLogging) {
            MessageOutput.printf("ZendureBattery: ");
            MessageOutput.printf(format, std::forward<Args>(args)...);
            MessageOutput.println();
        }
        return;
    };

#ifndef ZENDURE_NO_REDUCED_UPDATE
    uint32_t _rateUpdateMs;
    uint64_t _nextUpdate;
#endif

    uint32_t _rateFullUpdateMs = 0;
    uint64_t _nextFullUpdate = 0;

    uint32_t _rateTimesyncMs = 0;
    uint64_t _nextTimesync = 0;

    uint32_t _rateSunCalcMs = 0;
    uint64_t _nextSunCalc = 0;

    uint32_t _messageCounter = 0;

    String _deviceId = String();

    String _baseTopic = String();
    String _topicLog = String();
    String _topicReadReply = String();
    String _topicReport = String();
    String _topicRead = String();
    String _topicWrite = String();
    String _topicTimesync = String();
    String _topicPersistentSettings = String();

    String _payloadSettings = String();
    String _payloadFullUpdate = String();
#ifndef ZENDURE_NO_REDUCED_UPDATE
    String _payloadUpdate;
#endif

    void onMqttMessageReport(espMqttClientTypes::MessageProperties const& properties,
            char const* topic, uint8_t const* payload, size_t len, size_t index, size_t total);

    void onMqttMessageLog(espMqttClientTypes::MessageProperties const& properties,
            char const* topic, uint8_t const* payload, size_t len, size_t index, size_t total);

    void onMqttMessageTimesync(espMqttClientTypes::MessageProperties const& properties,
            char const* topic, uint8_t const* payload, size_t len, size_t index, size_t total);

    void onMqttMessagePersistentSettings(espMqttClientTypes::MessageProperties const& properties,
            char const* topic, uint8_t const* payload, size_t len, size_t index, size_t total);

#ifndef ZENDURE_NO_REDUCED_UPDATE
    void onMqttMessageRead(espMqttClientTypes::MessageProperties const& properties,
            char const* topic, uint8_t const* payload, size_t len, size_t index, size_t total);
#endif

};

} // namespace BatteryNs::Zendure
