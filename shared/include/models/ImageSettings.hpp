/**
 * @file ImageSettings.hpp
 * Shared image settings model used by the wxWidgets UI (formerly shared with Qt UI).
 */
#pragma once

struct ImageSettings
{
    int width {0};
    int height {0};
    int whiteThreshold {-1};
    double padding {0.05};
    int finalWidth {-1};
    int finalHeight {-1};
    int blurRadius {0};

    ImageSettings() = default;
    ImageSettings(int w, int h) : width(w), height(h) {}
    ImageSettings(int w, int h, int threshold, double pad, int fw = -1, int fh = -1)
        : width(w), height(h), whiteThreshold(threshold), padding(pad), finalWidth(fw), finalHeight(fh) {}
    ImageSettings(int w, int h, int threshold, double pad, int blur, int fw, int fh)
        : width(w), height(h), whiteThreshold(threshold), padding(pad), finalWidth(fw), finalHeight(fh), blurRadius(blur) {}
};
