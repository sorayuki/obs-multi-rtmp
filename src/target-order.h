#pragma once
#include <list>
#include <string>
#include <vector>
#include "output-config.h"

std::list<OutputTargetConfigPtr> ReorderTargets(
    const std::vector<std::string>& idOrder,
    const std::list<OutputTargetConfigPtr>& current);
