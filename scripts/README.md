SAM2 Vehicle Mask Integration
============================

The app can call an external Python script to generate vehicle masks using SAM2.

- Default script path: `scripts/sam2_vehicle_mask.py`
- Override path via env var: `SAM2_MASK_SCRIPT=/abs/path/to/your_script.py`
- Indicate SAM2 model via env var: `SAM2_MODEL=/path/to/weights_or_model_id`

CLI arguments expected by the script (kept in sync with the C++ UI):
- `--input <image>` — input image path
- `--output <mask.png>` — output mask (PNG recommended)
- `--canny-low <int>` `--canny-high <int>` — edge thresholds
- `--kernel <odd>` — morphology kernel size (odd)
- `--dilate <int>` `--erode <int>` — iterations for dilation/erosion
- `--white-cyc` `--white-thr <int>` — enable white cyc assist and optional fixed threshold (-1 = auto)
- `--min-area <int>` — remove small connected components
- `--feather <int>` — blur radius for mask then re-binarize
- `--invert` — invert final mask

Notes
- If SAM2 or dependencies aren’t installed, the script should exit non-zero. The C++ app will fall back to a heuristic mask.
- You can replace the heuristic in the script with your actual SAM2 inference code and keep the same post-processing parameters for consistency with the UI preview.

