// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2022-2024 Thomas Basler and others
 */
#include "Configuration.h"
#include "MessageOutput.h"
#include "NetworkSettings.h"
#include "Utils.h"
#include "defaults.h"
#include <LittleFS.h>
#include <nvs_flash.h>

CONFIG_T config;

static std::condition_variable sWriterCv;
static std::mutex sWriterMutex;
static unsigned sWriterCount = 0;

void ConfigurationClass::init(Scheduler& scheduler)
{
    scheduler.addTask(_loopTask);
    _loopTask.setCallback(std::bind(&ConfigurationClass::loop, this));
    _loopTask.setIterations(TASK_FOREVER);
    _loopTask.enable();

    memset(&config, 0x0, sizeof(config));
}

// we want a representation of our floating-point value in the JSON that
// uses the least amount of decimal digits possible to convey the value that
// is actually represented by the float. this is no easy task. ArduinoJson
// does this for us, however, it does it as expected only for variables of
// type double. this is probably because it assumes all floating-point
// values to have the precision of a double (64 bits), so it prints the
// respective number of siginificant decimals, which are too many if the
// actual value is a float (32 bits).
double ConfigurationClass::roundedFloat(float val)
{
    return static_cast<int>(val * 100 + (val > 0 ? 0.5 : -0.5)) / 100.0;
}

void ConfigurationClass::serializeHttpRequestConfig(HttpRequestConfig const& source, JsonObject& target)
{
    JsonObject target_http_config = target["http_request"].to<JsonObject>();
    target_http_config["url"] = source.Url;
    target_http_config["auth_type"] = source.AuthType;
    target_http_config["username"] = source.Username;
    target_http_config["password"] = source.Password;
    target_http_config["header_key"] = source.HeaderKey;
    target_http_config["header_value"] = source.HeaderValue;
    target_http_config["timeout"] = source.Timeout;
}

void ConfigurationClass::serializeSolarChargerConfig(SolarChargerConfig const& source, JsonObject& target)
{
    target["enabled"] = source.Enabled;
    target["verbose_logging"] = source.VerboseLogging;
    target["provider"] = source.Provider;
    target["publish_updates_only"] = source.PublishUpdatesOnly;
}

void ConfigurationClass::serializeSolarChargerMqttConfig(SolarChargerMqttConfig const& source, JsonObject& target)
{
    target["calculate_output_power"] = source.CalculateOutputPower;
    target["power_topic"] = source.PowerTopic;
    target["power_path"] = source.PowerJsonPath;
    target["power_unit"] = source.PowerUnit;
    target["voltage_topic"] = source.VoltageTopic;
    target["voltage_path"] = source.VoltageJsonPath;
    target["voltage_unit"] = source.VoltageTopicUnit;
    target["current_topic"] = source.CurrentTopic;
    target["current_path"] = source.CurrentJsonPath;
    target["current_unit"] = source.CurrentUnit;
}

void ConfigurationClass::serializePowerMeterMqttConfig(PowerMeterMqttConfig const& source, JsonObject& target)
{
    JsonArray values = target["values"].to<JsonArray>();
    for (size_t i = 0; i < POWERMETER_MQTT_MAX_VALUES; ++i) {
        JsonObject t = values.add<JsonObject>();
        PowerMeterMqttValue const& s = source.Values[i];

        t["topic"] = s.Topic;
        t["json_path"] = s.JsonPath;
        t["unit"] = s.PowerUnit;
        t["sign_inverted"] = s.SignInverted;
    }
}

void ConfigurationClass::serializePowerMeterSerialSdmConfig(PowerMeterSerialSdmConfig const& source, JsonObject& target)
{
    target["address"] = source.Address;
    target["polling_interval"] = source.PollingInterval;
}

void ConfigurationClass::serializePowerMeterHttpJsonConfig(PowerMeterHttpJsonConfig const& source, JsonObject& target)
{
    target["polling_interval"] = source.PollingInterval;
    target["individual_requests"] = source.IndividualRequests;

    JsonArray values = target["values"].to<JsonArray>();
    for (size_t i = 0; i < POWERMETER_HTTP_JSON_MAX_VALUES; ++i) {
        JsonObject t = values.add<JsonObject>();
        PowerMeterHttpJsonValue const& s = source.Values[i];

        serializeHttpRequestConfig(s.HttpRequest, t);

        t["enabled"] = s.Enabled;
        t["json_path"] = s.JsonPath;
        t["unit"] = s.PowerUnit;
        t["sign_inverted"] = s.SignInverted;
    }
}

void ConfigurationClass::serializePowerMeterHttpSmlConfig(PowerMeterHttpSmlConfig const& source, JsonObject& target)
{
    target["polling_interval"] = source.PollingInterval;
    serializeHttpRequestConfig(source.HttpRequest, target);
}

void ConfigurationClass::serializeBatteryConfig(BatteryConfig const& source, JsonObject& target)
{
    target["enabled"] = config.Battery.Enabled;
    target["verbose_logging"] = config.Battery.VerboseLogging;
    target["provider"] = config.Battery.Provider;
    target["jkbms_interface"] = config.Battery.JkBmsInterface;
    target["jkbms_polling_interval"] = config.Battery.JkBmsPollingInterval;
    target["mqtt_soc_topic"] = config.Battery.MqttSocTopic;
    target["mqtt_soc_json_path"] = config.Battery.MqttSocJsonPath;
    target["mqtt_voltage_topic"] = config.Battery.MqttVoltageTopic;
    target["mqtt_voltage_json_path"] = config.Battery.MqttVoltageJsonPath;
    target["mqtt_voltage_unit"] = config.Battery.MqttVoltageUnit;
    target["enable_discharge_current_limit"] = config.Battery.EnableDischargeCurrentLimit;
    target["discharge_current_limit"] = config.Battery.DischargeCurrentLimit;
    target["discharge_current_limit_below_soc"] = config.Battery.DischargeCurrentLimitBelowSoc;
    target["discharge_current_limit_below_voltage"] = config.Battery.DischargeCurrentLimitBelowVoltage;
    target["use_battery_reported_discharge_current_limit"] = config.Battery.UseBatteryReportedDischargeCurrentLimit;
    target["mqtt_discharge_current_topic"] = config.Battery.MqttDischargeCurrentTopic;
    target["mqtt_discharge_current_json_path"] = config.Battery.MqttDischargeCurrentJsonPath;
    target["mqtt_amperage_unit"] = config.Battery.MqttAmperageUnit;
}

void ConfigurationClass::serializePowerLimiterConfig(PowerLimiterConfig const& source, JsonObject& target)
{
    char serialBuffer[sizeof(uint64_t) * 8 + 1];
    auto serialStr = [&serialBuffer](uint64_t const& serial) -> String {
        snprintf(serialBuffer, sizeof(serialBuffer), "%0x%08x",
            static_cast<uint32_t>((serial >> 32) & 0xFFFFFFFF),
            static_cast<uint32_t>(serial & 0xFFFFFFFF));
        return String(serialBuffer);
    };

    target["enabled"] = source.Enabled;
    target["verbose_logging"] = source.VerboseLogging;
    target["solar_passthrough_enabled"] = source.SolarPassThroughEnabled;
    target["conduction_losses"] = source.ConductionLosses;
    target["battery_always_use_at_night"] = source.BatteryAlwaysUseAtNight;
    target["target_power_consumption"] = source.TargetPowerConsumption;
    target["target_power_consumption_hysteresis"] = source.TargetPowerConsumptionHysteresis;
    target["base_load_limit"] = source.BaseLoadLimit;
    target["ignore_soc"] = source.IgnoreSoc;
    target["battery_soc_start_threshold"] = source.BatterySocStartThreshold;
    target["battery_soc_stop_threshold"] = source.BatterySocStopThreshold;
    target["voltage_start_threshold"] = roundedFloat(source.VoltageStartThreshold);
    target["voltage_stop_threshold"] = roundedFloat(source.VoltageStopThreshold);
    target["voltage_load_correction_factor"] = source.VoltageLoadCorrectionFactor;
    target["full_solar_passthrough_soc"] = source.FullSolarPassThroughSoc;
    target["full_solar_passthrough_start_voltage"] = roundedFloat(source.FullSolarPassThroughStartVoltage);
    target["full_solar_passthrough_stop_voltage"] = roundedFloat(source.FullSolarPassThroughStopVoltage);
    target["inverter_serial_for_dc_voltage"] = serialStr(source.InverterSerialForDcVoltage);
    target["inverter_channel_id_for_dc_voltage"] = source.InverterChannelIdForDcVoltage;
    target["inverter_restart_hour"] = source.RestartHour;
    target["total_upper_power_limit"] = source.TotalUpperPowerLimit;

    JsonArray inverters = target["inverters"].to<JsonArray>();
    for (size_t i = 0; i < INV_MAX_COUNT; ++i) {
        PowerLimiterInverterConfig const& s = source.Inverters[i];
        if (s.Serial == 0ULL) { break; }
        JsonObject t = inverters.add<JsonObject>();

        t["serial"] = serialStr(s.Serial);
        t["is_governed"] = s.IsGoverned;
        t["is_behind_power_meter"] = s.IsBehindPowerMeter;
        t["power_source"] = s.PowerSource;
        t["use_overscaling_to_compensate_shading"] = s.UseOverscaling;
        t["lower_power_limit"] = s.LowerPowerLimit;
        t["upper_power_limit"] = s.UpperPowerLimit;
        t["scaling_threshold"] = s.ScalingThreshold;
    }
}

void ConfigurationClass::serializeGridChargerConfig(GridChargerConfig const& source, JsonObject& target)
{
    target["enabled"] = source.Enabled;
    target["verbose_logging"] = source.VerboseLogging;
    target["hardware_interface"] = source.HardwareInterface;
    target["can_controller_frequency"] = source.CAN_Controller_Frequency;
    target["auto_power_enabled"] = source.Auto_Power_Enabled;
    target["auto_power_batterysoc_limits_enabled"] = source.Auto_Power_BatterySoC_Limits_Enabled;
    target["emergency_charge_enabled"] = source.Emergency_Charge_Enabled;
    target["voltage_limit"] = roundedFloat(source.Auto_Power_Voltage_Limit);
    target["enable_voltage_limit"] = roundedFloat(source.Auto_Power_Enable_Voltage_Limit);
    target["lower_power_limit"] = source.Auto_Power_Lower_Power_Limit;
    target["upper_power_limit"] = source.Auto_Power_Upper_Power_Limit;
    target["stop_batterysoc_threshold"] = source.Auto_Power_Stop_BatterySoC_Threshold;
    target["target_power_consumption"] = source.Auto_Power_Target_Power_Consumption;
}

bool ConfigurationClass::write()
{
    File f = LittleFS.open(CONFIG_FILENAME, "w");
    if (!f) {
        return false;
    }
    config.Cfg.SaveCount++;

    JsonDocument doc;

    JsonObject cfg = doc["cfg"].to<JsonObject>();
    cfg["version"] = config.Cfg.Version;
    cfg["version_onbattery"] = config.Cfg.VersionOnBattery;
    cfg["save_count"] = config.Cfg.SaveCount;

    JsonObject wifi = doc["wifi"].to<JsonObject>();
    wifi["ssid"] = config.WiFi.Ssid;
    wifi["password"] = config.WiFi.Password;
    wifi["ip"] = IPAddress(config.WiFi.Ip).toString();
    wifi["netmask"] = IPAddress(config.WiFi.Netmask).toString();
    wifi["gateway"] = IPAddress(config.WiFi.Gateway).toString();
    wifi["dns1"] = IPAddress(config.WiFi.Dns1).toString();
    wifi["dns2"] = IPAddress(config.WiFi.Dns2).toString();
    wifi["dhcp"] = config.WiFi.Dhcp;
    wifi["hostname"] = config.WiFi.Hostname;
    wifi["aptimeout"] = config.WiFi.ApTimeout;

    JsonObject mdns = doc["mdns"].to<JsonObject>();
    mdns["enabled"] = config.Mdns.Enabled;

    JsonObject syslog = doc["syslog"].to<JsonObject>();
    syslog["enabled"] = config.Syslog.Enabled;
    syslog["hostname"] = config.Syslog.Hostname;
    syslog["port"] = config.Syslog.Port;

    JsonObject ntp = doc["ntp"].to<JsonObject>();
    ntp["server"] = config.Ntp.Server;
    ntp["timezone"] = config.Ntp.Timezone;
    ntp["timezone_descr"] = config.Ntp.TimezoneDescr;
    ntp["latitude"] = config.Ntp.Latitude;
    ntp["longitude"] = config.Ntp.Longitude;
    ntp["sunsettype"] = config.Ntp.SunsetType;

    JsonObject mqtt = doc["mqtt"].to<JsonObject>();
    mqtt["enabled"] = config.Mqtt.Enabled;
    mqtt["verbose_logging"] = config.Mqtt.VerboseLogging;
    mqtt["hostname"] = config.Mqtt.Hostname;
    mqtt["port"] = config.Mqtt.Port;
    mqtt["clientid"] = config.Mqtt.ClientId;
    mqtt["username"] = config.Mqtt.Username;
    mqtt["password"] = config.Mqtt.Password;
    mqtt["topic"] = config.Mqtt.Topic;
    mqtt["retain"] = config.Mqtt.Retain;
    mqtt["publish_interval"] = config.Mqtt.PublishInterval;
    mqtt["clean_session"] = config.Mqtt.CleanSession;

    JsonObject mqtt_lwt = mqtt["lwt"].to<JsonObject>();
    mqtt_lwt["topic"] = config.Mqtt.Lwt.Topic;
    mqtt_lwt["value_online"] = config.Mqtt.Lwt.Value_Online;
    mqtt_lwt["value_offline"] = config.Mqtt.Lwt.Value_Offline;
    mqtt_lwt["qos"] = config.Mqtt.Lwt.Qos;

    JsonObject mqtt_tls = mqtt["tls"].to<JsonObject>();
    mqtt_tls["enabled"] = config.Mqtt.Tls.Enabled;
    mqtt_tls["root_ca_cert"] = config.Mqtt.Tls.RootCaCert;
    mqtt_tls["certlogin"] = config.Mqtt.Tls.CertLogin;
    mqtt_tls["client_cert"] = config.Mqtt.Tls.ClientCert;
    mqtt_tls["client_key"] = config.Mqtt.Tls.ClientKey;

    JsonObject mqtt_hass = mqtt["hass"].to<JsonObject>();
    mqtt_hass["enabled"] = config.Mqtt.Hass.Enabled;
    mqtt_hass["retain"] = config.Mqtt.Hass.Retain;
    mqtt_hass["topic"] = config.Mqtt.Hass.Topic;
    mqtt_hass["individual_panels"] = config.Mqtt.Hass.IndividualPanels;
    mqtt_hass["expire"] = config.Mqtt.Hass.Expire;

    JsonObject dtu = doc["dtu"].to<JsonObject>();
    dtu["serial"] = config.Dtu.Serial;
    dtu["poll_interval"] = config.Dtu.PollInterval;
    dtu["verbose_logging"] = config.Dtu.VerboseLogging;
    dtu["nrf_pa_level"] = config.Dtu.Nrf.PaLevel;
    dtu["cmt_pa_level"] = config.Dtu.Cmt.PaLevel;
    dtu["cmt_frequency"] = config.Dtu.Cmt.Frequency;
    dtu["cmt_country_mode"] = config.Dtu.Cmt.CountryMode;

    JsonObject security = doc["security"].to<JsonObject>();
    security["password"] = config.Security.Password;
    security["allow_readonly"] = config.Security.AllowReadonly;

    JsonObject device = doc["device"].to<JsonObject>();
    device["pinmapping"] = config.Dev_PinMapping;

    JsonObject display = device["display"].to<JsonObject>();
    display["powersafe"] = config.Display.PowerSafe;
    display["screensaver"] = config.Display.ScreenSaver;
    display["rotation"] = config.Display.Rotation;
    display["contrast"] = config.Display.Contrast;
    display["locale"] = config.Display.Locale;
    display["diagram_duration"] = config.Display.Diagram.Duration;
    display["diagram_mode"] = config.Display.Diagram.Mode;

    JsonArray leds = device["led"].to<JsonArray>();
    for (uint8_t i = 0; i < PINMAPPING_LED_COUNT; i++) {
        JsonObject led = leds.add<JsonObject>();
        led["brightness"] = config.Led_Single[i].Brightness;
    }

    JsonArray inverters = doc["inverters"].to<JsonArray>();
    for (uint8_t i = 0; i < INV_MAX_COUNT; i++) {
        JsonObject inv = inverters.add<JsonObject>();
        inv["serial"] = config.Inverter[i].Serial;
        inv["name"] = config.Inverter[i].Name;
        inv["order"] = config.Inverter[i].Order;
        inv["poll_enable"] = config.Inverter[i].Poll_Enable;
        inv["poll_enable_night"] = config.Inverter[i].Poll_Enable_Night;
        inv["command_enable"] = config.Inverter[i].Command_Enable;
        inv["command_enable_night"] = config.Inverter[i].Command_Enable_Night;
        inv["reachable_threshold"] = config.Inverter[i].ReachableThreshold;
        inv["zero_runtime"] = config.Inverter[i].ZeroRuntimeDataIfUnrechable;
        inv["zero_day"] = config.Inverter[i].ZeroYieldDayOnMidnight;
        inv["clear_eventlog"] = config.Inverter[i].ClearEventlogOnMidnight;
        inv["yieldday_correction"] = config.Inverter[i].YieldDayCorrection;

        JsonArray channel = inv["channel"].to<JsonArray>();
        for (uint8_t c = 0; c < INV_MAX_CHAN_COUNT; c++) {
            JsonObject chanData = channel.add<JsonObject>();
            chanData["name"] = config.Inverter[i].channel[c].Name;
            chanData["max_power"] = config.Inverter[i].channel[c].MaxChannelPower;
            chanData["yield_total_offset"] = config.Inverter[i].channel[c].YieldTotalOffset;
        }
    }

    JsonObject solarcharger = doc["solarcharger"].to<JsonObject>();
    serializeSolarChargerConfig(config.SolarCharger, solarcharger);

    JsonObject solarcharger_mqtt = solarcharger["mqtt"].to<JsonObject>();
    serializeSolarChargerMqttConfig(config.SolarCharger.Mqtt, solarcharger_mqtt);

    JsonObject powermeter = doc["powermeter"].to<JsonObject>();
    powermeter["enabled"] = config.PowerMeter.Enabled;
    powermeter["verbose_logging"] = config.PowerMeter.VerboseLogging;
    powermeter["source"] = config.PowerMeter.Source;

    JsonObject powermeter_mqtt = powermeter["mqtt"].to<JsonObject>();
    serializePowerMeterMqttConfig(config.PowerMeter.Mqtt, powermeter_mqtt);

    JsonObject powermeter_serial_sdm = powermeter["serial_sdm"].to<JsonObject>();
    serializePowerMeterSerialSdmConfig(config.PowerMeter.SerialSdm, powermeter_serial_sdm);

    JsonObject powermeter_http_json = powermeter["http_json"].to<JsonObject>();
    serializePowerMeterHttpJsonConfig(config.PowerMeter.HttpJson, powermeter_http_json);

    JsonObject powermeter_http_sml = powermeter["http_sml"].to<JsonObject>();
    serializePowerMeterHttpSmlConfig(config.PowerMeter.HttpSml, powermeter_http_sml);

    JsonObject powerlimiter = doc["powerlimiter"].to<JsonObject>();
    serializePowerLimiterConfig(config.PowerLimiter, powerlimiter);

    JsonObject battery = doc["battery"].to<JsonObject>();
    serializeBatteryConfig(config.Battery, battery);

    JsonObject huawei = doc["huawei"].to<JsonObject>();
    serializeGridChargerConfig(config.Huawei, huawei);

    JsonObject shelly = doc["shelly"].to<JsonObject>();
    shelly["enabled"] = config.Shelly.Enabled;
    shelly["verbose_logging"] = config.Shelly.VerboseLogging;
    shelly["auto_power_batterysoc_limits_enabled"]= config.Shelly.Auto_Power_BatterySoC_Limits_Enabled ;
    shelly["emergency_charge_enabled"]= config.Shelly.Emergency_Charge_Enabled;
    shelly["stop_batterysoc_threshold"] = config.Shelly.stop_batterysoc_threshold;
    shelly["start_batterysoc_threshold"] = config.Shelly.start_batterysoc_threshold;
    shelly["url"] = config.Shelly.url;
    shelly["uri_on"] = config.Shelly.uri_on;
    shelly["uri_off"] = config.Shelly.uri_off;
    shelly["uri_stats"] = config.Shelly.uri_stats;
    shelly["uri_powerparam"] = config.Shelly.uri_powerparam;
    shelly["power_on_threshold"] = config.Shelly.POWER_ON_threshold;
    shelly["power_off_threshold"] = config.Shelly.POWER_OFF_threshold;

    if (!Utils::checkJsonAlloc(doc, __FUNCTION__, __LINE__)) {
        return false;
    }

    // Serialize JSON to file
    if (serializeJson(doc, f) == 0) {
        MessageOutput.println("Failed to write file");
        return false;
    }

    f.close();
    return true;
}

void ConfigurationClass::deserializeHttpRequestConfig(JsonObject const& source_http_config, HttpRequestConfig& target)
{
    strlcpy(target.Url, source_http_config["url"] | "", sizeof(target.Url));
    target.AuthType = source_http_config["auth_type"] | HttpRequestConfig::Auth::None;
    strlcpy(target.Username, source_http_config["username"] | "", sizeof(target.Username));
    strlcpy(target.Password, source_http_config["password"] | "", sizeof(target.Password));
    strlcpy(target.HeaderKey, source_http_config["header_key"] | "", sizeof(target.HeaderKey));
    strlcpy(target.HeaderValue, source_http_config["header_value"] | "", sizeof(target.HeaderValue));
    target.Timeout = source_http_config["timeout"] | HTTP_REQUEST_TIMEOUT_MS;
}

void ConfigurationClass::deserializeSolarChargerConfig(JsonObject const& source, SolarChargerConfig& target)
{
    target.Enabled = source["enabled"] | SOLAR_CHARGER_ENABLED;
    target.VerboseLogging = source["verbose_logging"] | VERBOSE_LOGGING;
    target.Provider = source["provider"] | SolarChargerProviderType::VEDIRECT;
    target.PublishUpdatesOnly = source["publish_updates_only"] | SOLAR_CHARGER_PUBLISH_UPDATES_ONLY;
}

void ConfigurationClass::deserializeSolarChargerMqttConfig(JsonObject const& source, SolarChargerMqttConfig& target)
{
    target.CalculateOutputPower = source["calculate_output_power"];
    strlcpy(target.PowerTopic, source["power_topic"] | "", sizeof(target.PowerTopic));
    strlcpy(target.PowerJsonPath, source["power_path"] | "", sizeof(target.PowerJsonPath));
    target.PowerUnit = source["power_unit"] | SolarChargerMqttConfig::WattageUnit::Watts;
    strlcpy(target.VoltageTopic, source["voltage_topic"] | "", sizeof(target.VoltageTopic));
    strlcpy(target.VoltageJsonPath, source["voltage_path"] | "", sizeof(target.VoltageJsonPath));
    target.VoltageTopicUnit = source["voltage_unit"] | SolarChargerMqttConfig::VoltageUnit::Volts;
    strlcpy(target.CurrentTopic, source["current_topic"] | "", sizeof(target.CurrentTopic));
    strlcpy(target.CurrentJsonPath, source["current_path"] | "", sizeof(target.CurrentJsonPath));
    target.CurrentUnit = source["current_unit"] | SolarChargerMqttConfig::AmperageUnit::Amps;
}

void ConfigurationClass::deserializePowerMeterMqttConfig(JsonObject const& source, PowerMeterMqttConfig& target)
{
    for (size_t i = 0; i < POWERMETER_MQTT_MAX_VALUES; ++i) {
        PowerMeterMqttValue& t = target.Values[i];
        JsonObject s = source["values"][i];

        strlcpy(t.Topic, s["topic"] | "", sizeof(t.Topic));
        strlcpy(t.JsonPath, s["json_path"] | "", sizeof(t.JsonPath));
        t.PowerUnit = s["unit"] | PowerMeterMqttValue::Unit::Watts;
        t.SignInverted = s["sign_inverted"] | false;
    }
}

void ConfigurationClass::deserializePowerMeterSerialSdmConfig(JsonObject const& source, PowerMeterSerialSdmConfig& target)
{
    target.PollingInterval = source["polling_interval"] | POWERMETER_POLLING_INTERVAL;
    target.Address = source["address"] | POWERMETER_SDMADDRESS;
}

void ConfigurationClass::deserializePowerMeterHttpJsonConfig(JsonObject const& source, PowerMeterHttpJsonConfig& target)
{
    target.PollingInterval = source["polling_interval"] | POWERMETER_POLLING_INTERVAL;
    target.IndividualRequests = source["individual_requests"] | false;

    JsonArray values = source["values"].as<JsonArray>();
    for (size_t i = 0; i < POWERMETER_HTTP_JSON_MAX_VALUES; ++i) {
        PowerMeterHttpJsonValue& t = target.Values[i];
        JsonObject s = values[i];

        deserializeHttpRequestConfig(s["http_request"], t.HttpRequest);

        t.Enabled = s["enabled"] | false;
        strlcpy(t.JsonPath, s["json_path"] | "", sizeof(t.JsonPath));
        t.PowerUnit = s["unit"] | PowerMeterHttpJsonValue::Unit::Watts;
        t.SignInverted = s["sign_inverted"] | false;
    }

    target.Values[0].Enabled = true;
}

void ConfigurationClass::deserializePowerMeterHttpSmlConfig(JsonObject const& source, PowerMeterHttpSmlConfig& target)
{
    target.PollingInterval = source["polling_interval"] | POWERMETER_POLLING_INTERVAL;
    deserializeHttpRequestConfig(source["http_request"], target.HttpRequest);
}

void ConfigurationClass::deserializeBatteryConfig(JsonObject const& source, BatteryConfig& target)
{
    target.Enabled = source["enabled"] | BATTERY_ENABLED;
    target.VerboseLogging = source["verbose_logging"] | VERBOSE_LOGGING;
    target.Provider = source["provider"] | BATTERY_PROVIDER;
    target.JkBmsInterface = source["jkbms_interface"] | BATTERY_JKBMS_INTERFACE;
    target.JkBmsPollingInterval = source["jkbms_polling_interval"] | BATTERY_JKBMS_POLLING_INTERVAL;
    strlcpy(target.MqttSocTopic, source["mqtt_soc_topic"] | source["mqtt_topic"] | "", sizeof(config.Battery.MqttSocTopic)); // mqtt_soc_topic was previously saved as mqtt_topic. Be nice and also try old key.
    strlcpy(target.MqttSocJsonPath, source["mqtt_soc_json_path"] | source["mqtt_json_path"] | "", sizeof(config.Battery.MqttSocJsonPath)); // mqtt_soc_json_path was previously saved as mqtt_json_path. Be nice and also try old key.
    strlcpy(target.MqttVoltageTopic, source["mqtt_voltage_topic"] | "", sizeof(config.Battery.MqttVoltageTopic));
    strlcpy(target.MqttVoltageJsonPath, source["mqtt_voltage_json_path"] | "", sizeof(config.Battery.MqttVoltageJsonPath));
    target.MqttVoltageUnit = source["mqtt_voltage_unit"] | BatteryVoltageUnit::Volts;
    target.EnableDischargeCurrentLimit = source["enable_discharge_current_limit"] | BATTERY_ENABLE_DISCHARGE_CURRENT_LIMIT;
    target.DischargeCurrentLimit = source["discharge_current_limit"] | BATTERY_DISCHARGE_CURRENT_LIMIT;
    target.DischargeCurrentLimitBelowSoc = source["discharge_current_limit_below_soc"] | BATTERY_DISCHARGE_CURRENT_LIMIT_BELOW_SOC;
    target.DischargeCurrentLimitBelowVoltage = source["discharge_current_limit_below_voltage"] | BATTERY_DISCHARGE_CURRENT_LIMIT_BELOW_VOLTAGE;
    target.UseBatteryReportedDischargeCurrentLimit = source["use_battery_reported_discharge_current_limit"] | BATTERY_USE_BATTERY_REPORTED_DISCHARGE_CURRENT_LIMIT;
    strlcpy(target.MqttDischargeCurrentTopic, source["mqtt_discharge_current_topic"] | "", sizeof(config.Battery.MqttDischargeCurrentTopic));
    strlcpy(target.MqttDischargeCurrentJsonPath, source["mqtt_discharge_current_json_path"] | "", sizeof(config.Battery.MqttDischargeCurrentJsonPath));
    target.MqttAmperageUnit = source["mqtt_amperage_unit"] | BatteryAmperageUnit::Amps;
}

void ConfigurationClass::deserializePowerLimiterConfig(JsonObject const& source, PowerLimiterConfig& target)
{
    auto serialBin = [](String const& input) -> uint64_t {
        return strtoll(input.c_str(), NULL, 16);
    };

    target.Enabled = source["enabled"] | POWERLIMITER_ENABLED;
    target.VerboseLogging = source["verbose_logging"] | VERBOSE_LOGGING;
    target.SolarPassThroughEnabled = source["solar_passthrough_enabled"] | POWERLIMITER_SOLAR_PASSTHROUGH_ENABLED;
    target.ConductionLosses = source["conduction_losses"] | POWERLIMITER_CONDUCTION_LOSSES;
    target.BatteryAlwaysUseAtNight = source["battery_always_use_at_night"] | POWERLIMITER_BATTERY_ALWAYS_USE_AT_NIGHT;
    target.TargetPowerConsumption = source["target_power_consumption"] | POWERLIMITER_TARGET_POWER_CONSUMPTION;
    target.TargetPowerConsumptionHysteresis = source["target_power_consumption_hysteresis"] | POWERLIMITER_TARGET_POWER_CONSUMPTION_HYSTERESIS;
    target.BaseLoadLimit = source["base_load_limit"] | POWERLIMITER_BASE_LOAD_LIMIT;
    target.IgnoreSoc = source["ignore_soc"] | POWERLIMITER_IGNORE_SOC;
    target.BatterySocStartThreshold = source["battery_soc_start_threshold"] | POWERLIMITER_BATTERY_SOC_START_THRESHOLD;
    target.BatterySocStopThreshold = source["battery_soc_stop_threshold"] | POWERLIMITER_BATTERY_SOC_STOP_THRESHOLD;
    target.VoltageStartThreshold = source["voltage_start_threshold"] | POWERLIMITER_VOLTAGE_START_THRESHOLD;
    target.VoltageStopThreshold = source["voltage_stop_threshold"] | POWERLIMITER_VOLTAGE_STOP_THRESHOLD;
    target.VoltageLoadCorrectionFactor = source["voltage_load_correction_factor"] | POWERLIMITER_VOLTAGE_LOAD_CORRECTION_FACTOR;
    target.FullSolarPassThroughSoc = source["full_solar_passthrough_soc"] | POWERLIMITER_FULL_SOLAR_PASSTHROUGH_SOC;
    target.FullSolarPassThroughStartVoltage = source["full_solar_passthrough_start_voltage"] | POWERLIMITER_FULL_SOLAR_PASSTHROUGH_START_VOLTAGE;
    target.FullSolarPassThroughStopVoltage = source["full_solar_passthrough_stop_voltage"] | POWERLIMITER_FULL_SOLAR_PASSTHROUGH_STOP_VOLTAGE;
    target.InverterSerialForDcVoltage = serialBin(source["inverter_serial_for_dc_voltage"] | String("0"));
    target.InverterChannelIdForDcVoltage = source["inverter_channel_id_for_dc_voltage"] | POWERLIMITER_INVERTER_CHANNEL_ID;
    target.RestartHour = source["inverter_restart_hour"] | POWERLIMITER_RESTART_HOUR;
    target.TotalUpperPowerLimit = source["total_upper_power_limit"] | POWERLIMITER_UPPER_POWER_LIMIT;

    JsonArray inverters = source["inverters"].as<JsonArray>();
    for (size_t i = 0; i < INV_MAX_COUNT; ++i) {
        PowerLimiterInverterConfig& inv = target.Inverters[i];
        JsonObject s = inverters[i];

        inv.Serial = serialBin(s["serial"] | String("0")); // 0 marks inverter slot as unused
        inv.IsGoverned = s["is_governed"] | false;
        inv.IsBehindPowerMeter = s["is_behind_power_meter"] | POWERLIMITER_IS_INVERTER_BEHIND_POWER_METER;
        inv.PowerSource = s["power_source"] | PowerLimiterInverterConfig::InverterPowerSource::Battery;
        inv.UseOverscaling = s["use_overscaling_to_compensate_shading"] | POWERLIMITER_USE_OVERSCALING;
        inv.LowerPowerLimit = s["lower_power_limit"] | POWERLIMITER_LOWER_POWER_LIMIT;
        inv.UpperPowerLimit = s["upper_power_limit"] | POWERLIMITER_UPPER_POWER_LIMIT;
        inv.ScalingThreshold = s["scaling_threshold"] | POWERLIMITER_SCALING_THRESHOLD;
    }
}

void ConfigurationClass::deserializeGridChargerConfig(JsonObject const& source, GridChargerConfig& target)
{
    target.Enabled = source["enabled"] | HUAWEI_ENABLED;
    target.VerboseLogging = source["verbose_logging"] | VERBOSE_LOGGING;
    target.HardwareInterface = source["hardware_interface"] | GridChargerHardwareInterface::MCP2515;
    target.CAN_Controller_Frequency = source["can_controller_frequency"] | HUAWEI_CAN_CONTROLLER_FREQUENCY;
    target.Auto_Power_Enabled = source["auto_power_enabled"] | false;
    target.Auto_Power_BatterySoC_Limits_Enabled = source["auto_power_batterysoc_limits_enabled"] | false;
    target.Emergency_Charge_Enabled = source["emergency_charge_enabled"] | false;
    target.Auto_Power_Voltage_Limit = source["voltage_limit"] | HUAWEI_AUTO_POWER_VOLTAGE_LIMIT;
    target.Auto_Power_Enable_Voltage_Limit =  source["enable_voltage_limit"] | HUAWEI_AUTO_POWER_ENABLE_VOLTAGE_LIMIT;
    target.Auto_Power_Lower_Power_Limit = source["lower_power_limit"] | HUAWEI_AUTO_POWER_LOWER_POWER_LIMIT;
    target.Auto_Power_Upper_Power_Limit = source["upper_power_limit"] | HUAWEI_AUTO_POWER_UPPER_POWER_LIMIT;
    target.Auto_Power_Stop_BatterySoC_Threshold = source["stop_batterysoc_threshold"] | HUAWEI_AUTO_POWER_STOP_BATTERYSOC_THRESHOLD;
    target.Auto_Power_Target_Power_Consumption = source["target_power_consumption"] | HUAWEI_AUTO_POWER_TARGET_POWER_CONSUMPTION;
}

bool ConfigurationClass::read()
{
    File f = LittleFS.open(CONFIG_FILENAME, "r", false);
    Utils::skipBom(f);

    JsonDocument doc;

    // as OpenDTU-OnBattery was in use a long time without the version marker
    // specific to OpenDTU-OnBattery, we must distinguish the cases (1) where a
    // valid legacy config.json file was read and (2) where there was no config
    // (or an error when reading occured). in the former case we want to
    // perform a migration, whereas in the latter there is no need for a
    // migration as the config is default-initialized to the current version.
    uint32_t version_onbattery = 0;

    // Deserialize the JSON document
    const DeserializationError error = deserializeJson(doc, f);
    if (error) {
        version_onbattery = CONFIG_VERSION_ONBATTERY;
        MessageOutput.println("Failed to read file, using default configuration");
    }

    if (!Utils::checkJsonAlloc(doc, __FUNCTION__, __LINE__)) {
        return false;
    }

    JsonObject cfg = doc["cfg"];
    config.Cfg.Version = cfg["version"] | CONFIG_VERSION;
    config.Cfg.VersionOnBattery = cfg["version_onbattery"] | version_onbattery;
    config.Cfg.SaveCount = cfg["save_count"] | 0;

    JsonObject wifi = doc["wifi"];
    strlcpy(config.WiFi.Ssid, wifi["ssid"] | WIFI_SSID, sizeof(config.WiFi.Ssid));
    strlcpy(config.WiFi.Password, wifi["password"] | WIFI_PASSWORD, sizeof(config.WiFi.Password));
    strlcpy(config.WiFi.Hostname, wifi["hostname"] | APP_HOSTNAME, sizeof(config.WiFi.Hostname));

    IPAddress wifi_ip;
    wifi_ip.fromString(wifi["ip"] | "");
    config.WiFi.Ip[0] = wifi_ip[0];
    config.WiFi.Ip[1] = wifi_ip[1];
    config.WiFi.Ip[2] = wifi_ip[2];
    config.WiFi.Ip[3] = wifi_ip[3];

    IPAddress wifi_netmask;
    wifi_netmask.fromString(wifi["netmask"] | "");
    config.WiFi.Netmask[0] = wifi_netmask[0];
    config.WiFi.Netmask[1] = wifi_netmask[1];
    config.WiFi.Netmask[2] = wifi_netmask[2];
    config.WiFi.Netmask[3] = wifi_netmask[3];

    IPAddress wifi_gateway;
    wifi_gateway.fromString(wifi["gateway"] | "");
    config.WiFi.Gateway[0] = wifi_gateway[0];
    config.WiFi.Gateway[1] = wifi_gateway[1];
    config.WiFi.Gateway[2] = wifi_gateway[2];
    config.WiFi.Gateway[3] = wifi_gateway[3];

    IPAddress wifi_dns1;
    wifi_dns1.fromString(wifi["dns1"] | "");
    config.WiFi.Dns1[0] = wifi_dns1[0];
    config.WiFi.Dns1[1] = wifi_dns1[1];
    config.WiFi.Dns1[2] = wifi_dns1[2];
    config.WiFi.Dns1[3] = wifi_dns1[3];

    IPAddress wifi_dns2;
    wifi_dns2.fromString(wifi["dns2"] | "");
    config.WiFi.Dns2[0] = wifi_dns2[0];
    config.WiFi.Dns2[1] = wifi_dns2[1];
    config.WiFi.Dns2[2] = wifi_dns2[2];
    config.WiFi.Dns2[3] = wifi_dns2[3];

    config.WiFi.Dhcp = wifi["dhcp"] | WIFI_DHCP;
    config.WiFi.ApTimeout = wifi["aptimeout"] | ACCESS_POINT_TIMEOUT;

    JsonObject mdns = doc["mdns"];
    config.Mdns.Enabled = mdns["enabled"] | MDNS_ENABLED;

    JsonObject syslog = doc["syslog"];
    config.Syslog.Enabled = syslog["enabled"] | SYSLOG_ENABLED;
    strlcpy(config.Syslog.Hostname, syslog["hostname"] | "", sizeof(config.Syslog.Hostname));
    config.Syslog.Port = syslog["port"] | SYSLOG_PORT;

    JsonObject ntp = doc["ntp"];
    strlcpy(config.Ntp.Server, ntp["server"] | NTP_SERVER, sizeof(config.Ntp.Server));
    strlcpy(config.Ntp.Timezone, ntp["timezone"] | NTP_TIMEZONE, sizeof(config.Ntp.Timezone));
    strlcpy(config.Ntp.TimezoneDescr, ntp["timezone_descr"] | NTP_TIMEZONEDESCR, sizeof(config.Ntp.TimezoneDescr));
    config.Ntp.Latitude = ntp["latitude"] | NTP_LATITUDE;
    config.Ntp.Longitude = ntp["longitude"] | NTP_LONGITUDE;
    config.Ntp.SunsetType = ntp["sunsettype"] | NTP_SUNSETTYPE;

    JsonObject mqtt = doc["mqtt"];
    config.Mqtt.Enabled = mqtt["enabled"] | MQTT_ENABLED;
    config.Mqtt.VerboseLogging = mqtt["verbose_logging"] | VERBOSE_LOGGING;
    strlcpy(config.Mqtt.Hostname, mqtt["hostname"] | MQTT_HOST, sizeof(config.Mqtt.Hostname));
    config.Mqtt.Port = mqtt["port"] | MQTT_PORT;
    strlcpy(config.Mqtt.ClientId, mqtt["clientid"] | NetworkSettings.getApName().c_str(), sizeof(config.Mqtt.ClientId));
    strlcpy(config.Mqtt.Username, mqtt["username"] | MQTT_USER, sizeof(config.Mqtt.Username));
    strlcpy(config.Mqtt.Password, mqtt["password"] | MQTT_PASSWORD, sizeof(config.Mqtt.Password));
    strlcpy(config.Mqtt.Topic, mqtt["topic"] | MQTT_TOPIC, sizeof(config.Mqtt.Topic));
    config.Mqtt.Retain = mqtt["retain"] | MQTT_RETAIN;
    config.Mqtt.PublishInterval = mqtt["publish_interval"] | MQTT_PUBLISH_INTERVAL;
    config.Mqtt.CleanSession = mqtt["clean_session"] | MQTT_CLEAN_SESSION;

    JsonObject mqtt_lwt = mqtt["lwt"];
    strlcpy(config.Mqtt.Lwt.Topic, mqtt_lwt["topic"] | MQTT_LWT_TOPIC, sizeof(config.Mqtt.Lwt.Topic));
    strlcpy(config.Mqtt.Lwt.Value_Online, mqtt_lwt["value_online"] | MQTT_LWT_ONLINE, sizeof(config.Mqtt.Lwt.Value_Online));
    strlcpy(config.Mqtt.Lwt.Value_Offline, mqtt_lwt["value_offline"] | MQTT_LWT_OFFLINE, sizeof(config.Mqtt.Lwt.Value_Offline));
    config.Mqtt.Lwt.Qos = mqtt_lwt["qos"] | MQTT_LWT_QOS;

    JsonObject mqtt_tls = mqtt["tls"];
    config.Mqtt.Tls.Enabled = mqtt_tls["enabled"] | MQTT_TLS;
    strlcpy(config.Mqtt.Tls.RootCaCert, mqtt_tls["root_ca_cert"] | MQTT_ROOT_CA_CERT, sizeof(config.Mqtt.Tls.RootCaCert));
    config.Mqtt.Tls.CertLogin = mqtt_tls["certlogin"] | MQTT_TLSCERTLOGIN;
    strlcpy(config.Mqtt.Tls.ClientCert, mqtt_tls["client_cert"] | MQTT_TLSCLIENTCERT, sizeof(config.Mqtt.Tls.ClientCert));
    strlcpy(config.Mqtt.Tls.ClientKey, mqtt_tls["client_key"] | MQTT_TLSCLIENTKEY, sizeof(config.Mqtt.Tls.ClientKey));

    JsonObject mqtt_hass = mqtt["hass"];
    config.Mqtt.Hass.Enabled = mqtt_hass["enabled"] | MQTT_HASS_ENABLED;
    config.Mqtt.Hass.Retain = mqtt_hass["retain"] | MQTT_HASS_RETAIN;
    config.Mqtt.Hass.Expire = mqtt_hass["expire"] | MQTT_HASS_EXPIRE;
    config.Mqtt.Hass.IndividualPanels = mqtt_hass["individual_panels"] | MQTT_HASS_INDIVIDUALPANELS;
    strlcpy(config.Mqtt.Hass.Topic, mqtt_hass["topic"] | MQTT_HASS_TOPIC, sizeof(config.Mqtt.Hass.Topic));

    JsonObject dtu = doc["dtu"];
    config.Dtu.Serial = dtu["serial"] | DTU_SERIAL;
    config.Dtu.PollInterval = dtu["poll_interval"] | DTU_POLL_INTERVAL;
    config.Dtu.VerboseLogging = dtu["verbose_logging"] | VERBOSE_LOGGING;
    config.Dtu.Nrf.PaLevel = dtu["nrf_pa_level"] | DTU_NRF_PA_LEVEL;
    config.Dtu.Cmt.PaLevel = dtu["cmt_pa_level"] | DTU_CMT_PA_LEVEL;
    config.Dtu.Cmt.Frequency = dtu["cmt_frequency"] | DTU_CMT_FREQUENCY;
    config.Dtu.Cmt.CountryMode = dtu["cmt_country_mode"] | DTU_CMT_COUNTRY_MODE;

    JsonObject security = doc["security"];
    strlcpy(config.Security.Password, security["password"] | ACCESS_POINT_PASSWORD, sizeof(config.Security.Password));
    config.Security.AllowReadonly = security["allow_readonly"] | SECURITY_ALLOW_READONLY;

    JsonObject device = doc["device"];
    strlcpy(config.Dev_PinMapping, device["pinmapping"] | DEV_PINMAPPING, sizeof(config.Dev_PinMapping));

    JsonObject display = device["display"];
    config.Display.PowerSafe = display["powersafe"] | DISPLAY_POWERSAFE;
    config.Display.ScreenSaver = display["screensaver"] | DISPLAY_SCREENSAVER;
    config.Display.Rotation = display["rotation"] | DISPLAY_ROTATION;
    config.Display.Contrast = display["contrast"] | DISPLAY_CONTRAST;
    strlcpy(config.Display.Locale, display["locale"] | DISPLAY_LOCALE, sizeof(config.Display.Locale));
    config.Display.Diagram.Duration = display["diagram_duration"] | DISPLAY_DIAGRAM_DURATION;
    config.Display.Diagram.Mode = display["diagram_mode"] | DISPLAY_DIAGRAM_MODE;

    JsonArray leds = device["led"];
    for (uint8_t i = 0; i < PINMAPPING_LED_COUNT; i++) {
        JsonObject led = leds[i].as<JsonObject>();
        config.Led_Single[i].Brightness = led["brightness"] | LED_BRIGHTNESS;
    }

    JsonArray inverters = doc["inverters"];
    for (uint8_t i = 0; i < INV_MAX_COUNT; i++) {
        JsonObject inv = inverters[i].as<JsonObject>();
        config.Inverter[i].Serial = inv["serial"] | 0ULL;
        strlcpy(config.Inverter[i].Name, inv["name"] | "", sizeof(config.Inverter[i].Name));
        config.Inverter[i].Order = inv["order"] | 0;

        config.Inverter[i].Poll_Enable = inv["poll_enable"] | true;
        config.Inverter[i].Poll_Enable_Night = inv["poll_enable_night"] | true;
        config.Inverter[i].Command_Enable = inv["command_enable"] | true;
        config.Inverter[i].Command_Enable_Night = inv["command_enable_night"] | true;
        config.Inverter[i].ReachableThreshold = inv["reachable_threshold"] | REACHABLE_THRESHOLD;
        config.Inverter[i].ZeroRuntimeDataIfUnrechable = inv["zero_runtime"] | false;
        config.Inverter[i].ZeroYieldDayOnMidnight = inv["zero_day"] | false;
        config.Inverter[i].ClearEventlogOnMidnight = inv["clear_eventlog"] | false;
        config.Inverter[i].YieldDayCorrection = inv["yieldday_correction"] | false;

        JsonArray channel = inv["channel"];
        for (uint8_t c = 0; c < INV_MAX_CHAN_COUNT; c++) {
            config.Inverter[i].channel[c].MaxChannelPower = channel[c]["max_power"] | 0;
            config.Inverter[i].channel[c].YieldTotalOffset = channel[c]["yield_total_offset"] | 0.0f;
            strlcpy(config.Inverter[i].channel[c].Name, channel[c]["name"] | "", sizeof(config.Inverter[i].channel[c].Name));
        }
    }

    JsonObject solarcharger = doc["solarcharger"];
    deserializeSolarChargerConfig(solarcharger, config.SolarCharger);
    deserializeSolarChargerMqttConfig(solarcharger["mqtt"], config.SolarCharger.Mqtt);

    JsonObject powermeter = doc["powermeter"];
    config.PowerMeter.Enabled = powermeter["enabled"] | POWERMETER_ENABLED;
    config.PowerMeter.VerboseLogging = powermeter["verbose_logging"] | VERBOSE_LOGGING;
    config.PowerMeter.Source =  powermeter["source"] | POWERMETER_SOURCE;

    deserializePowerMeterMqttConfig(powermeter["mqtt"], config.PowerMeter.Mqtt);

    deserializePowerMeterSerialSdmConfig(powermeter["serial_sdm"], config.PowerMeter.SerialSdm);

    deserializePowerMeterHttpJsonConfig(powermeter["http_json"], config.PowerMeter.HttpJson);

    deserializePowerMeterHttpSmlConfig(powermeter["http_sml"], config.PowerMeter.HttpSml);

    deserializePowerLimiterConfig(doc["powerlimiter"], config.PowerLimiter);

    deserializeBatteryConfig(doc["battery"], config.Battery);

    deserializeGridChargerConfig(doc["huawei"], config.Huawei);

    JsonObject shelly = doc["shelly"];
    config.Shelly.Enabled = shelly["enabled"] | SHELLY_ENABLED;
    config.Shelly.VerboseLogging = shelly["verbose_logging"] | VERBOSE_LOGGING;
    config.Shelly.Auto_Power_BatterySoC_Limits_Enabled = shelly["auto_power_batterysoc_limits_enabled"] | false;
    config.Shelly.Emergency_Charge_Enabled = shelly["emergency_charge_enabled"] | false;
    config.Shelly.stop_batterysoc_threshold = shelly["stop_batterysoc_threshold"] | SHELLY_STOP_BATTERYSOC_THRESHOLD;
    config.Shelly.start_batterysoc_threshold = shelly["start_batterysoc_threshold"] | SHELLY_START_BATTERYSOC_THRESHOLD;
    strlcpy(config.Shelly.url, shelly["url"] | SHELLY_IPADDRESS, sizeof(config.Shelly.url));
    strlcpy(config.Shelly.uri_on, shelly["uri_on"] | SHELLY_URION, sizeof(config.Shelly.uri_on));
    strlcpy(config.Shelly.uri_off, shelly["uri_off"] | SHELLY_URIOFF, sizeof(config.Shelly.uri_off));
    strlcpy(config.Shelly.uri_stats, shelly["uri_stats"] | SHELLY_URIOFF, sizeof(config.Shelly.uri_stats));
    strlcpy(config.Shelly.uri_powerparam, shelly["uri_powerparam"] | SHELLY_URIOFF, sizeof(config.Shelly.uri_powerparam));
    config.Shelly.POWER_ON_threshold = shelly["power_on_threshold"] | SHELLY_POWER_ON_THRESHOLD;
    config.Shelly.POWER_OFF_threshold = shelly["power_off_threshold"] | SHELLY_POWER_OFF_THRESHOLD;


    f.close();

    // Check for default DTU serial
    MessageOutput.print("Check for default DTU serial... ");
    if (config.Dtu.Serial == DTU_SERIAL) {
        MessageOutput.print("generate serial based on ESP chip id: ");
        const uint64_t dtuId = Utils::generateDtuSerial();
        MessageOutput.printf("%0" PRIx32 "%08" PRIx32 "... ",
            static_cast<uint32_t>((dtuId >> 32) & 0xFFFFFFFF),
            static_cast<uint32_t>(dtuId & 0xFFFFFFFF));
        config.Dtu.Serial = dtuId;
        write();
    }
    MessageOutput.println("done");

    return true;
}

void ConfigurationClass::migrate()
{
    File f = LittleFS.open(CONFIG_FILENAME, "r", false);
    if (!f) {
        MessageOutput.println("Failed to open file, cancel migration");
        return;
    }

    Utils::skipBom(f);

    JsonDocument doc;

    // Deserialize the JSON document
    const DeserializationError error = deserializeJson(doc, f);
    if (error) {
        MessageOutput.printf("Failed to read file, cancel migration: %s\r\n", error.c_str());
        return;
    }

    if (!Utils::checkJsonAlloc(doc, __FUNCTION__, __LINE__)) {
        return;
    }

    if (config.Cfg.Version < 0x00011700) {
        JsonArray inverters = doc["inverters"];
        for (uint8_t i = 0; i < INV_MAX_COUNT; i++) {
            JsonObject inv = inverters[i].as<JsonObject>();
            JsonArray channels = inv["channels"];
            for (uint8_t c = 0; c < INV_MAX_CHAN_COUNT; c++) {
                config.Inverter[i].channel[c].MaxChannelPower = channels[c];
                strlcpy(config.Inverter[i].channel[c].Name, "", sizeof(config.Inverter[i].channel[c].Name));
            }
        }
    }

    if (config.Cfg.Version < 0x00011800) {
        JsonObject mqtt = doc["mqtt"];
        config.Mqtt.PublishInterval = mqtt["publish_invterval"];
    }

    if (config.Cfg.Version < 0x00011900) {
        JsonObject dtu = doc["dtu"];
        config.Dtu.Nrf.PaLevel = dtu["pa_level"];
    }

    if (config.Cfg.Version < 0x00011a00) {
        // This migration fixes this issue: https://github.com/espressif/arduino-esp32/issues/8828
        // It occours when migrating from Core 2.0.9 to 2.0.14
        // which was done by updating ESP32 PlatformIO from 6.3.2 to 6.5.0
        nvs_flash_erase();
        nvs_flash_init();
    }

    if (config.Cfg.Version < 0x00011b00) {
        // Convert from kHz to Hz
        config.Dtu.Cmt.Frequency *= 1000;
    }

    if (config.Cfg.Version < 0x00011c00) {
        if (!strcmp(config.Ntp.Server, NTP_SERVER_OLD)) {
            strlcpy(config.Ntp.Server, NTP_SERVER, sizeof(config.Ntp.Server));
        }
    }

    if (config.Cfg.Version < 0x00011d00) {
        JsonObject device = doc["device"];
        JsonObject display = device["display"];
        switch (display["language"] | 0U) {
        case 0U:
            strlcpy(config.Display.Locale, "en", sizeof(config.Display.Locale));
            break;
        case 1U:
            strlcpy(config.Display.Locale, "de", sizeof(config.Display.Locale));
            break;
        case 2U:
            strlcpy(config.Display.Locale, "fr", sizeof(config.Display.Locale));
            break;
        }
    }

    f.close();

    config.Cfg.Version = CONFIG_VERSION;
    write();
    read();
}

void ConfigurationClass::migrateOnBattery()
{
    File f = LittleFS.open(CONFIG_FILENAME, "r", false);
    if (!f) {
        MessageOutput.println("Failed to open file, cancel OpenDTU-OnBattery migration");
        return;
    }

    Utils::skipBom(f);

    JsonDocument doc;

    // Deserialize the JSON document
    const DeserializationError error = deserializeJson(doc, f);
    if (error) {
        MessageOutput.printf("Failed to read file, cancel OpenDTU-OnBattery "
                "migration: %s\r\n", error.c_str());
        return;
    }

    if (!Utils::checkJsonAlloc(doc, __FUNCTION__, __LINE__)) {
        return;
    }

    if (config.Cfg.VersionOnBattery < 1) {
        // all migrations in this block need to check whether or not the
        // respective legacy setting is even present, as OpenDTU-OnBattery
        // config version 0 identifies multiple different legacy versions of
        // OpenDTU-OnBattery-specific settings, i.e., all before the
        // OpenDTU-OnBattery config version value was introduced.

        JsonObject powermeter = doc["powermeter"];

        if (!powermeter["mqtt_topic_powermeter_1"].isNull()) {
            auto& values = config.PowerMeter.Mqtt.Values;
            strlcpy(values[0].Topic, powermeter["mqtt_topic_powermeter_1"], sizeof(values[0].Topic));
            strlcpy(values[1].Topic, powermeter["mqtt_topic_powermeter_2"], sizeof(values[1].Topic));
            strlcpy(values[2].Topic, powermeter["mqtt_topic_powermeter_3"], sizeof(values[2].Topic));
        }

        if (!powermeter["sdmaddress"].isNull()) {
            config.PowerMeter.SerialSdm.Address = powermeter["sdmaddress"];
        }

        if (!powermeter["http_phases"].isNull()) {
            auto& target = config.PowerMeter.HttpJson;

            for (size_t i = 0; i < POWERMETER_HTTP_JSON_MAX_VALUES; ++i) {
                PowerMeterHttpJsonValue& t = target.Values[i];
                JsonObject s = powermeter["http_phases"][i];

                deserializeHttpRequestConfig(s, t.HttpRequest);

                t.Enabled = s["enabled"] | false;
                strlcpy(t.JsonPath, s["json_path"] | "", sizeof(t.JsonPath));
                t.PowerUnit = s["unit"] | PowerMeterHttpJsonValue::Unit::Watts;
                t.SignInverted = s["sign_inverted"] | false;
            }

            target.IndividualRequests = powermeter["http_individual_requests"] | false;
        }

        JsonObject powerlimiter = doc["powerlimiter"];

        if (powerlimiter["battery_drain_strategy"].as<uint8_t>() == 1) {
            config.PowerLimiter.BatteryAlwaysUseAtNight = true;
        }

        if (!powerlimiter["solar_passtrough_enabled"].isNull()) {
            config.PowerLimiter.SolarPassThroughEnabled = powerlimiter["solar_passtrough_enabled"].as<bool>();
        }

        if (!powerlimiter["solar_passtrough_losses"].isNull()) {
            config.PowerLimiter.ConductionLosses = powerlimiter["solar_passtrough_losses"].as<uint8_t>();
        }

        if (!powerlimiter["inverter_id"].isNull()) {
            config.PowerLimiter.InverterChannelIdForDcVoltage = powerlimiter["inverter_channel_id"] | POWERLIMITER_INVERTER_CHANNEL_ID;

            auto& inv = config.PowerLimiter.Inverters[0];
            uint64_t previousInverterSerial = powerlimiter["inverter_id"].as<uint64_t>();
            if (previousInverterSerial < INV_MAX_COUNT) {
                // we previously had an index (not a serial) saved as inverter_id.
                previousInverterSerial = config.Inverter[inv.Serial].Serial; // still 0 if no inverters configured
            }
            inv.Serial = previousInverterSerial;
            config.PowerLimiter.InverterSerialForDcVoltage = previousInverterSerial;
            inv.IsGoverned = true;
            inv.IsBehindPowerMeter = powerlimiter["is_inverter_behind_powermeter"] | POWERLIMITER_IS_INVERTER_BEHIND_POWER_METER;

            if (powerlimiter["is_inverter_solar_powered"]) {
                inv.PowerSource = PowerLimiterInverterConfig::InverterPowerSource::Solar;
            } else {
                inv.PowerSource = PowerLimiterInverterConfig::InverterPowerSource::Battery;
            }

            inv.UseOverscaling = powerlimiter["use_overscaling_to_compensate_shading"] | POWERLIMITER_USE_OVERSCALING;
            inv.LowerPowerLimit = powerlimiter["lower_power_limit"] | POWERLIMITER_LOWER_POWER_LIMIT;
            inv.UpperPowerLimit = powerlimiter["upper_power_limit"] | POWERLIMITER_UPPER_POWER_LIMIT;

            config.PowerLimiter.TotalUpperPowerLimit = inv.UpperPowerLimit;

            config.PowerLimiter.Inverters[1].Serial = 0;
        }
    }

    if (config.Cfg.VersionOnBattery < 2) {
        config.PowerLimiter.ConductionLosses = doc["powerlimiter"]["solar_passthrough_losses"].as<uint8_t>();
    }

    if (config.Cfg.VersionOnBattery < 3) {
        config.Dtu.PollInterval *= 1000; // new unit is milliseconds
    }

    if (config.Cfg.VersionOnBattery < 4) {
        JsonObject vedirect = doc["vedirect"];
        config.SolarCharger.Enabled = vedirect["enabled"] | SOLAR_CHARGER_ENABLED;
        config.SolarCharger.VerboseLogging = vedirect["verbose_logging"] | SOLAR_CHARGER_VERBOSE_LOGGING;
        config.SolarCharger.PublishUpdatesOnly = vedirect["updates_only"] | SOLAR_CHARGER_PUBLISH_UPDATES_ONLY;
    }

    if (config.Cfg.VersionOnBattery < 5) {
        JsonArray inverters = doc["powerlimiter"]["inverters"].as<JsonArray>();

        for (size_t i = 0; i < INV_MAX_COUNT; ++i) {
            PowerLimiterInverterConfig& inv = config.PowerLimiter.Inverters[i];
            JsonObject s = inverters[i];

            if (s["is_solar_powered"]) {
                inv.PowerSource = PowerLimiterInverterConfig::InverterPowerSource::Solar;
            } else {
                inv.PowerSource = PowerLimiterInverterConfig::InverterPowerSource::Battery;
            }
        }
    }

    f.close();

    config.Cfg.VersionOnBattery = CONFIG_VERSION_ONBATTERY;
    write();
    read();
}

CONFIG_T const& ConfigurationClass::get()
{
    return config;
}

ConfigurationClass::WriteGuard ConfigurationClass::getWriteGuard()
{
    return WriteGuard();
}

INVERTER_CONFIG_T* ConfigurationClass::getFreeInverterSlot()
{
    for (uint8_t i = 0; i < INV_MAX_COUNT; i++) {
        if (config.Inverter[i].Serial == 0) {
            return &config.Inverter[i];
        }
    }

    return nullptr;
}

INVERTER_CONFIG_T* ConfigurationClass::getInverterConfig(const uint64_t serial)
{
    for (uint8_t i = 0; i < INV_MAX_COUNT; i++) {
        if (config.Inverter[i].Serial == serial) {
            return &config.Inverter[i];
        }
    }

    return nullptr;
}

void ConfigurationClass::deleteInverterById(const uint8_t id)
{
    config.Inverter[id].Serial = 0ULL;
    strlcpy(config.Inverter[id].Name, "", sizeof(config.Inverter[id].Name));
    config.Inverter[id].Order = 0;

    config.Inverter[id].Poll_Enable = true;
    config.Inverter[id].Poll_Enable_Night = true;
    config.Inverter[id].Command_Enable = true;
    config.Inverter[id].Command_Enable_Night = true;
    config.Inverter[id].ReachableThreshold = REACHABLE_THRESHOLD;
    config.Inverter[id].ZeroRuntimeDataIfUnrechable = false;
    config.Inverter[id].ZeroYieldDayOnMidnight = false;
    config.Inverter[id].YieldDayCorrection = false;

    for (uint8_t c = 0; c < INV_MAX_CHAN_COUNT; c++) {
        config.Inverter[id].channel[c].MaxChannelPower = 0;
        config.Inverter[id].channel[c].YieldTotalOffset = 0.0f;
        strlcpy(config.Inverter[id].channel[c].Name, "", sizeof(config.Inverter[id].channel[c].Name));
    }
}

void ConfigurationClass::loop()
{
    std::unique_lock<std::mutex> lock(sWriterMutex);
    if (sWriterCount == 0) { return; }

    sWriterCv.notify_all();
    sWriterCv.wait(lock, [] { return sWriterCount == 0; });
}

CONFIG_T& ConfigurationClass::WriteGuard::getConfig()
{
    return config;
}

ConfigurationClass::WriteGuard::WriteGuard()
    : _lock(sWriterMutex)
{
    sWriterCount++;
    sWriterCv.wait(_lock);
}

ConfigurationClass::WriteGuard::~WriteGuard() {
    sWriterCount--;
    if (sWriterCount == 0) { sWriterCv.notify_all(); }
}

ConfigurationClass Configuration;
