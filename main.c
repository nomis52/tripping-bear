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

#define SYS_FREQ     80000000L  // 80Mhz
#define DMX_FREQ     250000     // 250kHz
#define DMX_UART UART1
// Port F, Bit 8 doubles as the UART1 TX pin
#define DMX_PORT IOPORT_F
#define DMX_PORT_BIT BIT_8

#define BREAK_TICKS 14040  // Should be 14080, but this gives a break of 176uS
#define MARK_TICKS 900  // Should be 960, but this gives a mark of of 16 uS

#define ARRAYSIZE(a) \
  ((sizeof(a) / sizeof(*(a))) / \
  (!(sizeof(a) % sizeof(*(a)))))

// Config, this makes the clock run at 80Mhz.
//---------------------

#pragma config FPLLMUL = MUL_20, FPLLIDIV = DIV_2
#pragma config FPLLODIV = DIV_1, FWDTEN = OFF
#pragma config POSCMOD = HS, FNOSC = PRIPLL

volatile bool finished_tx = false;

// Null start code.
const uint8_t data[] = {
  0, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10,
  11, 12, 13, 14, 15, 16, 17, 18,
  19, 20, 21, 22, 23, 24, 25, 26
};

typedef struct {
  // The next byte to send
  const uint8_t *ptr;
  // The last byte to send
  const uint8_t *end;
} TxBuffer;

typedef enum {
  BREAK,
  IN_BREAK,
  MARK,
  IN_MARK,
  BEGIN_SEND,
  SENDING,
  SLEEPING,
} AppState;

TxBuffer tx = {
  .ptr = NULL,
  .end = NULL,
};

AppState app_state = SLEEPING;

void sleep() {
  int i = 1024 * 1024;
  while (i--);
}

void sendBytes() {
  while (U1STAbits.UTXBF == 0 && tx.ptr <= tx.end) {
    UARTSendDataByte(DMX_UART, *(tx.ptr++));
  }
}

void startTx() {
  finished_tx = false;

  sendBytes();

  INTEnable(INT_SOURCE_UART_TX(DMX_UART), INT_ENABLED);
}

void resetTxBuffer() {
  tx.ptr = data;
  tx.end = data + ARRAYSIZE(data) - 1;
}

void __ISR(_UART1_VECTOR, ipl6)AdministratorUART1(void) {
  // Is this an RX interrupt?
  if (INTGetFlag(INT_SOURCE_UART_TX(DMX_UART))) {
    INTClearFlag(INT_SOURCE_UART_TX(DMX_UART));
    sendBytes();
    if (tx.ptr > tx.end) {
      finished_tx = true;
      INTEnable(INT_SOURCE_UART_TX(DMX_UART), INT_DISABLED);
    }
  }
}

void __ISR(_TIMER_1_VECTOR, ipl2) Timer1Handler(void) {
  mT1ClearIntFlag();
  switch (app_state) {
    case IN_BREAK:
      PORTSetBits(DMX_PORT, DMX_PORT_BIT);
      app_state = MARK;
      break;
    case IN_MARK:
      app_state = BEGIN_SEND;
      break;
    default:
      ;
  }
}

void Init() {
  // pb_clock is 80Mhz.
  unsigned int pb_clock = SYSTEMConfig(SYS_FREQ, SYS_CFG_ALL);

  INTConfigureSystem(INT_SYSTEM_CONFIG_MULT_VECTOR);

  // Port D configuration
  mPORTDClearBits(BIT_7 | BIT_6 | BIT_5 | BIT_4 |
                  BIT_3 | BIT_2 | BIT_1 | BIT_0);

  mPORTDSetPinsDigitalOut(BIT_7 | BIT_6 | BIT_5 | BIT_4 |
                          BIT_3 | BIT_2 | BIT_1 | BIT_0);

  PORTClearBits(DMX_PORT, DMX_PORT_BIT);
  PORTSetPinsDigitalOut(DMX_PORT, DMX_PORT_BIT);

  // UART configuration
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
}

/*
 * 
 */
int main(int argc, char** argv) {
  Init();
  PORTSetBits(DMX_PORT, DMX_PORT_BIT);

  while (1) {
    switch (app_state) {
      case BREAK:
        mPORTDSetBits(BIT_0);
        UARTEnable(DMX_UART, UART_DISABLE_FLAGS(UART_PERIPHERAL | UART_TX));
        OpenTimer1(T1_ON | T1_SOURCE_INT | T1_PS_1_1, BREAK_TICKS);
        ConfigIntTimer1(T1_INT_ON | T1_INT_PRIOR_2);
        PORTClearBits(DMX_PORT, DMX_PORT_BIT);
        app_state = IN_BREAK;
        break;
      case IN_BREAK:
        // noop
        break;
      case MARK:
        OpenTimer1(T1_ON | T1_SOURCE_INT | T1_PS_1_1, MARK_TICKS);
        ConfigIntTimer1(T1_INT_ON | T1_INT_PRIOR_2);
        app_state = IN_MARK;
        break;
      case IN_MARK:
        break;
      case BEGIN_SEND:
        UARTEnable(DMX_UART, UART_ENABLE_FLAGS(UART_PERIPHERAL | UART_TX));
        CloseTimer1();
        resetTxBuffer();
        startTx();
        app_state = SENDING;
        break;
      case SENDING:
        if (finished_tx) {
          app_state = SLEEPING;
          mPORTDClearBits(BIT_0);
        }
        break;
      case SLEEPING:
        sleep();
        app_state = BREAK;
        break;
    }
  }
  return (EXIT_SUCCESS);
}