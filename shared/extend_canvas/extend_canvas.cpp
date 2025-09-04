// Shared extend_canvas implementation (moved from Qt UI tree)
#include "extend_canvas.hpp"

#include <opencv2/opencv.hpp>
#include <filesystem>
#include <iostream>
#include <algorithm>

using namespace cv;

namespace
{
    inline std::string makeOutputPath(const std::filesystem::path &inPath)
    {
        return (inPath.parent_path() /
                (inPath.stem().string() + "_extended" + inPath.extension().string()))
            .string();
    }

    static int centerSampleThreshold(const Mat &img, int stripeH = 20, int stripeW = 40)
    {
        int cx = img.cols / 2;
        int w = std::min({stripeW, cx - 1, img.cols - cx - 1});
        int h = std::min(stripeH, img.rows / 10);

        Rect topR(cx - w, 0, 2 * w + 1, h);
        Rect botR(cx - w, img.rows - h, 2 * w + 1, h);

        Mat grayTop, grayBot;
        cvtColor(img(topR), grayTop, COLOR_BGR2GRAY);
        cvtColor(img(botR), grayBot, COLOR_BGR2GRAY);
        double mTop = mean(grayTop)[0];
        double mBot = mean(grayBot)[0];
        int thr = static_cast<int>(std::min(mTop, mBot) - 5.0);
        return std::clamp(thr, 180, 250);
    }

    static bool findForegroundBounds(const Mat &img, int &top, int &bot, int whiteThr)
    {
        Mat mask;
        inRange(img, Scalar(whiteThr, whiteThr, whiteThr), Scalar(255, 255, 255), mask);
        bitwise_not(mask, mask);
        reduce(mask, mask, 1, REDUCE_MAX, CV_8U);
        top = -1; bot = -1;
        for (int r = 0; r < mask.rows; ++r)
        {
            if (mask.at<uchar>(r, 0)) { if (top == -1) top = r; bot = r; }
        }
        return top != -1;
    }

    static bool findForegroundBoundsX(const Mat &img, int &left, int &right, int whiteThr)
    {
        Mat mask; inRange(img, Scalar(whiteThr, whiteThr, whiteThr), Scalar(255, 255, 255), mask);
        bitwise_not(mask, mask);
        reduce(mask, mask, 0, REDUCE_MAX, CV_8U);
        left = -1; right = -1;
        for (int c = 0; c < mask.cols; ++c)
            if (mask.at<uchar>(0, c)) { if (left == -1) left = c; right = c; }
        return left != -1;
    }

    static Mat makeStrip(const Mat &src, int newH, int W)
    {
        if (newH <= 0) return Mat();
        if (!src.empty()) { Mat dst; resize(src, dst, Size(W, newH), 0, 0, INTER_AREA); return dst; }
        return Mat(newH, W, CV_8UC3, Scalar(255, 255, 255));
    }

    static void blendVerticalSeam(Mat &img, int seamX, int overlap)
    {
        if (overlap <= 0) return;
        if (seamX - overlap < 0 || seamX + overlap > img.cols) return;
        Mat left = img(Rect(seamX - overlap, 0, overlap, img.rows)).clone();
        Mat right = img(Rect(seamX, 0, overlap, img.rows)).clone();
        for (int i = 0; i < overlap; ++i)
        {
            double a = (i + 1.0) / (overlap + 1.0);
            Mat dstCol = img(Rect(seamX - overlap + i, 0, 1, img.rows));
            addWeighted(right.col(i), a, left.col(i), 1.0 - a, 0.0, dstCol);
        }
    }

    static Mat applyFinalResize(const Mat &canvas, int requestedW, int requestedH)
    {
        if (requestedW <= 0 || requestedH <= 0) return canvas;
        double scaleX = static_cast<double>(requestedW) / canvas.cols;
        double scaleY = static_cast<double>(requestedH) / canvas.rows;
        double scale = std::min(scaleX, scaleY);
        int newWidth = static_cast<int>(canvas.cols * scale);
        int newHeight = static_cast<int>(canvas.rows * scale);
        Mat resized; resize(canvas, resized, Size(newWidth, newHeight), 0, 0, INTER_LANCZOS4);
        Mat finalCanvas(requestedH, requestedW, canvas.type(), Scalar(255, 255, 255));
        int xOffset = std::max(0, (requestedW - newWidth) / 2);
        int yOffset = std::max(0, (requestedH - newHeight) / 2);
        resized.copyTo(finalCanvas(Rect(xOffset, yOffset, newWidth, newHeight)));
        return finalCanvas;
    }
}

bool extendCanvas(const std::string &inPath, int reqW, int reqH, int whiteThr,
                  double padPct, int requestedW, int requestedH, int blurRadius)
{
    Mat img = imread(inPath);
    if (img.empty()) { std::cerr << "[extendCanvas] cannot open: " << inPath << "\n"; return false; }

    int actualWhiteThr = (whiteThr >= 0 && whiteThr <= 255) ? whiteThr : centerSampleThreshold(img);

    int fgTop, fgBot; if (!findForegroundBounds(img, fgTop, fgBot, actualWhiteThr)) return false;
    int fgLeft, fgRight; if (!findForegroundBoundsX(img, fgLeft, fgRight, actualWhiteThr)) return false;

    int carH = fgBot - fgTop + 1;
    int pad = static_cast<int>(carH * padPct + 0.5);
    int cropTop = std::max(0, fgTop - pad);
    int cropBot = std::min(img.rows - 1, fgBot + pad);
    Mat carReg = img.rowRange(cropTop, cropBot + 1);

    int desiredH = (reqH > 0) ? reqH : img.rows;
    int desiredW = (reqW > 0) ? reqW : img.cols;
    int W = img.cols;

    if (desiredH <= carReg.rows)
    {
        int yOff = (carReg.rows - desiredH) / 2;
        Mat result = carReg.rowRange(yOff, yOff + desiredH);
        if (desiredW != result.cols)
        {
            double scale = static_cast<double>(desiredW) / result.cols;
            int scaledHeight = static_cast<int>(result.rows * scale + 0.5);
            resize(result, result, Size(desiredW, scaledHeight), 0, 0, INTER_LANCZOS4);
            if (scaledHeight > desiredH)
            {
                int yy = (scaledHeight - desiredH) / 2;
                result = result.rowRange(yy, yy + desiredH);
            }
            else if (scaledHeight < desiredH)
            {
                Mat extended(desiredH, desiredW, result.type(), Scalar(255, 255, 255));
                int yy = (desiredH - scaledHeight) / 2;
                result.copyTo(extended.rowRange(yy, yy + scaledHeight));
                result = extended;
            }
        }
        result = applyFinalResize(result, requestedW, requestedH);
        const std::string outPath = makeOutputPath(std::filesystem::path(inPath));
        return imwrite(outPath, result);
    }

    int extra = desiredH - carReg.rows;
    int topH = extra / 2;
    int botH = extra - topH;

    Mat scaledCarReg = carReg;
    Mat scaledTopSrc, scaledBotSrc;
    int targetW = W;
    if (desiredW != W)
    {
        double scale = static_cast<double>(desiredW) / W;
        int scaledCarHeight = static_cast<int>(carReg.rows * scale + 0.5);
        resize(carReg, scaledCarReg, Size(desiredW, scaledCarHeight), 0, 0, INTER_LANCZOS4);
        Mat topSrc = cropTop > 0 ? img.rowRange(0, cropTop) : Mat();
        Mat botSrc = (cropBot + 1 < img.rows) ? img.rowRange(cropBot + 1, img.rows) : Mat();
        if (!topSrc.empty()) { int h = static_cast<int>(topSrc.rows * scale + 0.5); resize(topSrc, scaledTopSrc, Size(desiredW, h), 0, 0, INTER_LANCZOS4); }
        if (!botSrc.empty()) { int h = static_cast<int>(botSrc.rows * scale + 0.5); resize(botSrc, scaledBotSrc, Size(desiredW, h), 0, 0, INTER_LANCZOS4); }
        extra = desiredH - scaledCarReg.rows; topH = extra / 2; botH = extra - topH; targetW = desiredW;
    }
    else
    {
        Mat topSrc = cropTop > 0 ? img.rowRange(0, cropTop) : Mat();
        Mat botSrc = (cropBot + 1 < img.rows) ? img.rowRange(cropBot + 1, img.rows) : Mat();
        scaledTopSrc = topSrc; scaledBotSrc = botSrc; targetW = W;
    }

    Mat topStrip = makeStrip(scaledTopSrc, topH, targetW);
    Mat botStrip = makeStrip(scaledBotSrc, botH, targetW);
    if (blurRadius > 0)
    {
        int k = std::max(1, blurRadius * 2 + 1);
        if (!topStrip.empty()) GaussianBlur(topStrip, topStrip, Size(k, k), 0);
        if (!botStrip.empty()) GaussianBlur(botStrip, botStrip, Size(k, k), 0);
    }

    Mat canvas(desiredH, targetW, img.type());
    int y = 0; if (!topStrip.empty()) { topStrip.copyTo(canvas.rowRange(y, y + topStrip.rows)); y += topStrip.rows; }
    scaledCarReg.copyTo(canvas.rowRange(y, y + scaledCarReg.rows)); y += scaledCarReg.rows;
    if (!botStrip.empty()) { botStrip.copyTo(canvas.rowRange(y, y + botStrip.rows)); }

    canvas = applyFinalResize(canvas, requestedW, requestedH);
    const std::string outPath = makeOutputPath(std::filesystem::path(inPath));
    return imwrite(outPath, canvas);
}

