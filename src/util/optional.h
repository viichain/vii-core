#pragma once


#include "cereal/cereal.hpp"
#include <memory>

namespace viichain
{
template <typename T> using optional = std::shared_ptr<T>;

template <typename T, class... Args>
optional<T>
make_optional(Args&&... args)
{
    return std::make_shared<T>(std::forward<Args>(args)...);
}

template <typename T>
optional<T>
nullopt()
{
    return std::shared_ptr<T>();
};
}

namespace cereal
{
template <class Archive, class T>
void
save(Archive& ar, viichain::optional<T> const& opt)
{
    ar(make_nvp("has", !!opt));
    if (opt)
    {
        ar(make_nvp("val", *opt));
    }
}

template <class Archive, class T>
void
load(Archive& ar, viichain::optional<T>& o)
{
    bool isSet;
    o.reset();
    ar(make_nvp("has", isSet));
    if (isSet)
    {
        T v;
        ar(make_nvp("val", v));
        o = viichain::make_optional<T>(v);
    }
}
} // namespace cereal
