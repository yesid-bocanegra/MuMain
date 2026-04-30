#include "stdafx.h"
#include "CameraProjection.h"
#include "CameraState.h"
#include "CameraConfig.h"

// External window dimensions
extern unsigned int WindowWidth;
extern unsigned int WindowHeight;
extern int OpenglWindowX;
extern int OpenglWindowY;
extern int OpenglWindowWidth;
extern int OpenglWindowHeight;

// Actual viewport dimensions (distinct from OpenglWindowWidth/Height which are full window)
static int s_ViewportWidth = 0;
static int s_ViewportHeight = 0;

void CameraProjection::SetupPerspective(CameraState& state, float fov, float aspect,
                                          float zNear, float zFar)
{
    // Set up perspective. SDL3 branch routes this through gluPerspective2
    // (RenderFX/ZzzOpenglUtil.cpp) which calls a file-local MuRenderer-backed
    // implementation. Calling gluPerspective directly would resolve to a
    // different overload (or, depending on global shims, recurse infinitely).
    gluPerspective2(fov, aspect, zNear, zFar);

    // Use actual viewport dimensions (set by SetViewport) for screen center and
    // perspective. This accounts for the game viewport being narrower/shorter than
    // the full window when UI panels (inventory, NPC shop, etc.) are open.
    int vpWidth = s_ViewportWidth > 0 ? s_ViewportWidth : OpenglWindowWidth;
    int vpHeight = s_ViewportHeight > 0 ? s_ViewportHeight : OpenglWindowHeight;

    state.ScreenCenterX = OpenglWindowX + vpWidth / 2;
    state.ScreenCenterY = OpenglWindowY + vpHeight / 2;
    state.ScreenCenterYFlip = WindowHeight - state.ScreenCenterY;

    float fovRad = fov * 0.5f * Q_PI / 180.0f;
    state.PerspectiveX = tanf(fovRad) / (float)(vpWidth / 2) * aspect;
    state.PerspectiveY = tanf(fovRad) / (float)(vpHeight / 2);
}

void CameraProjection::SetViewport(int x, int y, int width, int height)
{
    OpenglWindowX = x;
    OpenglWindowY = y;
    s_ViewportWidth = width;
    s_ViewportHeight = height;

    // SDL3 branch routes viewport changes through MuRenderer via glViewport2
    // (RenderFX/ZzzOpenglUtil.cpp). Y coordinate is flipped to bottom-origin.
    glViewport2(x, WindowHeight - (y + height), width, height);
}

void CameraProjection::ScreenToWorldRay(const CameraState& state, int sx, int sy,
                                         vec3_t outTarget, bool bFixView)
{
    // Convert reference coordinates to actual pixels
    sx = sx * WindowWidth / REFERENCE_WIDTH;
    sy = sy * WindowHeight / REFERENCE_HEIGHT;

    vec3_t p1, p2;

    float farDist = bFixView ? state.ViewFar : RENDER_ITEMVIEW_FAR;

    p1[0] = (float)(sx - state.ScreenCenterX) * farDist * state.PerspectiveX;
    p1[1] = -(float)(sy - state.ScreenCenterY) * farDist * state.PerspectiveY;
    p1[2] = -farDist;

    p2[0] = -state.Matrix[0][3];
    p2[1] = -state.Matrix[1][3];
    p2[2] = -state.Matrix[2][3];
    VectorIRotate(p2, state.Matrix, MousePosition);
    VectorIRotate(p1, state.Matrix, p2);
    VectorAdd(MousePosition, p2, outTarget);
}

void CameraProjection::WorldToScreen(const CameraState& state, const vec3_t worldPos,
                                      int* outX, int* outY)
{
    vec3_t transformPos;
    VectorTransform(worldPos, state.Matrix, transformPos);

    // Project to screen
    *outX = -(int)(transformPos[0] / state.PerspectiveX / transformPos[2]) + state.ScreenCenterX;
    *outY = (int)(transformPos[1] / state.PerspectiveY / transformPos[2]) + state.ScreenCenterY;

    // Convert to 640×480 reference coordinates
    *outX = *outX * REFERENCE_WIDTH / (int)WindowWidth;
    *outY = *outY * REFERENCE_HEIGHT / (int)WindowHeight;
}

void CameraProjection::TransformPosition(const CameraState& state, const vec3_t position,
                                          vec3_t outWorldPosition, int* outX, int* outY)
{
    vec3_t temp;
    VectorSubtract(position, state.Position, temp);
    VectorRotate(temp, state.Matrix, outWorldPosition);

    // Project to screen (pixel coordinates)
    *outX = (int)(outWorldPosition[0] / state.PerspectiveX / -outWorldPosition[2]) + state.ScreenCenterX;
    *outY = (int)(outWorldPosition[1] / state.PerspectiveY / -outWorldPosition[2]) + state.ScreenCenterYFlip;
}

bool CameraProjection::TestDepthBuffer(const CameraState& /*state*/, const vec3_t /*position*/)
{
    // SDL3 branch port-stub: the upstream implementation calls glReadPixels
    // to sample the depth buffer, which the SDL3 GPU pipeline doesn't expose
    // via raw GL. No live caller on this branch (Default/Orbital cameras'
    // main update loops never reach this method), so returning "unoccluded"
    // is the safe default until a future commit wires this through
    // MuRenderer's depth-read path.
    return true;
}

void CameraProjection::GetOpenGLMatrix(float outMatrix[3][4])
{
    // SDL3 branch port-stub: the upstream implementation reads
    // GL_MODELVIEW_MATRIX via glGetFloatv. SDL3 GPU has no equivalent
    // immediate-mode matrix stack — MatrixStack lives in
    // RenderFX/MatrixStack.{cpp,h}. No live caller on this branch, so an
    // identity-ish zero matrix is fine until a future commit wires this
    // through MatrixStack's top.
    for (int i = 0; i < 3; i++)
        for (int j = 0; j < 4; j++)
            outMatrix[i][j] = 0.0f;
}
