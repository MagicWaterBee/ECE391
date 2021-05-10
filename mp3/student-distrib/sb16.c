#include "sb16.h"
#include "lib.h"
#include "i8259.h"

#define BLOCK_SIZE     (32*1024)
#define BUFFER_SIZE    (2 * BLOCK_SIZE)

//Allocate a buffer that does not cross a 64k physical page boundary
static int8_t DMA_Buffer[BUFFER_SIZE] __attribute__((aligned(32768))) = {};
static int8_t *block1 = DMA_Buffer;
static int8_t *block2 = &(DMA_Buffer[BLOCK_SIZE]);
static int8_t *cur_block = DMA_Buffer;
static int8_t audio_filename[33];
static uint32_t current_offset;
static int8_t is_playing = 0;
static uint32_t audio_file_inode = 0;

// I/O port addresses for lower page registers, channel 4 is unused
static int8_t page_ports[8] = {0x87, 0x83, 0x81, 0x82, 0x00, 0x8B, 0x89, 0x8A};

/* initialize SB16 */
void sb16_init(){
    enable_irq(SB16_IRQ);
}

void DSP_outb(uint8_t data, uint8_t port_offset){
    outb(data, SB16_IOBase + port_offset);
}

uint8_t DSP_inb(uint8_t port_offset){
    return inb(SB16_IOBase + port_offset);
}

int8_t Reset_DSP(){
    uint32_t _3ms = 1000 * 0.03;
    uint8_t i = 0;
    uint8_t read_val = 0;
    DSP_outb(1, DSP_Reset);
    while(i < _3ms)
        i ++;
    DSP_outb(0, DSP_Reset);

    while(1){
        read_val = DSP_inb(DSP_Read_Buffer_Status);
        if(read_val & (1 << 7))
            break;
    }

    while(1){
        read_val = DSP_inb(DSP_Read);
        if(read_val & 0xAA)
            break;
    }

    return 0;
}

int8_t Write_DSP(uint8_t data){
    volatile uint8_t read_val = 0;
    while(1){
        read_val = DSP_inb(DSP_Write);
        if(!(read_val & (1 << 7)))
            break;
    }

    DSP_outb(data, DSP_Write);

    return 0;
}

uint8_t Read_From_DSP(){
    uint8_t read_val = 0;
    while(1){
        read_val = DSP_inb(DSP_Read_Buffer_Status);
        if(!(read_val & (1 << 7)))
            break;
    }

    return DSP_inb(DSP_Read);
}

int8_t Transfer_Sound_DMA(uint8_t channel, uint8_t mode, uint32_t addr, uint32_t size){
    uint32_t count = size;

    if(channel <= 3)
        outb(channel | DMA_BIT_2, DMA_1_MASK);
    else
        outb((channel % 4) | DMA_BIT_2, DMA_2_MASK);

    if(channel <= 3)
        outb(0, DMA_1_CLEAR_PTR);
    else
        outb(0, DMA_2_CLEAR_PTR);

    if(channel <= 3)
        outb(mode | channel, DMA_1_MODE);
    else
        outb(mode | (channel % 4), DMA_2_MODE);

    if(channel <= 3)
        outb(addr >> 16, page_ports[channel]);
    else
        outb((addr >> 16) & 0xFE, page_ports[channel]);

    if(channel <= 3){
        outb(addr & 0xFF, DMA_1_BASE + (channel << 1));
        outb((addr >> 8) & 0xFF, DMA_1_BASE + (channel << 1));
    }
    else{
        outb((addr / 2) & 0xFF, DMA_2_BASE + (channel << 2));
        outb(((addr / 2) >> 8) & 0xFF, DMA_2_BASE + (channel << 2));
    }

    count --;
    if(channel <= 3){
        outb(count & 0xFF, DMA_1_BASE + (channel << 1) + 1);
        outb((count >> 8) & 0xFF, DMA_1_BASE + (channel << 1) + 1);
    }
    else{
        outb((count / 2) & 0xFF, DMA_2_BASE + (channel << 2) + 2);
        outb(((count / 2) >> 8) & 0xFF, DMA_2_BASE + (channel << 2) + 2);
    }

    if(channel <= 3)
        outb(channel, DMA_1_MASK);
    else
        outb((channel % 4), DMA_2_MASK);
    return 0;
}

int8_t Set_Sample_Rate(uint16_t frequency){
    Write_DSP(SB_OUTPUT_RATE);
    Write_DSP((uint8_t)((frequency >> 8) & 0xFF));
    Write_DSP((uint8_t)(frequency & 0xFF));
    return 0;
}


int8_t play_music(int8_t* filename){
    if(is_playing == 1){
        return 0;
    }
    dentry_t audio_dentry;
    if (read_dentry_by_name((uint8_t*)filename, &audio_dentry) == -1){
        printf("No Such File");
        return -1;
    }
    else
        audio_file_inode = audio_dentry.inode_idx;
    uint8_t magic[4];
    read_data(audio_file_inode, 0, magic, 4);
    if(*((uint32_t*)magic) != RIFF){
        printf("file cannot be played! %s\n", magic);
        return 0;
    }

    Reset_DSP();
    strncpy(audio_filename, filename, strlen(filename));
    
    uint32_t bytes_read = read_data(audio_file_inode,current_offset, (uint8_t*)DMA_Buffer, BLOCK_SIZE*2);
    Transfer_Sound_DMA(1, 0x48 | 0x10, (uint32_t)(&DMA_Buffer[0]), sizeof(DMA_Buffer));
    Set_Sample_Rate(8000);
    start_play(BLOCK_SIZE);
    is_playing = 1;
    current_offset += bytes_read;

    return 0;
}

void start_play(uint32_t block_size){
    uint16_t blksize = block_size;
    uint8_t commond = 0;
    uint8_t mode = 0;
    commond |= Start_8_bit;
    commond |= DSP_Reset;
    mode |= 0x00;
    Write_DSP(commond);
    Write_DSP(mode);
    blksize --;
    Write_DSP((uint8_t)(blksize & 0xFF));
    Write_DSP((uint8_t)((blksize >> 8) & 0xFF));
    //printf("helloworld");
    return;
}

void stop(){
    uint8_t i = 0;
    Write_DSP(Pause_8_bit);
    is_playing = 0;
    audio_file_inode = 0;
    current_offset = 0;
    cur_block = block1;
    for(i = 0; i < 33; i ++){
        audio_filename[i] = 0;
    }
}

void sb16_handler(){
    DSP_inb(0xE);
    send_eoi(5);
    cli();
    uint32_t bytes_read = read_data(audio_file_inode, current_offset, (uint8_t*)cur_block, BLOCK_SIZE);
    current_offset += bytes_read;
    sti();

    if(bytes_read == BLOCK_SIZE){
        uint16_t blksize = (uint16_t) bytes_read;
        if(cur_block == block1)
            cur_block = block2;
        else
            cur_block = block1;
        start_play(blksize);
    }
    else if(bytes_read == 0)
        stop();

    return;
}

