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

#include "ftui.h"
#include "ftgl.h"
#include "ftui_numbers.h"
#include <string.h>

#if defined(ARDUINO) || defined(AVR)
#include <avr/pgmspace.h>
#endif

typedef struct {
    int8_t hadTouch;
    int8_t hasTouch;
    int16_t touchX;
    int16_t touchY;
    int16_t touchTag;
    int8_t lastTag;
    int16_t active;

    // Controls may use the tag feature 
    // internally to track touches in subsections of its
    // area. For these controls, they need a little more 
    // state than just their id, they also need to know
    // what tag has focus (ie, the subfocus), so
    // this extra piece of state is used in that case.
    int16_t activeTag;

    // A font of the numbers 0-9 used to draw
    // FTUILargeNumber
    int numbersImage;
} FTUIState;

static FTUIState g_State;

#define LARGE_NUMBER_MAX_SCALE 4

void FTUIInitialize(void) {
    memset(&g_State, 0, sizeof(g_State));
    g_State.active = -1; 

    FTGLInitialize();

    g_State.numbersImage = 
        FTGLCreateBitmapVerbose(FT_L1, 
                                ftui_numbers_scanline_size, 
                                ftui_numbers_num_scanlines / 10, 10,
                                FT_NEAREST, FT_BORDER, FT_BORDER, 
                                ftui_numbers_width * LARGE_NUMBER_MAX_SCALE, 
                                (ftui_numbers_height / 10) * LARGE_NUMBER_MAX_SCALE);

// TODO(eric): Move bitmap loading into platform file?
// Would need a function like FTHWGetImageByte(uint8_t* addr);
// or FTHWGetImageData(uint8_t* addr, uint8_t *out, size_t count);
#if defined(ARDUINO) || defined(AVR)
    for (int i = 0; i < ftui_numbers_scanline_size * ftui_numbers_num_scanlines; i++) {
        uint8_t val = pgm_read_byte_near(ftui_numbers_data + i);
        FTGLBitmapBufferData(g_State.numbersImage, i, &val, 1);
    }
#else
    FTGLBitmapBufferData(g_State.numbersImage, 0, 
                         ftui_numbers_data,
                         ftui_numbers_scanline_size * ftui_numbers_num_scanlines);
#endif

}

void FTUIClearActive() { g_State.active = -1; }
void FTUISetActive(int id) { g_State.active = id; }
int FTUIGetActive() { return g_State.active; }
void FTUIClearActiveTag() { g_State.activeTag = -1; }
void FTUISetActiveTag(int id) { g_State.activeTag = id; }
int FTUIGetActiveTag() { return g_State.activeTag; }
int FTUIHasTouch(void) { return g_State.hasTouch; }
int FTUITouchX(void) { return g_State.touchX; }
int FTUITouchY(void) { return g_State.touchY; }
int FTUITouchTag(void) { return g_State.touchTag; }
int FTUITouched(void) { return g_State.hasTouch && !g_State.hadTouch; }
int FTUIInRect(int x, int y, int w, int h) {
    if (g_State.touchX < x) { return 0; }
    if (g_State.touchX > x + w) { return 0; }
    if (g_State.touchY < y) { return 0; }
    if (g_State.touchY > y + h) { return 0; }
    return 1;
}

void FTUIBegin(void) { FTGLBeginBuffer(); }

void FTUIEnd(void) { 
    FTGLSwapBuffers(); 
    g_State.hadTouch = g_State.hasTouch;
    g_State.hasTouch = FTGLHasTouch();
    if (g_State.hasTouch) {
        g_State.touchX = FTGLTouchX();
        g_State.touchY = FTGLTouchY();
    }
    
    // SO APPARENTLY, there is an undocumented 1 frame delay for the ft800 to compute the
    // tag, so the frame that the touch starts is not the frame that the tag gets
    // updated. So, instead of checking hasTouch and hadTouch, controls that use tags
    // will need to check lastTag and touchTag, where 0 == no touch
    g_State.lastTag = g_State.touchTag;
    g_State.touchTag = FTGLTouchTag();
    
}

// TODO(eric): Remove options from here and move them to a separate function to set state?
// TODO(eric): Add way to change button colors?
int FTUIButton(int id,
    int x, int y, int w, int h,
    int font, const char *text) {

    // Update state
    int hover = 0;
    int pressed = 0;
    if (g_State.active == id) { // This control is active
        if (!g_State.hasTouch) { // Drop focus when they lift finger
            if (FTUIInRect(x, y, w, h)) { // Lift on button is a press
                pressed = 1;
            }
            g_State.active = -1;
        } else if (FTUIInRect(x, y, w, h)) {
            hover = 1;
        }
    } else if (FTUITouched() && // User started touching screen
        FTUIInRect(x, y, w, h)) { // Touch was on control
        g_State.active = id; // This control is now active
        hover = 1;
    }

    // Redraw
    if (hover) {
        FTGLCmdButton(x, y, w, h, font, FT_OPT_FLAT, text, strlen(text) + 1);
    } else { 
        FTGLCmdButton(x, y, w, h, font, 0, text, strlen(text) + 1);
    }
    
    // returns true if pressed
    return pressed;
}

int FTUIBitmapButton(int id,
    int x, int y, int w, int h,
    int font, int bitmapId) {

    // Update state
    int hover = 0;
    int pressed = 0;
    if (g_State.active == id) { // This control is active
        if (!g_State.hasTouch) { // Drop focus when they lift finger
            if (FTUIInRect(x, y, w, h)) { // Lift on button is a press
                pressed = 1;
            }
            g_State.active = -1;
        } else if (FTUIInRect(x, y, w, h)) {
            hover = 1;
        }
    } else if (FTUITouched() && // User started touching screen
        FTUIInRect(x, y, w, h)) { // Touch was on control
        g_State.active = id; // This control is now active
        hover = 1;
    }

    int imgW, imgH;
    FTGLGetBitmapSize(bitmapId, &imgW, &imgH);
    
    // Redraw
    if (hover) {
        FTGLCmdButton(x, y, w, h, font, FT_OPT_FLAT, "", 1);
    } else {
        FTGLCmdButton(x, y, w, h, font, 0, "", 1);
    }
    
    FTGLCmdBitmap(bitmapId, (x + w/2) - imgW / 2, (y + h/2) - imgH / 2);

    // returns true if pressed
    return pressed;
}

int FTUIKeyRow(int id, int x, int y, int w, int h, int font, int centered, const char *row) {
    int hover = 0, pressed = 0;

    if (g_State.active == id) {
        if (g_State.touchTag == 0) {
            if (g_State.lastTag == g_State.activeTag) {
                pressed = g_State.activeTag;       
            }
            g_State.active = -1;
            g_State.activeTag = -1;
        } else if (g_State.touchTag == g_State.activeTag) {
            hover = 1;
        }
    } else if (g_State.lastTag == 0 && g_State.touchTag != 0 && g_State.touchTag != 255 &&
               FTUIInRect(x, y, w, h)) {
        g_State.active = id;
        g_State.activeTag = g_State.touchTag;
        hover = 1;
    }

    if (hover) {
        FTGLCmdKeys(x, y, w, h, font, centered | g_State.touchTag, row, strlen(row) + 1);
    } else {
        FTGLCmdKeys(x, y, w, h, font, centered, row, strlen(row) + 1);
    }

    return pressed;
}

int FTUIKeyRows(int id, int x, int y, int w, int rowHeight, int font, int numRows, const char *rows) {
    const char *row = rows;
    int hover = 0, pressed = 0;

    int h;
    if (numRows == 1) {
        h = rowHeight;
    } else {
        h = (rowHeight + 3) * numRows - 3;
    }

    if (g_State.active == id) {
        if (g_State.touchTag == 0) {
            if (g_State.lastTag == g_State.activeTag) {
                pressed = g_State.activeTag;       
            }
            g_State.active = -1;
            g_State.activeTag = -1;
        } else if (g_State.touchTag == g_State.activeTag) {
            hover = 1;
        }
    } else if (g_State.lastTag == 0 && g_State.touchTag != 0 && g_State.touchTag != 255 &&
               FTUIInRect(x, y, w, h)) {
        g_State.active = id;
        g_State.activeTag = g_State.touchTag;
        hover = 1;
    }

    int currentY = y;
    while (*row != '\0') {
        if (hover) {
            FTGLCmdKeys(x, currentY, w, rowHeight, font, g_State.touchTag, row, strlen(row) + 1);
        } else {
            FTGLCmdKeys(x, currentY, w, rowHeight, font, 0, row, strlen(row) + 1);
        }

        while (*row != '\0') { row++; }
        row++;
        currentY += rowHeight + 3;
    }

    return pressed;
}

void FTUIText(int x, int y, int font, int options, const char *str) {
    FTGLCmdText((int16_t)x, (int16_t)y, (int16_t)font, (uint16_t)options, str, strlen(str)+1);
}

void FTUINumber(int x, int y, int font, int options, int32_t n) {
    FTGLCmdNumber((int16_t)x, (int16_t)y, (int16_t)font, FT_OPT_SIGNED | (int16_t)options, n);
}

void FTUILargeNumber(int x, int y, int numDigits, int scale, int leadingZeros, int32_t n) {
    int i;
    FTGLBitmapTransformA(256 / scale);
    FTGLBitmapTransformE(256 / scale);
    x += numDigits * (FTUI_NUMBER_WIDTH * scale + 4);
    for (i = 0; i < numDigits; i++) {
        x -= FTUI_NUMBER_WIDTH * scale + 4;
        if (!(n == 0 && i != 0) || leadingZeros) { 
            FTGLCmdBitmapCell(g_State.numbersImage, x, y, n % 10);
        }
        n /= 10;
    }
    FTGLBitmapTransformA(256);
    FTGLBitmapTransformE(256);
}

void FTUIBackgroundRect(int x, int y, int w, int h, uint32_t color) {
    FTGLCmdFGColor(color);
    FTGLCmdButton(x, y, w, h, 31, /*FT_OPT_FLAT*/ 0, "", 1);
    FTGLCmdColdStart();
}

int32_t FTUIGetTicks(void) {
    return FTGLGetTicks();
}
