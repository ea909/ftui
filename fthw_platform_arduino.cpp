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
 * fthw_platform_arduino.cpp
 * --------------------------------------------------------
 *  An implementation of the ftui platform layer (ftui.h) on
 *  top of the arduino wiring and SPI libraries.
 ***********************************************************/ 
#include "fthw.h"

#include <Arduino.h>
#include <SPI.h>
#include <stdlib.h>

// The pins selected here are based on the pinout of the 4dsystems
// ADAM adaptor

// Normally, #define SLAVE_SELECT_PIN 10, but the ADAM is different
#define SLAVE_SELECT_PIN 9
#define POWER_DOWN_PIN 8
#define INTERRUPT_PIN 7
#define MAX_SPI_FREQ 30000000

SPISettings fastSettings(30000000, MSBFIRST, SPI_MODE0);
SPISettings slowSettings(100000, MSBFIRST, SPI_MODE0);
SPISettings currentSettings(30000000, MSBFIRST, SPI_MODE0);
uint32_t appendCount = 0;

int FTHWInitialize(void) {
    pinMode(SLAVE_SELECT_PIN, OUTPUT);
    pinMode(INTERRUPT_PIN, INPUT);
    pinMode(POWER_DOWN_PIN, OUTPUT);
    digitalWrite(SLAVE_SELECT_PIN, HIGH);
    digitalWrite(POWER_DOWN_PIN, HIGH);
    SPI.begin();
    return 0;
}

int FTHWSetSpeed(int speed) {
    switch (speed) {
        case FTHW_SPI_STARTUP_SPEED:
            currentSettings = slowSettings;
            return FTHW_SPI_STARTUP_SPEED;
        case FTHW_SPI_RUN_SPEED:
            currentSettings = fastSettings;
            return FTHW_SPI_RUN_SPEED;
        default:
            return -1;
    }
}

int FTHWSetReset(int inReset) {
    if (inReset) {
        digitalWrite(POWER_DOWN_PIN, LOW);
    } else {
        digitalWrite(POWER_DOWN_PIN, HIGH);
    }
    return inReset;
}

int FTHWWrite(uint32_t writeAddress, const uint8_t *data, uint16_t count) {
    uint16_t i;
    uint8_t *addr = (uint8_t*)&writeAddress;
    
    writeAddress |= 0x800000;

    digitalWrite(SLAVE_SELECT_PIN, LOW);
    SPI.beginTransaction(currentSettings);

    // Arduino is little endian
    SPI.transfer(addr[2]);
    SPI.transfer(addr[1]);
    SPI.transfer(addr[0]);

    // Unfortunately, we cannot use this
    // transfer function because it overwrites the
    // given array with read back data
    // TODO(eric): Write function directly against SPI registers
    // to do this the right way.
    //SPI.transfer(data, count);
    for (i = 0; i < count; i++) {
        SPI.transfer(data[i]);
    }

    SPI.endTransaction();
    digitalWrite(SLAVE_SELECT_PIN, HIGH);

    return count;
}

int FTHWRead(uint32_t readAddress, uint8_t *data, uint16_t count) {
    uint8_t *addr = (uint8_t*)&readAddress;
    
    digitalWrite(SLAVE_SELECT_PIN, LOW);
    SPI.beginTransaction(currentSettings);

    // Arduino is little endian
    SPI.transfer(addr[2]);
    SPI.transfer(addr[1]);
    SPI.transfer(addr[0]);
    SPI.transfer(0); // Read requires dummy byte

    memset(data, 0, count);

    SPI.transfer(data, count);

    SPI.endTransaction();
    digitalWrite(SLAVE_SELECT_PIN, HIGH);

    return count;
}

int FTHWBeginAppendWrite(uint32_t writeAddress) {
    uint8_t *addr = (uint8_t*)&writeAddress;
    
    writeAddress |= 0x800000;

    digitalWrite(SLAVE_SELECT_PIN, LOW);
    SPI.beginTransaction(currentSettings);

    // Arduino is little endian
    SPI.transfer(addr[2]);
    SPI.transfer(addr[1]);
    SPI.transfer(addr[0]);

    appendCount = 0;
    return 0;
}

int FTHWAppendWrite(const uint8_t *data, uint16_t count) {
    uint16_t i;
    // Unfortunately, we cannot use this
    // transfer function because it overwrites the
    // given array with read back data
    // TODO(eric): Write function directly against SPI registers
    // to do this the right way.
    //SPI.transfer(data, count);
    for (i = 0; i < count; i++) {
        SPI.transfer(data[i]);
    }
    return count;
}

int FTHWEndAppendWrite(void) {
    SPI.endTransaction();
    digitalWrite(SLAVE_SELECT_PIN, HIGH);
    return appendCount;
}

int FTHWHostCommand(uint8_t commandId) {
    digitalWrite(SLAVE_SELECT_PIN, LOW);
    SPI.beginTransaction(currentSettings);
    SPI.transfer(commandId == 0 ? 
                 commandId : commandId | 0x40);
    SPI.transfer(0);
    SPI.transfer(0);
    SPI.endTransaction();
    digitalWrite(SLAVE_SELECT_PIN, HIGH);
    return 0;
}

void FTHWDelayMS(int x) { delay(x); }
int32_t FTHWGetTicks(void) { return (int32_t)millis(); }

