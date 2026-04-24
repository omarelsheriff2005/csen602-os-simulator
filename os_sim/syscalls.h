#ifndef SYSCALLS_H
#define SYSCALLS_H

void  sysPrint(char* val);
char* sysGetInput();
void  sysWriteFile(char* filename, char* data);
char* sysReadFile(char* filename);   /* caller must free() the returned string */
char* sysMemRead(int addr);
void  sysMemWrite(int addr, char* val);

#endif
