// extend_canvas.cpp  (v6.1)
// Added support for requestedWidth and requestedHeight parameters
// to constrain output dimensions while maintaining proper aspect ratios.
// Fixed: Final resize now preserves aspect ratio and centers content.
//
// Build:
//   g++ -std=c++17 -O2 -Wall -o extend_canvas extend_canvas.cpp `pkg-config --cflags --libs opencv4`
// Usage:
//   ./extend_canvas <in> <out> <desired_h> [pad%] [white_thresh] [requested_w] [requested_h]
//      white_thresh:
//        • omit or  -1 → AUTO  (new center‑sample method)
//        •  0‑255         set manually
//      requested_w, requested_h:
//        • omit → use original width, desired height
//        • specify both → resize final output to fit dimensions while preserving aspect ratio
#include "extend_canvas.hpp"

#include <opencv2/opencv.hpp>
#include <filesystem>
#include <iostream>
#include <algorithm>

using namespace cv;

namespace
{

    /* Utility ----------------------------------------------------------------- */
    inline std::string makeOutputPath(const std::filesystem::path &inPath)
    {
        return (inPath.parent_path() /
                (inPath.stem().string() + "_extended" + inPath.extension().string()))
            .string();
    }

    /* Center-sample threshold detection --------------------------------------- */
    static int centerSampleThreshold(const Mat &img, int stripeH = 20, int stripeW = 40)
    {
        int cx = img.cols / 2;
        int w = std::min({stripeW, cx - 1, img.cols - cx - 1});
        int h = std::min(stripeH, img.rows / 10);

        Rect topR(cx - w, 0, 2 * w + 1, h);
        Rect botR(cx - w, img.rows - h, 2 * w + 1, h);

        std::cout << "[DEBUG] centerSampleThreshold: sampling regions top=" << topR
                  << ", bot=" << botR << std::endl;

        Mat grayTop, grayBot;
        cvtColor(img(topR), grayTop, COLOR_BGR2GRAY);
        cvtColor(img(botR), grayBot, COLOR_BGR2GRAY);

        double mTop = mean(grayTop)[0];
        double mBot = mean(grayBot)[0];

        std::cout << "[DEBUG] Mean values: top=" << mTop << ", bot=" << mBot << std::endl;

        int thr = static_cast<int>(std::min(mTop, mBot) - 5.0); // 5‑point cushion below white
        thr = std::clamp(thr, 180, 250);

        std::cout << "[DEBUG] Calculated threshold: " << thr << " (before clamp: "
                  << static_cast<int>(std::min(mTop, mBot) - 5.0) << ")" << std::endl;

        return thr;
    }

    /* Find foreground bounds (vertical) -------------------------------------- */
    static bool findForegroundBounds(const Mat &img, int &top, int &bot, int whiteThr)
    {
        std::cout << "[DEBUG] findForegroundBounds: threshold=" << whiteThr << ", image size=" << img.cols << "x" << img.rows << std::endl;

        Mat mask;
        inRange(img, Scalar(whiteThr, whiteThr, whiteThr), Scalar(255, 255, 255), mask);
        bitwise_not(mask, mask); // foreground = 255

        // Debug: Count foreground pixels
        int foregroundPixels = countNonZero(mask);
        int totalPixels = mask.rows * mask.cols;
        double foregroundPercent = (foregroundPixels * 100.0) / totalPixels;
        std::cout << "[DEBUG] Foreground pixels: " << foregroundPixels << "/" << totalPixels
                  << " (" << foregroundPercent << "%)" << std::endl;

        reduce(mask, mask, 1, REDUCE_MAX, CV_8U); // rows collapsed

        top = -1;
        bot = -1;
        int foregroundRows = 0;
        for (int r = 0; r < mask.rows; ++r)
        {
            if (mask.at<uchar>(r, 0))
            {
                if (top == -1)
                    top = r;
                bot = r;
                foregroundRows++;
            }
        }

        std::cout << "[DEBUG] Foreground bounds: top=" << top << ", bot=" << bot
                  << ", foregroundRows=" << foregroundRows << "/" << mask.rows << std::endl;

        return top != -1;
    }

    /* Find foreground bounds (horizontal) ------------------------------------ */
    static bool findForegroundBoundsX(const Mat &img, int &left, int &right, int whiteThr)
    {
        Mat mask;
        inRange(img, Scalar(whiteThr, whiteThr, whiteThr), Scalar(255, 255, 255), mask);
        bitwise_not(mask, mask); // foreground = 255

        // Collapse rows to detect any foreground in each column
        reduce(mask, mask, 0, REDUCE_MAX, CV_8U);

        left = -1;
        right = -1;
        for (int c = 0; c < mask.cols; ++c)
        {
            if (mask.at<uchar>(0, c))
            {
                if (left == -1)
                    left = c;
                right = c;
            }
        }
        return left != -1;
    }

    /* Create extension strips ------------------------------------------------- */
    static Mat makeStrip(const Mat &src, int newH, int W)
    {
        if (newH <= 0)
            return Mat();
        if (!src.empty())
        {
            Mat dst;
            resize(src, dst, Size(W, newH), 0, 0, INTER_AREA);
            return dst;
        }
        return Mat(newH, W, CV_8UC3, Scalar(255, 255, 255));
    }

    // Blend across a vertical seam to reduce visible edges between regions
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

    /* Apply final resize with aspect ratio preservation ---------------------- */
    static Mat applyFinalResize(const Mat &canvas, int requestedW, int requestedH)
    {
        if (requestedW <= 0 || requestedH <= 0)
        {
            return canvas;
        }

        // Calculate the scaling factor to fit within requested dimensions while preserving aspect ratio
        double scaleX = static_cast<double>(requestedW) / canvas.cols;
        double scaleY = static_cast<double>(requestedH) / canvas.rows;
        double scale = std::min(scaleX, scaleY); // Use the smaller scale to maintain aspect ratio

        // Calculate the new dimensions that preserve aspect ratio
        int newWidth = static_cast<int>(canvas.cols * scale);
        int newHeight = static_cast<int>(canvas.rows * scale);

        // Resize while preserving aspect ratio
        Mat resized;
        resize(canvas, resized, Size(newWidth, newHeight), 0, 0, INTER_LANCZOS4);

        // Create a canvas with the requested dimensions and center the resized image
        Mat finalCanvas(requestedH, requestedW, canvas.type(), Scalar(255, 255, 255)); // White background

        // Calculate position to center the resized image
        int xOffset = (requestedW - newWidth) / 2;
        int yOffset = (requestedH - newHeight) / 2;

        // Ensure offsets are non-negative
        xOffset = std::max(0, xOffset);
        yOffset = std::max(0, yOffset);

        // Copy the resized image to the center of the final canvas
        if (xOffset + newWidth <= requestedW && yOffset + newHeight <= requestedH)
        {
            Rect roi(xOffset, yOffset, newWidth, newHeight);
            resized.copyTo(finalCanvas(roi));
            std::cout << "[extendCanvas] Resized to requested dimensions with aspect ratio preserved: "
                      << requestedW << "x" << requestedH << std::endl;
            return finalCanvas;
        }
        else
        {
            // Fallback: just resize without centering if there's an issue
            resize(canvas, finalCanvas, Size(requestedW, requestedH), 0, 0, INTER_LANCZOS4);
            std::cout << "[extendCanvas] Resized to requested dimensions (fallback): "
                      << requestedW << "x" << requestedH << std::endl;
            return finalCanvas;
        }
    }

} // unnamed namespace

/*-------------------------------------------------------------------------*\
|  Public API                                                               |
\*-------------------------------------------------------------------------*/
bool extendCanvas(const std::string &inPath, int reqW, int reqH, int whiteThr,
                  double padPct, int requestedW, int requestedH, int blurRadius)
{
    std::cout << "\n=== EXTEND CANVAS DEBUG ===" << std::endl;
    std::cout << "[extendCanvas] Input parameters:" << std::endl;
    std::cout << "  - Input path: " << inPath << std::endl;
    std::cout << "  - Requested Width (reqW): " << reqW << std::endl;
    std::cout << "  - Requested Height (reqH): " << reqH << std::endl;
    std::cout << "  - White Threshold: " << whiteThr << std::endl;
    std::cout << "  - Padding %: " << padPct << std::endl;
    std::cout << "  - Final Width: " << requestedW << std::endl;
    std::cout << "  - Final Height: " << requestedH << std::endl;
    std::cout << "  - Blur Radius: " << blurRadius << std::endl;
    std::cout << "==============================\n"
              << std::endl;

    /* 1 · Load ------------------------------------------------------------ */
    Mat img = imread(inPath);
    if (img.empty())
    {
        std::cerr << "[extendCanvas] ERROR: cannot open \"" << inPath << "\"\n";
        return false;
    }

    /* 2 · Determine white threshold --------------------------------------- */
    int actualWhiteThr = (whiteThr >= 0 && whiteThr <= 255) ? whiteThr : centerSampleThreshold(img);
    std::cout << "[extendCanvas] Using white threshold: " << actualWhiteThr << std::endl;

    /* 3 · Find foreground bounds ------------------------------------------ */
    int fgTop, fgBot;
    if (!findForegroundBounds(img, fgTop, fgBot, actualWhiteThr))
    {
        std::cerr << "[extendCanvas] ERROR: Foreground not found (try lowering threshold)." << std::endl;
        return false;
    }

    // Horizontal bounds for optional horizontal extension
    int fgLeft = 0, fgRight = img.cols - 1;
    findForegroundBoundsX(img, fgLeft, fgRight, actualWhiteThr);

    /* 4 · Apply padding --------------------------------------------------- */
    int carH = fgBot - fgTop + 1;
    int pad = static_cast<int>(carH * padPct + 0.5);
    int cropTop = std::max(0, fgTop - pad);
    int cropBot = std::min(img.rows - 1, fgBot + pad);

    std::cout << "[DEBUG] Padding calculation:" << std::endl;
    std::cout << "  - fgTop=" << fgTop << ", fgBot=" << fgBot << std::endl;
    std::cout << "  - carH=" << carH << ", padPct=" << padPct << std::endl;
    std::cout << "  - pad=" << pad << std::endl;
    std::cout << "  - cropTop=" << cropTop << ", cropBot=" << cropBot << std::endl;
    std::cout << "  - img.rows=" << img.rows << std::endl;

    Mat carReg = img.rowRange(cropTop, cropBot + 1);

    // Horizontal crop metrics (for building left/right extension strips)
    int carW = std::max(1, fgRight - fgLeft + 1);
    int padX = static_cast<int>(carW * padPct + 0.5);
    int cropLeft = std::max(0, fgLeft - padX);
    int cropRight = std::min(img.cols - 1, fgRight + padX);

    std::cout << "[DEBUG] After cropping: carReg=" << carReg.cols << "x" << carReg.rows << std::endl;

    /* 5 · Determine target dimensions ------------------------------------- */
    int desiredH = (reqH > 0) ? reqH : img.rows;
    int desiredW = (reqW > 0) ? reqW : img.cols;
    int W = img.cols;

    std::cout << "[extendCanvas] Original image: " << img.cols << "x" << img.rows << std::endl;
    std::cout << "[extendCanvas] Cropped region: " << carReg.cols << "x" << carReg.rows << std::endl;
    std::cout << "[extendCanvas] Target dimensions: " << desiredW << "x" << desiredH << std::endl;

    /* 6 · Handle case where already tall enough --------------------------- */
    if (desiredH <= carReg.rows)
    {
        int yOff = (carReg.rows - desiredH) / 2;
        Mat result = carReg.rowRange(yOff, yOff + desiredH);

        // Horizontal extension removed: always scale if width differs
        if (false && desiredW > result.cols)
        {
            int extraW = desiredW - result.cols;
            int leftW = extraW / 2;
            int rightW = extraW - leftW;

            auto makeStripW = [](const Mat &src, int H, int newW) -> Mat {
                if (newW <= 0) return Mat();
                if (!src.empty()) { Mat dst; resize(src, dst, Size(newW, H), 0, 0, INTER_AREA); return dst; }
                return Mat(H, newW, CV_8UC3, Scalar(255, 255, 255));
            };

            Mat leftSrc = (cropLeft > 0) ? img.colRange(0, cropLeft) : Mat();
            Mat rightSrc = (cropRight + 1 < img.cols) ? img.colRange(cropRight + 1, img.cols) : Mat();
            Mat leftStrip = makeStripW(leftSrc, result.rows, leftW);
            Mat rightStrip = makeStripW(rightSrc, result.rows, rightW);

            if (blurRadius > 0)
            {
                int k = std::max(1, blurRadius * 2 + 1);
                if (!leftStrip.empty()) GaussianBlur(leftStrip, leftStrip, Size(k, k), 0);
                if (!rightStrip.empty()) GaussianBlur(rightStrip, rightStrip, Size(k, k), 0);
            }

            Mat wide(desiredH, desiredW, img.type());
            int x = 0;
            if (!leftStrip.empty()) { leftStrip.copyTo(wide(Rect(x, 0, leftStrip.cols, leftStrip.rows))); x += leftStrip.cols; }
            result.copyTo(wide(Rect(x, 0, result.cols, result.rows))); x += result.cols;
            if (!rightStrip.empty()) { rightStrip.copyTo(wide(Rect(x, 0, rightStrip.cols, rightStrip.rows))); }
            // Seam blending at joins
            if (!leftStrip.empty())
            {
                int seamX = leftStrip.cols;
                int ov = std::min({24, leftStrip.cols, wide.cols - seamX});
                blendVerticalSeam(wide, seamX, ov);
            }
            if (!rightStrip.empty())
            {
                int seamX = wide.cols - rightStrip.cols;
                int ov = std::min({24, rightStrip.cols, seamX});
                blendVerticalSeam(wide, seamX, ov);
            }
            result = wide;
        }
        // Scale to target width if needed
        if (desiredW != result.cols)
        {
            std::cout << "[extendCanvas] Scaling width: " << result.cols << " -> " << desiredW << std::endl;
            double scale = static_cast<double>(desiredW) / result.cols;
            int scaledHeight = static_cast<int>(result.rows * scale + 0.5);
            resize(result, result, Size(desiredW, scaledHeight), 0, 0, INTER_LANCZOS4);

            // If scaling changed height, adjust to target height
            if (scaledHeight != desiredH)
            {
                if (scaledHeight > desiredH)
                {
                    // Crop height
                    int yOffset = (scaledHeight - desiredH) / 2;
                    result = result.rowRange(yOffset, yOffset + desiredH);
                }
                else
                {
                    // Extend height with white
                    Mat extended(desiredH, desiredW, result.type(), Scalar(255, 255, 255));
                    int yOffset = (desiredH - scaledHeight) / 2;
                    result.copyTo(extended.rowRange(yOffset, yOffset + scaledHeight));
                    result = extended;
                }
            }
        }

        // Apply final resize if requested dimensions are specified
        result = applyFinalResize(result, requestedW, requestedH);

        const std::string outPath = makeOutputPath(std::filesystem::path(inPath));
        if (!imwrite(outPath, result))
        {
            std::cerr << "[extendCanvas] ERROR: failed to write \"" << outPath << "\"\n";
            return false;
        }

        std::cout << "[extendCanvas] Wrote \"" << outPath << "\" (" << result.cols << "x" << result.rows << ")" << std::endl;
        return true;
    }

    /* 7 · Extend canvas with intelligent padding -------------------------- */
    int extra = desiredH - carReg.rows;
    int topH = extra / 2;
    int botH = extra - topH;

    std::cout << "[extendCanvas] Extension needed: " << carReg.rows << " -> " << desiredH
              << " (+" << extra << " pixels)" << std::endl;
    std::cout << "[extendCanvas] Adding: top=" << topH << ", bottom=" << botH << std::endl;

    // First, scale everything to target width if needed
    Mat scaledCarReg = carReg;
    Mat scaledTopSrc, scaledBotSrc;
    // Only pre-scale to desiredW when it is less than or equal to the source width.
    // If desiredW is larger, we'll extend horizontally later instead of scaling.
    int targetW = W;

    if (desiredW != W)
    {
        std::cout << "[extendCanvas] Pre-scaling to target width: " << W << " -> " << desiredW << std::endl;

        // Scale the car region
        double scale = static_cast<double>(desiredW) / W;
        int scaledCarHeight = static_cast<int>(carReg.rows * scale + 0.5);
        resize(carReg, scaledCarReg, Size(desiredW, scaledCarHeight), 0, 0, INTER_LANCZOS4);

        // Scale the source regions for extension strips
        Mat topSrc = cropTop > 0 ? img.rowRange(0, cropTop) : Mat();
        Mat botSrc = (cropBot + 1 < img.rows) ? img.rowRange(cropBot + 1, img.rows) : Mat();

        if (!topSrc.empty())
        {
            int scaledTopHeight = static_cast<int>(topSrc.rows * scale + 0.5);
            resize(topSrc, scaledTopSrc, Size(desiredW, scaledTopHeight), 0, 0, INTER_LANCZOS4);
        }
        if (!botSrc.empty())
        {
            int scaledBotHeight = static_cast<int>(botSrc.rows * scale + 0.5);
            resize(botSrc, scaledBotSrc, Size(desiredW, scaledBotHeight), 0, 0, INTER_LANCZOS4);
        }

        // Recalculate extension needed after scaling
        extra = desiredH - scaledCarReg.rows;
        topH = extra / 2;
        botH = extra - topH;

        std::cout << "[extendCanvas] After width scaling - Extension needed: " << scaledCarReg.rows
                  << " -> " << desiredH << " (+" << extra << " pixels)" << std::endl;
        targetW = desiredW;
    }
    else
    {
        Mat topSrc = cropTop > 0 ? img.rowRange(0, cropTop) : Mat();
        Mat botSrc = (cropBot + 1 < img.rows) ? img.rowRange(cropBot + 1, img.rows) : Mat();
        scaledTopSrc = topSrc;
        scaledBotSrc = botSrc;
        targetW = W;
    }

    // Create extension strips at target width
    Mat topStrip = makeStrip(scaledTopSrc, topH, targetW);
    Mat botStrip = makeStrip(scaledBotSrc, botH, targetW);

    // Optional blur for extension strips to help blend and reduce stretched noise
    if (blurRadius > 0)
    {
        int k = std::max(1, blurRadius * 2 + 1); // ensure odd kernel size
        if (!topStrip.empty())
        {
            GaussianBlur(topStrip, topStrip, Size(k, k), 0);
        }
        if (!botStrip.empty())
        {
            GaussianBlur(botStrip, botStrip, Size(k, k), 0);
        }
    }

    Mat canvas(desiredH, targetW, img.type());
    int y = 0;
    topStrip.copyTo(canvas.rowRange(y, y + topStrip.rows));
    y += topStrip.rows;
    scaledCarReg.copyTo(canvas.rowRange(y, y + scaledCarReg.rows));
    y += scaledCarReg.rows;
    botStrip.copyTo(canvas.rowRange(y, y + botStrip.rows));

    std::cout << "[extendCanvas] Canvas extension complete: " << canvas.cols << "x" << canvas.rows << std::endl;

    /* 7b · Horizontal extension removed (no-op) --------------------------- */
    if (false && desiredW > canvas.cols)
    {
        int extraW = desiredW - canvas.cols;
        int leftW = extraW / 2;
        int rightW = extraW - leftW;

        // Build left/right source strips from outside the horizontal crop bounds
        Mat leftSrc = (cropLeft > 0) ? img.colRange(0, cropLeft) : Mat();
        Mat rightSrc = (cropRight + 1 < img.cols) ? img.colRange(cropRight + 1, img.cols) : Mat();

        auto makeStripW = [](const Mat &src, int H, int newW) -> Mat {
            if (newW <= 0)
                return Mat();
            if (!src.empty())
            {
                Mat dst;
                resize(src, dst, Size(newW, H), 0, 0, INTER_AREA);
                return dst;
            }
            return Mat(H, newW, CV_8UC3, Scalar(255, 255, 255));
        };

        Mat leftStrip = makeStripW(leftSrc, canvas.rows, leftW);
        Mat rightStrip = makeStripW(rightSrc, canvas.rows, rightW);

        if (blurRadius > 0)
        {
            int k = std::max(1, blurRadius * 2 + 1);
            if (!leftStrip.empty()) GaussianBlur(leftStrip, leftStrip, Size(k, k), 0);
            if (!rightStrip.empty()) GaussianBlur(rightStrip, rightStrip, Size(k, k), 0);
        }

        Mat wide(desiredH, desiredW, img.type());
        int x = 0;
        if (!leftStrip.empty())
        {
            leftStrip.copyTo(wide(Rect(x, 0, leftStrip.cols, leftStrip.rows)));
            x += leftStrip.cols;
        }
        canvas.copyTo(wide(Rect(x, 0, canvas.cols, canvas.rows)));
        x += canvas.cols;
        if (!rightStrip.empty())
        {
            rightStrip.copyTo(wide(Rect(x, 0, rightStrip.cols, rightStrip.rows)));
        }
        // Seam blending at joins
        if (!leftStrip.empty())
        {
            int seamX = leftStrip.cols;
            int ov = std::min({24, leftStrip.cols, wide.cols - seamX});
            blendVerticalSeam(wide, seamX, ov);
        }
        if (!rightStrip.empty())
        {
            int seamX = wide.cols - rightStrip.cols;
            int ov = std::min({24, rightStrip.cols, seamX});
            blendVerticalSeam(wide, seamX, ov);
        }
        canvas = wide;
        std::cout << "[extendCanvas] Horizontal extension complete: " << canvas.cols << "x" << canvas.rows << std::endl;
    }

    /* 8 · Apply final resize if requested --------------------------------- */
    canvas = applyFinalResize(canvas, requestedW, requestedH);

    /* 10 · Save result ----------------------------------------------------- */
    const std::string outPath = makeOutputPath(std::filesystem::path(inPath));
    if (!imwrite(outPath, canvas))
    {
        std::cerr << "[extendCanvas] ERROR: failed to write \"" << outPath << "\"\n";
        return false;
    }

    std::cout << "[extendCanvas] Wrote \"" << outPath << "\" (" << canvas.cols << "x" << canvas.rows
              << ", threshold=" << actualWhiteThr << ")" << std::endl;
    return true;
}

/*-------------------------------------------------------------------------*\
|  Optional tiny CLI for quick testing                                     |
\*-------------------------------------------------------------------------*/
#ifdef EXTEND_CANVAS_STANDALONE
int main(int argc, char **argv)
{
    if (argc < 4)
    {
        std::cerr << "Usage: " << argv[0] << " <in> <out> <desired_h> [pad%] [white_thresh|-1] [requested_w] [requested_h]" << std::endl;
        return 1;
    }

    std::string inPath = argv[1];
    std::string outPath = argv[2]; // Note: this version ignores outPath and uses auto-generated name
    int desiredH = std::stoi(argv[3]);
    double padPct = (argc >= 5 ? std::stod(argv[4]) : 0.05);
    int whiteThrArg = (argc >= 6 ? std::stoi(argv[5]) : -1);
    int requestedW = (argc >= 7 ? std::stoi(argv[6]) : -1);
    int requestedH = (argc >= 8 ? std::stoi(argv[7]) : -1);

    // For CLI, use requestedW as the canvas width if specified
    int canvasW = (requestedW > 0) ? requestedW : 0;

    return extendCanvas(inPath, canvasW, desiredH, whiteThrArg, padPct, -1, -1, 0) ? 0 : 1;
}
#endif
