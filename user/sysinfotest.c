#include "kernel/types.h" 
#include "kernel/stat.h" 
#include "user/user.h"
#include "kernel/fcntl.h"


int
main(int argc, char *argv[])
{
   int free_mem;
   sysinfo(&free_mem);
   printf("空闲内存：%d bytes\n",free_mem);
   exit(0);
}