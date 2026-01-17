/**
 * @file ProcessingMode.hpp
 * Defines processing modes for the application so the UI can switch
 * between different features built on the same workflow.
 */
#pragma once

enum class ProcessingMode
{
    ExtendCanvas = 0,
    AutoFitVehicle = 5,
    VehicleMask = 1,
    Crop = 2,
    Splitter = 3,
    SplitCollage = 6,
    FilmDevelop = 4,
};
