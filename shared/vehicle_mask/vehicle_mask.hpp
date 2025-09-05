#pragma once
#include <string>
#include "models/MaskSettings.hpp"
namespace cv { class Mat; }

// Generates a black-and-white vehicle mask for the input image and writes it to outPath (PNG recommended).
// This attempts to use an external SAM2 Python script if available; otherwise, it falls back to a simple
// OpenCV heuristic mask so the pipeline still works. Returns true on success.
bool generateVehicleMask(const std::string& inPath, const std::string& outPath);
bool generateVehicleMask(const std::string& inPath, const std::string& outPath, const MaskSettings& settings);

// Compute a vehicle mask directly from an input Mat (no disk I/O). Result is CV_8U {0,255}.
bool computeVehicleMaskMat(const cv::Mat& img, cv::Mat& outMask, const MaskSettings& settings);
