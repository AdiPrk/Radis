#pragma once

#include "LinearImage.h"

// Gives fully transparent texels the color of the nearest visible ones, so bilinear filtering
// and block compression never pick up whatever color the source left under alpha 0.
// Visible texels and all alpha values are left untouched.
void DilateTransparentTexels(LinearImage& image);