#pragma once


#include <cstdlib>
#include <random>
#include <stdexcept>

namespace viichain
{
double rand_fraction();

size_t rand_pareto(float alpha, size_t max);

bool rand_flip();

extern std::default_random_engine gRandomEngine;

template <typename T>
T
rand_uniform(T lo, T hi)
{
    return std::uniform_int_distribution<T>(lo, hi)(gRandomEngine);
}

template <typename T>
T const&
rand_element(std::vector<T> const& v)
{
    if (v.size() == 0)
    {
        throw std::range_error("rand_element on empty vector");
    }
    return v.at(rand_uniform<size_t>(0, v.size() - 1));
}
}
