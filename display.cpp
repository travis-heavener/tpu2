#include "display.hpp"
#include "tpu.hpp"

Display::Display() {
    // allocate the grid
    pDisplay = new u8[DISPLAY_HEIGHT * DISPLAY_WIDTH];
}

Display::~Display() {
    delete[] pDisplay; // free the grid
}

void Display::renderFrame(TPU& tpu, Memory& memory) {
    // store the IP in BP
    tpu.moveToRegister(Register::BP, tpu.readRegister16(Register::IP).getValue());
    tpu.sleep();

    // store frame index in AX
    tpu.moveToRegister(Register::AX, 0);
    tpu.sleep();

    // move IP to the start of the frame buffer
    tpu.moveToRegister(Register::IP, FRAME_BUF_LOWER_ADDR);
    tpu.sleep();

    // read words into BX
    tpu.moveToRegister( Register::BX, tpu.readWord(memory).getValue() );
    tpu.sleep(2);

    // use the lowest 6 bits as color
    pDisplay[ tpu.readRegister16(Register::AX).getValue() ] = tpu.readRegister16(Register::BX).getValue() & 63;
    tpu.sleep();

    // shift to set next bytes into the lowest bits
    tpu.moveToRegister( Register::BX, tpu.readRegister16(Register::BX).getValue() >> (DISPLAY_BIT_DEPTH * 3) );
    tpu.sleep();

    // read next 6 bits

    // move BP to IP
    tpu.moveToRegister(Register::IP, tpu.readRegister16(Register::BP).getValue());
    tpu.sleep();
}