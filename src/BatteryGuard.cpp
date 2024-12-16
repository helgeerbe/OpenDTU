/* Battery-Guard
 *
 * The Battery-Guard has several functions.
 * - Calculate the battery internal resistance
 * - Calculate the battery open circuit voltage
 * - Limit the power drawn from the battery, if the battery voltage is close to the stop threshold. (draft)
 * - Periodically recharge the battery to 100% SoC (draft)
 *
 * Basic principe of the function: "Battery internal resistance"
 * Collects minimum and maximum values (voltage and current) over a time frame. Calculates the resistance from these values
 * and build a weighed average.
 *
 * Basic principe of the function: "Open circuit voltage"
 * Use the battery internal resistance to calculate the open circuit voltage and build a weighed average.
 *
 * Basic principe of the function: "Low voltage limiter"
 * If the battery voltage is close to the stop threshold, the battery limiter will calculate a maximum power limit
 * to keep the battery voltage above the voltage threshold.
 * The inverter is only switched-off when the threshold is exceeded and the inverter output cannot be reduced any further.
 *
 * Basic principe of the function: "Periodically recharge the battery"
 * After some days we start to reduce barriers, to make it more easier to fully charge the battery.
 * When we reach 100% SoC we remove all restrictions and start a new period.
 * Especially usefull during winter to calibrate the SoC calculation of the BMS
 *
 * Notes:
 * Some function are still under development.
 *
 * 01.08.2024 - 0.1 - first version. "Low voltage power limiter"
 * 09.12.2024 - 0.2 - add of function "Periodically recharge the battery"
 * 11.12.2024 - 0.3 - add of function "Battery internal resistance" and "Open circuit voltage"
 */

#include <frozen/map.h>
#include "Configuration.h"
#include "MessageOutput.h"
#include "BatteryGuard.h"


// support for debugging, 0 = without extended logging, 1 = with extended logging, 2 = with much more logging
constexpr int MODULE_DEBUG = 0;

BatteryGuardClass BatteryGuard;


/*
 * Initialize the battery guard
 */
void BatteryGuardClass::init(Scheduler& scheduler) {

    // init the task loop
    scheduler.addTask(_loopTask);
    _loopTask.setCallback(std::bind(&BatteryGuardClass::loop, this));
    _loopTask.setIterations(TASK_FOREVER);
    _loopTask.setInterval(60*1000);
    _loopTask.enable();

    updateSettings();
}


/*
 * Update some settings of the battery guard
 */
void BatteryGuardClass::updateSettings(void) {

    // todo: get values from the configuration
    _verboseLogging = true;
    _useBatteryGuard = true;

    // used for "Open circuit voltage"
    _resistorConfig = 0.012f;
}


/*
 * Periodical tasks, will be called once a minute
 */
void BatteryGuardClass::loop(void) {

    if (_useBatteryGuard && _verboseLogging) {
        MessageOutput.printf("%s\r\n", getText(Text::T_HEAD).data());
        MessageOutput.printf("%s ---------------- Battery-Guard information block (every minute) ----------------\r\n",
            getText(Text::T_HEAD).data());
        MessageOutput.printf("%s\r\n", getText(Text::T_HEAD).data());
    }

    // "Open circuit voltage"
    if (_useBatteryGuard && _verboseLogging) {
        printOpenCircuitVoltageInformationBlock();
    }

    // "Low voltage power limiter"


    // "Periodically recharge the battery"

}



/*
 * Calculate the battery open circuit voltage.
 * Returns the weighted average value or nullptr if calculation is not possible or if the value is out of date.
 */
std::optional<float> BatteryGuardClass::calculateOpenCircuitVoltage(float const nowVoltage, float const nowCurrent) {

    // calculate the open circuit battery voltage (current flow into the battery must be positive)
    auto oResistor = getInternalResistance();
    if ((nowVoltage > 0.0f) && (oResistor.has_value())) {
        _openCircuitVoltageAVG.addNumber(nowVoltage - nowCurrent * oResistor.value());
        _lastOCVMillis = millis();
    }
    return getOpenCircuitVoltage();
}


/*
 * Returns the battery internal resistance, calculated / configured or nullopt if neither value is valid
 */
std::optional<float> BatteryGuardClass::getInternalResistance(void) const {
    if (_internalResistanceAVG.getCounts() > 4) { return _internalResistanceAVG.getAverage(); }
    if (_resistorConfig != 0.0f) { return _resistorConfig; }
    return std::nullopt;
}


/*
 * Returns the battery open circuit voltage or nullopt if value is not valid
 */
std::optional<float> BatteryGuardClass::getOpenCircuitVoltage(void) const {
  if ((_openCircuitVoltageAVG.getCounts() > 0) && (millis() - _lastOCVMillis) < 30*1000) {
        return _openCircuitVoltageAVG.getAverage();
    } else {
        return std::nullopt;
    }
}


/*
 * Calculate the battery internal resistance between the battery cells and the voltage measurement device. (BMS, MPPT, Inverter)
 * Returns the resistance, calculated / configured or nullopt if neither value is valid
 */
std::optional<float> BatteryGuardClass::calculateInternalResistance(float const nowVoltage, float const nowCurrent) {

    if (nowVoltage <= 0.0f) { return getInternalResistance(); }

    // we must avoid to use measurement values during any power transition.
    // To solve this problem, we check whether two consecutive measurements are almost identical (5mV, 200mA)
    if (!_firstOfTwoAvailable || (std::abs(_firstVolt.first - nowVoltage) > 0.005f) ||
        (std::abs(_firstVolt.second - nowCurrent) > 0.2f)) {
        _firstVolt.first = nowVoltage;
        _firstVolt.second = nowCurrent;
        _firstOfTwoAvailable = true;
        return getInternalResistance();
    }
    _firstOfTwoAvailable = false;  // prepair for the next calculation

    // store the average in min or max buffer
    std::pair<float,float> avgVolt = std::make_pair((nowVoltage + _firstVolt.first) / 2.0f, (nowCurrent + _firstVolt.second) / 2.0f);
    if (!_minMaxAvailable) {
        _minVolt = _maxVolt = avgVolt;
        _lastMinMaxMillis = millis();
        _minMaxAvailable = true;
    } else {
        if (avgVolt.first < _minVolt.first) { _minVolt = avgVolt; }
        if (avgVolt.first > _maxVolt.first) { _maxVolt = avgVolt; }
    }

    // we evaluate min and max values in a time duration of 30 sec
    if ((!_minMaxAvailable || (millis() - _lastMinMaxMillis) < 30*1000)) { return getInternalResistance(); }
    _minMaxAvailable = false;  // prepair for the next calculation

    // we need a minimum voltage difference to get a sufficiently good result (failure < 10%)
    // SmartShunt: 50mV (about 100W on VDC: 24V, Ri: 12mOhm)
    if ((_maxVolt.first - _minVolt.first) >= _minDiffVoltage) {
        float resistor = std::abs((_maxVolt.first - _minVolt.first) / (_maxVolt.second - _minVolt.second));

        // we try to keep out bad values from the average
        if (_internalResistanceAVG.getCounts() < 10) {
            _internalResistanceAVG.addNumber(resistor);
        } else {
            if ((resistor > _internalResistanceAVG.getAverage() / 2.0f) && (resistor < _internalResistanceAVG.getAverage() * 2.0f)) {
                _internalResistanceAVG.addNumber(resistor);
            }
        }

        // todo: delete after testing
        if constexpr(MODULE_DEBUG >= 1) {
            MessageOutput.printf("%s Resistor - Calculated: %0.3fOhm\r\n", getText(Text::T_HEAD).data(), resistor);
        }
    }
    return getInternalResistance();
}


/*
 * prints the "Battery open circuit voltage" information block
 */
void BatteryGuardClass::printOpenCircuitVoltageInformationBlock(void)
{
    MessageOutput.printf("%s 1) Function: Battery open circuit voltage\r\n",
        getText(Text::T_HEAD).data());

    MessageOutput.printf("%s Open circuit voltage: %0.3fV\r\n",
        getText(Text::T_HEAD).data(), _openCircuitVoltageAVG.getAverage());

    MessageOutput.printf("%s  Internal resistance: %0.4fOhm (Min: %0.4f, Max: %0.4f, Last: %0.4f, Amount: %i)\r\n",
        getText(Text::T_HEAD).data(), _internalResistanceAVG.getAverage(), _internalResistanceAVG.getMin(),
        _internalResistanceAVG.getMax(), _internalResistanceAVG.getLast(), _internalResistanceAVG.getCounts() - 1);
}


/*
 * Returns a string according to current text nr
 */
frozen::string const& BatteryGuardClass::getText(BatteryGuardClass::Text tNr)
{
    static const frozen::string missing = "programmer error: missing status text";

    static const frozen::map<Text, frozen::string, 5> texts = {
        { Text::Q_NODATA, "Insufficient data" },
        { Text::Q_EXCELLENT, "Excellent" },
        { Text::Q_GOOD, "Good" },
        { Text::Q_BAD, "Bad" },
        { Text::T_HEAD, "[Battery-Guard]"}
    };

    auto iter = texts.find(tNr);
    if (iter == texts.end()) { return missing; }

    return iter->second;
}
