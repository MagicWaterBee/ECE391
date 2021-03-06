#include "keyboard.h"
#include "lib.h"
#include "i8259.h"
#include "terminal.h"
#include "speaker.h"


static unsigned char caps_state = 0;
static unsigned char shift_state = 0;
static unsigned char ctrl_state = 0;
static unsigned char alt_state = 0;

// volatile unsigned char read_buffer[READ_BUFFER_SIZE];
// volatile int read_buffer_ptr = 0;

// array for basic key inputs
unsigned char key_table[4][KEY_NUM] = {
	// default
	{'\0', '\0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\0', '\0',
	'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\0', '\0', 'a', 's',
	'd', 'f', 'g', 'h', 'j', 'k', 'l' , ';', '\'', '`', '\0', '\\', 'z', 'x', 'c', 'v', 
	'b', 'n', 'm',',', '.', '/', '\0', '*', '\0', ' ', '\0'},
	// shift pressed
	{'\0', '\0', '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\0', '\0',
	'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\0', '\0', 'A', 'S',
	'D', 'F', 'G', 'H', 'J', 'K', 'L' , ':', '"', '~', '\0', '|', 'Z', 'X', 'C', 'V', 
	'B', 'N', 'M', '<', '>', '?', '\0', '*', '\0', ' ', '\0'},
	// caps lock enabled
	{'\0', '\0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\0', '\0',
	'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '[', ']', '\0', '\0', 'A', 'S',
	'D', 'F', 'G', 'H', 'J', 'K', 'L' , ';', '\'', '`', '\0', '\\', 'Z', 'X', 'C', 'V', 
	'B', 'N', 'M', ',', '.', '/', '\0', '*', '\0', ' ', '\0'},
	// both shift and caps lock
	{'\0', '\0', '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\0', '\0',
	'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '{', '}', '\0', '\0', 'a', 's',
	'd', 'f', 'g', 'h', 'j', 'k', 'l' , ':', '"', '~', '\0', '\\', 'z', 'x', 'c', 'v', 
	'b', 'n', 'm', '<', '>', '?', '\0', '*', '\0', ' ', '\0'}
};

/*
*	init_keyboard
*	Description: initializes the keyboard 
*	inputs:		nothing
*	outputs:	nothing
*	effects:	enables line for keyboard on the master PIC
*/
void keyboard_init(){
    enable_irq(KEYBOARD_IRQ);
}

/*
*	keyboard_handler
*	Description: If a valid key is pressed, the function echo it onto screen.
*	inputs:	 nothing
*	outputs: nothing
*	side effects: echo current pressed key to screen
*/
void keyboard_handler(){
    unsigned char scancode = 0;     /* scanned code */
    int i;                          /* loop index for tab */
    // mask interrupt
    cli();

    terminal_t* curr_term = &terminals[curr_term_id];
    volatile uint8_t* read_buffer = curr_term->term_buf;

    // wait for interrupt
    while(1){
        if (inb(KEYBOARD_PORT)){
            scancode = inb(KEYBOARD_PORT);
            break;
        }
    }

    
    switch (scancode){
        case CAPS_LOCK:
            caps_state = ~caps_state;
            break;
        case LSHIFT_DOWN:
            shift_state = 1;
            break;
        case RSHIFT_DOWN:
            shift_state = 1;
            break;
        case LSHIFT_UP:
            shift_state = 0;
            break;
        case RSHIFT_UP:
            shift_state = 0;
            break;
        case CTRL_DOWN:
            ctrl_state = 1;
            break;
        case CTRL_UP:
            ctrl_state = 0;
            break;
        case ALT_DOWN:
            alt_state = 1;
            break;
        case ALT_UP:
            alt_state = 0;
            break;
        case ENTER:
            read_buffer[curr_term->term_buf_offset] = '\n';
            curr_term->term_buf_offset += 1;
            /* if enter is pressed, set flag is_ready to tell the terminal ready to read */
            terminals[curr_term_id].is_enter = 1;
            newline();
            break;
        case BACKSPACE:
            if (curr_term->term_buf_offset>0){
                curr_term->term_buf_offset -= 1;
                read_buffer[curr_term->term_buf_offset] = '\0';
                delc();
            }
            break;
        case TAB:
            for (i=0; i<4; i++){
                read_buffer[curr_term->term_buf_offset] = ' ';
                curr_term->term_buf_offset += 1;
                putc(' '); 
            }
            break;
        default:
            print_key(scancode);
            break;
    }
    // end interrupt
    send_eoi(KEYBOARD_IRQ);
    // enable interrupt
    sti();
    beep();
}

/*
*	print_key
*	Description: If a valid scancode needs to be printed, show it to screen.
*	inputs:	 a scancode from keyboard.
*	outputs: nothing
*	side effects: echo character corresponds to scancode to screen.
*/
void print_key(unsigned char scancode){
    unsigned char key;  /* corresponding key value */
    
    terminal_t* curr_term = &terminals[curr_term_id];
    volatile uint8_t* read_buffer = curr_term->term_buf;

    // select different key modes based on shift and cpas state
    if (scancode >= KEY_NUM)
        return;
    else if (shift_state & caps_state){
        key = key_table[3][scancode];
    }
    else if (shift_state & (~caps_state)){
        key = key_table[1][scancode];
    }
    else if ((~shift_state) & caps_state){
        key = key_table[2][scancode];
    }
    else{
        key = key_table[0][scancode];
    }
        
    // we do not need print these NULL keys
    if (key == '\0')
    {
        // is_ready = 1;
        return;
    }
    // for alt+Fkeys, switch the terminal
    else if (alt_state){
        if (key == F1){
            send_eoi(KEYBOARD_IRQ);
            terminal_switch(0);
        }
        else if (key == F2){
            send_eoi(KEYBOARD_IRQ);
            terminal_switch(1);
        }
        else if (key == F3){
            send_eoi(KEYBOARD_IRQ);
            terminal_switch(2);
        }
    }
    // for ctrl+L, we clear the screen
    else if (ctrl_state){
        if (key == 'l' || key == 'L'){
            clear();
            reset_screen_xy();
            update_cursor(0,0);
            return;
        }
        else if (key == 'c')
            return;
    }
    // print the correct key
    else if (curr_term->term_buf_offset < READ_BUFFER_SIZE){
        read_buffer[curr_term->term_buf_offset] = key;
        curr_term->term_buf_offset += 1;
        putc(key);
    }
    // else
    //     is_ready = 1;
    return;
}

/*
*	clr_read_buffer
*	Description: Clear read buffer and reset its pointer.
*	inputs:	 nothing
*	outputs: nothing
*	side effects: the read buffer is reseted.
*/
void clr_read_buffer(){
    int i;  /* loop index for clearing read buffer */
    terminal_t* curr_term = &terminals[curr_term_id];
    volatile uint8_t* read_buffer = curr_term->term_buf;
    for (i=0; i<READ_BUFFER_SIZE; i++)
        read_buffer[i] = 0;
    curr_term->term_buf_offset = 0;
    /* clear is_ready */
	terminals[curr_term_id].is_enter = 0;
    return;
}

