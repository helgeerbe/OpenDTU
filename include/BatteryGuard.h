#pragma once
#include <Arduino.h>
#include <frozen/string.h>
#include <TaskSchedulerDeclarations.h>
#include "Statistic.h"


class BatteryGuardClass {
    public:
        BatteryGuardClass() = default;
        ~BatteryGuardClass() = default;

        void init(Scheduler& scheduler);
        void updateSettings(void);
        void updateBatteryValues(float const nowVoltage, float const nowCurrent, uint32_t const millisStamp);
        std::optional<float> getOpenCircuitVoltage(void);
        std::optional<float> getInternalResistance(void) const;

    private:
        enum class Text : uint8_t {
            Q_NODATA = 0,
            Q_EXCELLENT = 1,
            Q_GOOD = 2,
            Q_BAD = 3,
            T_HEAD = 4
        };

        void loop(void);
        bool calculateOpenCircuitVoltage(float const nowVoltage, float const nowCurrent);
        bool calculateInternalResistance(float const nowVoltage, float const nowCurrent);
        void printOpenCircuitVoltageInformationBlock(void);
        frozen::string const& getText(Text tNr);

        // Returns true if battery data is not older as 30 seconds
        bool isDataValid() { return (millis() - _battMillis) < 30*1000; }

        // the following values ​​are used to calculate the "Open circuit voltage"
        float _battVoltage = 0.0f;                          // actual battery voltage [V]
        float _battCurrent = 0.0f;                          // actual battery current [A]
        uint32_t _battMillis = 0;                           // measurement time stamp [millis()]
        WeightedAVG<uint32_t> _battPeriod {20};             // measurement period [ms]
        WeightedAVG<float> _battVoltageAVG {5};             // average battery voltage [V]
        WeightedAVG<float> _openCircuitVoltageAVG {5};      // average battery open circuit voltage [V]
        float _resistanceFromConfig = 0.0f;                 // configured battery resistance [Ohm]
        size_t _notAvailableCounter = 0;                    // voltage or current were not available counter

        // the following values ​​are used to calculate the "Battery internal resistance"
        WeightedAVG<float> _resistanceFromCalcAVG {10};     // calculated battery resistance [Ohm]
        bool _firstOfTwoAvailable = false;                  // true after to got the first of two values
        bool _minMaxAvailable = false;                      // true if minimum and maximum values are available
        std::pair<float,float> _pFirstVolt = {0.0f,0.0f};   // first of two voltages and related current [V,A]
        std::pair<float,float> _pMaxVolt = {0.0f,0.0f};     // maximum voltage and related current [V,A]
        std::pair<float,float> _pMinVolt = {0.0f,0.0f};     // minimum voltage and related current [V,A]
        uint32_t _lastMinMaxMillis = 0;                     // last millis from the first min/max values [millis()]
        float const _minDiffVoltage = 0.05f;                // minimum required difference [V]
                                                            // unclear if this value will also fit to other battery provider

        Task _loopTask;                                     // Task
        bool _verboseLogging = false;                       // Logging On/Off
        bool _useBatteryGuard = false;                      // "Battery guard" On/Off
};

extern BatteryGuardClass BatteryGuard;
