# PR #329 Review Follow-ups

Tracking for the technical issues flagged on
[sven-n/MuMain#329](https://github.com/sven-n/MuMain/pull/329) (closed 2026-04-26).
Two reviewers: `gemini-code-assist[bot]` (COMMENTED) and `Mosch0512` (CHANGES_REQUESTED, then closed).

The PR was closed for size reasons (~1,188 files, +223k / −68k LOC), not for the
technical issues — those remain valid feedback for the working branch
`cross-platform-sdl-migration-merged`.

## Status of Mosch's nine issues

### Closed by recent CI work

#### #6 — Clang-only `-Wno-error=` flags fail under GCC
- **Fixed** in `a68c796c` — gates each clang-only flag with
  `$<$<CXX_COMPILER_ID:Clang,AppleClang>:...>`.
- **Also extended** the relaxation list to `MURenderFX` in `e6ccc557`.
- Open follow-up: Mosch suggested adding GCC-flavored relaxations the legacy
  code typically needs. Add if a future GCC build trips them:
  ```
  $<$<CXX_COMPILER_ID:GNU>:-Wno-error=address>
  $<$<CXX_COMPILER_ID:GNU>:-Wno-error=parentheses>
  $<$<CXX_COMPILER_ID:GNU>:-Wno-error=class-memaccess>
  $<$<CXX_COMPILER_ID:GNU>:-Wno-error=stringop-truncation>
  $<$<CXX_COMPILER_ID:GNU>:-Wno-error=format>
  $<$<CXX_COMPILER_ID:GNU>:-Wno-error=format-security>
  $<$<CXX_COMPILER_ID:GNU>:-Wno-error=array-bounds>
  ```
- Open follow-up: relaxations only apply to `MUGame`/`Main`/`MURenderFX`. If
  legacy `Zzz*` patterns trip GCC inside `MUData` or `MUCore`, extend there
  too.

#### #8 — `find_package(CURL REQUIRED)` blocks configure
- **Sidestepped** in `2aefc9ef` — `vcpkg.json` provides curl on Windows MSVC.
- **Mosch's preferred fix is different**: graceful no-op fallback like
  OpenSSL already has (`mu_encrypt_blob` falls back to identity at
  `src/CMakeLists.txt:298`). For CURL, on `find_package` failure define a
  stub `CURL::libcurl` interface library and have `ShopListManager` log
  "downloads disabled". Lets contributors without vcpkg/system curl
  clone-and-configure immediately.
- **Worth doing** because it makes the project bootstrappable in any
  configuration, not just the ones CI exercises. Same shape as the
  existing OpenSSL fallback.

#### #9 — Preset's `toolchainFile` overrides downstream vcpkg overrides
- **Sidestepped** in `2aefc9ef` — CI uses `VCPKG_CHAINLOAD_TOOLCHAIN_FILE`
  on the CMake CLI to chain-load `toolchain-x64.cmake` through
  `vcpkg.cmake`.
- Mosch classified this exact pattern as *"option 3 — ugly but
  non-breaking."* He prefers:
  - **Option 1** — move `toolchainFile` to a leaf preset
    (`windows-x64-default-toolchain`) so other leaves
    (`windows-x64-vcpkg`, `windows-x64-mingw`) can override freely.
  - **Option 2** — delete the toolchain files entirely; they only set
    `CMAKE_GENERATOR_PLATFORM` which Ninja Multi-Config ignores.
- **Affects CLion / Visual Studio users locally**, not CI. Worth fixing
  before anyone else opens the project on Windows in those IDEs.

### Real bugs CI hasn't reached yet (HIGHEST priority)

#### #1 — Incomplete logging migration
After `Story 7.10.1` deleted `CErrorReport` and `CmuConsoleDebug`, these
symbols are still referenced:
- `g_ConsoleDebug`
- `g_ErrorReport`
- `MCD_NORMAL`, `MCD_RECEIVE`, `MCD_ERROR`
- `Utilities/Log/muConsoleDebug.h`

Files Mosch named (non-exhaustive):
- `Network/WSclient.cpp` (lines 350, 354, 358, 467, 480, 482, 490, 534)
- `Scenes/MainScene.cpp` — `extern CErrorReport g_ErrorReport;` declared,
  never defined
- `Scenes/CharacterScene.cpp` — same `extern`, plus
  `#include "Utilities/Log/muConsoleDebug.h"`
- `Scenes/SceneManager.cpp` — `g_ErrorReport.Write(...)` (~603-604)
- `GameShop/NewUIInGameShop.cpp` — `g_ConsoleDebug.Write(...)` (~615, 705)

**Why CI hasn't caught it**: every CI run so far has failed before reaching
`MUGame`. Once configure + early targets pass, link of `MUGame` will have
undefined references on every compiler — language-level, not platform.

**Pattern to match**: `Gameplay/Items/PersonalShopTitleImp.cpp` already uses
`mu::log::Get("module")->info(...)` correctly.

**Enumerate scope**:
```bash
grep -rn 'g_ConsoleDebug\|g_ErrorReport\|MCD_NORMAL\|MCD_RECEIVE\|MCD_ERROR\|muConsoleDebug.h' src/source/
```

#### #4 — Lingering OpenGL immediate-mode calls
`Story 7.9.3` removed the GL backend, but several files still emit
immediate-mode draw calls. Symbols don't exist anywhere in the build:
- `RenderFX/ZzzEffectFireLeave.cpp:547,572,573,598` — `glColor3f`,
  `glPushMatrix`, `glTranslatef`, `glPopMatrix`
- `Gameplay/Characters/ZzzObject.cpp` — `glColor3f`/`glColor4f`/`glColor3fv`
  at ~25 sites between lines 786-7840
- `GameShop/NewUIInGameShop.cpp:85, 312-337` — `glColor4f`, `glMatrixMode`,
  `glPushMatrix`, `glLoadIdentity`, `glClear`, `glPopMatrix`

Same CI caveat as #1.

**Fix path**: route through `mu::GetRenderer()`. Already exposes
`SetMatrixMode`, `PushMatrix`, `LoadIdentity`, `MultMatrix`, `Translate`,
`Rotate`, `ClearDepthBuffer`, `SetViewport`. Likely needs:
- `SetVertexColor(r,g,b,a)` extension for the `glColor*` sites
- A debug-primitive helper for `glBegin(GL_QUADS)`-style geometry (no
  equivalent on the abstraction yet)

**Enumerate scope**:
```bash
grep -rn 'gl[A-Z][a-zA-Z]*[(]' src/source/ | grep -v 'glm::' | grep -v '//'
```

### MinGW-only — low priority (we standardized on MSVC for Windows)

#### #2 — SDL3 helpers in wrong `#ifdef` branch
`Platform/PlatformCompat.h` has `#ifdef _WIN32 ... #else ... #endif`
(lines 3 → 50 → 2285). SDL3 helpers sit inside the `#else` branch
(nested `#ifdef MU_ENABLE_SDL3 ...` at ~924-960), so they're invisible
to MinGW (which defines `_WIN32`):
- `extern char g_szSDLTextInput[32];`
- `extern bool g_bSDLTextInputReady;`
- `void MuStartTextInput();`, `void MuStopTextInput();`
- `inline wchar_t MuSdlUtf8NextChar(const char*&);`

**Fix**: lift the SDL3 block out of the platform `#ifdef` and gate only
on `MU_ENABLE_SDL3`.

**Worth verifying anyway**: same root cause may affect `mu_narrow_path`
used by `Data/GlobalText.h`'s template via GCC two-phase name lookup —
could bite plain Linux GCC, not just MinGW. Reproduce before deferring.

#### #3 — POSIX-only types/headers in unconditional code
- `RenderFX/ZzzBMD.cpp:1022` — `static_cast<u_char>(...)`. Fix: `unsigned char`.
- `Network/WSclient.cpp:540-550` — `u_int64`. Fix: `uint64_t`.
- `Core/MuSystemInfo.cpp:13` — unconditional `#include <sys/utsname.h>`
  + `uname()`. Fix: wrap in `#ifndef _WIN32`, fall back to
  `RtlGetVersion` or compile-time identifier on Windows.

#### #5 — `MuPlatform::CreateWindow` Win32 macro collision
`<windows.h>` defines `CreateWindow → CreateWindowW`/`A`. Class
declaration becomes garbage on MinGW.

**Fix**: either `#undef CreateWindow`/`A`/`W` at top of `MuPlatform.h`,
or rename the method (`CreateGameWindow` / `CreateMainWindow`).

#### #7 — Stale `../`-prefixed `#include` paths
Pre-restructure paths still in:
- `Scenes/SceneManager.cpp`, `MainScene.cpp`, `CharacterScene.cpp`,
  `LoginScene.cpp`, `SceneCommon.cpp`
- `Network/WSclient.cpp` — `Guild/GuildCache.h`, `MUHelper/MuHelper.h`
- `UI/Legacy/ZzzInterface.cpp`
- `UI/Windows/HUD/NewUIMiniMap.cpp`
- `Gameplay/Items/PersonalShopTitleImp.cpp` — `Guild/UIGuildInfo.h`
- `RenderFX/ZzzOpenglUtil.cpp` — uses `ShellExecute` without
  `<shellapi.h>`

Related: `Data/GlobalText.h` should `#include "Platform/PlatformCompat.h"`
directly rather than rely on transitive includes — GCC two-phase name
lookup needs `mu_narrow_path` visible.

## Gemini bot inline comments

All previously deferred-style nits — none are blockers.

| File | Line | Comment |
|------|------|---------|
| `Core/BaseCls.h` | 121 | `CList`: missing Rule-of-Three. Add `= delete` copy ops since it manages raw pointers. |
| `Core/BaseCls.h` | 832 | `CDimension`: same — manages `m_pData` raw pointer. |
| `Core/IniFile.h` | 115 | `std::locale("")` env-dependent. **Already addressed** in `cb96217c` (UTF-8 facet). |
| `src/CMakeLists.txt` | 277 | `include_directories` global → prefer `target_include_directories(MUCommon INTERFACE ...)`. |
| `src/CMakeLists.txt` | 430 | `file(GLOB_RECURSE)` discouraged for source lists. |
| `CMakeLists.txt` (root) | 131 | Same `file(GLOB)` concern on shader blobs. **Mitigated** in `cb96217c` (`CONFIGURE_DEPENDS`). |

## Mosch's CI-coverage observation (worth flagging)

> Worth flagging up front: the existing `.github/workflows/ci.yml` covers four
> jobs - Quality Gates, Linux Native (system GCC), macOS Native (Brew clang),
> and Windows Native (`windows-latest` + MSYS2 **MinGW-w64 x86_64**, *not*
> MSVC, despite the `CMakePresets.json` `windows-base` description saying
> "MSVC"). The toolchain files don't actually force MSVC - they trust whatever
> compiler is on PATH, and the CI installs MinGW.

Implication: **MSVC has had zero CI coverage on `main` historically.**
Anyone building from Visual Studio is the first to hit MSVC-specific issues.

Our `2aefc9ef` switched the Windows job from MSYS2/MinGW to MSVC + vcpkg —
that's a meaningful CI coverage improvement, but as of this writing the
Windows job hasn't reached green yet. The "MSVC" wording in the preset
description is now accurate.

## Suggested research order

1. **Verify #1 and #4 scope** — run the grep commands above. If hits exist,
   plan a sweep before the Linux/MSVC build can plausibly reach `MUGame`.
2. **#8 graceful CURL fallback** — small CMake change, makes the project
   bootstrappable without vcpkg, matches OpenSSL precedent.
3. **#9 preset cleanup** — small CMake change, unblocks local CLion/VS
   developers. Likely option 2 (delete toolchain files) is simplest.
4. **#2 verification** — confirm whether `mu_narrow_path` two-phase
   name lookup actually fires on Linux GCC. If yes, promote to high
   priority. If MinGW-only, defer.
5. **#3, #5, #7** — only if MinGW becomes a supported config again.
