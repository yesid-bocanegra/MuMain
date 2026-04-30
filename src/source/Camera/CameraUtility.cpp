// CameraUtility.cpp - Camera movement and positioning utilities
// Extracted from ZzzScene.cpp as part of scene refactoring.
//
// PR #335 wired this through CameraManager. Branch's previous inline
// helpers (CalculateCameraViewFar / AdjustHeroHeight / CalculateCameraPosition
// / SetCameraAngle / UpdateCustomCameraDistance / UpdateCameraDistance /
// SetCameraFOV / HandleEditorMode) all duplicated logic that now lives in
// Camera/DefaultCamera.cpp (and Camera/OrbitalCamera.cpp for orbital mode),
// dispatched by Camera/CameraManager.cpp. This file is now a thin shim
// that delegates the per-frame camera tick to the manager and handles the
// F9 mode toggle.

#include "stdafx.h"
#include "Camera/CameraUtility.h"
#include "Camera/CameraManager.h"
#include "Camera/CameraMove.h"
#include "Input.h"

// External variable declarations
extern short g_shCameraLevel;
extern float g_fSpecialHeight;

namespace
{
// F9 key state tracking — detect press, not held.
bool s_bF9KeyPressed = false;

void HandleCameraModeToggle()
{
    bool bF9Down = CInput::Instance().IsKeyDown(VK_F9);
    if (bF9Down && !s_bF9KeyPressed)
    {
        // Cycles Default -> Orbital -> (FreeFly editor only) -> Default in MainScene.
        // CameraManager silently ignores Orbital outside MainScene.
        CameraManager::Instance().CycleToNextMode();
    }
    s_bF9KeyPressed = bF9Down;
}
} // namespace

/**
 * @brief Main camera controller — delegates to CameraManager.
 * @return Whatever the active camera's Update() reports (currently unused
 *         by callers — they ignore the return value).
 */
bool MoveMainCamera()
{
    HandleCameraModeToggle();
    return CameraManager::Instance().Update();
}
