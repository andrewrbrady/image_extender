#pragma once
#include <opencv2/opencv.hpp>

namespace util {

// Estimate a white threshold by sampling bright regions near top/bottom center
int centerSampleThreshold(const cv::Mat& img, int stripeH = 20, int stripeW = 40);

// Collapse rows to find top/bottom of non-white foreground
bool findForegroundBounds(const cv::Mat& img, int& top, int& bot, int whiteThr);

// Collapse cols to find left/right of non-white foreground
bool findForegroundBoundsX(const cv::Mat& img, int& left, int& right, int whiteThr);

}

