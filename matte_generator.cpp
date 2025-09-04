#include <opencv2/opencv.hpp>
#include <iostream>
#include <string>

cv::Scalar hexToScalar(const std::string& hex) {
    unsigned int r, g, b;
    if (hex[0] == '#') {
        sscanf(hex.c_str() + 1, "%02x%02x%02x", &r, &g, &b);
    } else {
        sscanf(hex.c_str(), "%02x%02x%02x", &r, &g, &b);
    }
    return cv::Scalar(b, g, r); // OpenCV uses BGR
}

int main(int argc, char** argv) {
    std::string inputPath, outputPath, hexColor = "#000000";
    int canvasWidth = 1920, canvasHeight = 1080;
    float paddingPercent = 0;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--input") inputPath = argv[++i];
        else if (arg == "--output") outputPath = argv[++i];
        else if (arg == "--width") canvasWidth = std::stoi(argv[++i]);
        else if (arg == "--height") canvasHeight = std::stoi(argv[++i]);
        else if (arg == "--padding") paddingPercent = std::stof(argv[++i]);
        else if (arg == "--color") hexColor = argv[++i];
    }

    cv::Mat input = cv::imread(inputPath);
    if (input.empty()) {
        std::cerr << "Error: Could not read input image.\n";
        return 1;
    }

    int padX = static_cast<int>(canvasWidth * paddingPercent / 100.0);
    int padY = static_cast<int>(canvasHeight * paddingPercent / 100.0);
    int contentWidth = canvasWidth - 2 * padX;
    int contentHeight = canvasHeight - 2 * padY;

    // Resize while keeping aspect ratio
    double inputRatio = static_cast<double>(input.cols) / input.rows;
    double canvasRatio = static_cast<double>(contentWidth) / contentHeight;

    int targetWidth, targetHeight;
    if (inputRatio > canvasRatio) {
        targetWidth = contentWidth;
        targetHeight = static_cast<int>(contentWidth / inputRatio);
    } else {
        targetHeight = contentHeight;
        targetWidth = static_cast<int>(contentHeight * inputRatio);
    }

    cv::Mat resized;
    cv::resize(input, resized, cv::Size(targetWidth, targetHeight));

    cv::Mat canvas(canvasHeight, canvasWidth, input.type(), hexToScalar(hexColor));

    int xOffset = (canvasWidth - targetWidth) / 2;
    int yOffset = (canvasHeight - targetHeight) / 2;

    resized.copyTo(canvas(cv::Rect(xOffset, yOffset, resized.cols, resized.rows)));

    cv::imwrite(outputPath, canvas);
    std::cout << "Saved to " << outputPath << "\n";
    return 0;
}