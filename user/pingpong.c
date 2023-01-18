#include "kernel/types.h" 
#include "kernel/stat.h" 
#include "user/user.h"
#include "kernel/fcntl.h"


int
main(int argc, char *argv[])
{
    if(argc>1){     //如果参数个数大于1，报错
        fprintf(2,"Error:pingpong\n");
        exit(1);
    }
    int p1[2],p2[2];    //建立两个管道
    pipe(p1);   //父进程写，子进程读
    pipe(p2);   //子进程写，父进程读

    char buf[1];    //一个字节内容传输
   
  
   if(fork()==0){   //子进程应该先读
       close(p1[1]);
       close(p2[0]);
       if(read(p1[0],buf,1)==1){    //子进程从p1管道读取
           close(p1[0]);
           fprintf(1,"%d:received ping\n",getpid());
       }
       write(p2[1],buf,1);  //子进程写入p2管道
       close(p2[1]);
   }
   else{    //父进程应该先写
       close(p1[0]);
       close(p2[1]);
       write(p1[1],buf,1);  //父进程写入p1管道
       close(p1[1]);
       if(read(p2[0],buf,1)==1){    //父进程从p2管道读取
           close(p2[0]);
           fprintf(1,"%d:received pong\n",getpid());
       }
   }
   exit(0);
}