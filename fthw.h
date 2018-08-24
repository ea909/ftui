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
 * fthw.h - FT800 Hardware Interface
 * -------------------------------------------------------
 *  Implements routines for accesing the device hardware needed to operate the
 *  ft800 chip. Each platform has a separate implementation file for these
 *  functions - to port the ftui package to your platform, write a .c
 *  file implementing each of these functions.
 *
 *  A brief explanation of the implementation for each FTHW call is provided
 *  below. Additionally, you many consult the included platform
 *  implementations as examples. For more details on the FT800 SPI protocols, 
 *  see the FT800 datasheet.
 ***********************************************************/ 
#ifndef FTHW_H
#define FTHW_H

#include <stdint.h>

#if defined(ARDUINO)
#include "fthw_platform_arduino.h"
#else
#error "Include your platform header here"
#endif

#ifdef __cplusplus
extern "C" {
#endif

/**
 * If the platform has IO operations that might fail, negative return values
 * in the range [-128, -1] (inclusive) should be used to indicate an error number.
 */
#define FTHW_SUCCESS(x) ((x) >= 0)

/**
* Called during startup to initialize everything.
*
* For the FT800:
*
* - CS is active low (ie, go low to start a transaction)
* - SPI mode 0 is used:
*     * Output on falling edge
*     * Capture on rising edge
*     * Params: CPOL = 0, CPHA = 0
*
*  The SPI speed should intially be <= 4 mhz (see FTHWSetSpeed below) since
*  the graphics layer will initialize the PLL and then request a higher SPI
*  speed.
*/
int FTHWInitialize(void);

/**
 * Set the speed of the SPI module, STARTUP_SPEED should be <= 4 mhz. It is
 * used when first initializing the FT800, before it has enabled its PLL.
 * RUN_SPEED should be as close to 30 mhz as possible - this is the rate that
 * will be used for sending drawing data.
 */
#define FTHW_SPI_STARTUP_SPEED 0
#define FTHW_SPI_RUN_SPEED 1
int FTHWSetSpeed(int speed);

/**
 * Puts the display into or out of reset. When released from reset, the
 * display must be completely reinitialized.
 */
int FTHWSetReset(int inReset);

/**
 * Writes data to a location on the FT800.
 *
 * This call should perform (or setup to perform later, in the FTHWFlush
 * call) an SPI transfer. It should first first write the lower three bytes of
 * ('writeAddress' | 0x800000), MSB first, and then write 'count' number of
 * bytes from 'data'.
 *
 */
int FTHWWrite(uint32_t writeAddress, const uint8_t *data, uint16_t count);

/**
 * Reads data from a location on the FT800
 *
 * This call should perform or setup an SPI tranfer. First, it should write the
 * lower three bytes of readAddress, MSG first. Next, without strobing the CS
 * line (which would reset the transfer and clear the address) it should write
 * 'count' bytes worth of zeroes out, and record the data it receives back into
 * the 'data' array.
 */
int FTHWRead(uint32_t readAddress, uint8_t *data, uint16_t count);

/** 
 * These three commands are used together to perform a write from multiple
 * buffers. That is, instead of having to pool all of the data into a big
 * buffer to then pass to a call to FTHWWrite, the data can be written
 * with multiple calls to FTHWAppendWrite.
 *
 * Compared to using FTHWWrite, this avoids the overhead of resending the
 * address for each call. (Additionally, many implementations will also be
 * able to avoid toggling the CS line)
 *
 * To use these, you first call FTHWBeginAppendWrite with the starting address
 * of the write. Next, you perform as many FTHWAppendWrite calls as needed.
 * Each block of data written will immediately follow the last (ie, it all
 * becomes one contiguous write starting at the given address). When you
 * finish this, you must call FTHWEndAppendWrite.
 */
int FTHWBeginAppendWrite(uint32_t writeAddress);
int FTHWAppendWrite(const uint8_t *data, uint16_t count);
int FTHWEndAppendWrite(void);

/**
 * Performs a Host Command.
 *
 * As per the FT800 datasheet, a Host Command is performed by first writing the
 * byte ('commandId' | 0x40) followed by three zero bytes.
 */
int FTHWHostCommand(uint8_t commandId);

/**
 * Block for x MS. Used during initialization only
 */
void FTHWDelayMS(int x);

/**
 * Get a millisecond timer count. Used to time
 * actions in the UI.
 */
int32_t FTHWGetTicks(void);

#ifdef __cplusplus
}
#endif

#endif
