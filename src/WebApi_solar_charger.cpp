// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2022-2024 Thomas Basler and others
 */
#include "WebApi_solarcharger.h"
#include "ArduinoJson.h"
#include "AsyncJson.h"
#include "Configuration.h"
#include "WebApi.h"
#include "WebApi_errors.h"
#include "helper.h"
#include "MqttHandlePowerLimiterHass.h"
#include <SolarCharger.h>

void WebApiSolarChargerlass::init(AsyncWebServer& server, Scheduler& scheduler)
{
    using std::placeholders::_1;

    _server = &server;

    _server->on("/api/solarcharger/config", HTTP_GET, std::bind(&WebApiSolarChargerlass::onAdminGet, this, _1));
    _server->on("/api/solarcharger/config", HTTP_POST, std::bind(&WebApiSolarChargerlass::onAdminPost, this, _1));
}

void WebApiSolarChargerlass::onAdminGet(AsyncWebServerRequest* request)
{
    if (!WebApi.checkCredentials(request)) {
        return;
    }

    AsyncJsonResponse* response = new AsyncJsonResponse();
    auto root = response->getRoot().as<JsonObject>();
    auto const& config = Configuration.get();

    ConfigurationClass::serializeSolarChargerConfig(config.SolarCharger, root);

    WebApi.sendJsonResponse(request, response, __FUNCTION__, __LINE__);
}

void WebApiSolarChargerlass::onAdminPost(AsyncWebServerRequest* request)
{
    if (!WebApi.checkCredentials(request)) {
        return;
    }

    AsyncJsonResponse* response = new AsyncJsonResponse();
    JsonDocument root;
    if (!WebApi.parseRequestData(request, response, root)) {
        return;
    }

    auto& retMsg = response->getRoot();

    if (!root["enabled"].is<bool>() ||
            !root["provider"].is<uint8_t>() ||
            !root["verbose_logging"].is<bool>()) {
        retMsg["message"] = "Values are missing!";
        retMsg["code"] = WebApiError::GenericValueMissing;
        WebApi.sendJsonResponse(request, response, __FUNCTION__, __LINE__);
        return;
    }

    {
        auto guard = Configuration.getWriteGuard();
        auto& config = guard.getConfig();
        ConfigurationClass::deserializeSolarChargerConfig(root.as<JsonObject>(), config.SolarCharger);
    }

    WebApi.writeConfig(retMsg);

    WebApi.sendJsonResponse(request, response, __FUNCTION__, __LINE__);

    SolarCharger.updateSettings();

    // potentially make solar passthrough thresholds auto-discoverable
    MqttHandlePowerLimiterHass.forceUpdate();
}
