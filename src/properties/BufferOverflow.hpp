#pragma once

#include "../Property.hpp"

namespace SVF {

class BufferOverflow final : public Property {
public:
    std::string_view name() const override { return "overflow"; }

    std::vector<Site> sites(const ICFG& icfg) const override
    {
        std::vector<Site> result;
        for (const auto& [id, node] : icfg)
        {
            // Call and return nodes can contain modeled effects of external functions.
            if (SVFUtil::isa<CallICFGNode>(node) || SVFUtil::isa<RetICFGNode>(node))
                continue;
            for (const auto* stmt : node->getSVFStmts())
            {
                if (SVFUtil::isa<LoadStmt>(stmt))
                    result.push_back({node, stmt, "load"});
                else if (SVFUtil::isa<StoreStmt>(stmt))
                    result.push_back({node, stmt, "store"});
            }
        }
        return result;
    }

    Result check(const Site& site, const StateView& state) const override
    {
        Result result{site, {}, Verdict::Unproved, "ae-no-state", {}};
        if (!state.reached(site.node))
            return result;

        const ValVar* pointer;
        const SVFType* type;
        if (const auto* load = SVFUtil::dyn_cast<LoadStmt>(site.stmt))
        {
            pointer = load->getRHSVar();
            type = load->getLHSVar()->getType();
        }
        else
        {
            const auto* store = SVFUtil::cast<StoreStmt>(site.stmt);
            pointer = store->getLHSVar();
            type = store->getRHSVar()->getType();
        }
        const auto width = type ? type->getByteSize() : 0;
        result.evidence.push_back({"width", width ? std::optional<IntervalValue>(width) : std::nullopt});
        const auto targets = state.addresses(pointer, site.node);
        result.reason = "ae-object-unknown";
        if (!targets || targets->empty())
            return result;

        bool missing = false;
        bool allIn = true;
        bool someOut = false;
        for (const auto address : *targets)
        {
            const auto* base = state.baseObject(address, site.node);
            const auto offset = state.byteOffset(pointer, address, site.node);
            const auto size = state.byteSize(base);
            const auto target = std::to_string(address);
            result.evidence.push_back({"offset." + target, offset});
            result.evidence.push_back({"size." + target, size});
            if (!offset || !size || !width)
            {
                missing = true;
                continue;
            }
            const auto end = *offset + IntervalValue(width);
            const BoundedInt zero(0);
            allIn &= offset->lb().geq(zero) && end.ub().leq(size->lb());
            someOut |= offset->ub() < zero || end.lb() > size->ub();
        }
        if (someOut)
            result.reason = "ae-out-of-bounds-if-reached";
        else if (missing)
            result.reason = "ae-bounds-unavailable";
        else if (allIn)
        {
            result.verdict = Verdict::Proved;
            result.reason = "ae-in-bounds";
        }
        else
            result.reason = "ae-bounds-unknown";
        return result;
    }
};

} // namespace SVF
