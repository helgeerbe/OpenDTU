// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2022 Thomas Basler and others
 */

#include "Battery.h"
#include "PowerMeter.h"
#include "PowerLimiter.h"
#include "Configuration.h"
#include "MqttSettings.h"
#include "NetworkSettings.h"
#include <VeDirectFrameHandler.h>
#include "MessageOutput.h"
#include <ctime>

PowerLimiterClass PowerLimiter;

void PowerLimiterClass::init()
{
    _lastCommandSent = 0;
    _lastLoop = 0;
    _lastRequestedPowerLimit = 0;
}

void PowerLimiterClass::loop()
{
    CONFIG_T& config = Configuration.get();
 
    // Run inital checks to make sure we have met the basic conditions
    if ( !config.PowerMeter_Enabled
            || !Hoymiles.getRadio()->isIdle()
            || (millis() - _lastCommandSent) < (config.PowerLimiter_Interval * 1000)
            || (millis() - _lastLoop) < (config.PowerLimiter_Interval * 1000)) {
        return;
    }

    _lastLoop = millis();

    // Debug state transistions, TODO: Remove
    MessageOutput.printf("****************** PL STATE: %i\r\n", _plState);

    std::shared_ptr<InverterAbstract> inverter = Hoymiles.getInverterByPos(config.PowerLimiter_InverterId);
    if (inverter == nullptr || !inverter->isReachable()) {
        return;
    }

    // Make sure inverter is turned off if PL is disabled by the user
    if (!config.PowerLimiter_Enabled && _plState != SHUTDOWN) {
        if (inverter->isProducing()) {
            MessageOutput.printf("PL initiated inverter shutdown.\r\n");
            inverter->sendPowerControlRequest(Hoymiles.getRadio(), false);
        } else {
            _plState = SHUTDOWN;
        }
        return;
    }

    // If power limiter is disabled
    if (!config.PowerLimiter_Enabled) {
      return;
    }

    float dcVoltage = inverter->Statistics()->getChannelFieldValue(TYPE_DC, (ChannelNum_t) config.PowerLimiter_InverterChannelId, FLD_UDC);
    //float acPower = inverter->Statistics()->getChannelFieldValue(TYPE_AC, (ChannelNum_t) config.PowerLimiter_InverterChannelId, FLD_PAC); 
    //float correctedDcVoltage = dcVoltage + (acPower * config.PowerLimiter_VoltageLoadCorrectionFactor);

    if ((millis() - inverter->Statistics()->getLastUpdate()) > 10000) {
        return;
    }

    if (millis() - PowerMeter.getLastPowerMeterUpdate() < (30 * 1000)) {
        MessageOutput.printf("[PowerLimiterClass::loop] dcVoltage: %.2f Voltage Start Threshold: %.2f Voltage Stop Threshold: %.2f inverter->isProducing(): %d\r\n",
            dcVoltage, config.PowerLimiter_VoltageStartThreshold, config.PowerLimiter_VoltageStopThreshold, inverter->isProducing());
    }

  	// If we're in shutdown move to active operation
    if (_plState == SHUTDOWN) {
      _plState = ACTIVE;
    }

    if (isStopThresholdReached(inverter)) {
      // Disable battery discharge when empty
      _batteryDischargeEnabled = false;
    } else if (!canUseDirectSolarPower() || 
                config.PowerLimiter_BatteryDrainStategy == EMPTY_AT_NIGHT) {
      // Enable battery discharge
      _batteryDischargeEnabled = true;
    }

    // This checks if the battery discharge start conditions are met for the EMPTY_WHEN_FULL case
    if (isStartThresholdReached(inverter) && config.PowerLimiter_BatteryDrainStategy == EMPTY_WHEN_FULL) {
      _batteryDischargeEnabled = true;
    }

    // We'll slowly ramp the percentage of MPPT power directed to the inverter up / down if the MPPT has reachted absorbtion phase
    if (VeDirect.veFrame.CS == 4) {
      // Absorbtion Phase
      _mpptDirectFeedToGridPercent += 0.01;
      if (_mpptDirectFeedToGridPercent > 0.9) {
        _mpptDirectFeedToGridPercent = 0.9;
      }
    } else {
      _mpptDirectFeedToGridPercent -= 0.01;
      if (_mpptDirectFeedToGridPercent < 0.0) {
        _mpptDirectFeedToGridPercent = 0.0;
      }
    }

    int32_t mpptDirectFeedToGridPower = _mpptDirectFeedToGridPercent * VeDirect.veFrame.PPV;

    //if (mpptDirectFeedToGridPower < config.PowerLimiter_LowerPowerLimit && VeDirect.veFrame.PPV > config.PowerLimiter_LowerPowerLimit) {
    //  mpptDirectFeedToGridPower = config.PowerLimiter_LowerPowerLimit;
    //} 
    int32_t newPowerLimit = calcPowerLimit(inverter, !_batteryDischargeEnabled);

    // Debug, TODO: Remove
    MessageOutput.printf("****************************** Powerlimit: %i, Mpptpower: %i, BatteryDischargeFlag: %i\r\n", newPowerLimit, mpptDirectFeedToGridPower, _batteryDischargeEnabled);
    if (newPowerLimit > mpptDirectFeedToGridPower) {
      setNewPowerLimit(inverter, newPowerLimit);
    } else {
      setNewPowerLimit(inverter, mpptDirectFeedToGridPower);
    }
}

plStates PowerLimiterClass::getPowerLimiterState() {
    return _plState;
}

int32_t PowerLimiterClass::getLastRequestedPowewrLimit() {
    return _lastRequestedPowerLimit;
}

bool PowerLimiterClass::canUseDirectSolarPower()
{
    CONFIG_T& config = Configuration.get();

    if (!config.PowerLimiter_SolarPassThroughEnabled
            || !config.Vedirect_Enabled) {
        return false;
    }

    if (VeDirect.veFrame.PPV < 20) {
        // Not enough power
        return false;
    }

    return true;
}




int32_t PowerLimiterClass::calcPowerLimit(std::shared_ptr<InverterAbstract> inverter, bool consumeSolarPowerOnly)
{
    CONFIG_T& config = Configuration.get();
    
    int32_t newPowerLimit = round(PowerMeter.getPowerTotal());

    // Safety check, return on too old power meter values
    if ((millis() - PowerMeter.getLastPowerMeterUpdate()) > (30 * 1000)) {
        // If the power meter values are older than 30 seconds,
        // set the limit to config.PowerLimiter_LowerPowerLimit for safety reasons.
        MessageOutput.println("[PowerLimiterClass::loop] Power Meter values too old. Using lower limit");
        return config.PowerLimiter_LowerPowerLimit;
    }

    // check if grid power consumption is within the limits of the target consumption + hysteresis
    if (newPowerLimit >= (config.PowerLimiter_TargetPowerConsumption - config.PowerLimiter_TargetPowerConsumptionHysteresis) &&
        newPowerLimit <= (config.PowerLimiter_TargetPowerConsumption + config.PowerLimiter_TargetPowerConsumptionHysteresis)) {
          // The values have not changed much. We just use the old setting
          MessageOutput.println("[PowerLimiterClass::loop] reusing old limit");
          return _lastRequestedPowerLimit;
    }

    if (config.PowerLimiter_IsInverterBehindPowerMeter) {
        // If the inverter the behind the power meter (part of measurement),
        // the produced power of this inverter has also to be taken into account.
        // We don't use FLD_PAC from the statistics, because that
        // data might be too old and unrelieable.
        newPowerLimit += _lastRequestedPowerLimit;
    }

    float efficency = inverter->Statistics()->getChannelFieldValue(TYPE_AC, (ChannelNum_t) config.PowerLimiter_InverterChannelId, FLD_EFF);
    int32_t victronChargePower = this->getDirectSolarPower();
    int32_t adjustedVictronChargePower = victronChargePower * (efficency > 0.0 ? (efficency / 100.0) : 1.0); // if inverter is off, use 1.0

    MessageOutput.printf("[PowerLimiterClass::loop] victronChargePower: %d, efficiency: %.2f, consumeSolarPowerOnly: %s, powerConsumption: %d \r\n", 
        victronChargePower, efficency, consumeSolarPowerOnly ? "true" : "false", newPowerLimit);

    // We're not trying to hit 0 exactly but take an offset into account
    // This means we never fully compensate the used power with the inverter 
    newPowerLimit -= config.PowerLimiter_TargetPowerConsumption;

    int32_t upperPowerLimit = config.PowerLimiter_UpperPowerLimit;
    if (consumeSolarPowerOnly && (upperPowerLimit > adjustedVictronChargePower)) {
        // Battery voltage too low, use Victron solar power (corrected by efficency factor) only
        upperPowerLimit = adjustedVictronChargePower;
    }

    if (newPowerLimit > upperPowerLimit) 
        newPowerLimit = upperPowerLimit;

    MessageOutput.printf("[PowerLimiterClass::loop] newPowerLimit: %d\r\n", newPowerLimit);
    return newPowerLimit;
}

void PowerLimiterClass::setNewPowerLimit(std::shared_ptr<InverterAbstract> inverter, int32_t newPowerLimit)
{
    CONFIG_T& config = Configuration.get();

    // Start the inverter in case it's inactive and if the requested power is high enough
    if (!inverter->isProducing() && newPowerLimit > config.PowerLimiter_LowerPowerLimit) {
        MessageOutput.println("[PowerLimiterClass::loop] Starting up inverter...");
        inverter->sendPowerControlRequest(Hoymiles.getRadio(), true);
        _lastCommandSent = millis();
    }

    // Stop the inverter if limit is below threshold.
    // We'll also set the power limit to the lower value in this case
    if (newPowerLimit < config.PowerLimiter_LowerPowerLimit) {
        if (inverter->isProducing()) {
            MessageOutput.println("[PowerLimiterClass::loop] Stopping inverter...");
            inverter->sendPowerControlRequest(Hoymiles.getRadio(), false);
            _lastCommandSent = millis();
        }
        newPowerLimit = config.PowerLimiter_LowerPowerLimit;
    }

    // Set the actual limit. We'll only do this is if the limit is in the right range
    if( _lastRequestedPowerLimit != newPowerLimit &&
          newPowerLimit > config.PowerLimiter_LowerPowerLimit &&   /* Will always be true, kept for code readability */
          newPowerLimit < config.PowerLimiter_UpperPowerLimit ) {
        MessageOutput.printf("[PowerLimiterClass::loop] Limit Non-Persistent: %d W\r\n", newPowerLimit);
        inverter->sendActivePowerControlRequest(Hoymiles.getRadio(), newPowerLimit, PowerLimitControlType::AbsolutNonPersistent);
        _lastRequestedPowerLimit = newPowerLimit;
    }
}

int32_t PowerLimiterClass::getDirectSolarPower()
{
    if (!canUseDirectSolarPower()) {
        return 0;
    }

    return VeDirect.veFrame.PPV;
}

float PowerLimiterClass::getLoadCorrectedVoltage(std::shared_ptr<InverterAbstract> inverter)
{
    CONFIG_T& config = Configuration.get();

    float acPower = inverter->Statistics()->getChannelFieldValue(TYPE_AC, (ChannelNum_t) config.PowerLimiter_InverterChannelId, FLD_PAC); 
    float dcVoltage = inverter->Statistics()->getChannelFieldValue(TYPE_DC, (ChannelNum_t) config.PowerLimiter_InverterChannelId, FLD_UDC); 

    if (dcVoltage <= 0.0) {
        return 0.0;
    }

    return dcVoltage + (acPower * config.PowerLimiter_VoltageLoadCorrectionFactor);
}

bool PowerLimiterClass::isStartThresholdReached(std::shared_ptr<InverterAbstract> inverter)
{
    CONFIG_T& config = Configuration.get();

    // Check if the Battery interface is enabled and the SOC start threshold is reached
    if (config.Battery_Enabled
            && config.PowerLimiter_BatterySocStartThreshold > 0.0
            && (millis() - Battery.stateOfChargeLastUpdate) < 60000
            && Battery.stateOfCharge >= config.PowerLimiter_BatterySocStartThreshold) {
        return true;
    }

    // Otherwise we use the voltage threshold
    if (config.PowerLimiter_VoltageStartThreshold <= 0.0) {
        return false;
    }

    float correctedDcVoltage = getLoadCorrectedVoltage(inverter);
    return correctedDcVoltage >= config.PowerLimiter_VoltageStartThreshold;
}

bool PowerLimiterClass::isStopThresholdReached(std::shared_ptr<InverterAbstract> inverter)
{
    CONFIG_T& config = Configuration.get();

    // Check if the Battery interface is enabled and the SOC stop threshold is reached
    if (config.Battery_Enabled
            && config.PowerLimiter_BatterySocStopThreshold > 0.0
            && (millis() - Battery.stateOfChargeLastUpdate) < 60000
            && Battery.stateOfCharge <= config.PowerLimiter_BatterySocStopThreshold) {
        return true;
    }

    // Otherwise we use the voltage threshold
    if (config.PowerLimiter_VoltageStopThreshold <= 0.0) {
        return false;
    }

    float correctedDcVoltage = getLoadCorrectedVoltage(inverter);
    return correctedDcVoltage <= config.PowerLimiter_VoltageStopThreshold;
}
