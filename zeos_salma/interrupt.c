/*
 * interrupt.c -
 */
#include <types.h>
#include <interrupt.h>
#include <segment.h>
#include <hardware.h>
#include <io.h>

#include <sched.h>

#include <zeos_interrupt.h>

#include <mm_address.h>
extern get_cr2();

#define MAX_THREAD_PAGES 4
#define DATA_START (L_USER_START + NUM_PAG_CODE*PAGE_SIZE)
#define DATA_END  (L_USER_START + (NUM_PAG_CODE+NUM_PAG_DATA)*PAGE_SIZE)


Gate idt[IDT_ENTRIES];
Register    idtR;

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

int zeos_ticks = 0;

void clock_routine()
{
  zeos_show_clock();
  zeos_ticks ++;
  
  schedule();
}
void keyboard_routine()
{
  unsigned char c = inb(0x60);
  
  if (c&0x80) printc_xy(0, 0, char_map[c&0x7f]);
}

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

void clock_handler();
void keyboard_handler();
void system_call_handler();

void setMSR(unsigned long msr_number, unsigned long high, unsigned long low);

void setSysenter()
{
  setMSR(0x174, 0, __KERNEL_CS);
  setMSR(0x175, 0, INITIAL_ESP);
  setMSR(0x176, 0, (unsigned long)system_call_handler);
}

void setIdt()
{
  /* Program interrups/exception service routines */
  idtR.base  = (DWord)idt;
  idtR.limit = IDT_ENTRIES * sizeof(Gate) - 1;
  
  set_handlers();

  /* ADD INITIALIZATION CODE FOR INTERRUPT VECTOR */
  setInterruptHandler(32, clock_handler, 0);
  setInterruptHandler(33, keyboard_handler, 0);

  setSysenter();

  set_idt_reg(&idtR);
}


void page_fault_routine() {

    unsigned long fault_address = get_cr2();

    printk("Process generates a PAGE FAULT exception at EIP: 0x");
    char buff[20];
    itoa_hexa(fault_address, buff);
    printk(buff);
    printk("\n");

	
    unsigned long fault_page = fault_address >> 12;
    unsigned long prev_fault_page = fault_page - 1;
    
    struct task_struct *current_task = current();
    

    // 1. nos aseguramos que est� dentro de la region de data+stack:
    if (fault_address < DATA_START || fault_address >= DATA_END) {
      printk("PAGE FAULT: address fuera de DATA+STACK");
      while(1);
    }
    
    // 2. esta dentro de la region del stack reservado a ESTE THREAD
    if (fault_address < current_task->ustack_bottom || fault_address >= current_task->ustack_top) {
      printk("stackoverflow");
      while(1);
    }
    
    // 3. ver si la p�gina anterior no esta mapeada, 
    page_table_entry *PT = get_PT(current_task);

    if (PT[prev_fault_page].bits.present == 0) {
      // entonces no sé que hacer
      printk("error de gaps?");
      while(1);
    }

    // 4. Asignación de la nueva página
    int frame = alloc_frame();
    if (frame == -1) {
        printk("No hay frames libres para expandir la pila\n");
        while(1);  // o matar thread
    }

    // Mapeamos la nueva página lógica a la memoria física
    set_ss_pag(current_task->page_table, fault_page, frame);

    // Actualizamos el puntero de la página más baja de la pila
    current_task->ustack_bottom_page = fault_page;

    // Hacemos el flush de la TLB para asegurarnos de que la traducción es correcta
    set_cr3(current_task->dir_pages_baseAddr);
    
}
