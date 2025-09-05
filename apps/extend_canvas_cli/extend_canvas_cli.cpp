// Simple CLI for extending canvas (legacy tool)
// Build via CMake target: extend_canvas_cli

#include <opencv2/opencv.hpp>
#include <iostream>
#include <string>
#include <algorithm>

using namespace cv;

static int centerSampleThreshold(const Mat& img, int stripeH = 20, int stripeW = 40)
{
    int cx = img.cols / 2;
    int w  = std::min({ stripeW, std::max(1, cx - 1), std::max(1, img.cols - cx - 1) });
    int h  = std::min(stripeH, std::max(1, img.rows / 10));
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

static bool findForegroundBounds(const Mat& img, int& top, int& bot, int whiteThr)
{
    Mat mask; inRange(img, Scalar(whiteThr, whiteThr, whiteThr), Scalar(255, 255, 255), mask);
    bitwise_not(mask, mask);
    reduce(mask, mask, 1, REDUCE_MAX, CV_8U);
    top = -1; bot = -1;
    for (int r=0; r<mask.rows; ++r) { if (mask.at<uchar>(r,0)) { if (top==-1) top=r; bot=r; } }
    return top != -1;
}

static Mat makeStrip(const Mat& src, int newH, int W)
{
    if (newH <= 0) return Mat();
    if (!src.empty()) { Mat dst; resize(src, dst, Size(W, newH), 0, 0, INTER_AREA); return dst; }
    return Mat(newH, W, CV_8UC3, Scalar(255,255,255));
}

int main(int argc, char** argv)
{
    if (argc < 4)
    {
        std::cerr << "Usage: " << argv[0] << " <in> <out> <desired_h> [pad%] [white_thresh|-1]\n";
        return 1;
    }
    std::string inP  = argv[1];
    std::string outP = argv[2];
    int desiredH     = std::stoi(argv[3]);
    double padPct    = (argc >= 5 ? std::stod(argv[4]) : 0.05);
    int whiteThrArg  = (argc >= 6 ? std::stoi(argv[5]) : -1);

    Mat img = imread(inP);
    if (img.empty()) { std::cerr << "Cannot open input\n"; return 1; }

    int whiteThr = (whiteThrArg >= 0 && whiteThrArg <= 255) ? whiteThrArg : centerSampleThreshold(img);

    int fgTop, fgBot; if (!findForegroundBounds(img, fgTop, fgBot, whiteThr)) { std::cerr << "Foreground not found\n"; return 1; }
    int carH = fgBot - fgTop + 1;
    int pad = static_cast<int>(carH * padPct + 0.5);
    int cropTop = std::max(0, fgTop - pad);
    int cropBot = std::min(img.rows - 1, fgBot + pad);
    Mat carReg = img.rowRange(cropTop, cropBot + 1);

    if (desiredH <= carReg.rows)
    {
        int yOff = (carReg.rows - desiredH) / 2; imwrite(outP, carReg.rowRange(yOff, yOff + desiredH)); return 0;
    }

    int extra = desiredH - carReg.rows; int topH = extra/2; int botH = extra - topH; int W = img.cols;
    Mat topSrc = cropTop > 0 ? img.rowRange(0, cropTop) : Mat();
    Mat botSrc = (cropBot + 1 < img.rows) ? img.rowRange(cropBot + 1, img.rows) : Mat();
    Mat topStrip = makeStrip(topSrc, topH, W);
    Mat botStrip = makeStrip(botSrc, botH, W);
    Mat canvas(desiredH, W, img.type()); int y = 0;
    if (!topStrip.empty()) { topStrip.copyTo(canvas.rowRange(y, y + topStrip.rows)); y += topStrip.rows; }
    carReg.copyTo(canvas.rowRange(y, y + carReg.rows)); y += carReg.rows;
    if (!botStrip.empty()) botStrip.copyTo(canvas.rowRange(y, y + botStrip.rows));
    imwrite(outP, canvas);
    std::cout << "Saved (thr=" << whiteThr << ") to " << outP << "\n";
    return 0;
}

