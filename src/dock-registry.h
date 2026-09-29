#pragma once

#include <string>
#include <vector>

#include "push-widget.h"
#include "output-config.h"

// Bridges the dock's live target list to code that isn't part of the Qt
// dock itself (namely the obs-websocket vendor API in websocket-api.cpp).
// Implemented in obs-multi-rtmp.cpp.
//
// All of these must only be called on the UI/Qt thread (e.g. from inside
// GetGlobalService().RunInUIThreadBlocking(...)), since they touch the
// dock's live Qt widget list.
std::vector<PushWidget*> GetAllStreamTargets();
PushWidget* FindStreamTargetById(const std::string& id);

// Adds `target` (already filled in by the caller, with a fresh id from
// GenerateId()) as a new row, without popping up the edit dialog - there's
// no one to click through it for a headless caller. Returns nullptr if the
// dock isn't ready yet.
PushWidget* CreateStreamTarget(OutputTargetConfigPtr target);

// Removes a target by id, same as the dock's own Delete button but without
// its confirmation dialog. The caller (see websocket-api.cpp's
// delete_target request) is responsible for any "are you sure"/"is it
// live" gating before calling this.
bool DeleteStreamTarget(const std::string& id);
