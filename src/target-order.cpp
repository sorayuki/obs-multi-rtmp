#include "target-order.h"
#include <unordered_map>

std::list<OutputTargetConfigPtr> ReorderTargets(
    const std::vector<std::string>& idOrder,
    const std::list<OutputTargetConfigPtr>& current)
{
    std::unordered_map<std::string, OutputTargetConfigPtr> byId;
    for (auto& t : current)
        if (t) byId.emplace(t->id, t);

    std::list<OutputTargetConfigPtr> result;
    for (auto& id : idOrder) {
        auto it = byId.find(id);
        if (it != byId.end()) {
            result.push_back(it->second);
            byId.erase(it);
        }
    }
    // keep unmatched in their original order
    for (auto& t : current) {
        if (!t) continue;
        auto it = byId.find(t->id);
        if (it != byId.end()) {
            result.push_back(it->second);
            byId.erase(it);
        }
    }
    return result;
}
