#include <functional>
#include <Configuration.h>
#include <battery/zendure/Provider.h>
#include <MqttSettings.h>
#include <SunPosition.h>
#include <MessageOutput.h>
#include <Utils.h>

namespace BatteryNs::Zendure {

bool Provider::init(bool verboseLogging)
{
    _verboseLogging = verboseLogging;

    auto const& config = Configuration.get();
    String deviceType = String();

    log("Settings %d", config.Battery.ZendureDeviceType);
    {
        String deviceName = String();
        switch (config.Battery.ZendureDeviceType) {
            case 0:
                deviceType = ZENDURE_HUB1200;
                deviceName = String("HUB 1200");
                break;
            case 1:
                deviceType = ZENDURE_HUB2000;
                deviceName = String("HUB 2000");
                break;
            case 2:
                deviceType = ZENDURE_AIO2400;
                deviceName = String("AIO 2400");
                break;
            case 3:
                deviceType = ZENDURE_ACE1500;
                deviceName = String("Ace 1500");
                break;
            case 4:
                deviceType = ZENDURE_HYPER2000;
                deviceName = String("Hyper 2000");
                break;
            default:
                log("Invalid device type!");
                return false;
        }

        if (strlen(config.Battery.ZendureDeviceId) != 8) {
            MessageOutput.printf("ZendureBattery: Invalid device id '%s'!\r\n", config.Battery.ZendureDeviceId);
            return false;
        }

        // setup static device info
        MessageOutput.printf("ZendureBattery: Device name '%s'\r\n", deviceName.c_str());
        _stats->setDevice(std::move(deviceName));
    }

    // store device ID as we will need them for checking when receiving messages
    _deviceId = config.Battery.ZendureDeviceId;

    _baseTopic = "/" + deviceType + "/" + _deviceId + "/";
    _topicRead = "iot" + _baseTopic + "properties/read";
    _topicWrite = "iot" + _baseTopic + "properties/write";

    _topicPersistentSettings = MqttSettings.getPrefix() + "battery/persistent/";

    auto topic = _topicPersistentSettings + "#";
    MqttSettings.subscribe(topic, 0/*QoS*/,
            std::bind(&Provider::onMqttMessagePersistentSettings,
                this, std::placeholders::_1, std::placeholders::_2,
                std::placeholders::_3, std::placeholders::_4,
                std::placeholders::_5, std::placeholders::_6)
            );
    log("Subscribed to '%s' for persistent settings", topic.c_str());

    // subscribe for log messages
    _topicLog = _baseTopic + "log";
    MqttSettings.subscribe(_topicLog, 0/*QoS*/,
            std::bind(&Provider::onMqttMessageLog,
                this, std::placeholders::_1, std::placeholders::_2,
                std::placeholders::_3, std::placeholders::_4,
                std::placeholders::_5, std::placeholders::_6)
            );
    log("Subscribed to '%s' for status readings", _topicLog.c_str());

    // subscribe for report messages
    _topicReport = _baseTopic + "properties/report";
    MqttSettings.subscribe(_topicReport, 0/*QoS*/,
            std::bind(&Provider::onMqttMessageReport,
                this, std::placeholders::_1, std::placeholders::_2,
                std::placeholders::_3, std::placeholders::_4,
                std::placeholders::_5, std::placeholders::_6)
            );
    log("Subscribed to '%s' for status readings", _topicReport.c_str());

    // subscribe for timesync messages
    _topicTimesync = _baseTopic + "time-sync";
    MqttSettings.subscribe(_topicTimesync, 0/*QoS*/,
            std::bind(&Provider::onMqttMessageTimesync,
                this, std::placeholders::_1, std::placeholders::_2,
                std::placeholders::_3, std::placeholders::_4,
                std::placeholders::_5, std::placeholders::_6)
            );
    log("Subscribed to '%s' for timesync requests", _topicTimesync.c_str());

#ifndef ZENDURE_NO_REDUCED_UPDATE
    // subscribe for read messages
    _topicReadReply = _baseTopic + "properties/read/reply";
    MqttSettings.subscribe(_topicReadReply, 0/*QoS*/,
            std::bind(&Provider::onMqttMessageReport,
                this, std::placeholders::_1, std::placeholders::_2,
                std::placeholders::_3, std::placeholders::_4,
                std::placeholders::_5, std::placeholders::_6)
            );
    log("Subscribed to '%s' for status readings\r\n", _topicReadReply.c_str());

    _rateUpdateMs       = min(static_cast<uint32_t>(config.Battery.ZendurePollingInterval * 100), 10U * 1000);
    _nextUpdate         = 0;
#endif

    _rateFullUpdateMs   = config.Battery.ZendurePollingInterval * 1000;
    _nextFullUpdate     = 0;
    _rateTimesyncMs     = ZENDURE_SECONDS_TIMESYNC * 1000;
    _nextTimesync       = 0;
    _rateSunCalcMs      = ZENDURE_SECONDS_SUNPOSITION * 1000;
    _nextSunCalc        = millis() + _rateSunCalcMs / 2;

    // pre-generate the settings request
    JsonDocument root;
    JsonVariant prop = root[ZENDURE_REPORT_PROPERTIES].to<JsonObject>();
    prop[ZENDURE_REPORT_PV_BRAND] = 1; // means Hoymiles
    prop[ZENDURE_REPORT_PV_AUTO_MODEL] = 0; // we did static setup
    prop[ZENDURE_REPORT_AUTO_RECOVER] = static_cast<uint8_t>(config.Battery.ZendureBypassMode == static_cast<uint8_t>(BypassMode::Automatic));
    prop[ZENDURE_REPORT_AUTO_SHUTDOWN] = static_cast<uint8_t>(config.Battery.ZendureAutoShutdown);
    prop[ZENDURE_REPORT_BUZZER_SWITCH] = 0; // disable, as it is anoying
    prop[ZENDURE_REPORT_BYPASS_MODE] = config.Battery.ZendureBypassMode;
    prop[ZENDURE_REPORT_SMART_MODE] = 0; // should be disabled
    serializeJson(root, _payloadSettings);

    // pre-generate the full update request
    root.clear();
    JsonArray array = root[ZENDURE_REPORT_PROPERTIES].to<JsonArray>();
    array.add("getAll");
    array.add("getInfo");
    serializeJson(root, _payloadFullUpdate);

#ifndef ZENDURE_NO_REDUCED_UPDATE
    // pre-generate the partitial update request
    root.clear();
    array = root[ZENDURE_REPORT_PROPERTIES].to<JsonArray>();
    array.add(ZENDURE_REPORT_MIN_SOC);
    array.add(ZENDURE_REPORT_MAX_SOC);
    array.add(ZENDURE_REPORT_INPUT_LIMIT);
    array.add(ZENDURE_REPORT_OUTPUT_LIMIT);
    array.add(ZENDURE_REPORT_INVERSE_MAX_POWER);
    array.add(ZENDURE_REPORT_BATTERY_STATE);
    array.add(ZENDURE_REPORT_HEAT_STATE);
    array.add(ZENDURE_REPORT_AUTO_SHUTDOWN);
    array.add(ZENDURE_REPORT_BUZZER_SWITCH);
    array.add(ZENDURE_REPORT_REMAIN_OUT_TIME);
    array.add(ZENDURE_REPORT_REMAIN_IN_TIME);
    serializeJson(root, _payloadUpdate);
#endif

    // initial setup
    if (!config.Battery.ZendureChargeThroughEnable) {
        setChargeThrough(false);
    }
    setTargetSoCs(config.Battery.ZendureMinSoC, config.Battery.ZendureMaxSoC);

    return true;
}

void Provider::deinit()
{
    if (!_topicReport.isEmpty()) {
        MqttSettings.unsubscribe(_topicReport);
        _topicReport.clear();
    }
    if (!_topicLog.isEmpty()) {
        MqttSettings.unsubscribe(_topicLog);
        _topicLog.clear();
    }
    if (!_topicTimesync.isEmpty()) {
        MqttSettings.unsubscribe(_topicTimesync);
        _topicTimesync.clear();
    }
    if (!_topicPersistentSettings.isEmpty()) {
        MqttSettings.unsubscribe(_topicPersistentSettings + "#");
        _topicPersistentSettings.clear();
    }
#ifndef ZENDURE_NO_REDUCED_UPDATE
    if (!_topicReadReply.isEmpty()) {
        MqttSettings.unsubscribe(_topicReadReply);
        _topicReadReply.clear();
    }
#endif
}

void Provider::loop()
{
    auto ms = millis();
    auto const& config = Configuration.get();
    const bool isDayPeriod = SunPosition.isSunsetAvailable() ? SunPosition.isDayPeriod() : true;

    // if auto shutdown is enabled and battery switches to idle at night, turn off status requests to prevent keeping battery awake
    if (config.Battery.ZendureAutoShutdown && !isDayPeriod && _stats->_state == State::Idle) {
        return;
    }

    // check if we run in schedule mode
    if (ms >= _nextSunCalc) {
        _nextSunCalc = ms + _rateSunCalcMs;

        calculateFullChargeAge();

        struct tm timeinfo_local;
        struct tm timeinfo_sun;
        if (getLocalTime(&timeinfo_local, 5)) {
            std::time_t current = std::mktime(&timeinfo_local);

            std::time_t sunrise = 0;
            std::time_t sunset = 0;

            if (SunPosition.sunriseTime(&timeinfo_sun)) {
                sunrise = std::mktime(&timeinfo_sun) + config.Battery.ZendureSunriseOffset * 60;
            }

            if (SunPosition.sunsetTime(&timeinfo_sun)) {
                sunset = std::mktime(&timeinfo_sun) + config.Battery.ZendureSunsetOffset * 60;
            }

            if (sunrise && sunset) {
                // check charge-through at sunrise (make sure its triggered at least once)
                if (current > sunrise && current < (sunrise + ZENDURE_SECONDS_SUNPOSITION + ZENDURE_SECONDS_SUNPOSITION/2)) {
                    // Calculate expected daylight to asure charge through starts in the morning if sheduled for this day
                    // We just use the time between rise and set, as we do not know anything about the actual conditions,
                    // we can only expect that there will be NO sun between sunset and sunrise ;)
                    uint32_t maxDaylightHours = (sunset - sunrise + 1800U) / 3600U;
                    checkChargeThrough(maxDaylightHours);
                }

                // running in appointment mode - set outputlimit accordingly
                if (config.Battery.ZendureOutputControl == ZendureBatteryOutputControl::ControlSchedule) {
                    if (current >= sunrise && current < sunset) {
                        setOutputLimit(min(config.Battery.ZendureMaxOutput, config.Battery.ZendureOutputLimitDay));
                    } else if (current >= sunset || current < sunrise) {
                        setOutputLimit(min(config.Battery.ZendureMaxOutput, config.Battery.ZendureOutputLimitNight));
                    }
                }
            }


        }

        // ensure charge through settings
        if (_stats->_charge_through_state.value_or(false) && config.Battery.ZendureChargeThroughEnable) {
            setTargetSoCs(config.Battery.ZendureMinSoC, 100);
            setOutputLimit(0);
        }else{
            setTargetSoCs(config.Battery.ZendureMinSoC, config.Battery.ZendureMaxSoC);
            if (config.Battery.ZendureOutputControl == ZendureBatteryOutputControl::ControlFixed) {
                setOutputLimit(min(config.Battery.ZendureMaxOutput, config.Battery.ZendureOutputLimit));
            }
        }
    }

    if (!_topicRead.isEmpty()) {
        if (!_payloadFullUpdate.isEmpty() && ms >= _nextFullUpdate) {
            _nextFullUpdate = ms + _rateFullUpdateMs;
#ifndef ZENDURE_NO_REDUCED_UPDATE
            _nextUpdate = ms + _rateUpdateMs;
#endif
            MqttSettings.publishGeneric(_topicRead, _payloadFullUpdate, false, 0);
        }
#ifndef ZENDURE_NO_REDUCED_UPDATE
        if (!_payloadUpdate.isEmpty() && ms >= _nextUpdate) {
            _nextUpdate = ms + _rateUpdateMs;
            MqttSettings.publishGeneric(_topicRead, _payloadUpdate, false, 0);
        }
#endif
    }

    if (ms >= _nextTimesync) {
        _nextTimesync = ms + _rateTimesyncMs;
        timesync();

        // update settings (will be skipped if unchanged)
        setInverterMax(config.Battery.ZendureMaxOutput);

        // republish settings - just to be sure
        if (!_topicWrite.isEmpty() && !_payloadSettings.isEmpty()) {
            MqttSettings.publishGeneric(_topicWrite, _payloadSettings, false, 0);
        }
    }
}

void Provider::calculateFullChargeAge()
{
    time_t now;
    if (Utils::getEpoch(&now, 20) && _stats->_last_full_timestamp.has_value()) {
        auto last_full = *(_stats->_last_full_timestamp);
        uint32_t age = now > last_full  ? (now - last_full) / 3600U : 0U;

        log("Now: %ld, LastFull: %ld, Diff: %d", now, last_full, age);

        // store for webview
        _stats->_last_full_charge_hours = age;
    }
}

bool Provider::checkChargeThrough(uint32_t predictHours /* = 0 */)
{
    auto const& config = Configuration.get();
    if (config.Battery.ZendureChargeThroughEnable && (
        !_stats->_last_full_timestamp.has_value() ||
        _stats->_last_full_charge_hours.value_or(0) + predictHours > config.Battery.ZendureChargeThroughInterval )
    ) {
        return setChargeThrough(true);
    }

    return false;
}

void Provider::setTargetSoCs(const float soc_min, const float soc_max)
{
    if (_topicWrite.isEmpty() || !alive()) {
        return;
    }

    if (_stats->_soc_min != soc_min || _stats->_soc_max != soc_max) {
        MqttSettings.publishGeneric(_topicWrite, "{\"properties\": {\"" ZENDURE_REPORT_MIN_SOC "\": " + String(soc_min * 10, 0) + ", \"" ZENDURE_REPORT_MAX_SOC  "\": " + String(soc_max * 10, 0) + "} }", false, 0);
        publishProperties(_topicWrite, ZENDURE_REPORT_MIN_SOC, String(soc_min * 10, 0), ZENDURE_REPORT_MAX_SOC, String(soc_max * 10, 0));
        log("Setting target minSoC from %.1f %% to %.1f %% and target maxSoC from %.1f %% to %.1f %%", _stats->_soc_min, soc_min, _stats->_soc_max, soc_max);
    }
}

uint16_t Provider::calcOutputLimit(uint16_t limit) const
{
    if (limit >= 100 || limit == 0 ) {
        return limit;
    }

    uint16_t base = limit / 30U;
    uint16_t remain = (limit % 30U) / 15U;
    return 30 * base + 30 * remain;
}

uint16_t Provider::setOutputLimit(uint16_t limit) const
{
    auto const& config = Configuration.get();

    if (_topicWrite.isEmpty() || !alive()) {
        return _stats->_output_limit;
    }

    // force valid limit and ensure fixed output is always dominant
    if (config.Battery.ZendureOutputControl == ZendureBatteryOutputControl::ControlFixed) {
        limit = config.Battery.ZendureOutputLimit;
    } else {
        limit = min(config.Battery.ZendureMaxOutput, limit);
    }

    // enforce output limit during charge through
    if (_stats->_charge_through_state.value_or(false)) {
        limit = 0;
    }

    if (_stats->_output_limit != limit) {
        limit = calcOutputLimit(limit);
        publishProperty(_topicWrite, ZENDURE_REPORT_OUTPUT_LIMIT, String(limit));
        log("Adjusting outputlimit from %d W to %d W", _stats->_output_limit, limit);
    }

    return limit;
}

uint16_t Provider::setInverterMax(uint16_t limit) const
{
    if (_topicWrite.isEmpty() || !alive()) {
        return _stats->_inverse_max;
    }

    if (_stats->_inverse_max != limit) {
        limit = calcOutputLimit(limit);
        publishProperty(_topicWrite, ZENDURE_REPORT_INVERSE_MAX_POWER, String(limit));
        log("Adjusting inverter max output from %d W to %d W", _stats->_inverse_max, limit);
    }

    return limit;
}

void Provider::shutdown() const
{
    if (!_topicWrite.isEmpty()) {
        publishProperty(_topicWrite, ZENDURE_REPORT_MASTER_SWITCH, "1");
        log("Shutting down HUB");
    }
}

void Provider::publishProperty(const String& topic, const String& property, const String& value) const
{
    MqttSettings.publishGeneric(topic, "{\"properties\": {\"" + property +  "\": " + value + "} }", false, 0);
}

template<typename... Arg>
void Provider::publishProperties(const String& topic, Arg&&... args) const
{
    static_assert((sizeof...(args) % 2) == 0);

    String out = "{\"properties\": {";
    bool even = true;
    for (const String d : std::initializer_list<String>({args...}))
    {
        if (even) {
            out += "\"" + d + "\": ";
        } else {
            out += d + ", ";
        }
        even = !even;
    }
    out += "} }";
    MqttSettings.publishGeneric(topic, out, false, 0);
}

void Provider::timesync()
{
    time_t now;
    if (!_baseTopic.isEmpty() && Utils::getEpoch(&now, 20)) {
        MqttSettings.publishGeneric("iot" + _baseTopic + "time-sync/reply", "{\"zoneOffset\": \"+00:00\", \"messageId\": " + String(++_messageCounter) + ", \"timestamp\": " + String(now) + "}", false, 0);
        log("Timesync Reply");
    }
}

bool Provider::setChargeThrough(const bool value, const bool publish /* = true */)
{
    if (!_stats->_charge_through_state.has_value() || value != _stats->_charge_through_state) {
        _stats->_charge_through_state = value;
        log("%s charge-through mode!", value ? "Enabling" : "Disabling");
        if (publish) {
            publishPersistentSettings(ZENDURE_PERSISTENT_SETTINGS_CHARGE_THROUGH, value ? "1" : "0");
        }

        // re-run suncalc to force updates in schedule mode
        rescheduleSunCalc();
    }

    return value;
}

#ifndef ZENDURE_NO_REDUCED_UPDATE
void Provider::onMqttMessageRead(espMqttClientTypes::MessageProperties const& properties,
        char const* topic, uint8_t const* payload, size_t len, size_t index, size_t total)
{
}
#endif

void Provider::onMqttMessagePersistentSettings(espMqttClientTypes::MessageProperties const& properties,
        char const* topic, uint8_t const* payload, size_t len, size_t index, size_t total)
{
    String t(topic);
    String p(reinterpret_cast<const char*>(payload), len);
    auto integer = static_cast<uint64_t>(p.toInt());

    log("Received Persistent Settings %s = %s [aka %" PRId64 "]", topic, p.substring(0, 32).c_str(), integer);

    if (t.endsWith(ZENDURE_PERSISTENT_SETTINGS_LAST_FULL) && integer) {
        _stats->_last_full_timestamp = integer;
        return;
    }
    if (t.endsWith(ZENDURE_PERSISTENT_SETTINGS_LAST_EMPTY) && integer) {
        _stats->_last_empty_timestamp = integer;
        return;
    }
    if (t.endsWith(ZENDURE_PERSISTENT_SETTINGS_CHARGE_THROUGH)) {
        setChargeThrough(integer > 0, false);
        return;
    }
}

void Provider::onMqttMessageTimesync(espMqttClientTypes::MessageProperties const& properties,
        char const* topic, uint8_t const* payload, size_t len, size_t index, size_t total)
{
    timesync();
}

void Provider::onMqttMessageReport(espMqttClientTypes::MessageProperties const& properties,
        char const* topic, uint8_t const* payload, size_t len, size_t index, size_t total)
{
    auto ms = millis();

    std::string const src = std::string(reinterpret_cast<const char*>(payload), len);
    std::string logValue = src.substr(0, 64);
    if (src.length() > logValue.length()) { logValue += "..."; }

    JsonDocument json;

    const DeserializationError error = deserializeJson(json, src);
    if (error) {
        return log("cannot parse payload '%s' as JSON", logValue.c_str());
    }

    if (json.overflowed()) {
        return log("payload too large to process as JSON");
    }

    auto obj = json.as<JsonObjectConst>();

    // validate input data
    // messageId has to be set to "123"
    // deviceId has to be set to the configured deviceId
    if (!json["messageId"].as<String>().equals("123")) {
        return log("Invalid or missing 'messageId' in '%s'", logValue.c_str());
    }
    if (!json["deviceId"].as<String>().equals(_deviceId)) {
        return log("Invalid or missing 'deviceId' in '%s'", logValue.c_str());
    }

    auto props = Utils::getJsonElement<JsonObjectConst>(obj, ZENDURE_REPORT_PROPERTIES, 1);
    if (props.has_value()) {
        auto sw_version = Utils::getJsonElement<uint32_t>(*props, ZENDURE_REPORT_MASTER_FW_VERSION);
        if (sw_version.has_value()) {
            _stats->setFwVersion(std::move(parseVersion(*sw_version)));
        }

        auto hw_version = Utils::getJsonElement<uint32_t>(*props, ZENDURE_REPORT_MASTER_HW_VERSION);
        if (hw_version.has_value()) {
            _stats->setHwVersion(std::move(parseVersion(*hw_version)));
        }

        auto soc_max = Utils::getJsonElement<float>(*props, ZENDURE_REPORT_MAX_SOC);
        if (soc_max.has_value()) {
            *soc_max /= 10;
            if (*soc_max >= 40 && *soc_max <= 100) {
                _stats->_soc_max = *soc_max;
            }
        }

        auto soc_min = Utils::getJsonElement<float>(*props, ZENDURE_REPORT_MIN_SOC);
        if (soc_min.has_value()) {
            *soc_min /= 10;
            if (*soc_min >= 0 && *soc_min <= 60) {
                _stats->_soc_min = *soc_min;
            }
        }

        auto input_limit = Utils::getJsonElement<uint16_t>(*props, ZENDURE_REPORT_INPUT_LIMIT);
        if (input_limit.has_value()) {
            _stats->_input_limit = *input_limit;
        }

        auto inverse_max = Utils::getJsonElement<uint16_t>(*props, ZENDURE_REPORT_INVERSE_MAX_POWER);
        if (inverse_max.has_value()) {
            _stats->_inverse_max = *inverse_max;
        }

        auto state = Utils::getJsonElement<uint8_t>(*props, ZENDURE_REPORT_BATTERY_STATE);
        if (state.has_value() && *state <= 2) {
            _stats->_state = static_cast<State>(*state);
        }

        auto heat_state = Utils::getJsonElement<uint8_t>(*props, ZENDURE_REPORT_HEAT_STATE);
        if (heat_state.has_value()) {
            _stats->_heat_state = static_cast<bool>(*heat_state);
        }

        auto auto_shutdown = Utils::getJsonElement<uint8_t>(*props, ZENDURE_REPORT_AUTO_SHUTDOWN);
        if (auto_shutdown.has_value()) {
            _stats->_auto_shutdown = static_cast<bool>(*auto_shutdown);
        }

        auto buzzer = Utils::getJsonElement<uint8_t>(*props, ZENDURE_REPORT_BUZZER_SWITCH);
        if (buzzer.has_value()) {
            _stats->_buzzer = static_cast<bool>(*buzzer);
        }

        auto outtime = Utils::getJsonElement<uint16_t>(*props, ZENDURE_REPORT_REMAIN_OUT_TIME);
        if (outtime.has_value()) {
            _stats->_remain_out_time = *outtime >= ZENDURE_REMAINING_TIME_OVERFLOW ? -1 : *outtime;
        }

        auto intime = Utils::getJsonElement<uint16_t>(*props, ZENDURE_REPORT_REMAIN_IN_TIME);
        if (intime.has_value()) {
            _stats->_remain_in_time = *intime >= ZENDURE_REMAINING_TIME_OVERFLOW ? -1 : *intime;
        }

        _stats->_lastUpdate = ms;
    }



    // stop processing here, if no pack data found in message
    auto packData = Utils::getJsonElement<JsonArrayConst>(obj, ZENDURE_REPORT_PACK_DATE, 2);
    if (!packData.has_value()) {
        return;
    }

    // get serial number related to index only if all packs given in message
    if (_stats->_num_batteries != 0 && (*packData).size() == _stats->_num_batteries) {
        for (size_t i = 0 ; i < _stats->_num_batteries ; i++) {
            auto serial = Utils::getJsonElement<String>((*packData)[i], ZENDURE_REPORT_PACK_SERIAL);
            if (!serial.has_value()) {
                log("Missing serial of battery pack in '%s'", logValue.c_str());
                continue;
            }
            if (_stats->addPackData(i+1, *serial) == nullptr) {
                log("Invalid or unkown serial '%s' in '%s'", (*serial).c_str(), logValue.c_str());
            }
        }
    }

    // get additional data only if all packs were identified
    if (_stats->_packData.size() != _stats->_num_batteries) {
        return;
    }

    for (auto packDataJson : *packData) {
        auto serial = Utils::getJsonElement<String>(packDataJson, ZENDURE_REPORT_PACK_SERIAL);
        auto state = Utils::getJsonElement<uint8_t>(packDataJson, ZENDURE_REPORT_PACK_STATE);
        auto version = Utils::getJsonElement<uint32_t>(packDataJson, ZENDURE_REPORT_PACK_FW_VERSION);
        auto soh = Utils::getJsonElement<uint16_t>(packDataJson, ZENDURE_REPORT_PACK_HEALTH);

        // do not waste processing time if nothing to do
        if (!serial.has_value() || !(state.has_value() || version.has_value())) {
            continue;
        }

        // find pack data related to serial number
        for (auto& entry : _stats->_packData) {
            auto pack = entry.second;
            if (pack->_serial != serial) {
                continue;
            }
            if (state.has_value()) {
                pack->_state = static_cast<State>(*state);
            }

            if (version.has_value()) {
                pack->setFwVersion(std::move(parseVersion(*version)));
            }

            if (soh.has_value()) {
                pack->_state_of_health = static_cast<float>(*soh) / 10.0;
                pack->_capacity_avail = pack->_capacity * pack->_state_of_health / 100.0;
            }

            pack->_lastUpdate = ms;

            // we found the pack we searched for, so terminate loop here
            break;
        }
    }
}

void Provider::onMqttMessageLog(espMqttClientTypes::MessageProperties const& properties,
        char const* topic, uint8_t const* payload, size_t len, size_t index, size_t total)
{
    auto ms = millis();

    std::string const src = std::string(reinterpret_cast<const char*>(payload), len);
    std::string logValue = src.substr(0, 64);
    if (src.length() > logValue.length()) { logValue += "..."; }

    JsonDocument json;

    const DeserializationError error = deserializeJson(json, src);
    if (error) {
        return log("cannot parse payload '%s' as JSON", logValue.c_str());
    }

    if (json.overflowed()) {
        return log("payload too large to process as JSON");
    }

    auto obj = json.as<JsonObjectConst>();

    // validate input data
    // deviceId has to be set to the configured deviceId
    // logType has to be set to "2"
    if (!json["deviceId"].as<String>().equals(_deviceId)) {
        return log("Invalid or missing 'deviceId' in '%s'", logValue.c_str());
    }
    if (!json["logType"].as<String>().equals("2")) {
        return log("Invalid or missing 'v' in '%s'", logValue.c_str());
    }

    auto data = Utils::getJsonElement<JsonObjectConst>(obj, ZENDURE_LOG_ROOT, 2);
    if (!data.has_value()) {
        return log("Unable to find 'log' in '%s'", logValue.c_str());
    }

    _stats->setSerial(Utils::getJsonElement<String>(*data, ZENDURE_LOG_SERIAL));

    auto params = Utils::getJsonElement<JsonArrayConst>(*data, ZENDURE_LOG_PARAMS, 1);
    if (!params.has_value()) {
        return log("Unable to find 'params' in '%s'", logValue.c_str());
    }

    auto v = *params;

    uint8_t num = v[ZENDURE_LOG_OFFSET_PACKNUM].as<uint8_t>();
    if (num > 0 && num <= ZENDURE_MAX_PACKS) {
        uint16_t soc = 0;
        uint16_t voltage = 0;
        int16_t power = 0;
        int16_t current = 0;
        uint32_t cellMin = UINT32_MAX;
        uint32_t cellMax = 0;
        uint32_t cellAvg = 0;
        uint32_t cellDelta = 0;
        int32_t cellTemp = 0;
        uint16_t capacity = 0;
        float capacity_avail = 0;

        for (size_t i = 1 ; i <= num ; i++) {
            auto pvol = v[ZENDURE_LOG_OFFSET_PACK_VOLTAGE(i)].as<uint16_t>() * 10;
            auto pcur = v[ZENDURE_LOG_OFFSET_PACK_CURRENT(i)].as<int16_t>();
            auto psoc = v[ZENDURE_LOG_OFFSET_PACK_SOC(i)].as<uint16_t>();

            auto ctmp = v[ZENDURE_LOG_OFFSET_PACK_TEMPERATURE(i)].as<int32_t>();
            auto cmin = v[ZENDURE_LOG_OFFSET_PACK_CELL_MIN(i)].as<uint32_t>() * 10;
            auto cmax = v[ZENDURE_LOG_OFFSET_PACK_CELL_MAX(i)].as<uint32_t>() * 10;
            auto cdel = cmax - cmin;

            auto pack = _stats->getPackData(i);
            if (pack != nullptr) {
                auto cavg = pvol / pack->getCellCount();

                pack->_cell_voltage_min = static_cast<uint16_t>(cmin);
                pack->_cell_voltage_max = static_cast<uint16_t>(cmax);
                pack->_cell_voltage_avg = static_cast<uint16_t>(cavg);
                pack->_cell_voltage_spread = static_cast<uint16_t>(cdel);
                pack->_cell_temperature_max = static_cast<int16_t>(ctmp);
                pack->_current = static_cast<float>(pcur) / 10.0;
                pack->_voltage_total = static_cast<float>(pvol) / 1000.0;
                pack->_soc_level = static_cast<float>(psoc) / 10.0;
                pack->_power = static_cast<int16_t>(pack->_current * pack->_voltage_total);
                pack->_lastUpdate = ms;

                capacity_avail += pack->_capacity_avail;
                capacity += pack->_capacity;
                cellAvg += cavg;
                power += pack->_power;
            }

            cellMin = min(cmin, cellMin);
            cellMax = max(cmax, cellMax);
            cellDelta = max(cdel, cellDelta);
            cellTemp = max(ctmp, cellTemp);

            soc += psoc;
            voltage += pvol;
            current += pcur;
        }

        _stats->_num_batteries = num;
        setSoC(static_cast<float>(soc) / 10.0 / num, ms);
        _stats->setVoltage(v[ZENDURE_LOG_OFFSET_VOLTAGE].as<float>() / 10.0, ms);
        _stats->setCurrent(static_cast<float>(current) / 10.0, 1, ms);
        _stats->setDischargeCurrentLimit(static_cast<float>(_stats->_inverse_max) / _stats->getVoltage(), ms);
        if (capacity) {
            _stats->_capacity = capacity;
        }
        if (capacity_avail) {
            _stats->_capacity_avail = static_cast<uint16_t>(capacity_avail);
        }

        _stats->_auto_recover = static_cast<bool>(v[ZENDURE_LOG_OFFSET_AUTO_RECOVER].as<uint8_t>());
        _stats->_bypass_mode = static_cast<BypassMode>(v[ZENDURE_LOG_OFFSET_BYPASS_MODE].as<uint8_t>());
        _stats->_soc_min = v[ZENDURE_LOG_OFFSET_MIN_SOC].as<float>();

        _stats->_solar_power_1 = v[ZENDURE_LOG_OFFSET_SOLAR_POWER_MPPT_1].as<uint16_t>();
        _stats->_solar_power_2 = v[ZENDURE_LOG_OFFSET_SOLAR_POWER_MPPT_2].as<uint16_t>();
        _stats->_input_power = _stats->_solar_power_1 + _stats->_solar_power_2;

        _stats->_output_limit = static_cast<uint16_t>(v[ZENDURE_LOG_OFFSET_OUTPUT_POWER_LIMIT].as<uint32_t>() / 100);
        //_stats->_input_power = v[ZENDURE_LOG_OFFSET_INPUT_POWER].as<uint16_t>();
        _stats->_output_power = v[ZENDURE_LOG_OFFSET_OUTPUT_POWER].as<uint16_t>();
        _stats->_charge_power = v[ZENDURE_LOG_OFFSET_CHARGE_POWER].as<uint16_t>();
        _stats->_discharge_power = v[ZENDURE_LOG_OFFSET_DISCHARGE_POWER].as<uint16_t>();

        _stats->_cellMinMilliVolt = static_cast<uint16_t>(cellMin);
        _stats->_cellMaxMilliVolt = static_cast<uint16_t>(cellMax);
        _stats->_cellAvgMilliVolt = static_cast<uint16_t>(cellAvg) / num;
        _stats->_cellDeltaMilliVolt = static_cast<uint16_t>(cellDelta);
        _stats->_cellTemperature = static_cast<int16_t>(cellTemp);

        _stats->_lastUpdate = ms;

        calculateEfficiency();
    }
}

String Provider::parseVersion(uint32_t version)
{
    if (version == 0) {
        return String();
    }

    uint8_t major = (version >> 12) & 0xF;
    uint8_t minor = (version >> 8) & 0xF;
    uint8_t bugfix = version & 0xFF;

    char buffer[16];
    snprintf(buffer, sizeof(buffer), "%d.%d.%d", major, minor, bugfix);
    return String(buffer);
}

void Provider::calculateEfficiency()
{
    float in = static_cast<float>(_stats->_input_power);
    float out = static_cast<float>(_stats->_output_power);
    float efficiency = 0.0;

    in += static_cast<float>(_stats->_discharge_power);
    out += static_cast<float>(_stats->_charge_power);

    efficiency = in ? out / in : 0.0;

    if (efficiency <= 1 && efficiency >= 0) {
        _stats->_efficiency = efficiency * 100;
    }
}

void Provider::setSoC(const float soc, const uint32_t timestamp /* = 0 */, const uint8_t precision /* = 2 */)
{
    time_t now;

    if (Utils::getEpoch(&now, 20)) {
        if (soc >= 100.0) {
            _stats->_last_full_timestamp = now;
            publishPersistentSettings(ZENDURE_PERSISTENT_SETTINGS_LAST_FULL, String(now));
            publishPersistentSettings(ZENDURE_PERSISTENT_SETTINGS_CHARGE_THROUGH, "0");
        }
        if (soc <= 0.0) {
            _stats->_last_empty_timestamp = now;
            publishPersistentSettings(ZENDURE_PERSISTENT_SETTINGS_LAST_EMPTY, String(now));
        }
    }

    _stats->setSoC(soc, precision, timestamp ? timestamp : millis());
}

void Provider::publishPersistentSettings(const char* subtopic, const String& payload)
{
    if (!_topicPersistentSettings.isEmpty())
    {
        log("Writing Persistent Settings %s = %s\r\n", String(_topicPersistentSettings + subtopic).c_str(), payload.substring(0, 32).c_str());
        MqttSettings.publishGeneric(_topicPersistentSettings + subtopic, payload, true);
    }
}


} // namespace BatteryNs::Zendure
