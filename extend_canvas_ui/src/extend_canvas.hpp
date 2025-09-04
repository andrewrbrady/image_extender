/*========================  extend_canvas.hpp  ========================

   A comprehensive interface for the canvas-extension utility.
   --------------------------------------------------------------------
   • declare `extendCanvas()` so any UI (Qt, wxWidgets, CLI, …) can call it
   • supports foreground detection, white threshold, padding, and resizing
   • no OpenCV headers leak into dependers
   • compile-time option EXTEND_CANVAS_STANDALONE enables a tiny CLI

   Build example (GUI + CLI):
     g++ -std=c++17 -O2 -Wall -DEXTEND_CANVAS_STANDALONE \
         extend_canvas.cpp `pkg-config --cflags --libs opencv4` -o extend_canvas
=====================================================================*/
#pragma once
#include <string>

/**
 * @brief Extends an image canvas using intelligent foreground detection and padding.
 *        Supports white threshold detection, padding, and final resizing while preserving
 *        aspect ratio, then writes the result alongside the original file.
 *
 * @param inPath      Absolute or relative path to the source image.
 * @param reqW        Requested canvas width  in pixels. Pass 0 to derive from reqH.
 * @param reqH        Requested canvas height in pixels. Pass 0 to derive from reqW.
 * @param whiteThr    White threshold for foreground detection. Pass -1 for auto-detection.
 * @param padPct      Padding percentage around detected foreground (default 0.05 = 5%).
 * @param requestedW  Final output width (optional resize). Pass -1 to skip.
 * @param requestedH  Final output height (optional resize). Pass -1 to skip.
 * @return true on success, false on failure (bad path, I/O error, etc.).
 */
bool extendCanvas(const std::string &inPath,
                  int reqW = 0,
                  int reqH = 0,
                  int whiteThr = -1,
                  double padPct = 0.05,
                  int requestedW = -1,
                  int requestedH = -1,
                  int blurRadius = 0);
