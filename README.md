
It turns out there is this excellent device from FTDI called 
the [FT800](http://www.ftdichip.com/Products/ICs/FT800.html). It lets even a
tiny 8 bit microcontroller have a viable touch screen UI. However, the example
code that comes with it is unsatisfying, and some of it is written in C++ (not
every microcontroller has a good C++ compiler).

FTUI provides two C APIs for writing applications that use the FT800.

# FTGL

FTGL is the low level rendering API. It is modeled loosely after the openGL
1.x immediate mode API, and maps almost 1 to 1 to commands written to the
FT800 command buffer.

An example of using FTGL to draw some bitmaps and a line:

```c
FTGLInitialize();
FTGLBlendFunc(FT_SRC_ALPHA, FT_ONE_MINUS_SRC_ALPHA);
int myBitmapId = FTGLCreateBitmap(
    /* format */ FT_RGB565, 
    /* width */ 64, 
    /* height */ 64);

FTGLBitmapBufferData(myBitmapId, 0, dataFromSomewhere, 64*64*sizeof(short));

int myOtherBitmapId = FTGLCreateBitmapVerbose(
    /* format */                     FT_ARGB1555, 
    /* linestride, bytes per line */ 128,
    /* height, num lines */          64,
    /* numCells, number of images */ 3, /* data size is 128 * 64 * 3 */
    /* texture filtering */          FT_BILINEAR, 
    /* wrap, x & y */                FT_BORDER, FT_BORDER
    /* render width and height */    64, 64);

FTGLBitmapBufferData(myBitmapId, 0, otherData, 64*64*3*sizeof(short));

while (running) {
    FTGLBeginBuffer();

    // There are a fixed set of bitmap handles for drawing bitmaps
    // Similar to openGL texture units.
    FTGLSetBitmapHandle(0, myBitmapId);
    FTGLSetBitmapHandle(1, myOtherBitmapId);
    FTGLBegin(FT_BITMAPS);
        FTGLVertex2ii(30, 30, 0, 0);
        FTGLVertex2ii(100, 100, 1, 0);
    FTGLEnd();

    // Higher level command, 
    // FTGL finds a free bitmap handle or evicts lesser used
    // bitmap
    FTGLCmdBitmap(myBitmapId, 30, 30)

    FTGLBegin(FT_LINES);
        FTGLVertex2f(37, 37); // 1/16th pixel units. The f is for fixed point,
        FTGLVertex2f(63, 63); // not floating
    FTGLEnd();

    FTGLSwapBuffers();
}
```

# FTUI

BUT WAIT THERE'S MORE.

The number one use of these FT800's is for making touch UI's, not
demoscene graphics. It includes many drawing commands to create prefab
buttons, sliders, etc on the display (ex, FTGLCmdButton draws a rounded,
gradient button). However, its up to the microcontroller to provide the logic
and behavior of the controls. That's where FTUI comes in.

Most UI frameworks involve a tree of nodes used to dispatch events and layout
controls. On small microcontrollers, this architecture is not ideal.
It uses a lot of RAM to store redundant copies of information, complicates the
architecture with events or data binding, and performs a
bunch of recursive layout computations that do not need to happen on a fixed
size display.

Instead, FTUI implements a GUI architecture called Immediate Mode GUI or
IMGUI. An IMGUI GUI is drawn by explicitly calling a function per frame to
handle the rendering and logic of each control. IMGUI controls access
application data directly and do not require events to stay in sync.
 
For a good introduction to IMGUI, see Casey Muratori's 
[original video on it](https://www.youtube.com/watch?v=Z1qyvQsjK5Y).

With FTUI, you can make touchscreen UI applications on a microcontroller using
very few lines of code. As an example, here is a function that shows a numeric
keypad for the user to input a number:

```c
static int32_t UI_Keypad(int32_t number, int numDigits, char *title) {
    static int32_t pow10[10] = {
        1, 10, 100, 1000, 10000, 100000, 
        1000000, 10000000, 100000000, 1000000000
    };
    int32_t limit = pow10[numDigits - 1];
    
    struct {
        int keyPressed;
        int zeroPressed;
        int deletePressed;
        int enterPressed;
    } ui = {
        0, 0, 0, 0
    };

    FTUIClearActive();
    FTUIClearActiveTag();

    while (1) {
        { FTUIBegin();
            FTUIText(20, 20, 30, 0, title);
            FTUIBackgroundRect(16, 16 + 60, 192, 57, 0x525252);
            FTUILargeNumber(16 + 192/2 - FTUI_LARGE_NUMBER_WIDTH(2, 3)/2, 
                            16 + 60 + 4, 
                            numDigits, 3, 0, number);
            // Key rows creates a grid of keypad/keyboard buttons
            // Each row is null terminated, and the entire grid is double
            // null terminated. Each character becomes its own button.
            ui.keyPressed = FTUIKeyRows(1, 480-16-240, 16, 240, 57, 28, 3, 
                                        "789\000456\000123\000\000");
            ui.zeroPressed = FTUIButton(/* control id */ 2,   
                                        /* x */ 480-16-240, 
                                        /* y */ 16 + 60*3, 
                                        /* w */ 118, 
                                        /* h */ 57, 
                                        /* font size */ 28, 
                                        /* text */ "0");
            ui.deletePressed = FTUIButton(3, 480-16-118, 16 + 60*3, 118, 57, 28, "Del");
            ui.enterPressed = FTUIButton(4, 16, 16 + 60*3, 192, 57, 29, "Enter");
        } FTUIEnd();
        if (number < limit) {
            switch (ui.keyPressed) {
                case '1': case '2': case '3':
                case '4': case '5': case '6': 
                case '7': case '8': case '9':
                number = 10 * number + (int)(ui.keyPressed - '0');
            }

            if (ui.zeroPressed) {
                number = 10 * number;
            }
        }

        if (ui.deletePressed) {
            number = number / 10;
        }

        if (ui.enterPressed) {
            return number;
        }
    }

    return 0;
}

int main(void) {
    FTUIInitialize();

    int32_t number = UI_Keypad(0, 3, "SHOW ME WHAT YOU GOT");
    // Use the number...
}
```

# Hardware

FTUI is completely separated from the hardware by a lightweight abstraction
layer (fthw.h)\*. To port FTUI to another device, write a new fthw.c to
perform the SPI communications.

Also, there are many options (ftgl\_config.h) to control RAM
usage. On large devices, extra space can be used to diff commands against the
current state and avoid sending redundant state changes over SPI. With all
options off, FTGL uses about 20 bytes of RAM and FTUI adds about 15 more.

Included in the open source release are files supporting the Arduino. 

\* (Actually, there is a little bit of Arduino specific stuff behind some
platform ifdefs in FTUI, due to its different handling of program memory
reads).

# Licensing

This release of FTUI is licensed under the GPL v3. If you would like to obtain
a separate license of FTUI for use in closed-source projects, please contact
srudolph@stepper3.com.

