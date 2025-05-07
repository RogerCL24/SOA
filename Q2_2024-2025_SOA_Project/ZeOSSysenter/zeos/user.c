#include <libc.h>

# define CLONE_THREAD 1

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


char keyboard[128];

void *thread_func_1(void *arg) {
    if (*(int*)arg == 1) 
    	write(1, "Hello from thread 1!\n", 22);
    pthread_exit();
}

void test_simple_clone() {
    int val = 1;
    int tid = clone(CLONE_THREAD, thread_func_1, &val, 1024);
    if (tid < 0) {
	write(1, "clone failed\n", 13);
    	perror();
    }
    pause(50);
    write(1, "Done boss\n", 10);
}

void *thread_func_N(void *arg) {
   
    char* msg = (char *)arg;
    write(1, msg, strlen(msg));
    pthread_exit();
}

void test_multiple_threads() {
    int tid =  clone(CLONE_THREAD, thread_func_N, "Thread A\n", 1024);
    if (tid < 0) write(1, "clone failed\n", 13);
    tid = clone(CLONE_THREAD, thread_func_N, "Thread B\n", 1024);
    if (tid < 0) write(1, "clone failed\n", 13);
    tid = clone(CLONE_THREAD, thread_func_N, "Thread C\n", 1024);
    if (tid < 0) write(1, "clone failed\n", 13);
    pause(200);
   write(1, "Done boss, multiple\n", 20);
}

void *prio_func(void *arg) {
    int val = *((int*)arg);   // race condition...
    if (val == 0) {
        SetPriority(25);   // LOW
       	pause(1);
    }
    else
        SetPriority(30);  // HIGH
    
    if (val == 0)
	    for (int i = 0; i < 10; ++i) 
        	write(1, "LOW\n", 4);
    else
	    for (int i = 0; i < 10; ++i) { 
		if (i == 5) pause(1);			
        	write(1, "HIGH\n", 5);
	    }

    pthread_exit();
}

void test_priority() {
    int val1 = 0;
    clone(CLONE_THREAD, prio_func, &val1, 1024);  // LOW
    int val2 = 1;
    clone(CLONE_THREAD, prio_func, &val2, 1024);  // HIGH
    pause(1);
    write(1, "HIGH GOES BEFORE, RIGHT?\n", 25);
}

void draw_screen(unsigned short* screen) {

  if (screen != (void*) -1) {
	for (int i = 0; i < 2000; ++i) {
		screen[i] = ' ';
	}
  } else perror("Screen error");

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
   
 
    while (1) {
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

void *son_thread(void *arg) {

    write(1, "THREAD RUNNING\n", 15);
    pause(500); 

    pthread_exit();
}

int __attribute__ ((__section__(".text.main")))
  main(void)
{
    /* Next line, tries to move value 0 to CR3 register. This register is a privileged one, and so it will raise an exception */
     /* __asm__ __volatile__ ("mov %0, %%cr3"::"r" (0) ); */

  for (int i = 0; i < 128; ++i) keyboard[i] = 0;

  unsigned short* screen = (unsigned short*) StartScreen();

   // test_simple_clone();
   test_multiple_threads();
   // test_priority();


    int pid = fork();
    if (pid < 0) perror();
    else if (pid > 0)
	   draw_screen(screen);
    else {
	    // Hijo crea thread
    	int ret = clone(CLONE_THREAD, son_thread, 0, 4096);
	if (ret < 0) perror();
	while(1) {

	}
    }

}
