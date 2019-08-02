#pragma once

#include <map>
#include <vector>


template <typename V, typename Extractor,
          typename K = typename std::result_of<Extractor(const V&)>::type>
std::map<K, std::vector<V>>
split(const std::vector<V>& data, Extractor extractor)
{
    auto r = std::map<K, std::vector<V>>{};
    for (auto&& v : data)
        r[extractor(v)].push_back(v);
    return r;
}
