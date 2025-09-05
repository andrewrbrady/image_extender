/**
 * @file MaskSettings.hpp
 * Parameters to refine vehicle mask generation in UI and batch.
 */
#pragma once

struct MaskSettings
{
    // Edge detection
    int cannyLow {50};
    int cannyHigh {150};

    // Morphology
    int morphKernel {7};     // odd size (3,5,7,...)
    int dilateIters {2};
    int erodeIters {0};

    // White cyc assistance
    bool useWhiteCycAssist {true};
    int whiteThreshold {-1}; // -1 = auto

    // Post-process
    int minArea {5000};      // remove small components (in px)
    int featherRadius {0};   // Gaussian blur radius; 0 = off
    bool invert {false};     // output background white (true) or object white (false)
};

