#include "vehicle_mask.hpp"
#include <opencv2/opencv.hpp>
#include <filesystem>
#include <cstdlib>
#include <iostream>

using namespace cv;

namespace {
    bool fileExists(const std::string& p) { return std::filesystem::exists(p); }

    static int autoWhiteThreshold(const cv::Mat& img)
    {
        int cx = img.cols / 2;
        int w = std::min(40, std::max(1, std::min(cx - 1, img.cols - cx - 1)));
        int h = std::max(1, img.rows / 10);
        cv::Rect topR(cx - w, 0, 2*w + 1, h);
        cv::Rect botR(cx - w, img.rows - h, 2*w + 1, h);
        cv::Mat gt, gb; cv::cvtColor(img(topR), gt, cv::COLOR_BGR2GRAY); cv::cvtColor(img(botR), gb, cv::COLOR_BGR2GRAY);
        double mt = cv::mean(gt)[0], mb = cv::mean(gb)[0];
        int thr = int(std::min(mt, mb) - 5.0);
        return std::clamp(thr, 200, 255);
    }

    // Heuristic mask generator with settings (from file path)
    bool heuristicMask(const std::string& inPath, const std::string& outPath, const MaskSettings& s)
    {
        Mat img = imread(inPath);
        if (img.empty()) return false;
        Mat mask;
        if (!computeVehicleMaskMat(img, mask, s)) return false;
        return imwrite(outPath, mask);
    }
}

bool generateVehicleMask(const std::string& inPath, const std::string& outPath)
{
    // Prefer external SAM2 script if present (no custom settings)
    const char* scriptEnv = std::getenv("SAM2_MASK_SCRIPT");
    std::string scriptPath = scriptEnv ? std::string(scriptEnv) : std::string("scripts/sam2_vehicle_mask.py");
    if (fileExists(scriptPath))
    {
        // Ensure output directory exists
        std::filesystem::create_directories(std::filesystem::path(outPath).parent_path());
        std::string cmd = "python3 \"" + scriptPath + "\" --input \"" + inPath + "\" --output \"" + outPath + "\"";
        int rc = std::system(cmd.c_str());
        if (rc == 0 && fileExists(outPath)) return true;
        std::cerr << "[generateVehicleMask] SAM2 script failed (rc=" << rc << ") or output missing. Falling back to heuristic mask.\n";
    }
    else
    {
        std::cerr << "[generateVehicleMask] SAM2 script not found at '" << scriptPath << "'. Using heuristic fallback.\n";
    }
    return heuristicMask(inPath, outPath, MaskSettings{});
}

bool generateVehicleMask(const std::string& inPath, const std::string& outPath, const MaskSettings& settings)
{
    const char* scriptEnv = std::getenv("SAM2_MASK_SCRIPT");
    std::string scriptPath = scriptEnv ? std::string(scriptEnv) : std::string("scripts/sam2_vehicle_mask.py");
    if (fileExists(scriptPath))
    {
        std::filesystem::create_directories(std::filesystem::path(outPath).parent_path());
        auto toStr = [](int v){ return std::to_string(v); };
        std::string cmd = std::string("python3 \"") + scriptPath + "\"" +
            " --input \"" + inPath + "\"" +
            " --output \"" + outPath + "\"" +
            " --canny-low " + toStr(settings.cannyLow) +
            " --canny-high " + toStr(settings.cannyHigh) +
            " --kernel " + toStr(std::max(1, settings.morphKernel|1)) +
            " --dilate " + toStr(settings.dilateIters) +
            " --erode " + toStr(settings.erodeIters) +
            (settings.useWhiteCycAssist ? std::string(" --white-cyc") : std::string()) +
            " --white-thr " + toStr(settings.whiteThreshold) +
            " --min-area " + toStr(settings.minArea) +
            " --feather " + toStr(settings.featherRadius) +
            (settings.invert ? std::string(" --invert") : std::string());
        int rc = std::system(cmd.c_str());
        if (rc == 0 && fileExists(outPath)) return true;
        std::cerr << "[generateVehicleMask] SAM2 script failed (rc=" << rc << ") or output missing. Falling back to heuristic mask.\n";
    }
    else
    {
        std::cerr << "[generateVehicleMask] SAM2 script not found at '" << scriptPath << "'. Using heuristic fallback.\n";
    }
    return heuristicMask(inPath, outPath, settings);
}

bool computeVehicleMaskMat(const cv::Mat& img, cv::Mat& outMask, const MaskSettings& s)
{
    if (img.empty()) return false;
    cv::Mat gray; cv::cvtColor(img, gray, cv::COLOR_BGR2GRAY);
    cv::medianBlur(gray, gray, 5);

    // White cyc assistance: identify non-white regions
    cv::Mat nonWhiteMask;
    if (s.useWhiteCycAssist)
    {
        int wthr = (s.whiteThreshold >= 0 && s.whiteThreshold <= 255) ? s.whiteThreshold : autoWhiteThreshold(img);
        cv::threshold(gray, nonWhiteMask, wthr, 255, cv::THRESH_BINARY_INV);
    }

    // Edges
    cv::Mat edges; cv::Canny(gray, edges, std::min(s.cannyLow, s.cannyHigh), std::max(s.cannyLow, s.cannyHigh));

    // Combine
    cv::Mat mask;
    if (!nonWhiteMask.empty()) cv::bitwise_or(edges, nonWhiteMask, mask);
    else mask = edges;

    // Morphology
    int k = std::max(1, s.morphKernel | 1); // force odd
    cv::Mat kernel = cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(k, k));
    if (s.dilateIters > 0) cv::dilate(mask, mask, kernel, cv::Point(-1,-1), s.dilateIters);
    if (s.erodeIters > 0) cv::erode(mask, mask, kernel, cv::Point(-1,-1), s.erodeIters);
    cv::threshold(mask, mask, 1, 255, cv::THRESH_BINARY);

    // Remove small components
    if (s.minArea > 0)
    {
        cv::Mat labels, stats, centroids;
        int n = cv::connectedComponentsWithStats(mask, labels, stats, centroids, 8, CV_32S);
        cv::Mat filtered = cv::Mat::zeros(mask.size(), CV_8U);
        for (int i = 1; i < n; ++i)
        {
            if (stats.at<int>(i, cv::CC_STAT_AREA) >= s.minArea)
            {
                filtered.setTo(255, labels == i);
            }
        }
        mask = filtered;
    }

    // Fill holes: flood fill from border on inverse and invert back
    {
        cv::Mat inv; cv::bitwise_not(mask, inv);
        cv::Mat ff = cv::Mat::zeros(inv.rows + 2, inv.cols + 2, CV_8U);
        cv::floodFill(inv, ff, cv::Point(0,0), cv::Scalar(0));
        cv::bitwise_not(inv, inv);
        cv::bitwise_or(mask, inv, mask);
    }

    // Feather edges then binarize
    if (s.featherRadius > 0)
    {
        int rr = std::max(1, s.featherRadius * 2 + 1);
        cv::GaussianBlur(mask, mask, cv::Size(rr, rr), 0);
        cv::threshold(mask, mask, 127, 255, cv::THRESH_BINARY);
    }

    // Invert if requested (object black, background white)
    if (s.invert) cv::bitwise_not(mask, mask);

    outMask = mask;
    return true;
}
