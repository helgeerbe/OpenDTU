// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2023 Malte Schmidt and others
 */
#include "Battery.h"
#include <gridcharger/huawei/Controller.h>
#include <gridcharger/huawei/MCP2515.h>
#include <gridcharger/huawei/TWAI.h>
#include "MessageOutput.h"
#include <powermeter/Controller.h>
#include "PowerLimiter.h"
#include "Configuration.h"

#include <functional>
#include <algorithm>

GridCharger::Huawei::Controller HuaweiCan;

namespace GridCharger::Huawei {

// Wait time/current before shuting down the PSU / charger
// This is set to allow the fan to run for some time
#define HUAWEI_AUTO_MODE_SHUTDOWN_DELAY 60000
#define HUAWEI_AUTO_MODE_SHUTDOWN_CURRENT 0.75

void Controller::init(Scheduler& scheduler)
{
    MessageOutput.print("Initialize Huawei AC charger interface...\r\n");

    scheduler.addTask(_loopTask);
    _loopTask.setCallback(std::bind(&Controller::loop, this));
    _loopTask.setIterations(TASK_FOREVER);
    _loopTask.enable();

    updateSettings();
}

void Controller::enableOutput()
{
    if (_huaweiPower < 0) { return; }
    digitalWrite(_huaweiPower, 0);
}

void Controller::disableOutput()
{
    if (_huaweiPower < 0) { return; }
    digitalWrite(_huaweiPower, 1);
}

void Controller::updateSettings()
{
    std::lock_guard<std::mutex> lock(_mutex);

    _upHardwareInterface.reset(nullptr);

    auto const& config = Configuration.get();

    if (!config.Huawei.Enabled) { return; }

    switch (config.Huawei.HardwareInterface) {
        case GridChargerHardwareInterface::MCP2515:
            _upHardwareInterface = std::make_unique<MCP2515>();
            break;
        case GridChargerHardwareInterface::TWAI:
            _upHardwareInterface = std::make_unique<TWAI>();
            break;
        default:
            MessageOutput.printf("[Huawei::Controller] Unknown hardware "
                    "interface setting %d\r\n", config.Huawei.HardwareInterface);
            return;
            break;
    }

    if (!_upHardwareInterface->init()) {
        MessageOutput.print("[Huawei::Controller] Error initializing hardware interface\r\n");
        _upHardwareInterface.reset(nullptr);
        return;
    };

    auto const& pin = PinMapping.get();
    if (pin.huawei_power >= 0) {
        _huaweiPower = pin.huawei_power;
        pinMode(_huaweiPower, OUTPUT);
        disableOutput();
    }

    if (config.Huawei.Auto_Power_Enabled) {
        _mode = HUAWEI_MODE_AUTO_INT;
    }

    MessageOutput.print("[Huawei::Controller] Hardware Interface initialized successfully\r\n");
}

void Controller::loop()
{
    std::lock_guard<std::mutex> lock(_mutex);

    if (!_upHardwareInterface) { return; }

    auto const& config = Configuration.get();

    bool verboseLogging = config.Huawei.VerboseLogging;

    auto upNewData = _upHardwareInterface->getCurrentData();
    if (upNewData) {
        _dataPoints.updateFrom(*upNewData);
    }

    auto oOutputCurrent = _dataPoints.get<DataPointLabel::OutputCurrent>();
    auto oOutputVoltage = _dataPoints.get<DataPointLabel::OutputVoltage>();
    auto oOutputPower = _dataPoints.get<DataPointLabel::OutputPower>();
    auto oEfficiency = _dataPoints.get<DataPointLabel::Efficiency>();
    auto efficiency = oEfficiency ? (*oEfficiency > 0.5 ? *oEfficiency : 1.0) : 1.0;

    // Internal PSU power pin (slot detect) control
    if (oOutputCurrent && *oOutputCurrent > HUAWEI_AUTO_MODE_SHUTDOWN_CURRENT) {
        _outputCurrentOnSinceMillis = millis();
    }

    if (_outputCurrentOnSinceMillis + HUAWEI_AUTO_MODE_SHUTDOWN_DELAY < millis() &&
            (_mode == HUAWEI_MODE_AUTO_EXT || _mode == HUAWEI_MODE_AUTO_INT)) {
        disableOutput();
    }

    using Setting = HardwareInterface::Setting;

    if (_mode == HUAWEI_MODE_AUTO_INT || _batteryEmergencyCharging) {

        // Set voltage limit in periodic intervals if we're in auto mode or if emergency battery charge is requested.
        if ( _nextAutoModePeriodicIntMillis < millis()) {
            MessageOutput.printf("[Huawei::Controller] Periodically setting "
                "voltage limit: %f \r\n", config.Huawei.Auto_Power_Voltage_Limit);
            _setParameter(config.Huawei.Auto_Power_Voltage_Limit, Setting::OnlineVoltage);
            _nextAutoModePeriodicIntMillis = millis() + 60000;
        }
    }

    // ***********************
    // Emergency charge
    // ***********************
    auto stats = Battery.getStats();
    if (!_batteryEmergencyCharging && config.Huawei.Emergency_Charge_Enabled && stats->getImmediateChargingRequest()) {
        if (!oOutputVoltage) {
            // TODO(schlimmchen): if this situation actually occurs, this message
            // will be printed with high frequency for a prolonged time. how can
            // we deal with that?
            MessageOutput.print("[Huawei::Controller] Cannot perform emergency "
                    "charging with unknown PSU output voltage value\r\n");
            return;
        }

        _batteryEmergencyCharging = true;

        // Set output current
        float outputCurrent = efficiency * (config.Huawei.Auto_Power_Upper_Power_Limit / *oOutputVoltage);
        MessageOutput.printf("[Huawei::Controller] Emergency Charge Output "
            "current %.02f \r\n", outputCurrent);
        _setParameter(outputCurrent, Setting::OnlineCurrent);
        return;
    }

    if (_batteryEmergencyCharging && !stats->getImmediateChargingRequest()) {
        // Battery request has changed. Set current to 0, wait for PSU to respond and then clear state
        // TODO(schlimmchen): this is repeated very often for up to (polling interval) seconds. maybe
        // trigger sending request for data immediately? otherwise implement a backoff instead.
        _setParameter(0, Setting::OnlineCurrent);
        if (oOutputCurrent && *oOutputCurrent < 1) {
            _batteryEmergencyCharging = false;
        }
        return;
    }

    // ***********************
    // Automatic power control
    // ***********************
    if (_mode == HUAWEI_MODE_AUTO_INT ) {
        // Check if we should run automatic power calculation at all.
        // We may have set a value recently and still wait for output stabilization
        if (_autoModeBlockedTillMillis > millis()) {
            return;
        }

        if (!oOutputVoltage || !oOutputPower || !oOutputCurrent) {
            MessageOutput.print("[Huawei::Controller] Cannot perform auto power "
                    "control while critical PSU values are still unknown\r\n");
            _autoModeBlockedTillMillis = millis() + 1000;
            return;
        }

        // Re-enable automatic power control if the output voltage has dropped below threshold
        if (oOutputVoltage && *oOutputVoltage < config.Huawei.Auto_Power_Enable_Voltage_Limit ) {
            _autoPowerEnabledCounter = 10;
        }

        if (PowerLimiter.isGovernedBatteryPoweredInverterProducing()) {
            _setParameter(0.0, Setting::OnlineCurrent);
            // Don't run auto mode for a second now. Otherwise we may send too much over the CAN bus
            _autoModeBlockedTillMillis = millis() + 1000;
            MessageOutput.printf("[Huawei::Controller] Inverter is active, disable PSU\r\n");
            return;
        }

        if (PowerMeter.getLastUpdate() > _lastPowerMeterUpdateReceivedMillis &&
                _autoPowerEnabledCounter > 0) {
            // We have received a new PowerMeter value. Also we're _autoPowerEnabled
            // So we're good to calculate a new limit

            _lastPowerMeterUpdateReceivedMillis = PowerMeter.getLastUpdate();

            // Calculate new power limit
            float newPowerLimit = -1 * round(PowerMeter.getPowerTotal());

            // Powerlimit is the requested output power + permissable Grid consumption factoring in the efficiency factor
            newPowerLimit += *oOutputPower + config.Huawei.Auto_Power_Target_Power_Consumption / efficiency;

            if (verboseLogging) {
                MessageOutput.printf("[Huawei::Controller] newPowerLimit: %.0f, "
                    "output_power: %.01f\r\n", newPowerLimit, *oOutputPower);
            }

            // Check whether the battery SoC limit setting is enabled
            if (config.Battery.Enabled && config.Huawei.Auto_Power_BatterySoC_Limits_Enabled) {
                uint8_t _batterySoC = Battery.getStats()->getSoC();
                // Sets power limit to 0 if the BMS reported SoC reaches or exceeds the user configured value
                if (_batterySoC >= config.Huawei.Auto_Power_Stop_BatterySoC_Threshold) {
                    newPowerLimit = 0;
                    if (verboseLogging) {
                        MessageOutput.printf("[Huawei::Controller] Current battery SoC %i reached "
                                "stop threshold %i, set newPowerLimit to %f \r\n", _batterySoC,
                                config.Huawei.Auto_Power_Stop_BatterySoC_Threshold, newPowerLimit);
                    }
                }
            }

            if (newPowerLimit > config.Huawei.Auto_Power_Lower_Power_Limit) {

                // Check if the output power has dropped below the lower limit (i.e. the battery is full)
                // and if the PSU should be turned off. Also we use a simple counter mechanism here to be able
                // to ramp up from zero output power when starting up
                if (*oOutputPower < config.Huawei.Auto_Power_Lower_Power_Limit) {
                    MessageOutput.print("[Huawei::Controller] Power and "
                        "voltage limit reached. Disabling automatic power "
                        "control.\r\n");
                    _autoPowerEnabledCounter--;
                    if (_autoPowerEnabledCounter == 0) {
                        _autoPowerEnabled = false;
                        _setParameter(0.0, Setting::OnlineCurrent);
                        return;
                    }
                } else {
                    _autoPowerEnabledCounter = 10;
                }

                // Limit power to maximum
                if (newPowerLimit > config.Huawei.Auto_Power_Upper_Power_Limit) {
                    newPowerLimit = config.Huawei.Auto_Power_Upper_Power_Limit;
                }

                // Calculate output current
                float calculatedCurrent = efficiency * (newPowerLimit / *oOutputVoltage);

                // Limit output current to value requested by BMS
                float permissableCurrent = stats->getChargeCurrentLimitation() - (stats->getChargeCurrent() - *oOutputCurrent); // BMS current limit - current from other sources, e.g. Victron MPPT charger
                float outputCurrent = std::min(calculatedCurrent, permissableCurrent);
                outputCurrent= outputCurrent > 0 ? outputCurrent : 0;

                if (verboseLogging) {
                    MessageOutput.printf("[Huawei::Controller] Setting output "
                        "current to %.2fA. This is the lower value of "
                        "calculated %.2fA and BMS permissable %.2fA "
                        "currents\r\n", outputCurrent, calculatedCurrent,
                        permissableCurrent);
                }
                _autoPowerEnabled = true;
                _setParameter(outputCurrent, Setting::OnlineCurrent);

                // Don't run auto mode some time to allow for output stabilization after issuing a new value
                _autoModeBlockedTillMillis = millis() + 2 * HardwareInterface::DataRequestIntervalMillis;
            } else {
                // requested PL is below minium. Set current to 0
                _autoPowerEnabled = false;
                _setParameter(0.0, Setting::OnlineCurrent);
            }
        }
    }
}

void Controller::setParameter(float val, HardwareInterface::Setting setting)
{
    std::lock_guard<std::mutex> lock(_mutex);

    if (_mode == HUAWEI_MODE_AUTO_INT &&
        setting != HardwareInterface::Setting::OfflineVoltage &&
        setting != HardwareInterface::Setting::OfflineCurrent) { return; }

    _setParameter(val, setting);
}

void Controller::_setParameter(float val, HardwareInterface::Setting setting)
{
    // NOTE: the mutex is locked by any method calling this private method

    if (!_upHardwareInterface) { return; }

    if (val < 0) {
        MessageOutput.printf("[Huawei::Controller] Error: Tried to set "
                "voltage/current to negative value %.2f\r\n", val);
        return;
    }

    using Setting = HardwareInterface::Setting;

    // Start PSU if needed
    if (val > HUAWEI_AUTO_MODE_SHUTDOWN_CURRENT &&
            setting == Setting::OnlineCurrent &&
            (_mode == HUAWEI_MODE_AUTO_EXT || _mode == HUAWEI_MODE_AUTO_INT)) {
        enableOutput();
        _outputCurrentOnSinceMillis = millis();
    }

    _upHardwareInterface->setParameter(setting, val);
}

void Controller::setMode(uint8_t mode) {
    std::lock_guard<std::mutex> lock(_mutex);

    if (!_upHardwareInterface) { return; }

    if (mode == HUAWEI_MODE_OFF) {
        disableOutput();
        _mode = HUAWEI_MODE_OFF;
    }
    if (mode == HUAWEI_MODE_ON) {
        enableOutput();
        _mode = HUAWEI_MODE_ON;
    }

    auto const& config = Configuration.get();

    if (mode == HUAWEI_MODE_AUTO_INT && !config.Huawei.Auto_Power_Enabled ) {
        MessageOutput.println("[Huawei::Controller] WARNING: Trying to set "
            "mode to internal automatic power control without being enabled "
            "in the UI. Ignoring command.");
        return;
    }

    if (_mode == HUAWEI_MODE_AUTO_INT && mode != HUAWEI_MODE_AUTO_INT) {
        _autoPowerEnabled = false;
        _setParameter(0, HardwareInterface::Setting::OnlineCurrent);
    }

    if (mode == HUAWEI_MODE_AUTO_EXT || mode == HUAWEI_MODE_AUTO_INT) {
        _mode = mode;
    }
}

void Controller::getJsonData(JsonVariant& root) const
{
    root["data_age"] = (millis() - _dataPoints.getLastUpdate()) / 1000;

    using Label = GridCharger::Huawei::DataPointLabel;
#define VAL(l, n) \
    { \
        auto oDataPoint = _dataPoints.getDataPointFor<Label::l>(); \
        if (oDataPoint) { \
            root[n]["v"] = *_dataPoints.get<Label::l>(); \
            root[n]["u"] = oDataPoint->getUnitText(); \
        } \
    }

    VAL(InputVoltage, "input_voltage");
    VAL(InputCurrent, "input_current");
    VAL(InputPower, "input_power");
    VAL(OutputVoltage, "output_voltage");
    VAL(OutputCurrent, "output_current");
    VAL(OutputCurrentMax, "max_output_current");
    VAL(OutputPower, "output_power");
    VAL(InputTemperature, "input_temp");
    VAL(OutputTemperature, "output_temp");
#undef VAL

    // special handling for efficiency, as we need to multiply it
    // to get the percentage (rather than the decimal notation).
    auto oEfficiency = _dataPoints.getDataPointFor<Label::Efficiency>();
    if (oEfficiency) {
        root["efficiency"]["v"] = *_dataPoints.get<Label::Efficiency>() * 100;
        root["efficiency"]["u"] = oEfficiency->getUnitText();
    }
}

} // namespace GridCharger::Huawei
