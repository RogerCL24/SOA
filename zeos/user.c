#include <libc.h>
#include <io.h>
#include <errno.h>

char buff[24];

int pid;

int add(int par1,int par2) {
	return par1 + par2;
}

int addAsm(int par1, int par2);

int __attribute__ ((__section__(".text.main")))
  main(void)
{
    /* Next line, tries to move value 0 to CR3 register. This register is a privileged one, and so it will raise an exception */
     /* __asm__ __volatile__ ("mov %0, %%cr3"::"r" (0) ); */

    
  //write(1, "Hello World!\n", 13);
  char* p = 0;
  *p = 'x';

  while(1) { 
  	//int res = add(0x42, 0x666);
  	//int resASM = addAsm(0x42, 0x666);
  	//printk("Hello World");
  }
}
