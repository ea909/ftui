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

#ifndef ftui_numbers_IMG_H
#define ftui_numbers_IMG_H
#include <stdint.h>
#ifdef ARDUINO
#include <avr/pgmspace.h>
#endif
#define ftui_numbers_width 12
#define ftui_numbers_height 160
#define ftui_numbers_scanline_size 2
#define ftui_numbers_num_scanlines 160
#define ftui_numbers_format 1
#ifndef ARDUINO
#define PROGMEM
#endif
const uint8_t ftui_numbers_data[] PROGMEM = {
     63, 192, 127, 224, 224, 112, 192,  48, 
    192, 240, 193, 240, 195, 176, 199,  48, 
    206,  48, 220,  48, 248,  48, 240,  48, 
    192,  48, 224, 112, 127, 224,  63, 192, 
      6,   0,  14,   0,  30,   0,  62,   0, 
     62,   0,   6,   0,   6,   0,   6,   0, 
      6,   0,   6,   0,   6,   0,   6,   0, 
      6,   0,   6,   0,  63, 192,  63, 192, 
     63, 192, 127, 224, 224, 112, 192,  48, 
    192,  48,   0,  48,   0, 112,  63, 224, 
    127, 192, 240,   0, 224,   0, 192,   0, 
    192,   0, 192,   0, 255, 240, 255, 240, 
     63, 192, 127, 224, 224, 112, 192,  48, 
      0,  48,   0,  48,   0, 112,  31, 224, 
     31, 224,   0, 112,   0,  48,   0,  48, 
    192,  48, 224, 112, 127, 224,  63, 192, 
      0, 192,   1, 192,   3, 192,   7, 192, 
     14, 192,  28, 192,  56, 192, 112, 192, 
    224, 192, 192, 192, 255, 240, 255, 240, 
      0, 192,   0, 192,   0, 192,   0, 192, 
    255, 240, 255, 240, 192,   0, 192,   0, 
    255, 192, 255, 224,   0, 112,   0,  48, 
      0,  48,   0,  48,   0,  48,   0,  48, 
    192,  48, 224, 112, 127, 224,  63, 192, 
     63, 192, 127, 224, 224, 112, 192,  48, 
    192,   0, 192,   0, 192,   0, 255, 192, 
    255, 224, 192, 112, 192,  48, 192,  48, 
    192,  48, 224, 112, 127, 224,  63, 192, 
    255, 240, 255, 240,   0,  48,   0,  48, 
      0, 112,   0, 224,   1, 192,   3, 128, 
      7,   0,   6,   0,   6,   0,   6,   0, 
      6,   0,   6,   0,   6,   0,   6,   0, 
     63, 192, 127, 224, 224, 112, 192,  48, 
    192,  48, 192,  48, 224, 112, 127, 224, 
    127, 224, 224, 112, 192,  48, 192,  48, 
    192,  48, 224, 112, 127, 224,  63, 192, 
     63, 192, 127, 224, 224, 112, 192,  48, 
    192,  48, 192,  48, 224,  48, 127, 240, 
     63, 240,   0,  48,   0,  48,   0,  48, 
    192,  48, 224, 112, 127, 224,  63, 192
};
#define ftui_numbers_size sizeof(ftui_numbers_data) / sizeof(uint8_t)
#endif
