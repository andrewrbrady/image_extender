#pragma once

/**
 * @struct ImageSettings
 * @brief Stores per-image processing settings
 *
 * This structure maintains individual settings for each image being processed,
 * allowing for customized processing parameters in batch operations.
 */
struct ImageSettings
{
    int width = 1080;        ///< Target canvas width in pixels
    int height = 1920;       ///< Target canvas height in pixels
    int whiteThreshold = 20; ///< Threshold for detecting white pixels (0-255)
    double padding = 0.05;   ///< Padding ratio (0.0-1.0)
    int blurRadius = 0;      ///< Blur radius in pixels for extended areas (0 disables)
    int finalWidth = -1;     ///< Optional final resize width (-1 to skip)
    int finalHeight = -1;    ///< Optional final resize height (-1 to skip)

    /**
     * @brief Default constructor with standard Instagram story dimensions
     */
    ImageSettings() = default;

    /**
     * @brief Constructor with custom dimensions
     * @param w Target width in pixels
     * @param h Target height in pixels
     */
    ImageSettings(int w, int h) : width(w), height(h) {}

    /**
     * @brief Full constructor with all parameters
     * @param w Target width in pixels
     * @param h Target height in pixels
     * @param threshold White detection threshold
     * @param pad Padding ratio
     * @param fw Final width (optional)
     * @param fh Final height (optional)
     */
    ImageSettings(int w, int h, int threshold, double pad, int fw = -1, int fh = -1)
        : width(w), height(h), whiteThreshold(threshold), padding(pad),
          blurRadius(0), finalWidth(fw), finalHeight(fh) {}

    /**
     * @brief Full constructor including blur radius
     */
    ImageSettings(int w, int h, int threshold, double pad, int blur, int fw, int fh)
        : width(w), height(h), whiteThreshold(threshold), padding(pad),
          blurRadius(blur), finalWidth(fw), finalHeight(fh) {}
};
