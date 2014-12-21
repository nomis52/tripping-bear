/* 
 * File:   main.c
 * Author: Simon Newton
 *
 * Created on December 20, 2014, 6:11 PM
 */
#ifdef __XC32
#include <xc.h>          /* Defines special funciton registers, CP0 regs  */
#endif

// TODO(simon): Switch to using harmony
#define _SUPPRESS_PLIB_WARNING


#include <stdio.h>
#include <stdlib.h>
#include <plib.h>           /* Include to use PIC32 peripheral libraries      */
#include <stdint.h>         /* For uint32_t definition                        */
#include <stdbool.h>        /* For true/false definition                      */


// DEFINES
//---------------------

#define SYS_FREQ     80000000L
#define DMX_FREQ     250000
#define DMX_UART UART1

// Config, this makes the clock run at 80Mhz.
//---------------------

#pragma config FPLLMUL = MUL_20, FPLLIDIV = DIV_2
#pragma config FPLLODIV = DIV_1, FWDTEN = OFF
#pragma config POSCMOD = HS, FNOSC = PRIPLL

volatile bool finished_tx = false;

/*
const uint8_t data[] = {
  1, 2, 3, 4, 5, 6, 7, 8, 9, 10,
  11, 12, 13, 14, 15, 16, 17, 18,
  19, 20, 21, 22, 23, 24, 25, 26
};

struct TxBuffer {
  const uint8_t *start;
  const uint8_t *end;
  uint8_t offset;
};
 */

void sleep() {
  int i = 1024 * 1024;
  while (i--);
}

void sendByteAndWait(uint8_t byte) {
  mPORTDSetBits(BIT_0);
  UARTSendDataByte(DMX_UART, byte);
  while (!UARTTransmissionHasCompleted(DMX_UART));
  mPORTDClearBits(BIT_0);
}

void sendBufferAndWaitForInterrupt() {
  mPORTDSetBits(BIT_0);
  finished_tx = false;

  UARTSendDataByte(DMX_UART, 1);
  UARTSendDataByte(DMX_UART, 4);
  INTEnable(INT_SOURCE_UART_TX(DMX_UART), INT_ENABLED);
  while (!finished_tx);
  INTEnable(INT_SOURCE_UART_TX(DMX_UART), INT_DISABLED);

  mPORTDClearBits(BIT_0);
}

void __ISR(_UART1_VECTOR, ipl6)AdministratorUART1(void) {
  // Is this an RX interrupt?
  if (INTGetFlag(INT_SOURCE_UART_TX(DMX_UART))) {
    finished_tx = true;
    // INTClearFlag(INT_SOURCE_UART_TX(DMX_UART));
    INTEnable(INT_SOURCE_UART_TX(DMX_UART), INT_DISABLED);
  }
}

/*
 * 
 */
int main(int argc, char** argv) {
  // pb_clock is 80Mhz.
  unsigned int pb_clock = SYSTEMConfig(SYS_FREQ, SYS_CFG_ALL);

  INTConfigureSystem(INT_SYSTEM_CONFIG_MULT_VECTOR);

  mPORTDClearBits(BIT_7 | BIT_6 | BIT_5 | BIT_4 |
                  BIT_3 | BIT_2 | BIT_1 | BIT_0);

  mPORTDSetPinsDigitalOut(BIT_7 | BIT_6 | BIT_5 | BIT_4 |
                          BIT_3 | BIT_2 | BIT_1 | BIT_0);

  // May want to include UART_ENABLE_LOOPBACK in here for testing
  UARTConfigure(DMX_UART, UART_ENABLE_PINS_TX_RX_ONLY | UART_ENABLE_PINS_BIT_CLOCK);
  // FPB = 80Mhz, Baud Rate = 250kHz. Low speed mode (16x) gives BRG = 19.0
  UARTSetDataRate(DMX_UART, pb_clock, DMX_FREQ);
  UARTSetLineControl(DMX_UART,
                     UART_DATA_SIZE_8_BITS | UART_PARITY_NONE |
                     UART_STOP_BITS_2);

  INTSetVectorPriority(INT_VECTOR_UART(DMX_UART), INT_PRIORITY_LEVEL_6);
  INTSetVectorSubPriority(INT_VECTOR_UART(DMX_UART), INT_SUB_PRIORITY_LEVEL_0);
  UARTSetFifoMode(DMX_UART, UART_INTERRUPT_ON_TX_BUFFER_EMPTY);

  INTClearFlag(INT_U1TX);
  INTEnableInterrupts();

  UARTEnable(DMX_UART, UART_ENABLE_FLAGS(UART_PERIPHERAL | UART_TX));

  // Initialization complete
  while (1) {
    mPORTDClearBits(BIT_2 | BIT_1 | BIT_0);

    //    sendByteAndWait(0x11);
    //    sendByteAndWait(0x55);
    //    sendByteAndWait(0xaa);
    //    sendByteAndWait(0x10);
    sendBufferAndWaitForInterrupt();


    // Insert some delay
    sleep();
  }
  return (EXIT_SUCCESS);
}

