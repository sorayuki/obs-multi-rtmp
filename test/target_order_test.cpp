#include "doctest.h"
#include "target-order.h"

static OutputTargetConfigPtr mk(const std::string& id) {
    auto p = std::make_shared<OutputTargetConfig>();
    p->id = id;
    return p;
}

TEST_CASE("ReorderTargets applies id order") {
    std::list<OutputTargetConfigPtr> cur = {mk("a"), mk("b"), mk("c")};
    auto out = ReorderTargets({"c", "a", "b"}, cur);
    std::vector<std::string> ids;
    for (auto& t : out) ids.push_back(t->id);
    CHECK(ids == std::vector<std::string>{"c", "a", "b"});
}

TEST_CASE("ReorderTargets keeps unmatched ids in original order at the end") {
    std::list<OutputTargetConfigPtr> cur = {mk("a"), mk("b"), mk("c")};
    // "c" missing from idOrder, "x" is unknown and ignored
    auto out = ReorderTargets({"b", "x", "a"}, cur);
    std::vector<std::string> ids;
    for (auto& t : out) ids.push_back(t->id);
    CHECK(ids == std::vector<std::string>{"b", "a", "c"});
}
