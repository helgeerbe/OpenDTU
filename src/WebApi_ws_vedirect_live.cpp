// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2022 Thomas Basler and others
 */
#include "WebApi_ws_vedirect_live.h"
#include "AsyncJson.h"
#include "Configuration.h"
#include "MessageOutput.h"
#include "WebApi.h"
#include "defaults.h"
#include "PowerLimiter.h"
#include "VictronMppt.h"

WebApiWsVedirectLiveClass::WebApiWsVedirectLiveClass()
    : _ws("/vedirectlivedata")
{
}

void WebApiWsVedirectLiveClass::init(AsyncWebServer* server)
{
    using std::placeholders::_1;
    using std::placeholders::_2;
    using std::placeholders::_3;
    using std::placeholders::_4;
    using std::placeholders::_5;
    using std::placeholders::_6;

    _server = server;
    _server->on("/api/vedirectlivedata/status", HTTP_GET, std::bind(&WebApiWsVedirectLiveClass::onLivedataStatus, this, _1));

    _server->addHandler(&_ws);
    _ws.onEvent(std::bind(&WebApiWsVedirectLiveClass::onWebsocketEvent, this, _1, _2, _3, _4, _5, _6));
}

void WebApiWsVedirectLiveClass::loop()
{
    // see: https://github.com/me-no-dev/ESPAsyncWebServer#limiting-the-number-of-web-socket-clients
    if (millis() - _lastWsCleanup > 1000) {
        _ws.cleanupClients();
        _lastWsCleanup = millis();
    }

    // do nothing if no WS client is connected
    if (_ws.count() == 0) {
        return;
    }

    if (millis() - _lastVedirectUpdateCheck < 1000) {
        return;
    }
    _lastVedirectUpdateCheck = millis();

    uint32_t maxTimeStamp = 0;
    if (VictronMppt.getLastUpdate() > maxTimeStamp) {
        maxTimeStamp = VictronMppt.getLastUpdate();
    }

    // Update on ve.direct change or at least after 10 seconds
    if (millis() - _lastWsPublish > (10 * 1000) || (maxTimeStamp != _newestVedirectTimestamp)) {
        
        try {
            String buffer;
            // free JsonDocument as soon as possible
            {
                DynamicJsonDocument root(_responseSize);
                JsonVariant var = root;
                generateJsonResponse(var);
                serializeJson(root, buffer);
            }
            
            if (buffer) {        
                if (Configuration.get().Security_AllowReadonly) {
                    _ws.setAuthentication("", "");
                } else {
                    _ws.setAuthentication(AUTH_USERNAME, Configuration.get().Security_Password);
                }

                _ws.textAll(buffer);
            }

        } catch (std::bad_alloc& bad_alloc) {
            MessageOutput.printf("Calling /api/vedirectlivedata/status has temporarily run out of resources. Reason: \"%s\".\r\n", bad_alloc.what());
        }

        _lastWsPublish = millis();
    }
}

void WebApiWsVedirectLiveClass::generateJsonResponse(JsonVariant& root)
{
    auto const& mpptData = VictronMppt.getData();

    // device info
    root["device"]["data_age"] = (millis() - VictronMppt.getLastUpdate() ) / 1000;
    root["device"]["age_critical"] = !VictronMppt.isDataValid();
    root["device"]["PID"] = VictronMppt.getPidAsString();
    root["device"]["SER"] = mpptData.SER;
    root["device"]["FW"] = mpptData.FW;
    root["device"]["LOAD"] = mpptData.LOAD == true ? "ON" : "OFF";
    root["device"]["CS"] = VictronMppt.getCsAsString();
    root["device"]["ERR"] = VictronMppt.getErrAsString();
    root["device"]["OR"] = VictronMppt.getOrAsString();
    root["device"]["MPPT"] = VictronMppt.getMpptAsString();
    root["device"]["HSDS"]["v"] = mpptData.HSDS;
    root["device"]["HSDS"]["u"] = "d";

    // battery info    
    root["output"]["P"]["v"] = mpptData.P;
    root["output"]["P"]["u"] = "W";
    root["output"]["P"]["d"] = 0;
    root["output"]["V"]["v"] = mpptData.V;
    root["output"]["V"]["u"] = "V";
    root["output"]["V"]["d"] = 2;
    root["output"]["I"]["v"] = mpptData.I;
    root["output"]["I"]["u"] = "A";
    root["output"]["I"]["d"] = 2;
    root["output"]["E"]["v"] = mpptData.E;
    root["output"]["E"]["u"] = "%";
    root["output"]["E"]["d"] = 1;

    // panel info
    root["input"]["PPV"]["v"] = mpptData.PPV;
    root["input"]["PPV"]["u"] = "W";
    root["input"]["PPV"]["d"] = 0;
    root["input"]["VPV"]["v"] = mpptData.VPV;
    root["input"]["VPV"]["u"] = "V";
    root["input"]["VPV"]["d"] = 2;
    root["input"]["IPV"]["v"] = mpptData.IPV;
    root["input"]["IPV"]["u"] = "A";
    root["input"]["IPV"]["d"] = 2;
    root["input"]["YieldToday"]["v"] = mpptData.H20;
    root["input"]["YieldToday"]["u"] = "kWh";
    root["input"]["YieldToday"]["d"] = 3;
    root["input"]["YieldYesterday"]["v"] = mpptData.H22;
    root["input"]["YieldYesterday"]["u"] = "kWh";
    root["input"]["YieldYesterday"]["d"] = 3;
    root["input"]["YieldTotal"]["v"] = mpptData.H19;
    root["input"]["YieldTotal"]["u"] = "kWh";
    root["input"]["YieldTotal"]["d"] = 3;
    root["input"]["MaximumPowerToday"]["v"] = mpptData.H21;
    root["input"]["MaximumPowerToday"]["u"] = "W";
    root["input"]["MaximumPowerToday"]["d"] = 0;
    root["input"]["MaximumPowerYesterday"]["v"] = mpptData.H23;
    root["input"]["MaximumPowerYesterday"]["u"] = "W";
    root["input"]["MaximumPowerYesterday"]["d"] = 0;

    // power limiter state
    root["dpl"]["PLSTATE"] = -1;
    if (Configuration.get().PowerLimiter_Enabled)
        root["dpl"]["PLSTATE"] = PowerLimiter.getPowerLimiterState();
    root["dpl"]["PLLIMIT"] = PowerLimiter.getLastRequestedPowerLimit();

    // TODO(schlimmchen): this breaks when millis() wraps around, i.e.,
    // _newestVedirectTimestamp will eventually be close to max(uint32_t) forever.
    if (VictronMppt.getLastUpdate() > _newestVedirectTimestamp) {
        _newestVedirectTimestamp = VictronMppt.getLastUpdate();
    }
}

void WebApiWsVedirectLiveClass::onWebsocketEvent(AsyncWebSocket* server, AsyncWebSocketClient* client, AwsEventType type, void* arg, uint8_t* data, size_t len)
{
    if (type == WS_EVT_CONNECT) {
        char str[64];
        snprintf(str, sizeof(str), "Websocket: [%s][%u] connect", server->url(), client->id());
        Serial.println(str);
        MessageOutput.println(str);
    } else if (type == WS_EVT_DISCONNECT) {
        char str[64];
        snprintf(str, sizeof(str), "Websocket: [%s][%u] disconnect", server->url(), client->id());
        Serial.println(str);
        MessageOutput.println(str);
    }
}

void WebApiWsVedirectLiveClass::onLivedataStatus(AsyncWebServerRequest* request)
{
    if (!WebApi.checkCredentialsReadonly(request)) {
        return;
    }
    try {
        AsyncJsonResponse* response = new AsyncJsonResponse(false, _responseSize);
        JsonVariant root = response->getRoot();

        generateJsonResponse(root);

        response->setLength();
        request->send(response);

    } catch (std::bad_alloc& bad_alloc) {
        MessageOutput.printf("Calling /api/livedata/status has temporarily run out of resources. Reason: \"%s\".\r\n", bad_alloc.what());

        WebApi.sendTooManyRequests(request);
    }
}