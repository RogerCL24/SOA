#include <libc.h>

char buff[24];

char char_map2[] =
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

int pid;

int __attribute__ ((__section__(".text.main")))
  main(void)
{
    /* Next line, tries to move value 0 to CR3 register. This register is a privileged one, and so it will raise an exception */
     /* __asm__ __volatile__ ("mov %0, %%cr3"::"r" (0) ); */

  char keyboard[128];
  for (int i = 0; i < 128; ++i) keyboard[i] = 0;
  write(1, "\nHello!\n", 8);


  //TEST pause
    /*
      write(1, "I WILL PAUSE\n", 13);
      if (pause(100) < 0) perror();
      write(1, "FIRST PAUSE\n", 12);
      if (pause(1000) < 0) perror();
      write(1, "SECOND PAUSE\n", 13);
      if (pause(10000) < 0) perror();
      write(1, "THIRD PAUSE\n", 12);
    */

  while(1) { 

    //TEST GetKeyboardState
    /*
      if (GetKeyboardState(keyboard) < 0) perror();
      if (write(1, "\nKeys pressed: ",15) <= 0) perror();
      for (int i = 0; i < 128; ++i) {
        if (keyboard[i] == 1) {
          buff[0] = char_map2[i];
          write(1, buff, 1);
          write (1, " ", 1);
        }
      }
      getpid();
    */

    //TEST GetKeyboardState y pause
      if (GetKeyboardState(keyboard) < 0) perror();
      if (write(1, "\nKeys pressed: ",15) <= 0) perror();
      for (int i = 0; i < 128; ++i) {
        if (keyboard[i] == 1) {
          buff[0] = char_map2[i];
          write(1, buff, 1);
          write (1, " ", 1);
        }
      }

      pause(1000);     

  }

}
