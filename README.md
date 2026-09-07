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

### Quick install (one command)

Open **Command Prompt** inside your project's `Plugins/` folder and run:

```cmd
curl -L -o fl.zip https://github.com/mihirbathani/fulcrum-lite/releases/download/v1.1.0-5.8/FulcrumLite-1.1.0-UE5.8.zip && tar -xf fl.zip && del fl.zip
```

`curl` and `tar` ship with Windows 10/11 — no extra tools needed. This drops a `FulcrumLite/` folder straight into `Plugins/`.

With Git you can instead clone this branch:

```cmd
git clone -b 5.8 https://github.com/mihirbathani/fulcrum-lite.git FulcrumLite
```

### Manual install

1. Download the 5.8 `.zip` from [Releases](../../releases/tag/v1.0.0-5.8) (or copy this plugin folder) into your project's `Plugins/` folder, e.g. `YourProject/Plugins/FulcrumLite/`.
2. Right-click your `.uproject` → **Generate Visual Studio project files**, then build — or just open the project and let the editor compile it.
3. Open the tool from **Tools → Fulcrum Lite**.

## Engine versions

This plugin is maintained per engine version on separate branches:

- [`5.6`](../../tree/5.6) — Unreal Engine 5.6
- [`5.8`](../../tree/5.8) — Unreal Engine 5.8

Check out the branch that matches your engine. `main` holds the shared source.

**You are on the `5.8` branch — built and tested against Unreal Engine 5.8.**

## Requirements

- Unreal Engine **5.8**
- A C++ toolchain (Visual Studio 2022) is required to build from source.

## Author

Made by **Mihir Bathani** — [www.mihirbathani.com](https://www.mihirbathani.com)

## License

Fulcrum Lite is proprietary software. See [LICENSE.txt](LICENSE.txt) for the full terms — a single-seat, non-redistributable license.

© 2026 Mihir Bathani. All Rights Reserved.
