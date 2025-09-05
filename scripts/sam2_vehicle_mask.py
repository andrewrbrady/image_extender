#!/usr/bin/env python3
"""
SAM2 Vehicle Mask Generator
---------------------------
CLI: python3 scripts/sam2_vehicle_mask.py --input <image> --output <mask.png>

This script is a placeholder entry point to integrate Meta's SAM 2.0 for
segmenting vehicles and producing a black/white mask. It expects a Python
environment with the appropriate SAM2 dependencies installed and model
weights accessible via an environment variable or configured path.

Environment variables:
  SAM2_MODEL            Path to SAM2 model weights or model name

Notes:
  - If SAM2 or its dependencies are not installed, this script exits with
    a non-zero status so the C++ app can fall back to a heuristic mask.
  - Replace the TODO block below with real SAM2 inference code.
"""
import argparse
import os
import sys
import cv2
import numpy as np

def main() -> int:
    p = argparse.ArgumentParser()
    p.add_argument('--input', required=True)
    p.add_argument('--output', required=True)
    # Post-processing / heuristic args (kept in sync with C++ MaskSettings)
    p.add_argument('--canny-low', type=int, default=50)
    p.add_argument('--canny-high', type=int, default=150)
    p.add_argument('--kernel', type=int, default=7)
    p.add_argument('--dilate', type=int, default=2)
    p.add_argument('--erode', type=int, default=0)
    p.add_argument('--white-cyc', action='store_true', default=False)
    p.add_argument('--white-thr', type=int, default=-1)
    p.add_argument('--min-area', type=int, default=5000)
    p.add_argument('--feather', type=int, default=0)
    p.add_argument('--invert', action='store_true', default=False)
    args = p.parse_args()

    # Quick dependency check: allow graceful fallback upstream
    try:
        # TODO: replace with actual SAM2 imports, e.g.:
        # from sam2 import SamPredictor, sam2_model_from_cfg
        # import torch
        have_sam2 = bool(os.environ.get('SAM2_MODEL'))
    except Exception as e:
        print(f"SAM2 import error: {e}", file=sys.stderr)
        return 2

    if not have_sam2:
        print("SAM2 not configured (set SAM2_MODEL).", file=sys.stderr)
        return 3

    # Load input
    img = cv2.imread(args.input)
    if img is None:
        print("Failed to read input image", file=sys.stderr)
        return 4

    # TODO: Implement real SAM2 inference targeting 'vehicle' class.
    # For now, reproduce the same heuristic/params as C++ for consistent results.
    gray = cv2.cvtColor(img, cv2.COLOR_BGR2GRAY)
    gray = cv2.medianBlur(gray, 5)
    lo, hi = min(args.canny_low, args.canny_high), max(args.canny_low, args.canny_high)
    edges = cv2.Canny(gray, lo, hi)
    k = max(1, args.kernel | 1)
    ker = cv2.getStructuringElement(cv2.MORPH_ELLIPSE, (k, k))
    if args.dilate > 0:
        edges = cv2.dilate(edges, ker, iterations=args.dilate)
    if args.erode > 0:
        edges = cv2.erode(edges, ker, iterations=args.erode)
    if args.white_cyc:
        cx = img.shape[1] // 2
        w = min(40, max(1, min(cx-1, img.shape[1]-cx-1)))
        h = max(1, img.shape[0] // 10)
        tR = (slice(0, h), slice(cx-w, cx+w+1))
        bR = (slice(img.shape[0]-h, img.shape[0]), slice(cx-w, cx+w+1))
        mt = np.mean(gray[tR])
        mb = np.mean(gray[bR])
        auto_thr = int(max(200, min(255, min(mt, mb) - 5)))
        thr_use = args.white_thr if 0 <= args.white_thr <= 255 else auto_thr
        non_white = (gray < thr_use).astype(np.uint8) * 255
        edges = np.maximum(edges, non_white)
    mask = (edges > 0).astype(np.uint8) * 255
    if args.min_area > 0:
        num, labels, stats, _ = cv2.connectedComponentsWithStats(mask, connectivity=8)
        keep = np.zeros(mask.shape, dtype=np.uint8)
        for i in range(1, num):
            if stats[i, cv2.CC_STAT_AREA] >= args.min_area:
                keep[labels == i] = 255
        mask = keep
    inv = cv2.bitwise_not(mask)
    ff = np.zeros((inv.shape[0]+2, inv.shape[1]+2), dtype=np.uint8)
    cv2.floodFill(inv, ff, (0,0), 0)
    inv = cv2.bitwise_not(inv)
    mask = cv2.bitwise_or(mask, inv)
    if args.feather > 0:
        rr = max(1, args.feather*2 + 1)
        mask = cv2.GaussianBlur(mask, (rr, rr), 0)
        _, mask = cv2.threshold(mask, 127, 255, cv2.THRESH_BINARY)
    if args.invert:
        mask = cv2.bitwise_not(mask)

    os.makedirs(os.path.dirname(args.output) or '.', exist_ok=True)
    if not cv2.imwrite(args.output, mask):
        print("Failed to write mask", file=sys.stderr)
        return 5
    return 0

if __name__ == '__main__':
    sys.exit(main())
