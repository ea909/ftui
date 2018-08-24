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
 * ftgl.c - FT800 graphics layer
 * -------------------------------------------------
 *  Implementation of the graphics layer api for the FT800. See ftgl.h for
 *  more details
 ***********************************************************/ 
#include "ftgl.h"
#include <string.h>

// To get some extra logging info on Arduino, and a slow
// step by step initialization, uncomment this, and
// rename the file to have a *.cpp extension so that
// it can compile as c++
//#define ARDUINO_DEBUG

#if defined(ARDUINO) && defined(ARDUINO_DEBUG) && __cplusplus
#   include <stdio.h>
#   include <stdarg.h>
#   include <Arduino.h>
    static void log(const char *file, int line, const char *fmt, ...) {
        char buf[128];
        va_list args;
        va_start(args, fmt);
        buf[127] = '\0';
        vsnprintf(buf, 127, fmt, args);
        va_end(args);
        Serial.print(buf);;
        Serial.print(" | ");
        Serial.print(file);
        Serial.print(":");
        Serial.println(line);
    }
#else
#   define log(...)
#   define delay(x) 
#endif

#define FTGL_CMD_QUEUE_SIZE 4092
#define FTGL_QUEUE_MASK 0xFFF

typedef enum {
    BMP_TRANSFORM_A,
    BMP_TRANSFORM_B,
    BMP_TRANSFORM_C,
    BMP_TRANSFORM_D,
    BMP_TRANSFORM_E,
    BMP_TRANSFORM_F
} BitmapTransformIndex;

typedef struct {
    // These often contain multiple packed parameters, and are
    // arranged and named to match the display list command that changes them.

    // Display Engine State
    uint32_t alphaFunc;
    uint32_t stencilFunc;
    uint32_t blendFunc;
    uint32_t bitmapCell;
    uint32_t colorAlpha;
    uint32_t colorRGB;
    uint32_t lineWidth;
    uint32_t pointSize;
    uint32_t scissorSize;
    uint32_t scissorXY;
    uint32_t bitmapHandle;

    // We do not cache the bitmap transform, since it can get changed by
    // the CMD_SETMATRIX command, and it is computationally expensive to
    // determine what it changed things to.

    uint32_t clearStencil;
    uint32_t clearTag;
    uint32_t stencilOp;
    uint32_t tag;
    uint32_t tagMask;
    uint32_t clearColorAlpha;
    uint32_t clearColorRGB;
} GraphicsContext;

typedef struct {
    // Coprocessor State
    uint32_t bgColor;
    uint32_t fgColor;
    uint32_t gradColor;

    // If there is already a continuous command active (one of spinner, screensaver or sketch), we
    // should stop the one running first
    uint8_t continuousCommandActive;

    // The coprocessor command CMD_TRACK can only track one thing at a time,
    // so there will not be a large number of calls to it. As such, it is not
    // cached

    // The coprocessor stores a transform matrix, but all of the operations on
    // it are transforms applied to the current values, 
    // so caching it here does not eliminate any operations.

    // The coprocessor stores a bitmap handle, but you have to provide it in
    // every call that uses it, so there is no use caching it.
    // uint8_t cmdBitmapHandle;
} CoprocessorContext;

typedef struct {
    uint32_t bitmapAddress;
    uint32_t bitmapDataSize;

    // Pre-formatted commands to
    // write bitmap info to handle
    uint32_t bitmapLayout;
    uint32_t bitmapSize;

    // >= 0 value indicates a handle with this bitmap's settings loaded into
    // it.
    int8_t activeHandle;

} BitmapInfo;

typedef struct {
    uint16_t cmdQueueReadIndex;
    uint16_t cmdQueueWriteIndex;
    uint16_t cmdQueueFreeSpace;

#if FTGL_CACHE_GRAPHICS_CONTEXT == 1
#define FTGL_CONTEXT_STACK_SIZE (1 + FTGL_CONTEXT_STACK_DEPTH)
    #if FTGL_CONTEXT_STACK_SIZE > 1
        GraphicsContext graphicsContext[FTGL_CONTEXT_STACK_SIZE];
        uint8_t contextStackIndex;
        #define GRAPHICS_CONTEXT(inst) inst.graphicsContext[inst.contextStackIndex]
        #define GRAPHICS_CONTEXT_INDEX(inst, idx) inst.graphicsContext[idx]
    #else
        GraphicsContext graphicsContext;
        #define GRAPHICS_CONTEXT(inst)            inst.graphicsContext
        #define GRAPHICS_CONTEXT_INDEX(inst, idx) inst.graphicsContext
    #endif
#endif


#if FTGL_CACHE_COMMAND_CONTEXT == 1
    CoprocessorContext commandContext;
#endif

    BitmapInfo bitmaps[FTGL_MAX_BITMAPS];

    // Next available bitmap struct
    int16_t bitmapIndex;

#if FTGL_CACHE_BITMAP_HANDLES == 1
    // Maps device bitmap handles to the bitmap currently loaded into them.
    // if < 0, no bitmap is loaded.
    // if >= 0, index the bitmaps array.
    int16_t bitmapHandles[FTGL_NUM_BITMAP_HANDLES];

    // The last used handle. If all of the handles are taken, a bitmap must be
    // evicted. We will evict the first bitmap handle we find that isn't
    // lastHandle. This provides a semi-LRU behavior to the bitmap
    // replacement, to at least eliminate the pathological case of alternating
    // between two images.
    int8_t lastHandle; 

    // The next index to look at when we need to either grab an
    // empty handle or evict a bitmap.
    // The next index to look at is the one after the last
    // one we used. 
    int8_t nextHandle;
#endif

    // Next free space in graphics ram for allocating bitmaps
    uint32_t graphicsRamIndex;

    // True if there currently is a finger touching the screen;
    uint8_t hasTouch;
    
    // Coordinates, only meaningful when hasTouch == TRUE
    uint16_t touchX, touchY;

    // The current tag value.
    // By properly using the TAG display list item, you can mark pixels as they
    // are drawn with a tag number.
    // When the user touches one of those pixels, this will be the tag number.
    uint8_t touchTag;

} FTGLInstance;

// THE GLOBAL INSTANCE
static FTGLInstance g_Inst;

//////////////////////////////////////////////////////
// Functions to do single immediate reads or writes
// These are used to read and write to registers.

static uint8_t ReadReg8(uint32_t addr) {
    uint8_t out;
    FTHWRead(addr, &out, sizeof(uint8_t));
    return out;
}

static uint16_t ReadReg16(uint32_t addr) {
    uint16_t out;
    FTHWRead(addr, (uint8_t*)&out, sizeof(uint16_t));
    return FT_TO_HOST_USHORT(out);
}

static uint32_t ReadReg32(uint32_t addr) {
    uint32_t out;
    FTHWRead(addr, (uint8_t*)&out, sizeof(uint32_t));
    return FT_TO_HOST_ULONG(out);
}

static void WriteReg8(uint32_t addr, uint8_t val) {
    FTHWWrite(addr, &val, sizeof(uint8_t));
}

static void WriteReg16(uint32_t addr, uint16_t val) {
    val = HOST_TO_FT_USHORT(val);
    FTHWWrite(addr, (uint8_t*)&val, sizeof(uint16_t));
}

static void WriteReg32(uint32_t addr, uint32_t val) {
    val = HOST_TO_FT_ULONG(val);
    FTHWWrite(addr, (uint8_t*)&val, sizeof(uint32_t));
}


// Whenever the cmd queue is full, or at the
// end of drawing a frame, we must wait for all of
// the commands added so far to finish executing.
static void WaitForQueueEmpty(void) {
    log(__FILE__, __LINE__, "Waiting for empty queue");
    do {
        g_Inst.cmdQueueReadIndex = ReadReg16(FT_REG_CMD_READ);
        g_Inst.cmdQueueWriteIndex = ReadReg16(FT_REG_CMD_WRITE);
        // TODO(eric): Do I need to read the write index?
    } while (g_Inst.cmdQueueReadIndex != g_Inst.cmdQueueWriteIndex);

    g_Inst.cmdQueueFreeSpace = FTGL_CMD_QUEUE_SIZE;
    log(__FILE__, __LINE__, "Queue empty r(%d) w(%d) f(%d)",
        g_Inst.cmdQueueReadIndex,
        g_Inst.cmdQueueWriteIndex,
        g_Inst.cmdQueueFreeSpace);
}

// Returns a count of milliseconds since some initialization point
// This is not used for absolute time but to provide a delta between
// each frame.
int32_t FTGLGetTicks(void) { return FTHWGetTicks(); }


// Updates the write index so that the ft800 begins running commands, and then
// waits for all the commands to have been run.
static void FlushCommands(void) {
    log(__FILE__, __LINE__, "Command buffer full, flushing...");
    FTHWEndAppendWrite();
    WriteReg16(FT_REG_CMD_WRITE, g_Inst.cmdQueueWriteIndex);
    WaitForQueueEmpty();
    FTHWBeginAppendWrite(FT_RAM_CMD + g_Inst.cmdQueueWriteIndex);
}

///////////////////////////////////////////////////////
// Functions to write data to the command queue

static void EnsureSpace(int amt) {
    if (g_Inst.cmdQueueFreeSpace < sizeof(amt)) { FlushCommands(); }
}

static uint16_t Aligned(uint16_t size) {
    uint16_t val = size & 0x3;
    if (val != 0) {
        return size + (4 - val);
    } else {
        return size;
    }
}

static void Append32(uint32_t val) {
    val = HOST_TO_FT_ULONG(val);
    FTHWAppendWrite((uint8_t*)&val, sizeof(uint32_t));
    g_Inst.cmdQueueWriteIndex = (g_Inst.cmdQueueWriteIndex + 4) & FTGL_QUEUE_MASK;
    g_Inst.cmdQueueFreeSpace -= 4;
}

// All display list commands are 32 bit, so they use this call to ensure space and
// write with one call
static void DLCommand(uint32_t val) {
    EnsureSpace(sizeof(uint32_t));
    Append32(val);
}

static void Append16(uint16_t val) {
    val = HOST_TO_FT_USHORT(val);
    FTHWAppendWrite((uint8_t*)&val, sizeof(uint16_t));
    g_Inst.cmdQueueWriteIndex = (g_Inst.cmdQueueWriteIndex + 2) & FTGL_QUEUE_MASK;
    g_Inst.cmdQueueFreeSpace -= 2;
}

/* Not used
static void Append8(uint8_t val) {
    FTHWAppendWrite(&val, sizeof(uint8_t));
    g_Inst.cmdQueueWriteIndex = (g_Inst.cmdQueueWriteIndex + 1) & FTGL_QUEUE_MASK;
    g_Inst.cmdQueueFreeSpace -= 1;
}
 */

static void AppendString(const uint8_t *data, uint16_t count) {
    FTHWAppendWrite(data, count);
    g_Inst.cmdQueueWriteIndex = (g_Inst.cmdQueueWriteIndex + count) & FTGL_QUEUE_MASK;
    g_Inst.cmdQueueFreeSpace -= count;
}

static void AlignBuffer(void) {
    uint16_t val = g_Inst.cmdQueueWriteIndex & 0x3;
    if (val != 0) {
        uint32_t zero = 0;
        val = 4 - val;
        FTHWAppendWrite((const uint8_t*)&zero, val);
        g_Inst.cmdQueueWriteIndex = (g_Inst.cmdQueueWriteIndex + val) & FTGL_QUEUE_MASK;
        g_Inst.cmdQueueFreeSpace -= val;
    }
}

int FTGLInitialize(void) {
    log(__FILE__, __LINE__, "Initializing FTGL");
    int i;
    memset(&g_Inst, 0, sizeof(g_Inst));
    
#if FTGL_CACHE_GRAPHICS_CONTEXT == 1
    log(__FILE__, __LINE__, "Setting defaults in graphics context");
    for (i = 0; i < FTGL_CONTEXT_STACK_SIZE; i++) {
        GRAPHICS_CONTEXT_INDEX(g_Inst, i).alphaFunc = FT_ALPHA_FUNC(FT_ALWAYS, 0);
        GRAPHICS_CONTEXT_INDEX(g_Inst, i).stencilFunc = FT_STENCIL_FUNC(255, FT_ALWAYS, 0);
        GRAPHICS_CONTEXT_INDEX(g_Inst, i).blendFunc = FT_BLEND_FUNC(FT_SRC_ALPHA, FT_ONE_MINUS_SRC_ALPHA);
        GRAPHICS_CONTEXT_INDEX(g_Inst, i).bitmapCell = FT_CELL(0);
        GRAPHICS_CONTEXT_INDEX(g_Inst, i).colorAlpha = FT_COLOR_A(0);
        GRAPHICS_CONTEXT_INDEX(g_Inst, i).colorRGB = FT_COLOR_RGB(255, 255, 255);
        GRAPHICS_CONTEXT_INDEX(g_Inst, i).lineWidth = FT_LINE_WIDTH(16);
        GRAPHICS_CONTEXT_INDEX(g_Inst, i).pointSize = FT_POINT_SIZE(16);
        GRAPHICS_CONTEXT_INDEX(g_Inst, i).scissorSize = FT_SCISSOR_SIZE(512, 512);
        GRAPHICS_CONTEXT_INDEX(g_Inst, i).scissorXY = FT_SCISSOR_XY(0, 0);
        GRAPHICS_CONTEXT_INDEX(g_Inst, i).bitmapHandle = FT_BITMAP_HANDLE(0);
        GRAPHICS_CONTEXT_INDEX(g_Inst, i).clearStencil = FT_CLEAR_STENCIL(0);
        GRAPHICS_CONTEXT_INDEX(g_Inst, i).clearTag = FT_CLEAR_TAG(0);
        GRAPHICS_CONTEXT_INDEX(g_Inst, i).stencilOp = FT_STENCIL_OP(FT_KEEP, FT_KEEP);
        GRAPHICS_CONTEXT_INDEX(g_Inst, i).tag = FT_TAG(255);
        GRAPHICS_CONTEXT_INDEX(g_Inst, i).tagMask = FT_TAG_MASK(1);
        GRAPHICS_CONTEXT_INDEX(g_Inst, i).clearColorAlpha = FT_CLEAR_COLOR_A(0);
        GRAPHICS_CONTEXT_INDEX(g_Inst, i).clearColorRGB = FT_CLEAR_COLOR_RGB(0, 0, 0);
    }
    
    delay(50);
    
#if FTGL_CONTEXT_STACK_SIZE > 1
    g_Inst.contextStackIndex = 0;
#endif

#endif

#if FTGL_CACHE_COMMAND_CONTEXT == 1
    log(__FILE__, __LINE__, "Settings defaults in command context");
    g_Inst.commandContext.bgColor =   0x002040;
    g_Inst.commandContext.fgColor =   0x003870;
    g_Inst.commandContext.gradColor = 0xffffff;
    g_Inst.commandContext.continuousCommandActive = 0;
    delay(50);
#endif

    log(__FILE__, __LINE__, "Initializing touch and bitmap info");
    g_Inst.lastHandle = -1;
    g_Inst.nextHandle = 0;
    g_Inst.graphicsRamIndex = FT_RAM_G;

    g_Inst.hasTouch = 0;
    g_Inst.touchX = 0;
    g_Inst.touchY = 0;
    g_Inst.touchTag = 0;

    for (i = 0; i < FTGL_MAX_BITMAPS; i++) {
        g_Inst.bitmaps[i].activeHandle = -1;
    }

    for (i = 0; i < FTGL_NUM_BITMAP_HANDLES; i++) {
        g_Inst.bitmapHandles[i] = -1;
    }

    g_Inst.cmdQueueReadIndex = 0;
    g_Inst.cmdQueueWriteIndex = 0;
    g_Inst.cmdQueueFreeSpace = FTGL_CMD_QUEUE_SIZE;
    // NOTE: Command queue index must always be 4 byte aligned
 

    log(__FILE__, __LINE__, "Initializing spi h/w");
    // Initialize hardware devices
    FTHWInitialize();
    FTHWSetSpeed(FTHW_SPI_STARTUP_SPEED);


    log(__FILE__, __LINE__, "Resetting the ft800");
    // Reset the FT800
    FTHWDelayMS(20);
    FTHWSetReset(1);
    FTHWDelayMS(20);
    FTHWSetReset(0);
    FTHWDelayMS(20);


    log(__FILE__, __LINE__, "Starting the ft800");
    // Start the ft800
    FTHWHostCommand(FT_HOSTCOMMAND_ACTIVE);
    FTHWDelayMS(6);
    FTHWHostCommand(FT_HOSTCOMMAND_CLKEXT);
    FTHWDelayMS(6);
    FTHWHostCommand(FT_HOSTCOMMAND_CLK48M);
    FTHWDelayMS(6);


    log(__FILE__, __LINE__, "Testing hardware response");
    // Ensure that SPI comms is working
    uint8_t testVal = ReadReg8(FT_REG_ID);
    if (testVal != 0x7c) {
        log(__FILE__, __LINE__, "Hardware response invalid; got %d", testVal);
        //return -1; // TODO(eric): Define error codes in header file
    }

    log(__FILE__, __LINE__, "Start with display off and unclocked");
    // Start with display off and unclocked
    WriteReg8(FT_REG_PCLK, FT_ZERO);
    WriteReg8(FT_REG_PWM_DUTY, FT_ZERO); // Turns off backlight
    // TODO(eric): Include API function to control backlight
    delay(50);

    log(__FILE__, __LINE__, "Configure display parameters");
    // Configure the display
    WriteReg16(FT_REG_VSYNC0, FT_DISPLAY_VSYNC0);
    WriteReg16(FT_REG_VSYNC1, FT_DISPLAY_VSYNC1);
    WriteReg16(FT_REG_VOFFSET, FT_DISPLAY_VOFFSET);
    WriteReg16(FT_REG_VCYCLE, FT_DISPLAY_VCYCLE);
    WriteReg16(FT_REG_HSYNC0, FT_DISPLAY_HSYNC0);
    WriteReg16(FT_REG_HSYNC1, FT_DISPLAY_HSYNC1);
    WriteReg16(FT_REG_HOFFSET, FT_DISPLAY_HOFFSET);
    WriteReg16(FT_REG_HCYCLE, FT_DISPLAY_HCYCLE);
    WriteReg16(FT_REG_HSIZE, FT_DISPLAY_HSIZE);
    WriteReg16(FT_REG_VSIZE, FT_DISPLAY_VSIZE);
    WriteReg8(FT_REG_SWIZZLE, FT_DISPLAY_SWIZZLE);
    WriteReg8(FT_REG_PCLK_POL, FT_DISPLAY_PCLKPOL);


    log(__FILE__, __LINE__, "Set up touch screen sampling");
    // Enable per-frame touch sampling
    WriteReg8(FT_REG_TOUCH_MODE, FT_TMODE_FRAME);
    WriteReg16(FT_REG_TOUCH_RZTHRESH, FTGL_DEFAULT_SENSITIVITY);    // Eliminate any false touches

    // EVENTUALLY: Include an audio api?
    WriteReg8(FT_REG_VOL_PB, 0);
    WriteReg8(FT_REG_VOL_SOUND, 0);
    WriteReg16(FT_REG_SOUND, 0x6000);

    // TODO(eric): Configure interrupt for vsync

    log(__FILE__, __LINE__, "Draw a blank screen.");
    // Draw blank screen
    WriteReg32(FT_RAM_DL, FT_CLEAR_COLOR_RGB(0, 0, 0)); 
    WriteReg32(FT_RAM_DL + 4, FT_CLEAR(1, 1, 1));
    WriteReg32(FT_RAM_DL + 8, FT_DISPLAY());
    WriteReg32(FT_REG_DLSWAP, FT_DLSWAP_FRAME);

    log(__FILE__, __LINE__, "Start the display");

    // Start the display
    uint8_t gpio = ReadReg8(FT_REG_GPIO);	
    gpio |= 0x80;		
    WriteReg8(FT_REG_GPIO, gpio);
    WriteReg8(FT_REG_PCLK, FT_DISPLAY_PCLK);

    /*
    // Enable interrupts
    if (FTHWInterruptAvailable()) {
        WriteReg32(FT_REG_INT_EN, 1);
        ReadReg32(FT_REG_INT_FLAGS);
        WriteReg32(FT_REG_INT_MASK, FT_INT_CMDEMPTY);
        ReadReg32(FT_REG_INT_FLAGS);
    }
    */


    log(__FILE__, __LINE__, "Raise the backlight");
    // Bring up the backlight
    for (i = 0; i <= 128; i++) {
        WriteReg8(FT_REG_PWM_DUTY, i);	
        FTHWDelayMS(10);
    }

    FTHWSetSpeed(FTHW_SPI_RUN_SPEED);

    log(__FILE__, __LINE__, "Done initializing.");

    return 0;
}

void FTGLBeginBuffer() {
    log(__FILE__, __LINE__, "Starting new buffer.");
    FTHWBeginAppendWrite(FT_RAM_CMD + g_Inst.cmdQueueWriteIndex);
    FTGLCmdDLStart();
    FTGLClear(FT_CLEAR_C); 
    // TODO(eric): This clear is here because the first few frames
    // rendered by the FT800 appear to be incorrect (vertically stretched)
    // Instead of requiring a clear every frame, remove this and make initialization
    // draw a number of dummy frames.
}

void FTGLSwapBuffers(void) {
    log(__FILE__, __LINE__, "Swapping buffer.");
    uint32_t val;
    FTGLDisplay();
    FTGLCmdSwap();
    FTHWEndAppendWrite();
    WriteReg16(FT_REG_CMD_WRITE, g_Inst.cmdQueueWriteIndex);
    WaitForQueueEmpty();

    g_Inst.touchTag = (uint8_t)ReadReg16(FT_REG_TOUCH_TAG);
    val = ReadReg32(FT_REG_TOUCH_SCREEN_XY);
    if (val == 0x80008000) {
        g_Inst.hasTouch = 0;
    } else {
        g_Inst.hasTouch = 1;
        g_Inst.touchY = (int16_t)(val & 0xFFFF);
        g_Inst.touchX = (int16_t)((val >> 16) & 0xFFFF);
    }
}

int FTGLHasTouch(void) { return g_Inst.hasTouch; }
int FTGLTouchX(void) { return g_Inst.touchX; }
int FTGLTouchY(void) { return g_Inst.touchY; }
int FTGLTouchTag(void) { return g_Inst.touchTag; }

////////////////////////////////////////////////////////
// Display list commands (primitives and low level stuff)
// Use these only between BeginBuffer and SwapBuffers
// See the FT800 programmers manual for full docs

// If we cache the graphics context, check it and only send 
// commands if they change the context
#ifdef FTGL_CACHE_GRAPHICS_CONTEXT
#define WRITE_DLCMD(cache, value) do {  \
        uint32_t computedValue = value; \
        if (cache != computedValue) { \
            cache = computedValue; \
            DLCommand(computedValue); \
        } \
    } while (0)
#else
#define WRITE_DLCMD(cache, value) DLCommand(value)
#endif

//// Vertex lists:
// Start a vertex list to render the given primitive type.
// Types are FT_POINTS, FT_BITMAPS, FT_LINES,
// FT_LINE_STRIP, FT_EDGE_STRIP_R, FT_EDGE_STRIP_L, 
// FT_EDGE_STRIP_A, FT_EDGE_STRIP_B, FT_RECTS
void FTGLBegin(uint8_t primitiveType) { DLCommand(FT_BEGIN(primitiveType)); }
void FTGLVertex2ii(uint16_t x, uint16_t y, uint8_t handle, uint8_t cell) { DLCommand(FT_VERTEX2II(x, y, handle, cell)); }
void FTGLVertex2f(uint16_t x, uint16_t y) { DLCommand(FT_VERTEX2F(x, y)); }
void FTGLEnd(void) { /* Intentionally empty */ }

//// Bitmaps:
// Set the current bitmap handle. All bitmap config commands will now effect this handle.
// Any Vertex2f calls will implicitly use this handle
void FTGLBitmapHandle(uint8_t handle) { 
    WRITE_DLCMD(GRAPHICS_CONTEXT(g_Inst).bitmapHandle, FT_BITMAP_HANDLE(handle)); 
}
void FTGLBitmapLayout(uint8_t format, uint16_t linestride, uint16_t height) { DLCommand(FT_BITMAP_LAYOUT(format, linestride, height)); }
void FTGLBitmapSize(uint8_t filter, uint8_t wrapx, uint8_t wrapy, uint16_t width, uint16_t height) { DLCommand(FT_BITMAP_SIZE(filter, wrapx, wrapy, width, height)); }
void FTGLBitmapSource(uint32_t sourceAddress) { DLCommand(FT_BITMAP_SOURCE(sourceAddress)); }
void FTGLBitmapCell(uint8_t cell) { WRITE_DLCMD(GRAPHICS_CONTEXT(g_Inst).bitmapCell, FT_CELL(cell)); }

// Transforms: Sets values in the matrix:
/*
[[ A B C ]
[ D E F ]]
*/
// Used to apply scaling, rotation, shearing, etc
// All values are 8.8 signed fixed point (17 bits total)

void FTGLBitmapTransformA(uint32_t a) { DLCommand(FT_BITMAP_TRANSFORM_A(a)); }
void FTGLBitmapTransformB(uint32_t b) { DLCommand(FT_BITMAP_TRANSFORM_B(b)); }
void FTGLBitmapTransformC(uint32_t c) { DLCommand(FT_BITMAP_TRANSFORM_C(c)); }
void FTGLBitmapTransformD(uint32_t d) { DLCommand(FT_BITMAP_TRANSFORM_D(d)); }
void FTGLBitmapTransformE(uint32_t e) { DLCommand(FT_BITMAP_TRANSFORM_E(e)); }
void FTGLBitmapTransformF(uint32_t f) { DLCommand(FT_BITMAP_TRANSFORM_F(f)); }

//// FUNC and CLEAR values
void FTGLAlphaFunc(uint8_t func, uint8_t refValue) { WRITE_DLCMD(GRAPHICS_CONTEXT(g_Inst).alphaFunc, FT_ALPHA_FUNC(func, refValue)); }
void FTGLBlendFunc(uint8_t src, uint8_t dst) { WRITE_DLCMD(GRAPHICS_CONTEXT(g_Inst).blendFunc, FT_BLEND_FUNC(src, dst)); }
void FTGLClearColorA(uint8_t alpha) { WRITE_DLCMD(GRAPHICS_CONTEXT(g_Inst).clearColorAlpha, FT_CLEAR_COLOR_A(alpha)); }

#define FT_CLEAR_COLOR_RGB32(color) ((2UL<<24)|color)
void FTGLClearColorRGB(uint32_t color) { WRITE_DLCMD(GRAPHICS_CONTEXT(g_Inst).clearColorRGB, FT_CLEAR_COLOR_RGB32(color)); }
void FTGLClearColorRGBComponents(uint8_t r, uint8_t g, uint8_t b) { WRITE_DLCMD(GRAPHICS_CONTEXT(g_Inst).clearColorRGB, FT_CLEAR_COLOR_RGB(r, g, b)); }
void FTGLClearStencil(uint8_t val) { WRITE_DLCMD(GRAPHICS_CONTEXT(g_Inst).clearStencil, FT_CLEAR_STENCIL(val)); }
void FTGLClearTag(uint8_t tag) { WRITE_DLCMD(GRAPHICS_CONTEXT(g_Inst).clearTag, FT_CLEAR_TAG(tag)); }

                                //// Clear
#define FT_CLEAR32(flags) ((38UL<<24)|flags)
void FTGLClear(ClearFlags flags) { DLCommand(FT_CLEAR32(flags)); }

//// Current Colors (these are the colors used when drawing primitives
#define FT_COLOR_RGB32(color) ((4UL<<24)|color)
void FTGLColorA(uint8_t alpha) { WRITE_DLCMD(GRAPHICS_CONTEXT(g_Inst).colorAlpha, alpha); }
void FTGLColorRGB(uint32_t color) { WRITE_DLCMD(GRAPHICS_CONTEXT(g_Inst).colorRGB, FT_COLOR_RGB32(color)); }
void FTGLColorRGBComponents(uint8_t r, uint8_t g, uint8_t b) { WRITE_DLCMD(GRAPHICS_CONTEXT(g_Inst).colorRGB, FT_COLOR_RGB(r, g, b)); }

#define FT_COLOR_MASK32(flags) ((32UL<<24)|flags)
void FTGLColorMask(MaskFlags flags) { DLCommand(FT_COLOR_MASK32(flags)); }

//// Control flow
// Ends the display list
void FTGLDisplay(void) { DLCommand(FT_DISPLAY()); }
void FTGLCall(uint16_t dest) { DLCommand(FT_CALL(dest)); }
void FTGLReturn(void) { DLCommand(FT_RETURN()); }
void FTGLMacro(uint8_t m) { DLCommand(FT_MACRO(m)); }

//// Context Saving
void FTGLSaveContext(void) {
#if FTGL_CACHE_GRAPHICS_CONTEXT && FTGL_CONTEXT_STACK_DEPTH > 0
    g_Inst.contextStackIndex++;
    g_Inst.graphicsContext[g_Inst.contextStackIndex] = g_Inst.graphicsContext[g_Inst.contextStackIndex - 1];
#endif
    DLCommand(FT_SAVE_CONTEXT());
}

void FTGLRestoreContext(void) {
#if FTGL_CACHE_GRAPHICS_CONTEXT && FTGL_CONTEXT_STACK_DEPTH > 0
    g_Inst.contextStackIndex--;
#endif
    DLCommand(FT_RESTORE_CONTEXT());
}

// Primitive params
void FTGLLineWidth(uint16_t width) { WRITE_DLCMD(GRAPHICS_CONTEXT(g_Inst).lineWidth, FT_LINE_WIDTH(width)); }
void FTGLPointSize(uint32_t size) {  WRITE_DLCMD(GRAPHICS_CONTEXT(g_Inst).pointSize, FT_POINT_SIZE(size)); }

// SCISSOR, STENCIL, TAG
void FTGLScissorSize(uint16_t width, uint16_t height) { WRITE_DLCMD(GRAPHICS_CONTEXT(g_Inst).scissorSize, FT_SCISSOR_SIZE(width, height)); }
void FTGLScissorXY(uint16_t x, uint16_t y) { WRITE_DLCMD(GRAPHICS_CONTEXT(g_Inst).scissorXY, FT_SCISSOR_XY(x, y)); }
void FTGLStencilFunc(uint8_t func, uint8_t ref, uint8_t mask) { WRITE_DLCMD(GRAPHICS_CONTEXT(g_Inst).stencilFunc, FT_STENCIL_FUNC(func, ref, mask)); }
void FTGLStencilOp(uint8_t sfail, uint8_t spass) { WRITE_DLCMD(GRAPHICS_CONTEXT(g_Inst).stencilOp, FT_STENCIL_OP(sfail, spass)); }
void FTGLTag(uint8_t tag) { WRITE_DLCMD(GRAPHICS_CONTEXT(g_Inst).tag, FT_TAG(tag)); }
void FTGLTagMask(uint8_t on) { WRITE_DLCMD(GRAPHICS_CONTEXT(g_Inst).tagMask, FT_TAG_MASK(on)); }

/////////////////////////////////////////////////////////////////////////////
// Command processor high level commands:

void FTGLCmdDLStart(void) { DLCommand(FT_CMD_DLSTART); }
void FTGLCmdSwap(void) { DLCommand(FT_CMD_SWAP); }
void FTGLCmdColdStart(void) { 
#if FTGL_CACHE_COMMAND_CONTEXT == 1
    g_Inst.commandContext.bgColor =   0x002040;
    g_Inst.commandContext.fgColor =   0x003870;
    g_Inst.commandContext.gradColor = 0xffffff;
#endif
    EnsureSpace(sizeof(uint32_t) * 2);
    Append32(FT_CMD_STOP);
    Append32(FT_CMD_COLDSTART); 
}
void FTGLCmdInflate(uint32_t ptr, uint8_t *data, uint32_t count) { 
    EnsureSpace(Aligned(sizeof(uint32_t) * 2 + count));
    Append32(FT_CMD_INFLATE); Append32(ptr); AppendString(data, count); 
    AlignBuffer();
}
void FTGLCmdLoadImage(uint32_t ptr, uint32_t options, uint8_t *data, uint32_t count) {
    EnsureSpace(Aligned(sizeof(uint32_t) * 3 + count));
    Append32(FT_CMD_LOADIMAGE); Append32(ptr); Append32(options); AppendString(data, count);
    AlignBuffer(); 
}

void FTGLCmdButton(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t font, uint16_t options, const char *str, uint16_t len) {
    EnsureSpace(Aligned(sizeof(uint32_t) + sizeof(uint16_t) * 6 + len));
    Append32(FT_CMD_BUTTON);
    Append16((uint16_t)x); Append16((uint16_t)y); Append16((uint16_t)w); Append16((uint16_t)h);
    Append16(font); Append16(options); 
    AppendString((const uint8_t*)str, len);
    AlignBuffer();
}

void FTGLCmdClock(int16_t x, int16_t y, int16_t radius, uint16_t options, uint16_t h, uint16_t m, uint16_t s, uint16_t ms) {
    EnsureSpace(sizeof(uint32_t) + sizeof(int16_t) * 8);
    Append32(FT_CMD_CLOCK);
    Append16((uint16_t)x); Append16((uint16_t)y); Append16((uint16_t)radius);
    Append16(options);
    Append16(h); Append16(m); Append16(s); Append16(ms);
}

void FTGLCmdFGColor(uint32_t color) {
#if FTGL_CACHE_COMMAND_CONTEXT == 1
    if (g_Inst.commandContext.fgColor != color) {
        g_Inst.commandContext.fgColor = color;
#endif
        EnsureSpace(sizeof(uint32_t) * 2);
        Append32(FT_CMD_FGCOLOR);
        Append32(color);
#if FTGL_CACHE_COMMAND_CONTEXT == 1
    }
#endif
}
void FTGLCmdBGColor(uint32_t color) {
#if FTGL_CACHE_COMMAND_CONTEXT == 1
    if (g_Inst.commandContext.bgColor != color) {
        g_Inst.commandContext.bgColor = color;
#endif
        EnsureSpace(sizeof(uint32_t) * 2);
        Append32(FT_CMD_BGCOLOR);
        Append32(color);
#if FTGL_CACHE_COMMAND_CONTEXT == 1
    }
#endif
}

void FTGLCmdGradColor(uint32_t color) {
#if FTGL_CACHE_COMMAND_CONTEXT == 1
    if (g_Inst.commandContext.gradColor != color) {
        g_Inst.commandContext.gradColor = color;
#endif
        EnsureSpace(sizeof(uint32_t) * 2);
        Append32(FT_CMD_GRADCOLOR);
        Append32(color);
#if FTGL_CACHE_COMMAND_CONTEXT == 1
    }
#endif

}

void FTGLCmdGauge(int16_t x, int16_t y, int16_t r, uint16_t options, uint16_t major, uint16_t minor, uint16_t val, uint16_t range) {
    EnsureSpace(sizeof(uint32_t) + sizeof(uint16_t) * 8);
    Append32(FT_CMD_GAUGE);
    Append16((uint16_t)x); Append16((uint16_t)y); Append16((uint16_t)r);
    Append16(options);
    Append16(major); Append16(minor);
    Append16(val); Append16(range);
}

void FTGLCmdGradient(int16_t x0, int16_t y0, uint32_t rgb0, int16_t x1, int16_t y1, uint32_t rgb1) {
    EnsureSpace(sizeof(uint32_t) * 3 + sizeof(uint16_t) * 4);
    Append32(FT_CMD_GRADIENT);
    Append16((uint16_t)x0); Append16((uint16_t)y0);
    Append32(rgb0);
    Append16((uint16_t)x1); Append16((uint16_t)y1);
    Append32(rgb1);
}

void FTGLCmdKeys(int16_t x, int16_t y, int16_t w, int16_t h, int16_t font, uint16_t options, const char* s, uint16_t len) {
    EnsureSpace(Aligned(sizeof(uint32_t) + sizeof(uint16_t) * 6 + len));
    Append32(FT_CMD_KEYS);
    Append16((uint16_t)x); Append16((uint16_t)y); Append16((uint16_t)w); Append16((uint16_t)h);
    Append16((uint16_t)font);
    Append16(options); AppendString((const uint8_t*)s, len); 
    AlignBuffer();
}

void FTGLCmdProgress(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t options, uint16_t val, uint16_t range) {
    EnsureSpace(sizeof(uint32_t) + sizeof(uint16_t) * 8);
    Append32(FT_CMD_PROGRESS);
    Append16((uint16_t)x); Append16((uint16_t)y); Append16((uint16_t)w); Append16((uint16_t)h);
    Append16(options);
    Append16(val);
    Append16(range);
    Append16(0); // For alignment
}

void FTGLCmdScrollbar(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t options, uint16_t val, uint16_t size, uint16_t range) {
    EnsureSpace(sizeof(uint32_t) + sizeof(uint16_t) * 8);
    Append32(FT_CMD_SCROLLBAR);
    Append16((uint16_t)x); Append16((uint16_t)y); Append16((uint16_t)w); Append16((uint16_t)h);
    Append16(options); Append16(val); Append16(size); Append16(range);
}

void FTGLCmdSlider(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t options, uint16_t val, uint16_t range) {
    EnsureSpace(sizeof(uint32_t) + sizeof(uint16_t) * 8);
    Append32(FT_CMD_SLIDER);
    Append16((uint16_t)x); Append16((uint16_t)y); Append16((uint16_t)w); Append16((uint16_t)h);
    Append16(options); Append16(val); Append16(range);
    Append16(0); // For alignment
}

void FTGLCmdDial(int16_t x, int16_t y, int16_t r, uint16_t options, uint16_t val) {
    EnsureSpace(sizeof(uint32_t) + sizeof(uint16_t) * 6);
    Append32(FT_CMD_DIAL);
    Append16((uint16_t)x); Append16((uint16_t)y); Append16((uint16_t)r);
    Append16(options); Append16(val); 
    Append16(0); // For alignment
}

void FTGLCmdToggle(int16_t x, int16_t y, int16_t w, int16_t font, uint16_t options, uint16_t state, const char* s, uint16_t len) {
    EnsureSpace(Aligned(sizeof(uint32_t) + sizeof(uint16_t) * 6 + len));
    Append32(FT_CMD_TOGGLE);
    Append16((uint16_t)x); Append16((uint16_t)y); Append16((uint16_t)w); 
    Append16((uint16_t)font);
    Append16(options); AppendString((const uint8_t*)s, len); 
    AlignBuffer();
}

void FTGLCmdText(int16_t x, int16_t y, int16_t font, uint16_t options, const char* s, uint16_t len) {
    EnsureSpace(Aligned(sizeof(uint32_t) + sizeof(uint16_t) * 4 + len));
    Append32(FT_CMD_TEXT);
    Append16((uint16_t)x); Append16((uint16_t)y);
    Append16((uint16_t)font);
    Append16(options); AppendString((const uint8_t*)s, len);
    AlignBuffer();
}

void FTGLCmdNumber(int16_t x, int16_t y, int16_t font, uint16_t options, int32_t n) {
    EnsureSpace(sizeof(uint32_t) * 2 + sizeof(uint16_t) * 4);
    Append32(FT_CMD_NUMBER);
    Append16((uint16_t)x); Append16((uint16_t)y);
    Append16((uint16_t)font);
    Append16(options); 
    Append32((uint32_t)n);

}

void FTGLCmdLoadIdentity(void) {
    DLCommand(FT_CMD_LOADIDENTITY);
}

void FTGLCmdTranslate(int32_t tx, int32_t ty) {
    EnsureSpace(sizeof(uint32_t) * 3);
    Append32(FT_CMD_TRANSLATE);
    Append32((uint32_t)tx);
    Append32((uint32_t)ty);
}

void FTGLCmdScale(int32_t sx, int32_t sy) {
    EnsureSpace(sizeof(uint32_t) * 3);
    Append32(FT_CMD_SCALE);
    Append32((uint32_t)sx);
    Append32((uint32_t)sy);
}

void FTGLCmdRotate(int32_t a) {
    EnsureSpace(sizeof(uint32_t) * 2);
    Append32(FT_CMD_ROTATE);
    Append32((uint32_t)a);
}

void FTGLCmdSetMatrix(void) {
    DLCommand(FT_CMD_SETMATRIX);
}

// CmdCalibrate not available as a command, instead, call FTGLRunCalibration(&output) to run a calibration routine

void FTGLCmdSpinner(int16_t x, int16_t y, uint16_t style, uint16_t scale) {
#if FTGL_CACHE_COMMAND_CONTEXT == 1
    if (g_Inst.commandContext.continuousCommandActive) {
        FTGLCmdStop();
    }
    g_Inst.commandContext.continuousCommandActive = 1;
#endif
    EnsureSpace(sizeof(uint32_t) + sizeof(int16_t) * 4);
    Append32(FT_CMD_SPINNER);
    Append16((uint16_t)x);
    Append16((uint16_t)y);
    Append16(style);
    Append16(scale);
}

void FTGLCmdScreensaver(void) {
#if FTGL_CACHE_COMMAND_CONTEXT == 1
    if (g_Inst.commandContext.continuousCommandActive) {
        FTGLCmdStop();
    }
    g_Inst.commandContext.continuousCommandActive = 1;
#endif
    DLCommand(FT_CMD_SCREENSAVER);
}

void FTGLCmdSketch(int16_t x, int16_t y, uint16_t w, uint16_t h, uint32_t ptr, uint16_t format) {
#if FTGL_CACHE_COMMAND_CONTEXT == 1
    if (g_Inst.commandContext.continuousCommandActive) {
        FTGLCmdStop();
    }
    g_Inst.commandContext.continuousCommandActive = 1;
#endif
    EnsureSpace(sizeof(uint32_t) * 2 + sizeof(uint16_t) * 6);
    Append32(FT_CMD_SKETCH);
    Append16((uint16_t)x);
    Append16((uint16_t)y);
    Append16((uint16_t)w);
    Append16((uint16_t)h);
    Append32(ptr);
    Append16(format);
    Append16(0); // For alignment
}

void FTGLCmdStop(void) {
#if FTGL_CACHE_COMMAND_CONTEXT == 1
    g_Inst.commandContext.continuousCommandActive = 0;
#endif
    DLCommand(FT_CMD_STOP);
}

void FTGLCmdSetFont(uint32_t font, uint32_t ptr) {
    EnsureSpace(sizeof(uint32_t) * 3);
    Append32(FT_CMD_SETFONT);
    Append32(font);
    Append32(ptr);
}

void FTGLCmdTrack(int16_t x, int16_t y, int16_t w, int16_t h, int16_t tag) {
    EnsureSpace(sizeof(uint32_t) + sizeof(uint16_t) * 6);
    Append32(FT_CMD_TRACK);
    Append16((uint16_t)x); Append16((uint16_t)y); Append16((uint16_t)w); Append16((uint16_t)h);
    Append16((uint16_t)tag);
    Append16(0); // For alignment
}

void FTGLCmdSnapshot(uint32_t ptr) {
    EnsureSpace(sizeof(uint32_t) * 2);
    Append32(FT_CMD_SNAPSHOT);
    Append32(ptr);
}

void FTGLCmdLogo(void) {
    DLCommand(FT_CMD_LOGO);
}

static void ComputeSizeAndStride(uint8_t format, uint32_t widthInPixels, uint32_t totalHeight, uint32_t *size, uint32_t *stride) {
    uint32_t multiplier = 1, divider = 1;
    switch (format) {
    case FT_L1: multiplier = 1; divider = 8; break;
    case FT_L4: multiplier = 1; divider = 2; break;
    case FT_L8: multiplier = 1; divider = 1; break;
    case FT_PALETTED: multiplier = 1; divider = 1; break;
    case FT_TEXT8X8: multiplier = 1; divider = 1; break;
    case FT_TEXTVGA: multiplier = 1; divider = 1; break;
    case FT_BARGRAPH: multiplier = 1; divider = 1; break;
    case FT_RGB332: multiplier = 1; divider = 1; break;
    case FT_ARGB2: multiplier = 1; divider = 1; break;
    case FT_ARGB1555: multiplier = 2; divider = 1; break;
    case FT_ARGB4: multiplier = 2; divider = 1; break;
    case FT_RGB565: multiplier = 2; divider = 1; break;
    default: //assert(0 && "Invalid format specifier given.");
        multiplier = 1; divider = 1;
        break;
    }

    uint32_t multiplied = (multiplier * widthInPixels);
    uint32_t linestride = multiplied / divider;
    if (linestride * divider < multiplied) { linestride++; }

    *size = linestride * totalHeight;
    *stride = linestride;
}

int FTGLCreateBitmap(uint8_t format, int widthInPixels, int heightInLines) {
    uint32_t stride, imgsize;
    ComputeSizeAndStride(format, widthInPixels, heightInLines, &imgsize, &stride);
    uint32_t addr = g_Inst.graphicsRamIndex;

    g_Inst.graphicsRamIndex += imgsize;
    int id = g_Inst.bitmapIndex++;

    g_Inst.bitmaps[id].bitmapAddress = addr;
    g_Inst.bitmaps[id].bitmapDataSize = imgsize;
    g_Inst.bitmaps[id].bitmapLayout = FT_BITMAP_LAYOUT(format, stride, heightInLines);
    g_Inst.bitmaps[id].bitmapSize = FT_BITMAP_SIZE(FT_BILINEAR, FT_BORDER, FT_BORDER, widthInPixels, heightInLines);
    g_Inst.bitmaps[id].activeHandle = -1;

    return id;
}

void FTGLSetBitmapParams(int id, uint8_t filter, uint8_t wrapx, uint8_t wrapy) {
    uint32_t bms = FT_BITMAP_SIZE(filter, wrapx, wrapy, 0, 0);
    g_Inst.bitmaps[id].bitmapSize = bms | (g_Inst.bitmaps[id].bitmapSize & 0x1FFFF);

#if FTGL_CACHE_BITMAP_HANDLES == 1
    if (g_Inst.bitmaps[id].activeHandle >= 0) {
        g_Inst.bitmapHandles[g_Inst.bitmaps[id].activeHandle] = -1;
        g_Inst.bitmaps[id].activeHandle = -1;
    }
#endif
}

void FTGLSetBitmapSize(int id, uint16_t renderWidth, uint16_t renderHeight) {
    uint32_t size = FT_BITMAP_SIZE(0, 0, 0, renderWidth, renderHeight);
    g_Inst.bitmaps[id].bitmapSize = size | (g_Inst.bitmaps[id].bitmapSize & ~0x1FFFF);
#if FTGL_CACHE_BITMAP_HANDLES == 1
    if (g_Inst.bitmaps[id].activeHandle >= 0) {
        g_Inst.bitmapHandles[g_Inst.bitmaps[id].activeHandle] = -1;
        g_Inst.bitmaps[id].activeHandle = -1;
    }
#endif
}

int FTGLCreateBitmapVerbose(uint8_t format, uint16_t stride, uint16_t layoutHeight, uint16_t numCells,
    uint8_t filter, uint8_t wrapx, uint8_t wrapy, uint16_t renderWidth, uint16_t renderHeight) {
    uint32_t size = stride * layoutHeight * numCells;
    uint32_t addr = g_Inst.graphicsRamIndex;

    g_Inst.graphicsRamIndex += size;
    int id = g_Inst.bitmapIndex++;

    g_Inst.bitmaps[id].bitmapAddress = addr;
    g_Inst.bitmaps[id].bitmapDataSize = size;
    g_Inst.bitmaps[id].bitmapLayout = FT_BITMAP_LAYOUT(format, stride, layoutHeight);
    g_Inst.bitmaps[id].bitmapSize = FT_BITMAP_SIZE(filter, wrapx, wrapy, renderWidth, renderHeight);
    g_Inst.bitmaps[id].activeHandle = -1;

    return id;
}

// Load bitmap data into RAM_G memory
void FTGLBitmapBufferData(int id, uint32_t offset, const uint8_t *data, uint32_t count) {
    FTHWWrite(g_Inst.bitmaps[id].bitmapAddress + offset, data, count);
}

// TODO: Maybe flip this around to match the same kind of ordering as the FT800 commands
// Psuedo command to draw a bitmap in one call. Highest level
void FTGLCmdBitmap(int id, int x, int y) {
    FTGLCmdBitmapCell(id, x, y, 0);
}

void FTGLCmdBitmapCell(int id, int x, int y, int cell) {
#if FTGL_CACHE_BITMAP_HANDLES == 1
    int8_t handle = g_Inst.bitmaps[id].activeHandle;
    if (handle < 0) { // Need to load the bitmap into a handle
        handle = FTGLUseBitmap(id); // Pick a handle and load into it
    }

    FTGLDrawBitmapInHandle(handle, x, y, cell);
#else 
    FTGLSetBitmapHandle(0, id);
    FTGLDrawBitmapInHandle(0, x, y, cell);
#endif
}

#if FTGL_CACHE_BITMAP_HANDLES == 1
static int8_t PickHandleToEvict(void) {
    int i, selected = -1;
    for (i = 0; i < FTGL_NUM_BITMAP_HANDLES; i++) {
        
        if (g_Inst.nextHandle == g_Inst.lastHandle) { 
            g_Inst.nextHandle++;
            continue; 
        } // Skip last used handle
        selected = g_Inst.nextHandle; // Select this handle to use
        if (g_Inst.bitmapHandles[g_Inst.nextHandle] == -1) { 
            g_Inst.nextHandle++;
            break; 
        } // Favor empty handles

        g_Inst.nextHandle += 1;
        if (g_Inst.nextHandle >= FTGL_NUM_BITMAP_HANDLES) {
            g_Inst.nextHandle = 0;
        }
    }
    return selected;
}

int8_t FTGLUseBitmap(int bitmapId) {
    int selected = PickHandleToEvict();
    return FTGLSetBitmapHandle(selected, bitmapId);
}   

int8_t FTGLGetEmptyHandle(void) {
    int selected = PickHandleToEvict();
    uint8_t oldBitmapId = g_Inst.bitmapHandles[selected];
    if (oldBitmapId >= 0) {
        g_Inst.bitmaps[oldBitmapId].activeHandle = -1;
    }
    g_Inst.bitmapHandles[selected] = -1;
    return selected;
}
#endif

int8_t FTGLSetBitmapHandle(int8_t handle, int bitmapId) {
#if FTGL_CACHE_BITMAP_HANDLES == 1
    int8_t oldBitmapId = g_Inst.bitmapHandles[handle];
    if (oldBitmapId >= 0) {
        g_Inst.bitmaps[oldBitmapId].activeHandle = -1;
    }
#endif

    FTGLBitmapHandle(handle);
    EnsureSpace(sizeof(uint32_t) * 3);
    Append32(FT_BITMAP_SOURCE(g_Inst.bitmaps[bitmapId].bitmapAddress));
    Append32(g_Inst.bitmaps[bitmapId].bitmapLayout);
    Append32(g_Inst.bitmaps[bitmapId].bitmapSize);

    g_Inst.bitmaps[bitmapId].activeHandle = handle;
    return handle;
}

void FTGLDrawBitmapInHandle(int8_t handle, int x, int y, int cell) {
    FTGLBegin(FT_BITMAPS);
        FTGLVertex2ii(x, y, handle, cell);
    FTGLEnd();
    g_Inst.lastHandle = (int8_t)handle;
}

void FTGLGetBitmapSize(int id, int *width, int *height) {
    *width = (g_Inst.bitmaps[id].bitmapSize >> 9) & 511;
    *height = (g_Inst.bitmaps[id].bitmapSize) & 511;
}


void FTGLLoadPalleteData(uint8_t offset, uint32_t *colors, uint8_t count) {
    FTHWWrite(FT_RAM_PAL + offset * sizeof(uint32_t), (const uint8_t*)colors, count * sizeof(uint32_t));
}

void FTGLSetPalleteColor(uint8_t value, uint32_t color) {
    WriteReg32(FT_RAM_PAL + value, HOST_TO_FT_ULONG(color));
}

// This is a blocking call that runs the calibration routine and loads the results into the 
// transform registers
void FTGLRunCalibration(void) {
    static const char * msg = "Calibration: Touch the Dots";
    FTGLBeginBuffer();
    FTGLCmdText(240, 130, 27, FT_OPT_CENTER, msg, strlen(msg) + 1);
    EnsureSpace(sizeof(uint32_t) * 2);
    Append32(FT_CMD_CALIBRATE);
    Append32(0);
    FTHWEndAppendWrite();
    WriteReg16(FT_REG_CMD_WRITE, g_Inst.cmdQueueWriteIndex);
    WaitForQueueEmpty();
}

// Load the 6 touch transform register values into the given array
void FTGLGetTouchCalibrationParams(uint32_t params[6]) {
    params[0] = ReadReg32(FT_REG_TOUCH_TRANSFORM_A);
    params[1] = ReadReg32(FT_REG_TOUCH_TRANSFORM_B);
    params[2] = ReadReg32(FT_REG_TOUCH_TRANSFORM_C);
    params[3] = ReadReg32(FT_REG_TOUCH_TRANSFORM_D);
    params[4] = ReadReg32(FT_REG_TOUCH_TRANSFORM_E);
    params[5] = ReadReg32(FT_REG_TOUCH_TRANSFORM_F);
}
// Write the 6 touch transform values (a,b,c,d,e,f in that order) from
// the given array to the registers
void FTGLSetTouchCalibrationParams(uint32_t params[6]) {
    WriteReg32(FT_REG_TOUCH_TRANSFORM_A, params[0]);
    WriteReg32(FT_REG_TOUCH_TRANSFORM_B, params[1]);
    WriteReg32(FT_REG_TOUCH_TRANSFORM_C, params[2]);
    WriteReg32(FT_REG_TOUCH_TRANSFORM_D, params[3]);
    WriteReg32(FT_REG_TOUCH_TRANSFORM_E, params[4]);
    WriteReg32(FT_REG_TOUCH_TRANSFORM_F, params[5]);
}

void FTGLSetTouchSensitivity(uint16_t sens) {
    WriteReg16(FT_REG_TOUCH_RZTHRESH, sens);
}

// TODO(eric): Add variants of commands that take strings that do not need to know the string length

