/*
Copyright 2016 Stepper 3 LLC
Copyright 2016 Eric Alzheimer

Licensed under the GNU GPL version 3.0 license:

This program is free software: you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation, either version 3 of the License, or (at your option) any later
version.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
details.

You should have received a copy of the GNU General Public License along with
this program.  If not, see <https://www.gnu.org/licenses/>.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

/************************************************************ 
 * ftgl.h - FT800 graphics layer
 * -------------------------------------------------------
 *  This implements a graphics api on top the the fthw platform layer. It
 *  presents an API simliar to the OpenGL fixed function pipeline, since
 *  the FT800 itself is stylistically similar. 
 *
 *  All graphics commands write to the FT800's command queue (if needed).
 *  If the queue is full, a command will block until it empties. As per the
 *  FTGL_CONFIG_USE_WRITE_BUFFER option, the commands may be buffered on the
 *  microcontroller and sent to the FT800 in a bulk write to improve
 *  throughput.
 *
 *  A brief description of using the API:
 *  1. Initialize FTGL.
 *  2. Call FTGLBeginBuffer to start drawing.
 *  3. Call the desired functions to draw your scene.
 *  4. Call FTGLSwapBuffers to present the scene and block until VSYNC. 
 *
 *  FTGLInitialize();
 *
 *  FTGLBeginBuffer();
 *
 *  FTGLBegin(FT_POINTS);
 *      FTGLVertex2ii(10, 10);
 *      FTGLVertex2ii(20, 20);
 *  FTGLEnd();
 *
 *  FTGLCmdButton(20, 30, 64, 64, 
 *      FONT_SMOOTH_BIG, OPT_CENTERX, "Free Real Estate");
 *
 *  FTGLBlendFunc(FT_SRC_ALPHA, FT_ONE_MINUS_SRC_ALPHA);
 *
 *  // High level bitmap drawing, manages handles for you
 *  FTGLCmdBitmap(myBitmapId, 30, 30);
 *
 *  // Lower level bitmap drawing, manage handles manually
 *  // With many bitmap draws, reduces command overhead.
 *  FTGLSetBitmapHandle(0, myBitmapId);
 *  FTGLSetBitmapHandle(1, myOtherBitmapId);
 *  FTGLBegin(FT_BITMAPS);
 *      FTGLVertex2ii(30, 30, 0, 0);
 *      FTGLVertex2ii(30, 30, 1, 0);
 *  FTGLEnd();
 *
 *  FTGLBegin(FT_LINES);
 *      FTGLVertex2f(37, 37); // 1/16th pixel units. The f is for fixed point,
 *      FTGLVertex2f(63, 63); // not floating
 *  FTGLEnd();
 *
 *  FTGLSwapBuffers();
 *  
 ***********************************************************/ 
#ifndef FTGL_H
#define FTGL_H

#include "ftgl_config.h"
#include "FT800.h"
#include "fthw.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef min
#define min(x, y) ((x) < (y) ? (x) : (y))
#endif

#define f4(x) ((x) << 4)
#define f8(x) ((x) << 8)
#define f16(x) ((x) << 16)

// TODO(eric): Calculate ram usage from defines and fail if too large
#define FTGL_MAX_BITMAPS                FTGL_CONFIG_MAX_BITMAPS
#define FTGL_CACHE_GRAPHICS_CONTEXT     FTGL_CONFIG_CACHE_GRAPHICS_CONTEXT
#define FTGL_CACHE_BITMAP_HANDLES       FTGL_CONFIG_CACHE_BITMAP_HANDLES
#define FTGL_CACHE_COMMAND_CONTEXT      FTGL_CONFIG_CACHE_COMMAND_CONTEXT
#define FTGL_CONTEXT_STACK_DEPTH        FTGL_CONFIG_CONTEXT_STACK_DEPTH      
#define FTGL_DEFAULT_SENSITIVITY        FTGL_CONFIG_DEFAULT_SENSITIVITY

#if FTGL_CONFIG_DISPLAY_TYPE == FTGL_DISPLAY_WQVGA
    #define FT_DISPLAY_VSYNC0 				FT_DISPLAY_VSYNC0_WQVGA 
    #define FT_DISPLAY_VSYNC1 				FT_DISPLAY_VSYNC1_WQVGA 
    #define FT_DISPLAY_VOFFSET				FT_DISPLAY_VOFFSET_WQVGA
    #define FT_DISPLAY_VCYCLE 				FT_DISPLAY_VCYCLE_WQVGA 
    #define FT_DISPLAY_HSYNC0 				FT_DISPLAY_HSYNC0_WQVGA 
    #define FT_DISPLAY_HSYNC1 				FT_DISPLAY_HSYNC1_WQVGA 
    #define FT_DISPLAY_HOFFSET 				FT_DISPLAY_HOFFSET_WQVGA
    #define FT_DISPLAY_HCYCLE 				FT_DISPLAY_HCYCLE_WQVGA 
    #define FT_DISPLAY_HSIZE				FT_DISPLAY_HSIZE_WQVGA 	
    #define FT_DISPLAY_VSIZE				FT_DISPLAY_VSIZE_WQVGA 	
    #define FT_DISPLAY_PCLKPOL 				FT_DISPLAY_PCLKPOL_WQVGA
    #define FT_DISPLAY_SWIZZLE 				FT_DISPLAY_SWIZZLE_WQVGA
    #define FT_DISPLAY_PCLK					FT_DISPLAY_PCLK_WQVGA 	
#elif FTGL_CONFIG_DISPLAY_TYPE == FTGL_DISPLAY_QVGA 
    #define FT_DISPLAY_VSYNC0 				FT_DISPLAY_VSYNC0_QVGA 
    #define FT_DISPLAY_VSYNC1 				FT_DISPLAY_VSYNC1_QVGA 
    #define FT_DISPLAY_VOFFSET				FT_DISPLAY_VOFFSET_QVGA
    #define FT_DISPLAY_VCYCLE 				FT_DISPLAY_VCYCLE_QVGA 
    #define FT_DISPLAY_HSYNC0 				FT_DISPLAY_HSYNC0_QVGA 
    #define FT_DISPLAY_HSYNC1 				FT_DISPLAY_HSYNC1_QVGA 
    #define FT_DISPLAY_HOFFSET 				FT_DISPLAY_HOFFSET_QVGA
    #define FT_DISPLAY_HCYCLE 				FT_DISPLAY_HCYCLE_QVGA 
    #define FT_DISPLAY_HSIZE				FT_DISPLAY_HSIZE_QVGA 	
    #define FT_DISPLAY_VSIZE				FT_DISPLAY_VSIZE_QVGA 	
    #define FT_DISPLAY_PCLKPOL 				FT_DISPLAY_PCLKPOL_QVGA
    #define FT_DISPLAY_SWIZZLE 				FT_DISPLAY_SWIZZLE_QVGA
    #define FT_DISPLAY_PCLK					FT_DISPLAY_PCLK_QVGA 	
#else
    #define FT_DISPLAY_VSYNC0 				FT_DISPLAY_VSYNC0_CUSTOM 
    #define FT_DISPLAY_VSYNC1 				FT_DISPLAY_VSYNC1_CUSTOM 
    #define FT_DISPLAY_VOFFSET				FT_DISPLAY_VOFFSET_CUSTOM
    #define FT_DISPLAY_VCYCLE 				FT_DISPLAY_VCYCLE_CUSTOM 
    #define FT_DISPLAY_HSYNC0 				FT_DISPLAY_HSYNC0_CUSTOM 
    #define FT_DISPLAY_HSYNC1 				FT_DISPLAY_HSYNC1_CUSTOM 
    #define FT_DISPLAY_HOFFSET 				FT_DISPLAY_HOFFSET_CUSTOM
    #define FT_DISPLAY_HCYCLE 				FT_DISPLAY_HCYCLE_CUSTOM 
    #define FT_DISPLAY_HSIZE				FT_DISPLAY_HSIZE_CUSTOM 	
    #define FT_DISPLAY_VSIZE				FT_DISPLAY_VSIZE_CUSTOM 	
    #define FT_DISPLAY_PCLKPOL 				FT_DISPLAY_PCLKPOL_CUSTOM
    #define FT_DISPLAY_SWIZZLE 				FT_DISPLAY_SWIZZLE_CUSTOM
    #define FT_DISPLAY_PCLK					FT_DISPLAY_PCLK_CUSTOM 	
#endif

// Shorter macros for width and height for user code math
#define FTGL_WIDTH FT_DISPLAY_HSIZE
#define FTGL_HEIGHT FT_DISPLAY_VSIZE
    
#define FTGL_NUM_BITMAP_HANDLES 15

/************************************************************ 
 * FTGL FUNCTION DECLARATIONS
 ***********************************************************/ 

#define FTGL_SUCCESS(x) ((x) >= 0)

/**
 * Initializes the hardware layer (fthw.h, fthw_platform_*.c[pp]) and then
 * attempts to set up the ft800. If it is unable to communicate with the
 * FT800, it will return -1, otherwise 0.
 *
 * Call this first.
 */
int FTGLInitialize(void);

/**
 * Returns the number of milliseconds that have passed since initialization.
 * This is not a meaningful absolute timestamp, but available for
 * measuring time deltas for delays, animations, etc.
 */
int32_t FTGLGetTicks(void);

/**
 * This function is called to begin the rendering of a frame.
 *
 * Initializes a new buffer of commands to run on the FT800. All command and
 * rendering functions must be in between calls to FTGLBeginBuffer and FTGLSwapBuffers
 */
void FTGLBeginBuffer(void);

/** 
 * This function is called to end the rendering of a frame and wait for it to
 * render.
 *
 * Adds a CMD_SWAP command to the buffer and then updates the write index to make 
 * the command processor begin executing the commands in the buffer.
 * 
 * All command functions and rendering functions must be between calls to
 * FTGLBeginBuffer and FTGLSwapBuffers.
 * 
 * In addition to rendering the frame, this function also updates the
 * touch information
 */
void FTGLSwapBuffers(void);

/**
 * Returns true if there is currently a touch.
 *
 * The FT800 samples touches every vsync, so
 * this value is updated by FTGLSwapBuffers, right
 * after the frame was rendered.
 */
int FTGLHasTouch(void);

/**
 * If FTGLHasTouch() is true, returns the X
 * coordinate of the touch. If FTGLHasTouch() is false, contains nonsense.
 *
 * The FT800 samples touches every vsync, so
 * this value is updated by FTGLSwapBuffers, right
 * after the frame was rendered.
 */
int FTGLTouchX(void);

/**
 * If FTGLHasTouch() is true, returns the Y
 * coordinate of the touch. If FTGLHasTouch() is false, contains nonsense.
 *
 * The FT800 samples touches every vsync, so
 * this value is updated by FTGLSwapBuffers, right
 * after the frame was rendered.
 */
int FTGLTouchY(void);

/**
 * Returns the current tag number. The tag number, if non-zero, is the value of the
 * tag buffer at the point the user is currently touching. If zero, the user is not 
 * touching the screen at the moment.
 */
int FTGLTouchTag(void);

////////////////////////////////////////////////////////
// Display list commands (primitives and low level stuff)
// Use these only between BeginBuffer and SwapBuffers
// See the FT800 programmers manual for full docs

//// Vertex lists:
// Start a vertex list to render the given primitive type.
// Types are FT_POINTS, FT_BITMAPS, FT_LINES,
// FT_LINE_STRIP, FT_EDGE_STRIP_R, FT_EDGE_STRIP_L, 
// FT_EDGE_STRIP_A, FT_EDGE_STRIP_B, FT_RECTS
void FTGLBegin(uint8_t primitiveType); 
void FTGLVertex2ii(uint16_t x, uint16_t y, uint8_t handle, uint8_t cell); // Integer coordinate vertex, with bitmap handle and cell
void FTGLVertex2f(uint16_t x, uint16_t y); // 11.4 fixed point coordinate vertex (ie, if you want 35.5, use (int)(16*35.5))
void FTGLEnd(void); // Ends a vertex list

//// Bitmaps:
// Set the current bitmap handle. All bitmap config commands will now effect this handle.
// Any Vertex2f calls will implicitly use this handle
void FTGLBitmapHandle(uint8_t handle);
void FTGLBitmapLayout(uint8_t format, uint16_t linestride, uint16_t height); // Set memory format and layout
void FTGLBitmapSize(uint8_t filter, uint8_t wrapx, uint8_t wrapy, uint16_t width, uint16_t height); // Set size of rendered area
void FTGLBitmapSource(uint32_t sourceAddress); // Set location in RAM_G to read bitmap from
void FTGLBitmapCell(uint8_t cell); // Select the current cell number. Used implicity for each Vertex2f

// Transforms: Sets values in the matrix:
/*
    [[ A B C ]
     [ D E F ]]
*/
// Used to apply scaling, rotation, shearing, etc
// All values are 8.8 signed fixed point (17 bits total)
void FTGLBitmapTransformA(uint32_t a);
void FTGLBitmapTransformB(uint32_t b);
void FTGLBitmapTransformC(uint32_t c);
void FTGLBitmapTransformD(uint32_t d);
void FTGLBitmapTransformE(uint32_t e);
void FTGLBitmapTransformF(uint32_t f);

//// FUNC and CLEAR values
void FTGLAlphaFunc(uint8_t func, uint8_t refValue); // Set the function and threshold for the alpha test
void FTGLBlendFunc(uint8_t src, uint8_t dst); // Same as openGL, by the way
void FTGLClearColorA(uint8_t alpha); // Sets the value the alpha channel is cleared to
void FTGLClearColorRGB(uint32_t color); // Sets the color that the screen is cleared to
void FTGLClearColorRGBComponents(uint8_t r, uint8_t g, uint8_t b); // Convenience function wrapping the one above
void FTGLClearStencil(uint8_t val); // Clear value for the stencil buffer
void FTGLClearTag(uint8_t tag); // Clear value for the tag buffer

//// Clear
typedef enum { FT_CLEAR_C = 4, FT_CLEAR_S = 2, FT_CLEAR_T = 1 } ClearFlags;
void FTGLClear(ClearFlags flags);

//// Current Colors (these are the colors used when drawing primitives
void FTGLColorA(uint8_t alpha); // set the current alpha
void FTGLColorRGB(uint32_t color); // set the current color
void FTGLColorRGBComponents(uint8_t r, uint8_t g, uint8_t b); 

typedef enum { FT_MASK_A = 1, FT_MASK_B = 2, FT_MASK_G = 4, FT_MASK_R = 8 } MaskFlags;
void FTGLColorMask(MaskFlags flags); // sets the color mask

//// Control flow
// Ends the display list
void FTGLDisplay(void); // Not needed, since SwapBuffers does this for you
void FTGLCall(uint16_t dest); // procedure call to another location in the display list
void FTGLReturn(void); // returns from a procedure call
void FTGLMacro(uint8_t m); // Run the command in macro register 0 or 1

//// Context Saving
void FTGLSaveContext(void);
void FTGLRestoreContext(void);

// Primitive params
void FTGLLineWidth(uint16_t width); // Set the width of FT_LINES primitives. 8.4 fixed point
void FTGLPointSize(uint32_t size); // Set the radius of FT_POINTS primitives. 13.4 fixed point

// SCISSOR, STENCIL, TAG
void FTGLScissorSize(uint16_t width, uint16_t height); // Sets the size of the scissor rect
void FTGLScissorXY(uint16_t x, uint16_t y); // Set the (x, y) coord of the top left corner of the scissor rect
void FTGLStencilFunc(uint8_t func, uint8_t ref, uint8_t mask); // Set the function, reference level and mask of the stencil
void FTGLStencilOp(uint8_t sfail, uint8_t spass); // Choose the opration performed on the stencil buffer when the test fails or passes
void FTGLTag(uint8_t tag); // Set the tag value for all the following drawing
void FTGLTagMask(uint8_t on); // Turn on or off writing to the tag buffer

///////////////////////////////////////////////////////////////////
// Command processor commands
// Currently, not all commands are present, since they are not used in the ftui

void FTGLCmdDLStart(void); // Starts a display list. NOT NEEDED. BeginBuffer does this for you
void FTGLCmdSwap(void); // Ends the display list and displays it. NOT NEEDED. SwapBuffers does this for you
void FTGLCmdColdStart(void); // Restores coprocessor state to defaults
void FTGLCmdInflate(uint32_t ptr, uint8_t *data, uint32_t count); // decompress deflate archive into memory at ptr
void FTGLCmdLoadImage(uint32_t ptr, uint32_t options, uint8_t *data, uint32_t count); // decompress a jpeg into memory at ptr. 
void FTGLCmdButton(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t font, uint16_t options, const char *str, uint16_t len); // Draw a button
void FTGLCmdClock(int16_t x, int16_t y, int16_t radius, uint16_t options, uint16_t h, uint16_t m, uint16_t s, uint16_t ms);
void FTGLCmdFGColor(uint32_t color);
void FTGLCmdBGColor(uint32_t color);
void FTGLCmdGradColor(uint32_t color);
void FTGLCmdGauge(int16_t x, int16_t y, int16_t r, uint16_t options, uint16_t major, uint16_t minor, uint16_t val, uint16_t range);
void FTGLCmdGradient(int16_t x0, int16_t y0, uint32_t rgb0, int16_t x1, int16_t y1, uint32_t rgb1);
void FTGLCmdKeys(int16_t x, int16_t y, int16_t w, int16_t h, int16_t font, uint16_t options, const char* s, uint16_t len);
void FTGLCmdProgress(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t options, uint16_t val, uint16_t range);
void FTGLCmdScrollbar(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t options, uint16_t val, uint16_t size, uint16_t range);
void FTGLCmdSlider(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t options, uint16_t val, uint16_t range);
void FTGLCmdDial(int16_t x, int16_t y, int16_t r, uint16_t options, uint16_t val);
void FTGLCmdToggle(int16_t x, int16_t y, int16_t w, int16_t font, uint16_t options, uint16_t state, const char* s, uint16_t len);
void FTGLCmdText(int16_t x, int16_t y, int16_t font, uint16_t options, const char* s, uint16_t len);
void FTGLCmdNumber(int16_t x, int16_t y, int16_t font, uint16_t options, int32_t n);
void FTGLCmdLoadIdentity(void);
void FTGLCmdTranslate(int32_t tx, int32_t ty);
void FTGLCmdScale(int32_t sx, int32_t sy); 
void FTGLCmdRotate(int32_t a);
void FTGLCmdSetMatrix(void);
// CmdCalibrate not available as a command, instead, call FTGLRunCalibration(&output) to run a calibration routine
void FTGLCmdSpinner(int16_t x, int16_t y, uint16_t style, uint16_t scale);
void FTGLCmdScreensaver(void);
void FTGLCmdSketch(int16_t x, int16_t y, uint16_t w, uint16_t h, uint32_t ptr, uint16_t format);
void FTGLCmdStop(void);
void FTGLCmdSetFont(uint32_t font, uint32_t ptr);
void FTGLCmdTrack(int16_t x, int16_t y, int16_t w, int16_t h, int16_t tag);
void FTGLCmdSnapshot(uint32_t ptr);
void FTGLCmdLogo(void);

////////////////////////////////////////////////////////////////////
// Bitmap handling

// Bitmap management involves:
// * Bitmap IDs: integers used by FTGL to keep track of bitmaps.
//               When you make a bitmap, FTGL gives you an id. With this id,
//               you can set params for the bitmap, which FTGL stores and
//               upload bitmap data to the FT800 RAM_G memory.
// * Bitmap handles: 
//     - The FT800 has a small number of bitmap handles. When vertices are
//       specified in an FT_BITMAPS list, each vertex takes a bitmap handle.
//       The FT800 uses the information in the selected handle to render a
//       bitmap at the location of the vertex.
//     - In order to use a bitmap loaded into RAM_G, you have to set up the
//       handle to point to it and configure the handle with the bitmap's width
//       and height, etc.
//     - To support using many bitmaps despite a small number of handles, you
//       have to swap bitmap info in and out of the handles.
//     - To make this process easier, FTGL's bitmap system can manage handles
//       for you automatically. You only need to get an id from CreateBitmap.
//       When you pass the id to CmdBitmap or UseBitmap, FTGL will find a free
//       handle to load the bitmap info into or evict a less used handle.
//     - Alternatively, lower level api calls can be used to manage handles
//       manually.

// Creates a bitmap with the given parameters and allocates space for it 
// in the RAM_G area of the FT800. Returns the bitmap id, used to refer to the
// bitmap in other functions.
int FTGLCreateBitmap(uint8_t format, int widthInPixels, int heightInLines);

// These can be used to change the parameters of a bitmap.
// This will invalidate any bitmap handle that currently has this image
// loaded, forcing the bitmap to be reloaded.
void FTGLSetBitmapParams(int bitmapId, uint8_t filter, uint8_t wrapx, uint8_t wrapy);
void FTGLSetBitmapSize(int bitmapId, uint16_t sizeWidth, uint16_t sizeHeight);

// Also creates a bitmap and returns its id, but allows the user to configure the options directly and individually 
// instead of calculating it for them. See the FT800 programmer's guide for
// details on all these settings.
int FTGLCreateBitmapVerbose(uint8_t format, uint16_t linestride, uint16_t layoutHeight, uint16_t numCells,
    uint8_t filter, uint8_t wrapx, uint8_t wrapy, uint16_t sizeWidth, uint16_t sizeHeight);

// Psuedo command to draw a bitmap in one call. Highest level
// If you have FTGL_CONFIG_CACHE_BITMAP_HANDLES off, this command very
// inefficiently always loades the bitmap into handle 0 before rendering it
// If you have it on, it will render it out of its current handle if it is
// loaded into one, otherwise, it will find a free handle or evict an existing bitmap
void FTGLCmdBitmap(int bitmapId, int x, int y);
void FTGLCmdBitmapCell(int bitmapId, int x, int y, int cell);

#if FTGL_CACHE_BITMAP_HANDLES == 1
// Medium level - ask FTGL to find a handle to load your bitmap into.
// Then, you are responsible for drawing it with the FT_BITMAPS primitive 
// or FTGLDrawBitmapInHandle
//
// Returns the handle number that was selected.
// If you try to load more than FTGL_NUM_BITMAP_HANDLES at once, 
// you will end up evicting one of the ones you loaded.
int8_t FTGLUseBitmap(int bitmapId);

// Lower level command to get an empty handle to do what you want with
// without messing up the automatic loading of bitmaps.
// This is not persistent - it will get evicted to make room if there
// are later calls to CmdBitmap/UseBitmap, so use this as soon as you
// get it and then don't use it again.
int8_t FTGLGetEmptyHandle(void);
#endif

// Lower level command to load a bitmap into a manually selected handle
int8_t FTGLSetBitmapHandle(int8_t handle, int bitmapId);

// Lower level command to draw the bitmap loaded into the given handle
void FTGLDrawBitmapInHandle(int8_t handle, int x, int y, int cell);

void FTGLGetBitmapSize(int bitmapId, int *width, int *height);

/////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////
// NOTE: Unlike all of the functions above, these below must be used outside of the
// BeginBuffer/SwapBuffer calls. The pallete should be set up before you start
// rendering with it. Since the rendering doesn't happen until swapbuffers and
// after all commands have been executed, you can only use one pallete per
// frame anyways.
/////////////////////////////////////////////////////////

// Load bitmap data into RAM_G memory
// offset selects where in the bitmaps memory to write the data, count is the
// size of the data in bytes.
// Ex. To upload a whole bitmap:
//
// int id = FTGLCreateBitmap(FT_RGB565, 64, 64);
// FTGLBitmapBufferData(id, 0, bitmap, sizeof(uint16_t) * 64 * 64);
void FTGLBitmapBufferData(int bitmapId, uint32_t offset, const uint8_t *data, uint32_t count);

// LoadPalleteData is for loading entire palletes in a single operation.
// It assumes that the endianess of the uint32_t's matches that of the
// FT800 (ie, they are all little endian)
void FTGLLoadPalleteData(uint8_t offset, uint32_t *colors, uint8_t count);

// SetPalleteColor changes a single color in the pallete. It will correct for the
// endianess difference between the FT800 and the host platform
void FTGLSetPalleteColor(uint8_t value, uint32_t color);

////////////////////////////////////////////////////////
//// Calibration routine

// This is a blocking call that runs the calibration routine and loads the results into the 
// transform registers
void FTGLRunCalibration(void); 

// Load the 6 touch transform register values into the given array
void FTGLGetTouchCalibrationParams(uint32_t params[6]);

// Write the 6 touch transform values (a,b,c,d,e,f in that order) from
// the given array to the registers
void FTGLSetTouchCalibrationParams(uint32_t params[6]);

// Set the touchscreen sensitivity, where 0 registers no touches and
// 65535 is max sensisivity. Default is 1200
void FTGLSetTouchSensitivity(uint16_t sens);

#ifdef __cplusplus
}
#endif

#endif
