# Fulcrum Lite — Pivot Manager for Unreal Engine

**Fulcrum Lite** lets you reposition a Static Mesh's pivot point directly inside the Unreal Editor — one click, no switching to Modeling Mode. It's a streamlined version of [Fulcrum](https://www.mihirbathani.com) that keeps just the essentials.

Changing a pivot the native way means enabling Modeling Mode, finding *XForm → Edit Pivot*, dragging the gizmo, and clicking Accept — every single time. Fulcrum Lite puts it in one dockable panel.

## Features

- One-click pivot presets via a visual box picker — center, edges, and corners
- The mesh always stays in place — only the pivot moves
- Collision shapes and sockets shift **with** the pivot
- Multi-select support — fix a batch of meshes at once
- Reset to original pivot
- Full Undo / Redo
- Shared-mesh safety check (warns before changing a mesh used elsewhere)
- Native Unreal editor UI

## What's different from full Fulcrum

Fulcrum Lite drops the advanced tools — non-destructive mode, custom move/rotate, group pivots, depth (front/back) presets, copy/paste pivot, match pivot, and pivot-to-world-origin — for a simpler, focused workflow.

## Installation

1. Copy this plugin into your project's `Plugins/` folder, e.g. `YourProject/Plugins/FulcrumLite/`.
2. Right-click your `.uproject` → **Generate Visual Studio project files**, then build — or just open the project and let the editor compile it.
3. Open the tool from **Tools → Fulcrum Lite**.

## Engine versions

This plugin is maintained per engine version on separate branches:

- [`5.6`](../../tree/5.6) — Unreal Engine 5.6
- [`5.8`](../../tree/5.8) — Unreal Engine 5.8

Check out the branch that matches your engine. `main` holds the shared source.

## Requirements

- Unreal Engine **5.6** or **5.8** (see the branch matching your version)
- A C++ toolchain (Visual Studio 2022) is required to build from source.

## Author

Made by **Mihir Bathani** — [www.mihirbathani.com](https://www.mihirbathani.com)
