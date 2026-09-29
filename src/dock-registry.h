#pragma once

#include <string>
#include <vector>

#include "push-widget.h"

// Bridges the dock's live target list to code that isn't part of the Qt
// dock itself (namely the obs-websocket vendor API in websocket-api.cpp).
// Implemented in obs-multi-rtmp.cpp.
//
// Both functions must only be called on the UI/Qt thread (e.g. from inside
// GetGlobalService().RunInUIThreadBlocking(...)), since they walk the
// dock's live Qt widget list.
std::vector<PushWidget*> GetAllStreamTargets();
PushWidget* FindStreamTargetById(const std::string& id);
