# Upstream merge: PR #325 + PR #335 follow-ups

Tracking the partial port of two upstream PRs onto
`cross-platform-sdl-migration-merged`, started 2026-04-28. Companion to
`upstream-merge-features.md` and `pr-329-review-followups.md`.

> **Resume hint for a future session:** read this whole doc, then `git log
> --oneline 33529d81..HEAD` to confirm the five session commits below are
> still on top. The next concrete task is the camera-globals migration in
> §3 (PR #335 phase 2c).

## 1. Branch state at session start

- Branch: `cross-platform-sdl-migration-merged`
- HEAD before session: `33529d81 docs: refresh PR-329 follow-ups doc`
- Merge-base with `origin/main`: `c134d086` (PR #308 merge, 2026-04-25)
- Divergence at start: 56 commits on main, 430 on branch

Two upstream PRs accumulated since merge-base:

| PR | Subject | Merge commit | Status after this session |
|---|---|---|---|
| **#325** | packet-enums (sven-n) | `a8d8611f` | **Fully ported** ✅ |
| **#335** | 3d-camera-rework (Mosch0512) | `f1ffa170` | **Camera-globals migration done** ✅ — DevEditor / option polish / overlay wiring still pending (§4) |

## 2. Commits added this session (oldest → newest)

```
d1bd1b09  build(dotnet): port packet-enum codegen + 0.9.9 bump from main (PR #325)
e2880fd2  refactor(packets): adopt typed packet enums at call-sites (PR #325)
ebd0704e  refactor(camera): consolidate camera code under src/source/Camera/
7e9dbdba  feat(camera): add modular camera architecture from main (PR #335)
c453fac6  feat(ui,profiler): add NewUIComboBox + FrameProfiler from main (PR #335)
ddba58d3  docs: add resumption notes for partial PR #325 + #335 port (this doc)
197ff67d  fix(build): make Phase 2b camera framework compile on the SDL3 branch
fa71393a  docs: record Phase 2b build-fix shims and the eighth session commit
68120dd0  feat(camera): port LoginScene waypoint offset corrections (PR #335)
d4787dd8  refactor(camera): drop dead legacy CameraState wrapper + 3D-anaglyph globals (PR #335)
b14bf725  refactor(camera): migrate eight small users from legacy globals to g_Camera (PR #335)
56a72c76  refactor(camera): migrate ten more single-ref users to g_Camera (PR #335)
80c808bf  refactor(camera): migrate ZzzObject + MainScene + LoginScene to g_Camera (PR #335)
0bd4bbde  refactor(camera): migrate ZzzLodTerrain to g_Camera (PR #335)
c3a02c0f  refactor(camera): finish g_Camera migration — remove legacy globals (PR #335)
```

Phase 1 (`d1bd1b09` + `e2880fd2`) was build-and-run-verified. Everything
from `ebd0704e` through `c3a02c0f` builds clean on macOS-arm64 (Main + all
four test executables link green; only the pre-existing
ld macos-12.0-vs-Homebrew-26.0 warnings remain). Runtime behaviour through
the new `g_Camera` path is **not yet verified** — see §6 for the testable
checkpoint.

### 2.1 Build-fix shims introduced in `197ff67d`

These are deliberate stop-gaps that need replacement when Phase 2c+ wires
the camera framework through MuRenderer.

**`src/source/Main/stdafx.h`** — grew an "upstream compat" block near the GL
constant defines:

- `inline constexpr int REFERENCE_WIDTH = 640;`
- `inline constexpr int REFERENCE_HEIGHT = 480;`
  These mirror `origin/main:src/source/stdafx.h:149` and are referenced
  directly by `Camera/CameraProjection.cpp`. Real values, not stubs —
  keep these.
- Inline forwarders: `gluPerspective(...)` → `gluPerspective2(...)` and
  `glViewport(...)` → `glViewport2(...)`. The branch's `ZzzOpenglUtil.h`
  exposes the wrapped names; new Camera code uses the unwrapped names.
  Forwarders should keep working long term; can drop once new files use
  the wrapped names directly.
- **No-op stubs** that future work must replace with MuRenderer calls
  before the affected code paths become live:
  - `glReadPixels` writes 1.0f to dst — `CameraProjection::TestDepthBuffer`
    will always report "unoccluded" until a real MuRenderer-backed depth
    read is wired in.
  - `glGetFloatv` zeros 16 floats — `CameraProjection::GetOpenGLMatrix`
    returns an all-zero matrix until a real MuRenderer-backed matrix
    query is wired in.
  - Immediate-mode draw stubs (`glPushMatrix`, `glPopMatrix`,
    `glLoadIdentity`, `glRotatef`, `glLineWidth`, `glBegin`, `glEnd`,
    `glEnable`, `glDisable`, `glIsEnabled`, `glBlendFunc`, `glColor4f`,
    `glVertex2f`, `glVertex3f`, `glVertex3fv`, `glTexCoord2f`,
    `glNormal3f`) — these no-op the FrustumRenderer / DefaultCamera
    debug visualizers. They render nothing until replaced with
    MuRenderer line / matrix-stack equivalents.

  Search for `TODO(PR #335 wiring)` in `stdafx.h` to find the live ones
  before declaring the camera port complete.

**`src/source/Data/GameConfig.{h,cpp}` + `GameConfigConstants.h`** — added
the `Get/SetZoom` API + `[Camera]/Zoom = 1100` ini section/key/default.
This is permanent — `OrbitalCamera` persists its radius via this.

**`src/source/Camera/*`** — local include-path patches for SDL-branch
header layout (`_types.h` → `mu_base_types.h`, `_define.h` → `mu_define.h`,
`GameConfig/GameConfig.h` → `GameConfig.h`). Keep these patches; they are
not aspirational shims, they are the correct paths on this branch.

**`src/CMakeLists.txt`** — added
`-Wno-error=return-type-c-linkage` to the Clang relaxation list so
`extern "C" CameraManager& CameraManager_Instance()` doesn't trip
`-Werror`. Aligns with main's tolerance — keep.

## 3. PR #335 camera-globals migration — DONE ✅

**Status: completed in commits `68120dd0` … `c3a02c0f`.** All twelve legacy
camera globals are gone from the codebase, replaced with fields on the new
`g_Camera` instance (`CameraState`, defined in
`src/source/Camera/CameraState.cpp`, declared in `Camera/CameraState.h`
which is now pulled in by `RenderFX/ZzzOpenglUtil.h`). The two sub-sections
below are kept for historical context (the "before" snapshot of the work);
§3.5 documents the actual sequence we landed.

> If you're resuming and just want the next thing to do, skip to §4.

### 3.1 Globals to migrate (~210 references total)

| Old global (branch) | New (`g_Camera.X`) | Where defined on branch |
|---|---|---|
| `CameraDistance` | `g_Camera.Distance` | `Scenes/SceneCore.cpp:106` |
| `CameraDistanceTarget` | `g_Camera.DistanceTarget` | `Scenes/SceneCore.cpp:105` |
| `Camera3DFov` | *removed* | `Scenes/SceneCore.cpp:107` |
| `Camera3DRoll` | *removed* | `Scenes/SceneCore.cpp:108` |
| `g_CameraState` | *removed* (new `g_Camera` replaces it) | `Scenes/SceneCore.cpp:111` |
| `CameraPosition` | `g_Camera.Position` | extern in `RenderFX/ZzzOpenglUtil.h` |
| `CameraAngle` | `g_Camera.Angle` | extern in same file |
| `CameraMatrix` | `g_Camera.Matrix` | extern in same file |
| `CameraFOV` | `g_Camera.FOV` | extern in same file |
| `CameraViewNear` | `g_Camera.ViewNear` | extern in same file |
| `CameraViewFar` | `g_Camera.ViewFar` | extern in same file |
| `CameraTopViewEnable` | `g_Camera.TopViewEnable` | extern in same file |
| `PerspectiveX` | `g_Camera.PerspectiveX` | extern in same file |
| `PerspectiveY` | `g_Camera.PerspectiveY` | extern in same file |
| `g_fCameraCustomDistance` | `g_Camera.CustomDistance` | extern in same file |

Order matters when scripting substitutions — do `CameraDistanceTarget` *before*
`CameraDistance`, `CameraTopViewEnable` *before* `CameraViewFar/Near`, etc.
(longer-prefix-first).

### 3.2 Files to migrate (26)

```
src/source/Gameplay/Characters/ZzzObject.cpp
src/source/Gameplay/Items/PersonalShopTitleImp.cpp
src/source/GameShop/NewUIInGameShop.cpp
src/source/Platform/PlatformGlobalStubs.cpp        ← branch-only stub file
src/source/RenderFX/SideHair.cpp
src/source/RenderFX/ZzzEffectFireLeave.cpp
src/source/RenderFX/ZzzEffectPoint.cpp
src/source/RenderFX/ZzzOpenglUtil.cpp
src/source/RenderFX/ZzzOpenglUtil.h
src/source/Scenes/CharacterScene.cpp
src/source/Scenes/LoginScene.cpp
src/source/Scenes/MainScene.cpp
src/source/Scenes/SceneCommon.cpp
src/source/Scenes/SceneCore.cpp                    ← drop legacy global defs here
src/source/Scenes/SceneManager.cpp
src/source/UI/Events/NewUIGoldBowmanLena.cpp
src/source/UI/Framework/NewUI3DRenderMng.cpp
src/source/UI/Legacy/CharMakeWin.cpp
src/source/UI/Legacy/UIWindows.cpp
src/source/UI/Legacy/ZzzInterface.cpp
src/source/UI/Windows/Commerce/NewUIRegistrationLuckyCoin.cpp
src/source/World/Maps/GM_PK_Field.cpp
src/source/World/Maps/GM3rdChangeUp.cpp
src/source/World/Maps/GMBattleCastle.cpp
src/source/World/Maps/GMDoppelGanger2.cpp
src/source/World/ZzzLodTerrain.cpp
```

Plus the four files in `src/source/Camera/` whose main version is the
target state of the migration (currently still on branch's pre-PR-335
content):

```
src/source/Camera/CameraMove.cpp     ← take main's version (adds ApplyLoginSceneOffset)
src/source/Camera/CameraMove.h       ← take main's version (adds LoginSceneCameraDefaults namespace + extern offsets)
src/source/Camera/CameraUtility.cpp  ← take main's version (heavy gut — most logic moved into CameraManager)
src/source/Camera/CameraUtility.h    ← take main's version (drops legacy CameraState struct, includes new Camera/CameraState.h)
```

### 3.3 Path mapping (branch ↔ main)

The SDL reorg moved many files. Use this when scripting per-file 3-way merges:

```
src/source/Camera/CameraMove.cpp                            ↔ src/source/CameraMove.cpp
src/source/Camera/CameraMove.h                              ↔ src/source/CameraMove.h
src/source/Camera/CameraUtility.cpp                         ↔ src/source/Camera/CameraUtility.cpp   (same)
src/source/Camera/CameraUtility.h                           ↔ src/source/Camera/CameraUtility.h     (same)
src/source/RenderFX/ZzzOpenglUtil.cpp                       ↔ src/source/ZzzOpenglUtil.cpp
src/source/RenderFX/ZzzOpenglUtil.h                         ↔ src/source/ZzzOpenglUtil.h
src/source/RenderFX/SideHair.cpp                            ↔ src/source/SideHair.cpp
src/source/RenderFX/ZzzEffectPoint.cpp                      ↔ src/source/ZzzEffectPoint.cpp
src/source/RenderFX/ZzzEffectFireLeave.cpp                  ↔ src/source/ZzzEffectFireLeave.cpp
src/source/Scenes/MainScene.cpp                             ↔ src/source/Scenes/MainScene.cpp        (same)
src/source/Scenes/LoginScene.cpp                            ↔ src/source/Scenes/LoginScene.cpp       (same)
src/source/Scenes/CharacterScene.cpp                        ↔ src/source/Scenes/CharacterScene.cpp   (same)
src/source/Scenes/LoadingScene.cpp                          ↔ src/source/Scenes/LoadingScene.cpp     (same)
src/source/Scenes/SceneCommon.cpp                           ↔ src/source/Scenes/SceneCommon.cpp      (same)
src/source/Scenes/SceneCore.cpp                             ↔ src/source/Scenes/SceneCore.cpp        (same)
src/source/Scenes/SceneManager.cpp                          ↔ src/source/Scenes/SceneManager.cpp     (same)
src/source/Scenes/WebzenScene.cpp                           ↔ src/source/Scenes/WebzenScene.cpp      (same)
src/source/Gameplay/Characters/ZzzObject.cpp                ↔ src/source/ZzzObject.cpp
src/source/Gameplay/Items/PersonalShopTitleImp.cpp          ↔ src/source/PersonalShopTitleImp.cpp
src/source/GameShop/NewUIInGameShop.cpp                     ↔ src/source/GameShop/NewUIInGameShop.cpp (same)
src/source/World/ZzzLodTerrain.cpp                          ↔ src/source/ZzzLodTerrain.cpp
src/source/World/ZzzLodTerrain.h                            ↔ src/source/ZzzLodTerrain.h
src/source/World/Maps/GM_PK_Field.cpp                       ↔ src/source/GM_PK_Field.cpp
src/source/World/Maps/GM3rdChangeUp.cpp                     ↔ src/source/GM3rdChangeUp.cpp
src/source/World/Maps/GMBattleCastle.cpp                    ↔ src/source/GMBattleCastle.cpp
src/source/World/Maps/GMDoppelGanger2.cpp                   ↔ src/source/GMDoppelGanger2.cpp
src/source/UI/Framework/NewUI3DRenderMng.cpp                ↔ src/source/NewUI3DRenderMng.cpp
src/source/UI/Framework/NewUI3DRenderMng.h                  ↔ src/source/NewUI3DRenderMng.h
src/source/UI/Legacy/CharMakeWin.cpp                        ↔ src/source/CharMakeWin.cpp
src/source/UI/Legacy/UIWindows.cpp                          ↔ src/source/UIWindows.cpp
src/source/UI/Legacy/ZzzInterface.cpp                       ↔ src/source/ZzzInterface.cpp
src/source/UI/Events/NewUIGoldBowmanLena.cpp                ↔ src/source/NewUIGoldBowmanLena.cpp
src/source/UI/Windows/Commerce/NewUIRegistrationLuckyCoin.cpp ↔ src/source/NewUIRegistrationLuckyCoin.cpp
```

### 3.4 Gotchas surfaced during this session

**A. Local-variable shadowing in `CreateFrustrum2D`**
`src/source/World/ZzzLodTerrain.cpp:1969` declares
`float Width = 0.0f, CameraViewFar = 0.0f, CameraViewNear = 0.0f, CameraViewTarget = 0.0f;`
— these are *local* variables that shadow the globals. A naive
`s/CameraViewFar/g_Camera.ViewFar/g` would corrupt the local declaration
(`float g_Camera.ViewFar = 0.0f` is invalid). Either rename the locals first
(main does this in its version of the file) or hand-merge that hunk.

**B. `extern` re-declarations in scenes**
Several scenes have lines like `extern float CameraPosition[3];` near the top
of the .cpp. Those have to be *deleted* (not rewritten) when migrating, since
`g_Camera.Position` is provided through `Camera/CameraState.h` (transitively
included via `ZzzOpenglUtil.h`).

**C. Branch's char16_t / `mu::log::Get(...)` substitutions**
PR #325 phase 1b painstakingly preserved branch-side substitutions:
- `const wchar_t*` → `const char16_t*` (cross-platform string type)
- `g_ConsoleDebug->Write(MCD_SEND, L"...")` → `mu::log::Get("ui")->debug("...")`
- `symLoad(` → `mu::platform::GetSymbol(` (in regenerated `Dotnet/PacketBindings_*.h`)

When taking main's version of any file in §3.2, re-apply these substitutions
afterward via `sed` so we don't regress phase 1b. The Dotnet codegen targets
specifically need the `symLoad` and `wchar_t*` substitutions.

**D. ours-vs-theirs reformatting collisions**
`UI/Legacy/ZzzInterface.cpp` has 3,788 lines of branch-side reformatting +
log-refactor diff vs merge-base. Main's PR-#335 changes to that file are
small but land on lines the branch already touched. 3-way merge produces 13
conflict regions there that are 95% reformatting noise; the mitigation that
worked in phase 1b was: reset the file to branch HEAD, extract main's
*semantic* diff with `git diff -w c134d086..origin/main -- <file>`, and
apply only those substitutions surgically. Same trick applies to
`ZzzLodTerrain.cpp` (18 conflict regions), `ZzzOpenglUtil.cpp` (9), and
`UIWindows.cpp` (7).

**E. `_define.h` was consolidated into `Core/mu_define.h`**
Two of the 22 new Camera files (`Frustum.cpp`, `FrustumRenderer.cpp`) include
`"_define.h"` which doesn't exist on the branch — it was merged into
`Core/mu_define.h` during the SDL reorg. Phase 2b already rewrote those
includes; if any future file from main pulls in `_define.h`, do the same
rewrite.

**F. `g_Camera` visibility through include chains**
The new `g_Camera` is declared in `Camera/CameraState.h`. Files that need it
must reach that header transitively. On main, the canonical path is via
`RenderFX/ZzzOpenglUtil.h` which now `#include "Camera/CameraState.h"`. The
GM* maps and effect files don't directly include ZzzOpenglUtil.h on the
branch — verify each file's include chain when migrating, and add an
explicit `#include "Camera/CameraState.h"` if needed.

### 3.5 What actually landed (commit-by-commit)

The plan mostly held; we sequenced bottom-up (small users first) instead
of top-down (foundation first) so the build stayed green at every step,
which made each commit reviewable in isolation. Build was verified after
every commit with `cmake --build out/build/macos-arm64`.

1. **`68120dd0` LoginScene waypoint offsets** — additive port of main's
   `CameraMove.{cpp,h}` changes (LoginSceneCameraDefaults namespace,
   `g_LoginScene{Offset,Angle}*` runtime globals, `ApplyLoginSceneOffset`
   helper, `extern "C"` instance accessor). One conflict resolved: kept
   branch's two-line formatting of the `CameraVector2 toTarget{...}`
   declaration while inserting main's three new offset calls before it.
2. **`d4787dd8` Drop dead 3D-anaglyph globals + legacy `struct CameraState`** —
   removes `Camera3DFov`, `Camera3DRoll`, `g_CameraState`, and the legacy
   `struct CameraState` from `Camera/CameraUtility.h` (no live callers
   anywhere). `CameraDistance`/`Target` kept here pending §3.5 step 6.
3. **`b14bf725` Migrate eight small users** — 1-2 references each in
   `PersonalShopTitleImp`, `NewUIInGameShop`, `NewUIGoldBowmanLena`,
   `NewUIRegistrationLuckyCoin`, `CharMakeWin`, `SideHair`,
   `ZzzEffectPoint`, `ZzzEffectFireLeave`. The single architectural
   move in this commit was adding `#include "Camera/CameraState.h"` to
   `RenderFX/ZzzOpenglUtil.h` so every TU that already pulls in
   `ZzzOpenglUtil.h` gets `g_Camera` transitively (matches main's layout).
4. **`56a72c76` Migrate ten more single-ref users** — `ZzzInterface.cpp`,
   four `GM*.cpp` (the `CameraPosition[1] + 400.f` cull check),
   `NewUI3DRenderMng`, `UIWindows`, `SceneCommon`, `SceneManager`,
   plus dropping a stale local `extern float CameraViewFar` in
   `CharacterScene.cpp`.
5. **`80c808bf` ZzzObject + MainScene + LoginScene** — 6/8/14 refs each.
   Stale local externs (`extern float CameraAngle[3];` at the top of
   MainScene + three more in LoginScene) deleted along with the migration.
6. **`0bd4bbde` ZzzLodTerrain** — 26 refs across the heaviest user. The
   local-shadow gotcha in `CreateFrustrum2D` (locals named
   `CameraViewFar` / `Near` / `Target` shadow the globals inside a
   178-line block) was handled by renaming those locals to
   `localFar` / `localNear` / `localTarget` first, then bulk-substituting
   only the genuine global references. The two local
   `extern float CameraDistance{,Target};` lines at the top of the
   file dropped at the same time.
7. **`c3a02c0f` Final teardown** — the storage-owning translation units.
   `RenderFX/ZzzOpenglUtil.cpp` (60 substitutions + remove the eleven
   legacy storage definitions), `RenderFX/ZzzOpenglUtil.h` (drop ten
   externs), `Camera/CameraUtility.cpp` (the file we forgot earlier — 59
   substitutions + drop its top-of-file externs), `CameraUtility.h` (drop
   the last two externs), `Scenes/SceneCore.cpp` (drop the two
   `CameraDistance{,Target}` storage definitions),
   `Platform/PlatformGlobalStubs.cpp` (drop the non-Win32
   `g_fCameraCustomDistance` stub). Plus a CMake fix:
   `Camera/CameraState.cpp` moved from MUGame to MURenderFX so the test
   binaries (which link MURenderFX without MUGame) can resolve `g_Camera`.

After step 7, `grep -rE '\b(CameraPosition|CameraAngle|CameraMatrix|...|g_fCameraCustomDistance)\b'` across `src/source` returns only matches inside `g_Camera.X` field accesses or `state.X` parameter
accesses inside `Camera/CameraProjection.cpp`.

**Stub still owed.** `Camera/CameraProjection.cpp` calls
`gluPerspective` / `glViewport` / `glReadPixels` / `glGetFloatv` plus the
immediate-mode draw primitives that the SDL3 branch's `MuRenderer` no
longer exposes. `stdafx.h` has inline forwarders/no-ops for all of these
(see §2.1), so the camera framework compiles and links. The framework
itself isn't called yet — `OrbitalCamera`, `CameraManager`,
`FrustumRenderer` etc. exist but no scene routes through them. When that
wiring lands the stubs in `stdafx.h` need to be replaced with
MuRenderer-backed equivalents, otherwise the depth-buffer occlusion test
will misbehave and the frustum debug viz won't draw.

## 4. PR #335 — what's still pending

The big-rock migration (§3) is done. What remains is the polish stack
and the actual *wiring* of the new architecture. None of these block
each other — order is up to the next session.

### 4.1 New camera framework is unused

The 22-file Camera framework (`CameraManager`, `OrbitalCamera`,
`DefaultCamera`, `FreeFlyCamera`, `Frustum`, `FrustumRenderer`,
`CameraProjection`) compiles, links, and is ready — but **no scene calls
into it yet**. The branch's existing `CCameraMove` / `CameraUtility::MoveMainCamera()`
flow keeps driving the camera, with the legacy globals now backed by
`g_Camera.X` storage.

To activate the new architecture, scenes need to be migrated to call
`CameraManager::Instance().GetActiveCamera()->Update()` etc. instead of
the legacy flow. Main's PR #335 does this in `Scenes/MainScene.cpp`,
`Scenes/CharacterScene.cpp`, `Scenes/LoginScene.cpp`. This is a real
behaviour change — not just a refactor — so it deserves a careful commit
of its own, with smoke-testing of all three scenes (login → char select →
main world → orbital zoom) before declaring it done.

When that wiring lands, the GL stubs in `Main/stdafx.h` flagged in §2.1
(`glReadPixels`, `glGetFloatv`, immediate-mode draw primitives) need to
be replaced with MuRenderer-backed calls — otherwise
`CameraProjection::TestDepthBuffer` is permanently "unoccluded" and
`FrustumRenderer` draws nothing.

### 4.2 NewUIOptionWindow merge

Main's PR #335 reworks the option window: rounded volume / render
sliders, a `NewUIComboBox`-driven resolution dropdown processed before
the checkboxes/sliders, a windowed-mode toggle that picks consistent
window styles across entry points, and a HARDEN pass for option-window
config. The branch already has its own SDL3-flavoured option-window
edits (+432 LOC vs main's +528 LOC, both touching overlapping logic),
so this is a real 3-way merge. The new `NewUIComboBox.{cpp,h}` is
already in place from `c453fac6` and ready to be wired.

Files to merge: `UI/Windows/NewUIOptionWindow.{cpp,h}` plus the
upstream commits `ffc3e580`, `9d7d6b32`, `9f17a79c`, `a9b5b4dd`,
`6732c1e9`, `316ef1fa`, `0c5fb402`, `e8b15d35`, `e94adadc`.

### 4.3 $details overlay + per-pass timing

Wires `Utilities/FrameProfiler.h` (already added in `c453fac6`) into
the scene-render loop and the `$details` debug overlay. Upstream
commits: `2362457a` (per-pass timing), `7e5f0d5a` (camera mode + scene
visibility stats), `a0b7204c` (drop entity/triangle stats),
`efb5a283` (fix overlay crash when iterating object arrays),
`8861041f` (extend side view at wider aspects + surface build info).
Touches `Scenes/SceneManager.cpp` and a few other scenes.

### 4.4 DevEditor port (editor-only build)

Adds `src/MuEditor/UI/DevEditor/DevEditorUI.{cpp,h}` (NEW on main, 2
files) and edits `MuEditorCore.{cpp,h}`, `MuInputBlockerCore.cpp`,
`MuEditorUI.{cpp,h}`. The new files exercise the camera framework's
`extern "C"` accessors (`CameraManager_Instance`,
`GetOrbitalCameraInstance`, etc.) — those are already in place from
`7e9dbdba`. Only builds when `ENABLE_EDITOR=ON`. Upstream commits:
`a31ed451`, `b1f49dcd`, `69e754f9`, `aba6d484`, plus the smaller
follow-ups.

### 4.5 Per-map camera-overrides cleanup

Main's `889eb9f0` "Remove per-gameplay-map camera overrides" deletes
the per-map `CameraViewFar = ...` lines from a handful of GM* files
(`GM3rdChangeUp`, `GMBattleCastle`, `GMCrywolf1st`, `GMDoppelGanger2`,
`GMEmpireGuardian1`, `GMHellas`, `GMSwampOfQuiet`, `GM_Kanturu_3rd`,
`GM_Raklion`) so the unified camera config in `OrbitalCamera`'s default
profile drives everything. This is gameplay-visible — verify each map
still looks right before/after.

### 4.6 Trivial cleanups

- **Macro.txt deletion** — main deletes `src/bin/Data/Macro.txt`. Need
  to verify nothing on the branch still references it (the SDL3 input
  rework may or may not still consume it).
- **`.gitignore` additions** — main adds `docs/CODING_RULES.md` and
  `docs/reviews/`. Trivial cherry-pick, no semantic impact.
- **Edge-of-map terrain tile rendering** (`455f8034`) — clamps the
  bound to `TERRAIN_SIZE - T` so terrain tiles at the map edge render.
  Single-line fix in `ZzzLodTerrain.cpp` if the branch hasn't already
  picked this up; check first.

## 5. Reusable scripts from this session

The 3-way merge driver used for phase 1a / 2c-attempt is worth keeping
around. Re-create it as needed:

```bash
# Per-file 3-way merge driver. Pass branch_path<TAB>main_path tuples on stdin.
T=/tmp/3way; mkdir -p "$T"
while IFS=$'\t' read -r branch_path main_path; do
  safe=$(echo "${branch_path}" | tr '/' '_')
  git show "c134d086:${main_path}"     > "$T/${safe}.base"   2>/dev/null \
    || git show "c134d086:${branch_path}" > "$T/${safe}.base" 2>/dev/null
  git show "HEAD:${branch_path}"       > "$T/${safe}.ours"   2>/dev/null
  git show "origin/main:${main_path}"  > "$T/${safe}.theirs" 2>/dev/null
  cp "$T/${safe}.ours" "$T/${safe}.merged"
  if git merge-file -L ours -L base -L theirs \
       "$T/${safe}.merged" "$T/${safe}.base" "$T/${safe}.theirs"; then
    cp "$T/${safe}.merged" "$branch_path"; echo "OK       $branch_path"
  else
    cp "$T/${safe}.merged" "$branch_path"; echo "CONFLICT $branch_path"
  fi
done
```

Use with the path map in §3.3 (one tuple per line) to drive a fresh batch
3-way merge for the camera migration files.

## 6. Open questions surfaced during this session

- **Macro.txt deletion** (PR #335) — should the branch follow? Need to
  check whether the SDL3 work added any consumer of that data file.
- **`Camera3DFov` / `Camera3DRoll`** — main drops these entirely. Confirm
  no branch-side feature still depends on stereoscopic / anaglyph rendering
  before deleting from `Scenes/SceneCore.cpp`. (Branch has 1 reference each
  per the §3.1 table — likely the definition itself only.)
- **`Utilities/` vs `Core/` for `FrameProfiler.h`** — chose `Utilities/` to
  match main. If branch standardizes everything header-only into `Core/`
  later, this can be moved.
- **Network nuget 0.9.8 → 0.9.9 + `net9.0` → `net10.0`** — adopted from
  main in `d1bd1b09`. Verify the nuget package restore still resolves on
  CI; if not, downgrade in the .csproj is a one-line change.
