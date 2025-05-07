/*
 * libc.h - macros per fer els traps amb diferents arguments
 *          definició de les crides a sistema
 */
 
#ifndef __LIBC_H__
#define __LIBC_H__

#include <stats.h>

int write(int fd, char *buffer, int size);
int fast_write(int fd, char *buffer, int size);

int gettime();
int fast_gettime();

void itoa(int a, char *b);

void perror();

int strlen(char *a);

int getpid();

int fork();

void exit();

void block();

int unblock(int pid);
#endif  /* __LIBC_H__ */
