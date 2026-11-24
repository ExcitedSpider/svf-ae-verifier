#pragma once

#include "Property.hpp"
#include <memory>

namespace SVF {

struct PropertyResults {
    std::string name;
    std::vector<Result> sites;
};

class Verifier {
public:
    void add(std::unique_ptr<Property> property) { properties.push_back(std::move(property)); }

    std::vector<PropertyResults> run(const ICFG& icfg, const StateView& state) const
    {
        std::vector<PropertyResults> results;
        for (const auto& property : properties)
        {
            auto& group = results.emplace_back(PropertyResults{std::string(property->name()), {}});
            for (const auto& site : property->sites(icfg))
            {
                auto result = property->check(site, state);
                const auto* function = site.node->getFun();
                auto location = parseSourceLocation(site.node->getSourceLoc(),
                                                    function ? function->getName() : "");
                if (result.location.line)
                    location.line = result.location.line;
                result.location = std::move(location);
                group.sites.push_back(std::move(result));
            }
        }
        return results;
    }

private:
    std::vector<std::unique_ptr<Property>> properties;
};

} // namespace SVF
