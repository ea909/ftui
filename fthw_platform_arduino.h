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
 * fthw_platform_arduino.h
 * --------------------------------------------------
 *  Platform specific defines for the arduino
 ***********************************************************/ 
#ifndef FTHW_PLATFORM_ARDUINO_H
#define FTHW_PLATFORM_ARDUINO_H

/** 
 * These macros should convert the endianess of a word and dword to little
 * endian, which is the expected output endianess for the ft800.
 *
 * If the platform is already little endian, these can do nothing
 */
#define HOST_TO_FT_SHORT(x) (x)
#define HOST_TO_FT_LONG(x) (x)
#define HOST_TO_FT_USHORT(x) (x)
#define HOST_TO_FT_ULONG(x) (x)

/**
 * Simlarly, these should convert from the incoming little endian to the
 * endianess of the host platform.
 */
#define FT_TO_HOST_SHORT(x) (x)
#define FT_TO_HOST_LONG(x) (x)
#define FT_TO_HOST_USHORT(x) (x)
#define FT_TO_HOST_ULONG(x) (x)
 
#endif
