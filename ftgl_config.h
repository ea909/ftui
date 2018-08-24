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
 * ftgl_config.h - Build configurations.
 * -------------------------------------------------------
 *  This file lets you set various FTGL options.
 *  Most of these increase or decrease the use of caching, 
 *  buffering etc, so by decreasing/disabling things here you can decrease RAM
 *  usage for more constrained devices.
 ***********************************************************/ 
#ifndef FTGL_CONFIG_H
#define FTGL_CONFIG_H

// To disable a boolean option, set it to zero. To enable it, set it to a
// value >= 1.

// When enabled, FTGL will keep a local copy of the graphics context state,
// and use it to avoid redundant commands. For example, if you draw 10
// buttons, and each one sets the color to red, with caching on, only one
// color command at the start will be sent, FTGL will ignore color changes to
// the current color; with it off, each buttons' request
// for red color will produce a command sent to the FT800, since there is no
// local cache of the graphics context state to compare to.
// Disabling this will decrease the RAM usage at the cost of higher command
// overhead. If you have a very fast SPI bus, and small display lists, you may
// get better performance with this turned off.
#define FTGL_CONFIG_CACHE_GRAPHICS_CONTEXT 1

// Like the above, but caches information about the state of the command
// coprocessor, which is not part of the graphics context
#define FTGL_CONFIG_CACHE_COMMAND_CONTEXT 1

// Like the above, but it caches information about the state of each bitmap
// handle, which are not part of the graphics context, It is recommended that
// you keep this enabled, because without it, the engine cannot use multiple
// bitmap handles and must instead load the data for each bitmap every time it
// is used.
#define FTGL_CONFIG_CACHE_BITMAP_HANDLES 1


// The depth of the context stack. It is possible to use the SaveContext and
// RestoreContext commands to save and restore the FT800 state from a stack.
// If you are using graphics content caching, then the context data structure
// has to know the maximum depth of SaveContexts you will perform, so it can
// allocate a stack of contexts that is the appropriate size.
//
// If FTGL_CONFIG_CACHE_GRAPHICS_CONTEXT is disabled, this option is ignored,
// since the graphics context is not cached.
//
// If you do not expect to use SaveContext/RestoreContext, this can be safely
// set to 0.
#define FTGL_CONFIG_CONTEXT_STACK_DEPTH 0

// The number of bitmap objects to create. 
#define FTGL_CONFIG_MAX_BITMAPS 16

// This is the maximum amount of RAM that you want to use. If this is enabled
// (ie, > 0),  and the options above require more than the amount defined
// here, FTGL produce a build error informing you that the settings exceed
// your RAM budget.  
// NOT IMPLEMENTED YET
// #define FTGL_CONFIG_MAX_RAM 0

// This sets the sensitivity of the touch screen.
// The value is in the range of 0 to 65535, with zero being no
// sensitivity (no touches detected) and 65535 is max sensitivity.
// According to the programmers guide, a reasonable default is 1200
#define FTGL_CONFIG_DEFAULT_SENSITIVITY 1200

// Select the display type, or provide custom parameters
#define FTGL_DISPLAY_WQVGA 0
#define FTGL_DISPLAY_QVGA 1
#define FTGL_DISPLAY_CUSTOM 2
#define FTGL_CONFIG_DISPLAY_TYPE    FTGL_DISPLAY_WQVGA

// If you are using FTGL_DISPLAY_CUSTOM, put your custom display parameters
// here:
#define FT_DISPLAY_VSYNC0_CUSTOM 	  (0L)
#define FT_DISPLAY_VSYNC1_CUSTOM      (10L)
#define FT_DISPLAY_VOFFSET_CUSTOM     (12L)
#define FT_DISPLAY_VCYCLE_CUSTOM      (292L)
#define FT_DISPLAY_HSYNC0_CUSTOM      (0L)
#define FT_DISPLAY_HSYNC1_CUSTOM      (41L)
#define FT_DISPLAY_HOFFSET_CUSTOM     (43L)
#define FT_DISPLAY_HCYCLE_CUSTOM      (548L)
#define FT_DISPLAY_HSIZE_CUSTOM       (480L) //display width
#define FT_DISPLAY_VSIZE_CUSTOM       (272L) //display height
#define FT_DISPLAY_PCLKPOL_CUSTOM     (1L)
#define FT_DISPLAY_SWIZZLE_CUSTOM     (0L)
#define FT_DISPLAY_PCLK_CUSTOM        (5L)

#endif

