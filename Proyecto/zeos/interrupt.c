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

#include <utils.h>

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
char keys[128];   //Vector de les tecles possibles -> 1 si s'ha apretat 0 si no.

int zeos_ticks = 0;

extern struct list_head blocked;
extern struct list_head readyqueue;
int update_blocked_time() {
  //Guardarem quin es el procés de més prioritat dels que es desbloquejen
  struct task_struct *max_prio_proc = NULL;
	int max_prio = current()->priority;

  struct list_head *pos, *tmp;    
  list_for_each_safe(pos, tmp, &blocked) {
    struct task_struct *t= list_entry(pos, struct task_struct, list);
    t->pause_time--;
    if (t->pause_time <= 0) {
      update_process_state_rr(t, &readyqueue);

      //Si la prioritat es la maxima dels procéssos que es desbloquejen es guarda a la variable
      if (t->priority > max_prio) {
				max_prio_proc = t;
				max_prio = t->priority;
			}
    }
  }
  //En afegir ara el prcés de màxima prioritat, aquest serà el primer a la cua (Crec q no es necessari tot això de max_prio)
  if (max_prio_proc) {
		if (current() != idle_task) {
			update_process_state_rr(current(), &readyqueue);
		}
		sched_next_rr();
		return 1;
	}

	return 0;
}

void dumpScreen() {
  Word* screen = (Word *)0xb8000;
  Word* content = (Word*)(current()->screen_page);
  Word color = 0x07;

  for (int i = 0; i < 80*25; ++i) {
    screen[i] = content[i];
  }
}

void clock_routine()
{
  zeos_show_clock();
  zeos_ticks ++;
  
  //Per gestió Keyboard
  int changed = update_blocked_time();

  //Per gestió pantalla
    //La primera entrada a clock_routine encara no estem executant init (Això feia que screen_page no estigués inicialitzat) -> Solucio?
  if (current()->PID != -1 && current()->screen_page != (void*)-1) dumpScreen();

  if (!changed) schedule();
}

void keyboard_routine()
{
  unsigned char c = inb(0x60);
  
  if (!(c&0x80)) {
    keys[c&0x7f] = 1;
    //printc_xy(0, 0, char_map[c&0x7f]);
  }
  else if ((c&0x80)) {
    keys[c&0x7f] = 0;
    //printc_xy(0, 0, char_map[c&0x7f]);
  }
  
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

void my_page_fault_handler();

void setIdt()
{
  /* Program interrups/exception service routines */
  idtR.base  = (DWord)idt;
  idtR.limit = IDT_ENTRIES * sizeof(Gate) - 1;
  
  set_handlers();

  //Excepciones
  setInterruptHandler(14, my_page_fault_handler, 0);

  /* ADD INITIALIZATION CODE FOR INTERRUPT VECTOR */
  setInterruptHandler(32, clock_handler, 0);
  setInterruptHandler(33, keyboard_handler, 0);

  setSysenter();

  set_idt_reg(&idtR);
}

void my_page_fault_routine(int aux, int addr) {
  printk("Process generates a PAGE FAULT exception at EIP: 0x");
  char buff[20];
  itoa_hexa(addr, buff);
  printk(buff);
  printk("\n");
  while(1);
}