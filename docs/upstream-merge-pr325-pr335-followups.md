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
| **#335** | 3d-camera-rework (Mosch0512) | `f1ffa170` | **Partially ported** — framework in, wiring pending |

## 2. Commits added this session (oldest → newest)

```
d1bd1b09  build(dotnet): port packet-enum codegen + 0.9.9 bump from main (PR #325)
e2880fd2  refactor(packets): adopt typed packet enums at call-sites (PR #325)
ebd0704e  refactor(camera): consolidate camera code under src/source/Camera/
7e9dbdba  feat(camera): add modular camera architecture from main (PR #335)
c453fac6  feat(ui,profiler): add NewUIComboBox + FrameProfiler from main (PR #335)
```

The user verified `d1bd1b09` + `e2880fd2` (Phase 1, PR #325) build and run.
The three Phase-2 commits (`ebd0704e` + `7e9dbdba` + `c453fac6`) add the new
framework files alongside the existing layout and should still build because
no caller has been switched over yet — they form unreferenced static-library
content until the migration in §3 lands.

## 3. PR #335 — what's left: the camera-globals migration

This is the next concrete chunk of work. PR #335 deletes 12 legacy camera
globals and replaces them with fields on the new `g_Camera` instance
(`CameraState`, defined in `src/source/Camera/CameraState.cpp`, declared in
`Camera/CameraState.h` which is now pulled in by `RenderFX/ZzzOpenglUtil.h`
on main).

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

### 3.5 Recommended commit shape for the migration

Build + smoke-test between each:

1. **Foundation (one commit, will break build → triggers steps 2-5):**
   - Take main's `RenderFX/ZzzOpenglUtil.h` (drops 8 externs, adds `Camera/CameraState.h` include)
   - Take main's `RenderFX/ZzzOpenglUtil.cpp`
   - Take main's `Camera/CameraMove.{cpp,h}` and `Camera/CameraUtility.{cpp,h}`
   - Drop legacy global definitions from `Scenes/SceneCore.cpp:105-111`
2. **Scenes:** the eight `Scenes/*.cpp` files
3. **Render core:** `World/ZzzLodTerrain.{cpp,h}` (handle local-shadow rename),
   `Gameplay/Characters/ZzzObject.cpp`,
   `UI/Framework/NewUI3DRenderMng.{cpp,h}`
4. **UI tail:** `UI/Legacy/UIWindows.cpp`, `UI/Legacy/CharMakeWin.cpp`,
   `UI/Legacy/ZzzInterface.cpp`, smaller UI files
5. **GM maps + effects:** four GM files + `SideHair`, `ZzzEffectPoint`,
   `ZzzEffectFireLeave`
6. **Polish stack** (multiple small commits): NewUIOptionWindow merge,
   $details overlay edits (now that `Utilities/FrameProfiler.h` is in),
   DevEditor port, slider-rounding fixes, build-info surfacing,
   FrustumCache aspect tracking, etc.

## 4. PR #335 — independent slices not yet ported

These don't depend on the §3 migration but were skipped this session for
scope reasons. Tackle independently when convenient:

- **DevEditor port:** `src/MuEditor/UI/DevEditor/DevEditorUI.{cpp,h}` (NEW
  on main, 2 files) + edits to `MuEditorCore.{cpp,h}`,
  `MuInputBlockerCore.cpp`, `MuEditorUI.{cpp,h}`. Builds only when
  `ENABLE_EDITOR` is on.
- **Macro.txt deletion:** main deletes `src/bin/Data/Macro.txt`. Verify
  whether anything on the branch still references it before deleting.
- **`.gitignore` additions** (main adds `docs/CODING_RULES.md` and
  `docs/reviews/`) — trivial cherry-pick, no semantic impact.
- **Per-pass timing in $details overlay:** wires
  `Utilities/FrameProfiler.h` (already added in `c453fac6`) into the
  scene-render loop. Touches `SceneManager.cpp` and a couple of other
  scenes, so usually folds in with phase 2c step 6 above.

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
