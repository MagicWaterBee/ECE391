#include "mouse.h"
#include "keyboard.h"
#include "lib.h"


int32_t mouse_x = 0;
int32_t mouse_y = 0;


/* 
 *  mouse_init(void)
 *  DESCRIPTION: Initialize the mouse drive
 */
void mouse_init(void){
    _output_wait();
    outb(0xa8,MOUSE_PORT);
    _output_wait();
    outb(0x20,MOUSE_PORT);
    _input_wait();
    uint8_t status = inb(KEYBOARD_PORT);
    status |= 2;
    status &= 0xdf;
    _output_wait();
    outb(KEYBOARD_PORT,MOUSE_PORT);
    _output_wait();
    outb(status,KEYBOARD_PORT);
    write_60(0xf6);
    read_60();
    write_60(0xf4);
    read_60();
    write_60(0xf3);
    read_60();
    _output_wait();
    outb(200, KEYBOARD_PORT);
    enable_irq(MOUSE_IRQ);
}

/* 
 *  mouse_irq_handler(void)
 *  DESCRIPTION: handle mouse movement
 */
void mouse_handler(void){
    cli();
    send_eoi(MOUSE_IRQ);
    mouse_packet_t packet;
    packet.val = read_60();

    /* If invalid return */
    if (packet.x_overflow || packet.y_overflow || !packet.always1)
        return;
    /* Get the delta x and y, used to update the position of the mouse*/
    int32_t x_move = read_60();
    int32_t y_move = read_60();

    /* Sign check, if it is negative, change its value to approprate amount*/
    if (packet.x_sign)
        x_move = 0xFFFFFF00 | x_move;
    if (packet.y_sign)
        y_move = 0xFFFFFF00 | y_move;
    
    x_move /= 5;
    y_move /= 5;

    change_color(mouse_x, mouse_y, ATTRIB);

    mouse_x += x_move;
    mouse_y -= y_move;

    if(mouse_x < 0)
        mouse_x = 0;
    if(mouse_y < 0)
        mouse_y = 0;
    if(mouse_x > NUM_COLS-1)
        mouse_x = NUM_COLS-1;
    if(mouse_y > NUM_ROWS-1 )
        mouse_y = NUM_ROWS-1;

    change_color(mouse_x, mouse_y, ATTRIB_BG);
    sti();
}


/* 
 *  write_60(uint8_t data)
 *  DESCRIPTION: send command first to MOUSE_PORT, then write data to KEYBPARD_PORT
 */
void write_60(uint8_t data){
    _output_wait();
    outb(0xd4, MOUSE_PORT);
    _output_wait();
    outb(data, KEYBOARD_PORT);
}

/* 
 *  read_60(void)
 *  DESCRIPTION: read the data from the port KEYBOARD_PORT
 */
uint8_t read_60(void){
    _input_wait();
    return inb(KEYBOARD_PORT);
}

/* waiting functions for input and output */
void _output_wait(void){
    int i = 100000;
    while(i-- &&inb(MOUSE_PORT)&2);
}
void _input_wait(void){
    int i = 100000;
    while(i-- && inb(MOUSE_PORT)&1);
}
