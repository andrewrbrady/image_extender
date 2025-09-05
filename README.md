Extend Canvas — wxWidgets UI

This repository now uses a wxWidgets-based UI as the supported desktop application. The previous Qt-based UI (`extend_canvas_ui`) has been removed in favor of a single UI stack to simplify maintenance and builds.

What changed
- Default and only maintained UI is under `extend_canvas_wx/`.
- Common processing code lives under `shared/extend_canvas/` and shared models under `shared/include/`.
- The former Qt UI sources have been removed. If you need the last Qt-based version, use the last commit before this change in your Git history.

Build (local)
- Prereqs: OpenCV, wxWidgets 3.2+.
- macOS (Homebrew): `brew install opencv wxwidgets`
- Ubuntu: `sudo apt-get update && sudo apt-get install -y libopencv-dev libwxgtk3.2-dev build-essential cmake`

Build (one command from repo root)
- `make`
  - Optional: `make CMAKE_ARGS="-DwxWidgets_CONFIG_EXECUTABLE=$(which wx-config)"`

Output binaries
- wx app: `build/extend_canvas_wx/extend_canvas_wx` (or `.app` on macOS)
- matte generator: `build/apps/matte_generator/matte_generator`
- extend canvas CLI: `build/apps/extend_canvas_cli/extend_canvas_cli`

New: Modes and Vehicle Mask (SAM2)
- The UI now supports multiple modes via a "Mode" selector in the left panel.
  - Extend Canvas: original behavior for smart canvas extension (default output: `extended_images/`).
  - Vehicle Mask (SAM2): generates a black/white mask of the vehicle (default output: `masks/`).

Vehicle Mask Integration
- The app looks for a script at `scripts/sam2_vehicle_mask.py` (or `SAM2_MASK_SCRIPT` env var) to run SAM2.
- If the script or SAM2 dependencies are not available, it falls back to a heuristic OpenCV mask so the pipeline still runs.
- To enable SAM2:
  1. Create/activate a Python environment with SAM2 installed.
  2. Set `SAM2_MODEL` to point to your model weights or identifier.
  3. Optionally set `SAM2_MASK_SCRIPT` to the absolute path of your script (defaults to `scripts/sam2_vehicle_mask.py`).
  4. Implement the TODO in `scripts/sam2_vehicle_mask.py` with real SAM2 inference.


Deprecation note
- The Qt UI is no longer built or maintained. The underlying processing logic was extracted into `shared/` for reuse by the wx UI and any future CLIs or tools.
