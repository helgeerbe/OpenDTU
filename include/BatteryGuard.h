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

        std::optional<float> calculateOpenCircuitVoltage(float const nowVoltage, float const nowCurrent);
        std::optional<float> getOpenCircuitVoltage(void) const;
        std::optional<float> calculateInternalResistance(float const nowVoltage, float const nowCurrent);
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
        void printOpenCircuitVoltageInformationBlock(void);
        frozen::string const& getText(Text tNr);

        // used for calculation of the "Open circuit voltage"
        WeightedAVG<float> _openCircuitVoltageAVG {10};     // battery open circuit voltage (average factor 10%)
        uint32_t _lastOCVMillis = 0;                        // last millis of calculation of the open circuit voltage
        float _resistorConfig = 0.0f;                       // value from configuration or resistance calculation

        // used for calculation of the "Battery internal resistance"
        WeightedAVG<float> _internalResistanceAVG {10};     // resistor (average factor 10%)
        bool _firstOfTwoAvailable = false;                  // false after to got the first of two values
        bool _minMaxAvailable = false;                      // minimum and maximum values available
        std::pair<float,float> _firstVolt = {0.0f,0.0f};    // first of two voltage and related current
        std::pair<float,float> _maxVolt = {0.0f,0.0f};      // maximum voltage and related current
        std::pair<float,float> _minVolt = {0.0f,0.0f};      // minimum voltage and related current
        uint32_t _lastMinMaxMillis = 0;                     // last millis from the first min/max values
        float const _minDiffVoltage = 0.05f;                // 50mV minimum difference to calculate a resistance (Smart Shunt)
                                                            // unclear if this value will also fit to other battery provider

        Task _loopTask;                                     // Task
        bool _verboseLogging = false;                       // Logging On/Off
        bool _useBatteryGuard = false;                      // "Battery guard" On/Off
};

extern BatteryGuardClass BatteryGuard;
