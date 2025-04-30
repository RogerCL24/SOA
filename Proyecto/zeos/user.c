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

  unsigned short* screen = (unsigned short*)StartScreen();
  if (screen == (void*)-1) perror("Error al acceder a la pantalla");
  else {
    for (int i = 0; i < 80 * 25; ++i) {
        screen[i] = ' ';
    }
  }


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


  //TEST StartScreen
    /*

      if (screen == (void*)-1) perror();
      else {
        for (int i = 0; i < 80 * 25; ++i) {
            screen[i] = ' ';
        }

        screen[0] = 'H';
        screen[1] = 'e';
        screen[2] = 'l';
        screen[3] = 'l';
        screen[4] = 'o';
        screen[5] = ' ';
        screen[6] = 'W';
        screen[7] = 'o';
        screen[8] = 'r';
        screen[9] = 'l';
        screen[10] = 'd';

        write(1, "Screen initialized and written!\n", 32);

        for (int i = 0; i < 11; ++i) {
          buff[i] = screen[i];
        }
        buff[11] = '\0';

        write(1, "Wrote in screen: ", 17);
        write(1, buff, strlen(buff));
      }
    */

    pid = getpid;
    int cpid = fork();
    if (cpid > 0) {

    unsigned short color = 0x07;

    screen[0] = color << 8 | 'k';
    screen[1] = color << 8 | 'e';
    screen[2] = color << 8 | 'y';
    screen[3] = color << 8 | 's';
    screen[4] = color << 8 | ' ';
    screen[5] = color << 8 | 'p';
    screen[6] = color << 8 | 'r';
    screen[7] = color << 8 | 'e';
    screen[8] = color << 8 | 's';
    screen[9] = color << 8 | 's';
    screen[10] = color << 8 | 'e';
    screen[11] = color << 8 | 'd';
    screen[12] = color << 8 | ':';
    screen[13] = color << 8 | ' ';
      

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

      pause(1000);  
    */

    
    //TEST GetKeyboarState, pause i StartScreen
      
      //Si aqui fessim un fork no funcionaria pq screen guardaria una pos d'un altre proces, es un problema?  
      if (GetKeyboardState(keyboard) < 0) perror();
      int pos = 14;

      color = 1;
      for (int i = 0; i < 128; ++i) {
        if (keyboard[i] == 1) {
          screen[pos] = color << 8 | char_map2[i];
          pos += 2;
          ++color;
        }
      }
      for (int i = pos; i < 128; ++i) screen[i] = ' ';
      pause(1000);

  }
}
else {
  while(1) {
    //pid = getpid();
  }
}

}
