// Shared extend_canvas implementation
#include "extend_canvas.hpp"
#include "util/ImageOps.hpp"

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

    using util::centerSampleThreshold;
    using util::findForegroundBounds;
    using util::findForegroundBoundsX;

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
