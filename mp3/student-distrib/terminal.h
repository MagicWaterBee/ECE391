/*
    terminal driver header file
*/

#ifndef _TERMINAL_H
#define _TERMINAL_H

#include "types.h"

#define MAX_TERMINAL_BUF_SIZE   128
#define TERMINAL_NUM            3
#define FIRST_TERMINAL_ID       0

typedef struct terminal_t{

    uint32_t id;
    uint32_t is_running;
    uint32_t curr_pid;
    uint32_t pnum;      /* number of process running in this terminal */
    uint32_t cursor_x;
    uint32_t cursor_y;
    volatile uint32_t is_enter;
    volatile uint8_t term_buf[MAX_TERMINAL_BUF_SIZE];
    volatile uint8_t term_buf_offset;
    uint8_t *vid_buf;

} terminal_t;

terminal_t terminals[TERMINAL_NUM];

uint32_t curr_term_id;

int32_t terminal_init();

int32_t terminal_switch(uint32_t term_id);

int32_t terminal_save(uint32_t term_id);

int32_t terminal_restore();

int32_t launch_first_terminal();

/* open a terminal */
int32_t terminal_open(const char* filename);

/* close a terminal */
int32_t terminal_close(int32_t fd);

/* read the terminal input and store to a buffer */
int32_t terminal_read(int32_t fd, void* buf, int32_t nbytes);

/* write the corresponding number of bytes of a buffer to the terminal */
int32_t terminal_write(int32_t fd, void* buf, int32_t nbytes);


#endif
