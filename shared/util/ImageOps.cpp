#include "util/ImageOps.hpp"
#include <algorithm>

namespace util {

int centerSampleThreshold(const cv::Mat& img, int stripeH, int stripeW)
{
    int cx = img.cols / 2;
    int w = std::min({ stripeW, std::max(1, cx - 1), std::max(1, img.cols - cx - 1) });
    int h = std::min(stripeH, std::max(1, img.rows / 10));
    cv::Rect topR(cx - w, 0, 2*w + 1, h);
    cv::Rect botR(cx - w, img.rows - h, 2*w + 1, h);
    cv::Mat grayTop, grayBot;
    cv::cvtColor(img(topR), grayTop, cv::COLOR_BGR2GRAY);
    cv::cvtColor(img(botR), grayBot, cv::COLOR_BGR2GRAY);
    double mTop = cv::mean(grayTop)[0];
    double mBot = cv::mean(grayBot)[0];
    int thr = static_cast<int>(std::min(mTop, mBot) - 5.0);
    return std::clamp(thr, 180, 250);
}

bool findForegroundBounds(const cv::Mat& img, int& top, int& bot, int whiteThr)
{
    cv::Mat mask;
    cv::inRange(img, cv::Scalar(whiteThr, whiteThr, whiteThr), cv::Scalar(255, 255, 255), mask);
    cv::bitwise_not(mask, mask);
    cv::reduce(mask, mask, 1, cv::REDUCE_MAX, CV_8U);
    top = -1; bot = -1;
    for (int r = 0; r < mask.rows; ++r)
    {
        if (mask.at<uchar>(r, 0)) { if (top == -1) top = r; bot = r; }
    }
    return top != -1;
}

bool findForegroundBoundsX(const cv::Mat& img, int& left, int& right, int whiteThr)
{
    cv::Mat mask;
    cv::inRange(img, cv::Scalar(whiteThr, whiteThr, whiteThr), cv::Scalar(255, 255, 255), mask);
    cv::bitwise_not(mask, mask);
    cv::reduce(mask, mask, 0, cv::REDUCE_MAX, CV_8U);
    left = -1; right = -1;
    for (int c = 0; c < mask.cols; ++c)
        if (mask.at<uchar>(0, c)) { if (left == -1) left = c; right = c; }
    return left != -1;
}

}

