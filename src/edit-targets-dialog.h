#pragma once

#include <optional>
#include <string>

#include "pch.h"

struct GlobalEditSnapshot;

// Shows the multi-target editor for the given serialized MultiOutputConfig.
// The dialog works on its own copy; when accepted it returns the serialized,
// edited config. Returns std::nullopt when the user cancels or the input can't
// be parsed.
std::optional<std::string> showEditTargetsDialog(
	const std::string& configJson,
	const GlobalEditSnapshot& snapshot,
	QWidget* parent = 0);
