// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include "Configuration.h"
#include <espMqttClient.h>
#include <Arduino.h>
#include <Hoymiles.h>
#include <memory>

typedef enum {
    STATE_PL_SHUTDOWN = 0, 
    STATE_ACTIVE
} plStates;

typedef enum {
    EMPTY_WHEN_FULL= 0, 
    EMPTY_AT_NIGHT
} batDrainStrategy;
   

class PowerLimiterClass {
public:
    void init();
    void loop();
    plStates getPowerLimiterState();
    int32_t getLastRequestedPowewrLimit();

private:
    uint32_t _lastCommandSent;
    uint32_t _lastLoop;
    int32_t _lastRequestedPowerLimit;
    plStates _plState = STATE_ACTIVE; 

    float _powerMeter1Power;
    float _powerMeter2Power;
    float _powerMeter3Power;

    bool canUseDirectSolarPower();
    int32_t calcPowerLimit(std::shared_ptr<InverterAbstract> inverter, bool consumeSolarPowerOnly);
    void setNewPowerLimit(std::shared_ptr<InverterAbstract> inverter, int32_t newPowerLimit);
    int32_t getDirectSolarPower();
    float getLoadCorrectedVoltage(std::shared_ptr<InverterAbstract> inverter);
    bool isStartThresholdReached(std::shared_ptr<InverterAbstract> inverter);
    bool isStopThresholdReached(std::shared_ptr<InverterAbstract> inverter);
};

extern PowerLimiterClass PowerLimiter;
