/*
 * interrupt.c -
 */
#include <types.h>
#include <interrupt.h>
#include <segment.h>
#include <hardware.h>
#include <io.h>
#include <libc.h>
#include <sched.h>

#include <zeos_interrupt.h>

Gate idt[IDT_ENTRIES];
Register    idtR;

int zeos_ticks = 0;

char char_map[] =
{
  '\0','\0','1','2','3','4','5','6',
  '7','8','9','0','\'','�','\0','\0',
  'q','w','e','r','t','y','u','i',
  'o','p','`','+','\0','\0','a','s',
  'd','f','g','h','j','k','l','�',
  '\0','�','\0','�','z','x','c','v',
  'b','n','m',',','.','-','\0','*',
  '\0','\0','\0','\0','\0','\0','\0','\0',
  '\0','\0','\0','\0','\0','\0','\0','7',
  '8','9','-','4','5','6','+','1',
  '2','3','0','\0','\0','\0','<','\0',
  '\0','\0','\0','\0','\0','\0','\0','\0',
  '\0','\0'
};

void setInterruptHandler(int vector, void (*handler)(), int maxAccessibleFromPL)
{
  /***********************************************************************/
  /* THE INTERRUPTION GATE FLAGS:                          R1: pg. 5-11  */
  /* ***************************                                         */
  /* flags = x xx 0x110 000 ?????                                        */
  /*         |  |  |                                                     */
  /*         |  |   \ D = Size of gate: 1 = 32 bits; 0 = 16 bits         */
  /*         |   \ DPL = Num. higher PL from which it is accessible      */
  /*          \ P = Segment Present bit                                  */
  /***********************************************************************/
  Word flags = (Word)(maxAccessibleFromPL << 13);
  flags |= 0x8E00;    /* P = 1, D = 1, Type = 1110 (Interrupt Gate) */

  idt[vector].lowOffset       = lowWord((DWord)handler);
  idt[vector].segmentSelector = __KERNEL_CS;
  idt[vector].flags           = flags;
  idt[vector].highOffset      = highWord((DWord)handler);
}

void setTrapHandler(int vector, void (*handler)(), int maxAccessibleFromPL)
{
  /***********************************************************************/
  /* THE TRAP GATE FLAGS:                                  R1: pg. 5-11  */
  /* ********************                                                */
  /* flags = x xx 0x111 000 ?????                                        */
  /*         |  |  |                                                     */
  /*         |  |   \ D = Size of gate: 1 = 32 bits; 0 = 16 bits         */
  /*         |   \ DPL = Num. higher PL from which it is accessible      */
  /*          \ P = Segment Present bit                                  */
  /***********************************************************************/
  Word flags = (Word)(maxAccessibleFromPL << 13);

  //flags |= 0x8F00;    /* P = 1, D = 1, Type = 1111 (Trap Gate) */
  /* Changed to 0x8e00 to convert it to an 'interrupt gate' and so
     the system calls will be thread-safe. */
  flags |= 0x8E00;    /* P = 1, D = 1, Type = 1110 (Interrupt Gate) */

  idt[vector].lowOffset       = lowWord((DWord)handler);
  idt[vector].segmentSelector = __KERNEL_CS;
  idt[vector].flags           = flags;
  idt[vector].highOffset      = highWord((DWord)handler);
}

void my_page_fault_handler();

void keyboard_handler();
void clock_handler();

void system_call_handler();

void syscall_handler_sysenter();
void writeMSR(unsigned long msr, unsigned long valor);

void setIdt()
{
  /* Program interrups/exception service routines */
  idtR.base  = (DWord)idt;
  idtR.limit = IDT_ENTRIES * sizeof(Gate) - 1;
  
  set_handlers();

  /* ADD INITIALIZATION CODE FOR INTERRUPT VECTOR */

  //Excepciones
  setInterruptHandler(14, my_page_fault_handler, 0);

  //Interrupciones
  setInterruptHandler(33, keyboard_handler, 0);
  setInterruptHandler(32, clock_handler, 0);

  //Syscalls por interrupcion
  setInterruptHandler(0x80, system_call_handler, 3);

  //Syscalls por sysenter (Metodo MSR)
  writeMSR(0x174,   __KERNEL_CS);
  writeMSR(0x175,  INITIAL_ESP);
  writeMSR(0x176,  (long)syscall_handler_sysenter);


  set_idt_reg(&idtR);
}


void keyboard_routine() {
  //Lee caracter y si es make/break.
  unsigned char port_info = inb(0x60);   

  //Bit 7: 0=Make / 1=Break  --> Uso i_break para q concuerden 1 o 0.
  //Bit 0..6: Character
  int is_break = port_info & 0x80;

  //Si es make imprime el caracter usando char_map.
  if (is_break == 0) {
    char c = char_map[port_info & 0x7F];
    printc_xy(0, 0, c);
  } 
}

void clock_routine() {
  static int ticks = 0;
  static int state = 0;

  zeos_ticks++;
  zeos_show_clock();

  ticks++;

  if (ticks == 100) {
	  ticks = 0;
	  if (state == 0 ) {
	  	task_switch((union task_union *)idle_task);
		state = 1;
	  } else {
		 task_switch((union task_union *)init_task);
		 state = 0;
 	 }
  }
}

void my_page_fault_routine(int aux, int addr) {
  printk("Process generates a PAGE FAULT exception at EIP: 0x");
  char buff[20];
  itoa_hexa(addr, buff);
  printk(buff);
  printk("\n");
  while(1);
}
