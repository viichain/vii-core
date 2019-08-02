
#include "Math.h"
#include <cmath>

namespace viichain
{

std::default_random_engine gRandomEngine;
std::uniform_real_distribution<double> uniformFractionDistribution(0.0, 1.0);

double
rand_fraction()
{
    return uniformFractionDistribution(gRandomEngine);
}

size_t
rand_pareto(float alpha, size_t max)
{
        float f =
        static_cast<float>(1) /
        static_cast<float>(pow(rand_fraction(), static_cast<float>(1) / alpha));
        return static_cast<size_t>(f * max - 1) % max;
}

bool
rand_flip()
{
    return (gRandomEngine() & 1);
}
}
