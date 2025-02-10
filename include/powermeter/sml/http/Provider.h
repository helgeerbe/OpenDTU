// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <atomic>
#include <memory>
#include <condition_variable>
#include <mutex>
#include <stdint.h>
#include <Arduino.h>
#include <HttpGetter.h>
#include <Configuration.h>
#include <powermeter/sml/Provider.h>

namespace PowerMeters::Sml::Http {

class Provider : public ::PowerMeters::Sml::Provider {
public:
    explicit Provider(PowerMeterHttpSmlConfig const& cfg)
        : ::PowerMeters::Sml::Provider("PowerMeterHttpSml")
        , _cfg(cfg) { }

    ~Provider();

    bool init() final;
    void loop() final;
    bool isDataValid() const final;

    // returns an empty string on success,
    // returns an error message otherwise.
    String poll();

private:
    static void pollingLoopHelper(void* context);
    std::atomic<bool> _taskDone;
    void pollingLoop();

    PowerMeterHttpSmlConfig const _cfg;

    uint32_t _lastPoll = 0;

    std::unique_ptr<HttpGetter> _upHttpGetter;

    TaskHandle_t _taskHandle = nullptr;
    bool _stopPolling;
    mutable std::mutex _pollingMutex;
    std::condition_variable _cv;
};

} // namespace PowerMeters::Sml::Http
