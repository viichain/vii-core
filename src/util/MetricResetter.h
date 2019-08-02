#pragma once


#include "medida/metrics_registry.h"

namespace medida
{
class MetricsRegistry;
class Meter;
class Counter;
class Timer;
}

namespace viichain
{

class MetricResetter : public medida::MetricProcessor
{
  public:
    MetricResetter() = default;
    ~MetricResetter() override = default;
    void Process(medida::Counter& counter) override;
    void Process(medida::Meter& meter) override;
    void Process(medida::Histogram& histogram) override;
    void Process(medida::Timer& timer) override;
};
}
