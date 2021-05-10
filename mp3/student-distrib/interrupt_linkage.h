#ifndef _INTERRUPT_LINKAGE_H
#define _INTERRUPT_LINKAGE_H
#ifndef ASM

/* RTC interrupt linkage code */
extern void int_rtc();
/* keyboard interrupt linkage code */
extern void int_keyboard();
/* mouse interrupt linkage code */
extern void int_mouse();
/* PIT interrupt linkage code */
extern void int_pit();
/* sb16 interrupt linkage code */
extern void int_sb16();

#endif
#endif
