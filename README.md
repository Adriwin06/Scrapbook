# Scrapbook

Scrapbook is a native Qt 6 desktop front-end for `libatlas`.

This repo now owns the application layer. `libatlas` lives here as a git submodule under [`third_party/libatlas`](third_party/libatlas), while Scrapbook provides:

- a modern Qt Quick desktop UI for running the fixture workflow
- a native `scrapbook_pipeline` CLI that replaces the old Python UI wrapper/orchestrator
- review tooling for unresolved duplicate groups and logical texture decisions

## Layout

- `apps/scrapbook/`: Qt Quick desktop application
- `tools/scrapbook_pipeline/`: native pipeline/orchestration CLI
- `third_party/libatlas/`: upstream library and `libatlas_tool` submodule

## Build

Requirements:

- CMake 3.27+
- Qt 6.6+ with:
  - `QtCore`
  - `QtGui`
  - `QtQml`
  - `QtQuick`
  - `QtQuickControls2`
  - Qt Image Formats plugins for DDS/TGA input support
- A C++20-capable compiler

Clone with submodules:

```bash
git clone --recurse-submodules git@github.com:Adriwin06/Scrapbook.git
```

If you already cloned it:

```bash
git submodule update --init --recursive
```

Configure and build:

```bash
cmake -S . -B build
cmake --build build --config Debug
```

On Windows, the easiest way is to use the repo scripts instead of setting up the toolchain manually every time:

```powershell
.\build.cmd
.\launch.cmd
.\package.cmd
```

Those scripts:

- try `SCRAPBOOK_QT_ROOT`, `Qt6_DIR`, `CMAKE_PREFIX_PATH`, `qmake`/`windeployqt` on `PATH`, then finally auto-detect a Qt kit under `C:\Qt`
- force the Visual Studio 2022 x64 generator
- rebuild if the existing `build/` directory was created with an incompatible generator
- run `windeployqt` so the app can launch directly from the build output
- can package a self-contained deployed runtime into `dist/` for another PC

That produces, in the same runtime output folder:

- `scrapbook`
- `scrapbook_pipeline`
- `libatlas_tool`

## Run

From the build output directory:

```bash
./scrapbook
```

The UI runs `scrapbook_pipeline`, which in turn drives `libatlas_tool` and writes the same review artifacts that the old Tk UI edited:

- `fixture_logical_store/review_candidates/review_groups.json`
- per-group `group.json`
- per-group `decision.json`

## VS Code

The repo now includes:

- [CMakePresets.json](CMakePresets.json)
- [.vscode/settings.json](.vscode/settings.json)
- [.vscode/tasks.json](.vscode/tasks.json)
- [.vscode/launch.json](.vscode/launch.json)

Recommended workflow in VS Code:

1. Open the repo root.
2. Let `CMake Tools` use the `debug-msvc` preset.
3. Use `Terminal -> Run Build Task` to run `Build + Deploy (Debug)`.
4. Press `F5` and choose `Scrapbook (Debug)` to build, deploy Qt runtime files, and launch the app.

If you want a portable runtime bundle for another PC, run the `Package (Release)` task or:

```powershell
.\package.cmd
```

For one-click local launching outside VS Code, use:

```powershell
.\launch.cmd
```

## Machine-Local Qt Setup

Repo-tracked presets no longer hardcode a Qt install path.

If you want `CMake Tools` configure buttons to work with a custom Qt location on your machine, create a local `CMakeUserPresets.json` from [CMakeUserPresets.json.example](CMakeUserPresets.json.example) and set your own `Qt6_DIR` / `CMAKE_PREFIX_PATH` there. That file should stay local and not be committed.

## Current Scope

The native app covers the desktop review workflow and native pipeline orchestration path. It is structured so more of the old fixture logic can continue moving out of `libatlas` cleanly without putting application concerns back into the library repo.
