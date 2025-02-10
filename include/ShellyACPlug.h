// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <memory>
#include <condition_variable>
#include "HttpGetter.h"
#include "Configuration.h"
#include <TaskSchedulerDeclarations.h>
#include <stdint.h>
#include <atomic>
#include <array>
#include <variant>
#include <mutex>

class ShellyACPlugClass {
    public:
        bool init(Scheduler& scheduler);
        void loop();
        void PowerON();
        void PowerOFF();
        float _readpower;
    private:
        bool _initialized = false;
        Task _loopTask;
        const uint16_t _period = 2001;
        float _acPower;
        float _SoC;
        bool _emergcharge;
        bool send_http(String uri);
        float read_http(String uri);
        std::unique_ptr<HttpGetter> _HttpGetter;
        bool powerstate = false;
        bool verboselogging;
        String uri_on;
        String uri_off;
};

extern ShellyACPlugClass ShellyACPlug;
