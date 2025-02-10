#pragma once

/*
 * Weighted average and statistics class (initialising value defines the weighted average 10 = 10%)
*/
template <typename T>
class WeightedAVG {
public:
    explicit WeightedAVG(size_t factor)
      : _countMax(factor)
      , _count(0), _countNum(0), _avgV(0), _minV(0), _maxV(0), _lastV(0)  {}

    // Add a value to the statistics
    void addNumber(const T& num) {
      if (_count == 0){
          _count++;
          _avgV = num;
          _minV = num;
          _maxV = num;
          _countNum = 1;
      } else {
          if (_count < _countMax)
              _count++;
          _avgV = (_avgV * (_count - 1) + num) / _count;
          if (num < _minV) { _minV = num; }
          if (num > _maxV) { _maxV = num; }
          if (_countNum < 10000) { _countNum++; }
      }
      _lastV = num;
    }

    // Reset the statistic data
    void reset(void) { _count = 0; _avgV = 0; _minV = 0; _maxV = 0; _lastV = 0; _countNum = 0; }
    // Reset the statistic data and initialize with first value
    void reset(const T& num) { _count = 0; addNumber(num); }
    // Returns the weighted average
    T getAverage() const { return _avgV; }
    // Returns the minimum value
    T getMin() const { return _minV; }
    // Returns the maximum value
    T getMax() const { return _maxV; }
    // Returns the last added value
    T getLast() const { return _lastV; }
    // Returns the amount of added values. Limited to 10000
    size_t getCounts() const { return _countNum; }

private:
    size_t _countMax;      // weighting factor (10 => 1/10 => 10%)
    size_t _count;         // counter (0 - _countMax)
    size_t _countNum;      // counts the amount of added values (0 - 10000)
    T _avgV;               // average value
    T _minV;               // minimum value
    T _maxV;               // maximum value
    T _lastV;              // last value
};

