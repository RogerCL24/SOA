/*
 * io.c - 
 */

#include <io.h>

#include <types.h>

/**************/
/** Screen  ***/
/**************/

#define NUM_COLUMNS 80
#define NUM_ROWS    25

Byte x, y=19;

/* Read a byte from 'port' */
Byte inb (unsigned short port)
{
  Byte v;

  __asm__ __volatile__ ("inb %w1,%0":"=a" (v):"Nd" (port));
  return v;
}

void scrollScreen() {
	Word *screen = (Word *)0xb8000;
	
	for (int i = 0; i < NUM_ROWS-1; i++) {
		for (int j = 0; j < NUM_COLUMNS; j++) {
			screen[i * NUM_COLUMNS + j] = screen[(i+1) * NUM_COLUMNS + j];
		}
	}
	
	for (int j = 0; j < NUM_COLUMNS; j++) {
        	screen[(NUM_ROWS - 1) * NUM_COLUMNS + j] = 0x0720; //Deixa espai buit
    	}
    	
    	y = NUM_ROWS-1;
}

void printc(char c)
{
     __asm__ __volatile__ ( "movb %0, %%al; outb $0xe9" ::"a"(c)); /* Magic BOCHS debug: writes 'c' to port 0xe9 */
  if (c=='\n')
  {
    x = 0;
    y++;
  }
  else
  {
    Word ch = (Word) (c & 0x00FF) | 0x0200;
	Word *screen = (Word *)0xb8000;
	screen[(y * NUM_COLUMNS + x)] = ch;
    if (++x >= NUM_COLUMNS)
    {
      x = 0;
      y++;
    }
  }
  
  if (y >= NUM_ROWS) {
  	scrollScreen();
  }
}

void printc_color(char c, Byte color)
{
  __asm__ __volatile__ ( "movb %0, %%al; outb $0xe9" ::"a"(c)); /* Magic BOCHS debug: writes 'c' to port 0xe9 */
  if (c=='\n')
  {
    x = 0;
    y++;
  }
  else
  {
    Word ch = (Word) (c & 0x00FF) | ((Word)color << 8);
	Word *screen = (Word *)0xb8000;
	screen[(y * NUM_COLUMNS + x)] = ch;
    if (++x >= NUM_COLUMNS)
    {
      x = 0;
      y++;
    }
  }
  
  if (y >= NUM_ROWS) {
  	scrollScreen();
  }
}

void printc_xy(Byte mx, Byte my, char c)
{
  Byte cx, cy;
  cx=x;
  cy=y;
  x=mx;
  y=my;
  printc(c);
  x=cx;
  y=cy;
}

void printk(char *string)
{
  int i;
  for (i = 0; string[i]; i++)
    printc(string[i]);
}

void printk_color(char *string, Byte color) {
	int i;
	for(i = 0; string[i]; i++)
		printc_color(string[i], color);
}
