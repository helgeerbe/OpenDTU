/* Battery-Guard
 *
 * The Battery-Guard has several features.
 * - Calculate the battery open circuit voltage
 * - Calculate the battery internal resistance
 * - Limit the power drawn from the battery, if the battery voltage is close to the stop threshold. (draft)
 * - Periodically recharge the battery to 100% SoC (draft)
 *
 * Basic principe of the feature: "Battery open circuit voltage"
 * As soon as we kow the battery internal resistance we calculate the open circuit voltage.
 * open circuit voltage = battery voltage - battery current * resistance.
 *
 * Basic principe of the feature: "Battery internal resistance"
 * Collects minimum and maximum values (voltage and current) over a time frame of 30sec.
 * Calculates the resistance from these values and build a weighed average.
 * Note: We need load changes to get sufficient calculation results. About 100W on 24VDC or 180W on 48VDC.
 * The resistance on LifePO4 batteries is not a fixed value, he depends from temperature, charge and time
 * after a load change.
 *
 * Basic principe of the function: "Low voltage limiter"
 * If the battery voltage is close to the stop threshold, the battery limiter will calculate a maximum power limit
 * to keep the battery voltage above the voltage threshold.
 * The inverter is only switched-off when the threshold is exceeded and the inverter output cannot be reduced any further.
 *
 * Basic principe of the function: "Periodically recharge the battery"
 * After some days we start to reduce barriers, to make it more easier for the sun to fully charge the battery.
 * When we reach 100% SoC we remove all restrictions and start a new period.
 * Especially usefull during winter to support the SoC calibration of the BMS
 *
 * Notes:
 * Some function are still under development.
 * These functions were developed for the battery provider "Smart Shunt", but should also work with other providers
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
    _resistanceFromConfig = 0.0f;   // if 0 we must wait until the resistance is calculated
    _battPeriod.addNumber(1000);    // start with 1 second
}


/*
 * Periodical tasks, will be called once a minute
 */
void BatteryGuardClass::loop(void) {

    if (!_useBatteryGuard) { return; }

    if (_verboseLogging) {
        MessageOutput.printf("%s\r\n", getText(Text::T_HEAD).data());
        MessageOutput.printf("%s ---------------- Battery-Guard information block (every minute) ----------------\r\n",
            getText(Text::T_HEAD).data());

        // "Open circuit voltage"
        printOpenCircuitVoltageInformationBlock();
    }

    // "Low voltage power limiter"


    // "Periodically recharge the battery"


    if (_verboseLogging) {
        MessageOutput.printf("%s\r\n", getText(Text::T_HEAD).data());
        MessageOutput.printf("%s --------------------------------------------------------------------------------\r\n",
            getText(Text::T_HEAD).data());
        MessageOutput.printf("%s\r\n", getText(Text::T_HEAD).data());
    }
}


/*
 * Update the battery guard with new values. (voltage[V], current[A], millisStamp[ms])
 * Function should be called from battery provider
 * Note: Just call the function if new values are available.
 */
void BatteryGuardClass::updateBatteryValues(float const voltage, float const current, uint32_t const millisStamp) {
    if (_useBatteryGuard && (voltage >= 0.0f)) {
        // analyse the measurement period
        if ((_battMillis != 0) && (voltage != _battVoltage)) {
            _battPeriod.addNumber(millisStamp - _battMillis);
        }
        _battVoltage = voltage;
        _battCurrent = current;
        _battMillis = millisStamp;
        _battVoltageAVG.addNumber(_battVoltage);
        calculateInternalResistance(_battVoltage, _battCurrent);
        calculateOpenCircuitVoltage(_battVoltage, _battCurrent);
    }
}


/*
 * Calculate the battery open circuit voltage.
 * Returns true if a new value was calculated
 */
bool BatteryGuardClass::calculateOpenCircuitVoltage(float const nowVoltage, float const nowCurrent) {

    // calculate the open circuit battery voltage (current flow into the battery must be positive)
    auto oResistor = getInternalResistance();
    if (oResistor.has_value()) {
        _openCircuitVoltageAVG.addNumber(nowVoltage - nowCurrent * oResistor.value());
        return true;
    }
    return false;
}


/*
 * Returns the battery internal resistance, calculated / configured or nullopt if neither value is valid
 */
std::optional<float> BatteryGuardClass::getInternalResistance(void) const {
    if (_resistanceFromCalcAVG.getCounts() > 4) { return _resistanceFromCalcAVG.getAverage(); }
    if (_resistanceFromConfig != 0.0f) { return _resistanceFromConfig; }
    return std::nullopt;
}


/*
 * Returns the battery open circuit voltage or nullopt if value is not valid
 */
std::optional<float> BatteryGuardClass::getOpenCircuitVoltage(void) {
  if ((_openCircuitVoltageAVG.getCounts() > 0) && isDataValid()) {
        return _openCircuitVoltageAVG.getAverage();
    } else {
        _notAvailableCounter++;
        return std::nullopt;
    }
}


/*
 * Calculate the battery resistance between the battery cells and the voltage measurement device.
 * Returns true if a new resistance value was calculated
 */
bool BatteryGuardClass::calculateInternalResistance(float const nowVoltage, float const nowCurrent) {

    // we must avoid to use measurement values during any power transition.
    // To solve this problem, we check whether two consecutive measurements are almost identical
    if (!_firstOfTwoAvailable || (std::abs(_pFirstVolt.first - nowVoltage) > 0.005f) ||
        (std::abs(_pFirstVolt.second - nowCurrent) > 0.2f)) {
        _pFirstVolt.first = nowVoltage;
        _pFirstVolt.second = nowCurrent;
        _firstOfTwoAvailable = true;
        return false;
    }
    _firstOfTwoAvailable = false;  // prepair for the next calculation

    // store the average in min or max buffer
    std::pair<float,float> avgVolt = std::make_pair((nowVoltage + _pFirstVolt.first) / 2.0f, (nowCurrent + _pFirstVolt.second) / 2.0f);
    if (!_minMaxAvailable) {
        _pMinVolt = _pMaxVolt = avgVolt;
        _lastMinMaxMillis = millis();
        _minMaxAvailable = true;
    } else {
        if (avgVolt.first < _pMinVolt.first) { _pMinVolt = avgVolt; }
        if (avgVolt.first > _pMaxVolt.first) { _pMaxVolt = avgVolt; }
    }

    // we evaluate min and max values in a time duration of 30 sec
    if ((!_minMaxAvailable || (millis() - _lastMinMaxMillis) < 30*1000)) { return false; }
    _minMaxAvailable = false;  // prepair for the next calculation

    // we need a minimum voltage difference to get a sufficiently good result (failure < 10%)
    // SmartShunt: 50mV (about 100W on VDC: 24V, Ri: 12mOhm)
    if ((_pMaxVolt.first - _pMinVolt.first) >= _minDiffVoltage) {
        float resistor = std::abs((_pMaxVolt.first - _pMinVolt.first) / (_pMaxVolt.second - _pMinVolt.second));

        // we try to keep out bad values from the average
        if (_resistanceFromCalcAVG.getCounts() < 10) {
            _resistanceFromCalcAVG.addNumber(resistor);
        } else {
            if ((resistor > _resistanceFromCalcAVG.getAverage() / 2.0f) && (resistor < _resistanceFromCalcAVG.getAverage() * 2.0f)) {
                _resistanceFromCalcAVG.addNumber(resistor);
            }
        }

        // todo: delete after testing
        if constexpr(MODULE_DEBUG >= 1) {
            MessageOutput.printf("%s Resistor - Calculated: %0.3fOhm\r\n", getText(Text::T_HEAD).data(), resistor);
        }
        return true;
    }
    return false;
}


/*
 * Prints the "Battery open circuit voltage" information block
 */
void BatteryGuardClass::printOpenCircuitVoltageInformationBlock(void)
{
    MessageOutput.printf("%s\r\n", getText(Text::T_HEAD).data());
    MessageOutput.printf("%s 1) Function: Battery open circuit voltage\r\n",
        getText(Text::T_HEAD).data());

    MessageOutput.printf("%s Actual voltage: %0.3fV, Avarage voltage: %0.3fV, Open circuit voltage: %0.3fV\r\n",
        getText(Text::T_HEAD).data(), _battVoltage, _battVoltageAVG.getAverage(), _openCircuitVoltageAVG.getAverage());

  auto oResistance = getInternalResistance();
    if (!oResistance.has_value()) {
        MessageOutput.printf("%s Resistance neither calculated or configured\r\n", getText(Text::T_HEAD).data());
    } else {
        auto resCalc = (_resistanceFromCalcAVG.getCounts() > 4) ? _resistanceFromCalcAVG.getAverage() * 1000.0f : 0.0f;
        MessageOutput.printf("%s Resistance in use: %0.1fmOhm, (Calculated: %0.1fmOhm, Configured: %0.1fmOhm)\r\n",
            getText(Text::T_HEAD).data(), oResistance.value() * 1000.0f, resCalc, _resistanceFromConfig * 1000.0f);
    }

    MessageOutput.printf("%s Calculated resistance: %0.1fmOhm (Min: %0.1f, Max: %0.1f, Last: %0.1f, Amount: %i)\r\n",
        getText(Text::T_HEAD).data(), _resistanceFromCalcAVG.getAverage()*1000.0f, _resistanceFromCalcAVG.getMin()*1000.0f,
        _resistanceFromCalcAVG.getMax()*1000.0f, _resistanceFromCalcAVG.getLast()*1000.0f, _resistanceFromCalcAVG.getCounts());

    MessageOutput.printf("%s Measurement period: %ims, Voltage and current not available counter: %i\r\n",
        getText(Text::T_HEAD).data(), _battPeriod.getAverage(), _notAvailableCounter);
}


/*
 * Returns a string according to current text number
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
