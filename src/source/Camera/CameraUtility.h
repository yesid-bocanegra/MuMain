#pragma once

// CameraUtility.h - Camera movement and positioning utilities

#include "CameraState.h"

// Legacy distance globals — kept until World/ZzzLodTerrain.cpp's two callers
// migrate to g_Camera.Distance / g_Camera.DistanceTarget.
extern float CameraDistanceTarget;
extern float CameraDistance;

// Main camera controller
// Returns true if camera is locked
bool MoveMainCamera();
