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

Build steps
1. `cd extend_canvas_wx`
2. `cmake -S . -B build -DCMAKE_BUILD_TYPE=Release`
3. `cmake --build build --config Release`

Deprecation note
- The Qt UI is no longer built or maintained. The underlying processing logic was extracted into `shared/` for reuse by the wx UI and any future CLIs or tools.

