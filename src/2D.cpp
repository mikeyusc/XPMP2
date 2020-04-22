/// @file       2D.cpp
/// @brief      Implementation of 2-D routines, like drawing aircraft labels
/// @details    2-D drawing is a bit "unnatural" as the aircraft are put into a 3-D world.
///             These functions require to turn 3D coordinates into 2D coordinates.
/// @see        Laminar's sample code at https://developer.x-plane.com/code-sample/coachmarks/
///             is the basis, then been taken apart.
/// @author     Laminar Research
/// @author     Birger Hoppe
/// @copyright  (c) 2020 Birger Hoppe
/// @copyright  Permission is hereby granted, free of charge, to any person obtaining a
///             copy of this software and associated documentation files (the "Software"),
///             to deal in the Software without restriction, including without limitation
///             the rights to use, copy, modify, merge, publish, distribute, sublicense,
///             and/or sell copies of the Software, and to permit persons to whom the
///             Software is furnished to do so, subject to the following conditions:\n
///             The above copyright notice and this permission notice shall be included in
///             all copies or substantial portions of the Software.\n
///             THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
///             IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
///             FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
///             AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
///             LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
///             OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
///             THE SOFTWARE.

#include "XPMP2.h"

#define DEBUG_ENABLE_AC_LABELS  "Aircraft labels %s"

namespace XPMP2 {

//
// MARK: 2-D projection calculations
//

// Data refs we need
static XPLMDataRef drMatrixWrld     = nullptr;  ///< sim/graphics/view/world_matrix
static XPLMDataRef drMatrixProj     = nullptr;  ///< sim/graphics/view/projection_matrix_3d
static XPLMDataRef drScreenWidth    = nullptr;  ///< sim/graphics/view/window_width
static XPLMDataRef drScreenHeight   = nullptr;  ///< sim/graphics/view/window_height
static XPLMDataRef drVisibility     = nullptr;  ///< sim/graphics/view/visibility_effective_m or sim/weather/visibility_effective_m
static XPLMDataRef drFieldOfView    = nullptr;  ///< sim/graphics/view/field_of_view_deg

/// world matrix (updates once per cycle)
static float gMatrixWrld[16];
/// projection matrix (updated once per cycle)
static float gMatrixProj[16];
/// Screen size (with, height)
static float gScreenW, gScreenH;
/// Field of view
static float gFOV;

/// 4x4 matrix transform of an XYZW coordinate - this matches OpenGL matrix conventions.
static void mult_matrix_vec(float dst[4], const float m[16], const float v[4])
{
    dst[0] = v[0] * m[0] + v[1] * m[4] + v[2] * m[8] + v[3] * m[12];
    dst[1] = v[0] * m[1] + v[1] * m[5] + v[2] * m[9] + v[3] * m[13];
    dst[2] = v[0] * m[2] + v[1] * m[6] + v[2] * m[10] + v[3] * m[14];
    dst[3] = v[0] * m[3] + v[1] * m[7] + v[2] * m[11] + v[3] * m[15];
}


/// Once per cycle read necessary matrices from X-Plane
static void read_matrices ()
{
    // Read the model view and projection matrices from this frame
    XPLMGetDatavf(drMatrixWrld,gMatrixWrld,0,16);
    XPLMGetDatavf(drMatrixProj,gMatrixProj,0,16);
    
    // Read the screen size (won't change often if at all...but could!)
    gScreenW = (float)XPLMGetDatai(drScreenWidth);
    gScreenH = (float)XPLMGetDatai(drScreenHeight);
    
    // Field of view
    gFOV = XPLMGetDataf(drFieldOfView);
}

// This drawing callback will draw a label to the screen where the

/// @brief Converts 3D local coordinates to 2D screen coordinates
/// @note Requires matrices to be set up already by a call to read_matrices()
static void ConvertTo2d(const float x, const float y, const float z,
                        int& out_x, int& out_y)
{
    // the position to convert
    const float afPos[4] = { x, y, z, 1.0f };
    float afEye[4], afNdc[4];
    
    // Simulate the OpenGL transformation to get screen coordinates.
    mult_matrix_vec(afEye, gMatrixWrld, afPos);
    mult_matrix_vec(afNdc, gMatrixProj, afEye);
    
    afNdc[3] = 1.0f / afNdc[3];
    afNdc[0] *= afNdc[3];
    afNdc[1] *= afNdc[3];
    afNdc[2] *= afNdc[3];
    
    out_x = (int)std::lround(gScreenW * (afNdc[0] * 0.5f + 0.5f));
    out_y = (int)std::lround(gScreenH * (afNdc[1] * 0.5f + 0.5f));
}

//
// MARK: Drawing Control
//

/// @brief Write the labels of all aircraft
/// @see This code bases on the last part of `XPMPDefaultPlaneRenderer` of the original libxplanemp
/// @author Ben Supnik, Chris Serio, Chris Collins, Birger Hoppe
void TwoDDrawLabels ()
{
    XPLMCameraPosition_t posCamera;
    
    // short-cut if label-writing is completely switched off
    if (!glob.bDrawLabels) return;
    
    // Set up required matrices once
    read_matrices();
    
    // Determine the maximum distance for label drawing.
    // Depends on current actual visibility as well as a configurable maximum
    XPLMReadCameraPosition(&posCamera);
    const float maxLabelDist = (std::min(glob.maxLabelDist,
                                         drVisibility ? XPLMGetDataf(drVisibility) : glob.maxLabelDist)
                                * posCamera.zoom);    // Labels get easier to see when users zooms.
    
    // Loop over all aircraft and draw their labels
    for (const auto& p: glob.mapAc)
    {
        // skip if a/c is invisible
        const Aircraft& ac = *p.second;
        if (!ac.IsVisible())
            continue;
        
        // Exit if aircraft is father away from camera than we would draw labels for
        if (ac.GetCameraDist() > maxLabelDist)
            continue;
        
        // Exit if aircraft is "in the back" of the camera, ie. invisible
        if (std::abs(headDiff(ac.GetCameraBearing(), posCamera.heading)) > 90.0f)
            continue;
        
        // Map the 3D coordinates of the aircraft to 2D coordinates of the flat screen
        int x = -1, y = -1;
        ConvertTo2d(ac.drawInfo.x,
                    ac.drawInfo.y - 10.0f,      // make the label appear "10m" above the plane
                    ac.drawInfo.z, x, y);
        
        // Determine text color:
        // It stays as defined by application for half the way to maxLabelDist.
        // For the other half, it gradually fades to gray.
        // `rat` determines how much it faded already (factor from 0..1)
        const float rat =
        ac.GetCameraDist() < maxLabelDist / 2.0f ? 0.0f :                 // first half: no fading
        (ac.GetCameraDist() - maxLabelDist/2.0f) / (maxLabelDist/2.0f);   // Second half: fade to gray (remember: acDist <= maxLabelDist!)
        constexpr float gray[4] = {0.6f, 0.6f, 0.6f, 1.0f};
        float c[4] = {
            (1.0f-rat) * ac.colLabel[0] + rat * gray[0],     // red
            (1.0f-rat) * ac.colLabel[1] + rat * gray[1],     // green
            (1.0f-rat) * ac.colLabel[2] + rat * gray[2],     // blue
            (1.0f-rat) * ac.colLabel[3] + rat * gray[3]      // alpha? (not used for text anyway)
        };
        
        // Finally: Draw the label
        XPLMDrawString(c, x, y, (char*)ac.label.c_str(), NULL, xplmFont_Basic);
    }
}


/// Drawing callback, called by X-Plane in every drawing cycle
int CPLabelDrawing (XPLMDrawingPhase     /*inPhase*/,
                    int                  /*inIsBefore*/,
                    void *               /*inRefcon*/)
{
    UPDATE_CYCLE_NUM;               // DEBUG only: Store current cycle number in glob.xpCycleNum
    TwoDDrawLabels();
    return 1;
}


/// Activate actual label drawing, esp. set up drawing callback
void TwoDActivate ()
{
    // Register the actual drawing func.
    // TODO: This is a deprecated call!
    XPLMRegisterDrawCallback(CPLabelDrawing,
                             xplm_Phase_Window,
                             0,                        // after
                             nullptr);
}


/// Deactivate actual label drawing, esp. stop drawing callback
void TwoDDeactivate ()
{
    // Unregister the drawing callback
    // TODO: This is a deprecated call!
    XPLMUnregisterDrawCallback(CPLabelDrawing, xplm_Phase_Window, 0, nullptr);
}


// Initialize the module
void TwoDInit ()
{
    // initialize dataRef handles:
    drMatrixWrld   = XPLMFindDataRef("sim/graphics/view/world_matrix");
    drMatrixProj   = XPLMFindDataRef("sim/graphics/view/projection_matrix_3d");
    drScreenWidth  = XPLMFindDataRef("sim/graphics/view/window_width");
    drScreenHeight = XPLMFindDataRef("sim/graphics/view/window_height");
    drVisibility   = XPLMFindDataRef("sim/graphics/view/visibility_effective_m");
    if (!drVisibility)
        drVisibility    = XPLMFindDataRef("sim/weather/visibility_effective_m");
    drFieldOfView  = XPLMFindDataRef("sim/graphics/view/field_of_view_deg");
    
    // Register the drawing callback if need be
    if (glob.bDrawLabels)
        TwoDActivate();
}

// Grace cleanup
void TwoDCleanup ()
{
    // Remove drawing callbacks
    TwoDDeactivate();
}


}  // namespace XPMP2

//
// MARK: General API functions outside XPMP2 namespace
//

using namespace XPMP2;

// Enable/Disable/Query drawing of labels
void XPMPEnableAircraftLabels (bool _enable)
{
    // Only do anything if this actually is a change to prevent log spamming
    if (glob.bDrawLabels != _enable) {
        LOG_MSG(logDEBUG, DEBUG_ENABLE_AC_LABELS, _enable ? "enabled" : "disabled");
        glob.bDrawLabels = _enable;
        
        // Start/stop drawing as requested
        if (glob.bDrawLabels)
            TwoDActivate();
        else
            TwoDDeactivate();
    }
}

void XPMPDisableAircraftLabels()
{
    XPMPEnableAircraftLabels(false);
}

bool XPMPDrawingAircraftLabels()
{
    return glob.bDrawLabels;
}

