
#include "MetricResetter.h"

namespace viichain
{

void
MetricResetter::Process(medida::Counter& counter)
{
    counter.clear();
}

void
MetricResetter::Process(medida::Meter& meter)
{
    meter.Clear();
}

void
MetricResetter::Process(medida::Histogram& histogram)
{
    histogram.Clear();
}

void
MetricResetter::Process(medida::Timer& timer)
{
    timer.Clear();
}
}
