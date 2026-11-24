#pragma once

#include "Result.hpp"
#include "StateView.hpp"
#include "Graphs/ICFG.h"

namespace SVF {

class Property {
public:
    virtual ~Property() = default;
    virtual std::string_view name() const = 0;
    virtual std::vector<Site> sites(const ICFG& icfg) const = 0;
    virtual Result check(const Site& site, const StateView& state) const = 0;
};

} // namespace SVF
