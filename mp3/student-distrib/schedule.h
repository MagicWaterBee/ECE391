#ifndef _SCHEDULE_H
#define _SCHEDULE_H

#include "i8259.h"

#define PIT_CMD_PORT        0x43
#define PIT_CHANNEL_0       0x40
#define PIT_BINARY_MODE     0           /* 0b0      16-bit binary                    */
#define PIT_OP_MODE         3           /* 0b011    Mode 3 (square wave generator)   */
#define PIT_AC_MODE         3           /* 0b11     lobyte / hibyte                  */
#define PIT_CHANNEL         0           /* 0b00     Channel 0                        */
#define PIT_CMD             ((PIT_CHANNEL << 6) | (PIT_AC_MODE << 4) | (PIT_OP_MODE << 1) | (PIT_BINARY_MODE))
#define PIT_FREQ            100         /* PIT frequency in Hz              */
#define PIT_MAX_FREQ        1193180     /* PIT max freqncy in Hz            */
#define PIT_LATCH           ((int)((PIT_MAX_FREQ + PIT_FREQ / 2) / PIT_FREQ))   /* number of periods to wait */
#define PIT_BITMASK         0xff        /* mask most significant bits       */
#define PIT_MSB_OFFSET      8

extern void pit_init();

extern void pit_handler();

void scheduler();

void timer_wait(uint32_t ms);

#endif
