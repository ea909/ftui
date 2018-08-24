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
 * ftui.h - Immediate Mode GUI for FT800
 * --------------------------------------------------
 *  Implements an immediate mode gui (imgui) for the FT800. 
 *
 *  An immediate mode ui is a design popularized in game 
 *  development for the creation of debugging and tool user
 *  interfaces. A more familiar "retained" UI, such as Qt, is
 *  built around a hierarchy of widgets.
 *
 *  Each of these widgets stores state and are connected to the application
 *  data by event handlers (or a data binding framework at higher levels).
 *  This has the benefit of avoiding repeated computation - most notably,
 *  things are only redrawn when they are changed.
 *
 *  On modern graphics hardware, you can afford to redraw every frame, 
 *  especially in a game which is redrawing many triangles every frame
 *  anyways. So, instead of storing state in widgets, an imgui might recompute
 *  the layout and redraws the entire ui every frame.
 *
 *  This is a good choice in this environment, where:
 *  1. The FT800 does all the drawing, so we can redraw every frame
 *     easily.
 *  2. There aren't that many controls on a 480x272 touchscreen, and
 *     the layouts are all fixed and fullscreen, so the per frame
 *     computations are small.
 *  3. There is not a lot of RAM, and there is unlikely to be 
 *     heap allocation, so constructing a widget hierarchy not only wastes
 *     resources but is also a pain.
 *  4. The UI is simple and is not being created by a dedicated graphics
 *     designer, so the design can be written out as code.
 *
 *  So thats why we are using imgui. So how does imgui work? There is
 *  a small piece of state that holds 4 things: 
 *  * was the user touching the screen last frame?
 *  * is the user touching the screen?
 *  * if so, where?
 *  * the current active id.
 *
 *  The active id is used to implement the concept of focus. Every control,
 *  is given an id, and this id is used to track the lifetime of the control
 *  across multiple frames.
 *
 *  An example makes this more clear. Here is an example of implementing a
 *  control in imgui:
 *
 *    int MyButton(int id, 
 *                 int x, int y, int w, int h, 
 *                 int font, int options,
 *                 const char *text) {
 *
 *        // Update state
 *        int pressed = 0;
 *        if (g_State.active == id) { // This control is active
 *            if (!FTUIHasTouch()) { // Drop focus when they lift finger
 *                if (FTUIInRect(x, y, w, h)) { // Lift on button is a press
 *                    pressed = 1;
 *                }
 *                g_State.active = -1;
 *            }
 *        } else if (FTUITouched() && // User started touching screen
 *                   FTUIInRect(x, y, w, h)) { // Touch was on control
 *            g_State.active = id; // This control is now active
 *        }
 *
 *        // Redraw
 *        FTGLCmdButton(x, y, w, h, font, options, text, strlen(text)+1);
 *        
 *        // returns true if pressed
 *        return pressed;
 *    } 
 * 
 * You would then use such a control like this:
 */
 /*
        #define ID_BUTTON 0 
        #define ID_SLIDER 1
        FTUIInitialize();
        int sliderValue = 40;
        while (1) {
            FTUIBegin();
            if (MyButton(ID_BUTTON, 10, 10, 50, 50, 24, FT_OPT_FLAT, "test")) {
                // handle button press
            }
            
            FTUISlider(ID_SLIDER, 80, 80, 100, 10, 0, &sliderValue, 100);
            FTUIEnd();
        }
 */
 /*
  * If you want to separate the UI from the logic that handles events
  * you could do something like this:
  */
 /*
        typedef struct {
            int reticulateSplines;
            int numberOfSplines;
        } UIModel;

        typedef struct {
            // blah blah blah
        } AppModel;

        void main(void) {
            UIModel ui;
            AppModel app;
            InitUI(&ui);
            InitMyApp(&app);
            FTUIInitialize();
            while (1) {
                RunUI(&ui);
                RunApp(&app, &ui);
            }
        }

        void RunUI(UIModel *ui) {
        #define ID_BUTTON 0
        #define ID_SLIDER 1
            FTUIBegin();
            ui->reticulateSplines = FTUIButton(ID_BUTTON, 10, 10, 40, 40, 24, 0, "reticulate");
            FTUISlider(ID_SLIDER, 80, 80, 100, 10, 0, &(ui->numberOfSplines), 15);
            FTUIEnd();
        }

        void RunApp(AppModel *app, UIModel *ui) {
            if (ui->reticulateSplines) {
                // blah blah blah
            }
            // etc
        }
 */
 /*
  * The UIModel can of course be more complex than this example.
  *
  * Last but not least: If you use FTGL commands that change
  * the FT800's state (ex, foreground color), you will need
  * to change it back manually or it will get applied to 
  * everything drawn next. That is, the commands do not
  * explicity set things such as their color.
  ************************************************************/  

#ifndef FTUI_H
#define FTUI_H
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#include "ftgl.h"

#define FTUI_USE_OPTIONS 1

// TODO(eric): Give the fonts names
////////////////////////////////////////////////////

// Initializes FTUI, which initializes FTGL, which initializes FTHW.
// Remember to use FTGLRunCalibration or FTGLSetTouchCalibrationParams
// so that the touch screen coordinates are correct.
void FTUIInitialize(void);

// Call this to begin drawing your UI
// Internally, calls BeginBuffer, so you can use any ftgl commands you want
// in between this and FTUIEnd
void FTUIBegin(void);

// And call this once you have finished drawing the UI
// Internally, call SwapBuffers, so you can use any ftgl commands you want
// in between FTUIBegin and this.
void FTUIEnd(void);

// A button drawn at the rectangle specified.
// See the programmers manual for font ids (generally, bigger id is a larger
// font). Draw the given text centered on the button.
//
// Returns true when the button is pressed.
int FTUIButton(int id, int x, int y, int w, int h, int font, const char *text);

// A button with a bitmap instead of text.
// Does the computation to center the bitmap for you
// You still need to supply a font because it is used 
// to calculate how rounded the corners are.
//
// Returns true when the button is pressed.
int FTUIBitmapButton(int id, int x, int y, int w, int h, int font, int bitmapId);

// Creates a row of keys using CMD_KEYS
// Each key is for a single ascii character in the string.
// When it is pressed, it returns the ascii value of the pressed key.
// Otherwise, it returns 0.
// 
// Centered is a boolean denoting whether or not to center the keys (true)
// or enlarge them to fill the entire rect (false)
int FTUIKeyRow(int id, int x, int y, int w, int h, int font, int centered, const char *row);

// Creates multiple rows of keys using CMD_KEYS. 
// Each key is for a single ascii character in the string.
// Returns 0 if there is no press, returns the ascii value of the
// key if there is a press.
// The (x, y) coord is where the rows start. The width and height is for a single row.
// Each additional row will be 'rowHeight + 3' pixels below the previous.
// The 3 pixels is to match the spacing applied by CMD_KEYS itself between each key on a
// row.
//
// To specify the row data, provide a DOUBLE NULL TERMINATED STRING, such that each row is
// null terminated and two nulls in a row denotes the end of the list:
// ex: "row1\0row2\0row3\0\0"
// 
// To save some computation time, you must also give the number of rows in the numRows 
// argument, so that the bounding box of the keys can be determined without scanning the
// entire string.
int FTUIKeyRows(int id, int x, int y, int w, int rowHeight, int font, int numRows, const char *rows);

// Draws text using FTGLCmdText
void FTUIText(int x, int y, int font, int options, const char *str);

// Draws a signed number using FTGLCmdNumber
void FTUINumber(int x, int y, int font, int options, int32_t n);

// Uses some bitmap scaling to render a number in very big font
// Scale can be one of 1, 2, 3, or 4
#define FTUI_NUMBER_WIDTH  12
#define FTUI_NUMBER_HEIGHT 16
void FTUILargeNumber(int x, int y, int numDigits, int scale, int leadingZeros, int32_t n);
#define FTUI_LARGE_NUMBER_WIDTH(numDigits, scale) (numDigits * (FTUI_NUMBER_WIDTH*scale + 4) - 4)
#define FTUI_LARGE_NUMBER_HEIGHT(numDigits, scale) (FTUI_NUMBER_HEIGHT*scale)

// Draw a flat rounded rectangle to serve as the background for a label, etc
void FTUIBackgroundRect(int x, int y, int w, int h, uint32_t color);

/** TODO(eric) *********************************************************
// To avoid making calls for control rendering extremely large, many parameters regarding the controls
// can be set through a collection of parameters, if the option is enabled.
#if defined(FTUI_USE_OPTIONS) && FTUI_USE_OPTIONS == 1
typedef enum {
    FTUI_OPTION_BUTTON_MAINCOLOR, // Primary color of button
    FTUI_OPTION_BUTTON_FADECOLOR, // Color of gradient on button
    FTUI_OPTION_BUTTON_LABELCOLOR, // Color of text on button
} UIOption;
#endif
uint32_t FTUIGetOption(UIOption option);
void FTUISetOption(UIOption option, uint32_t);
*/

// For use in creating additional controls:
void FTUIClearActive();
void FTUISetActive(int id);
int FTUIGetActive();
void FTUIClearActiveTag();
void FTUISetActiveTag(int id);
int FTUIGetActiveTag();
int FTUIHasTouch(void);
int FTUITouchX(void);
int FTUITouchY(void);
int FTUITouchTag(void);
int FTUITouched(void);
int FTUIInRect(int x, int y, int w, int h);
int32_t FTUIGetTicks(void);

#ifdef __cplusplus
}
#endif



#endif
