#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fcntl.h"

int main(int argc, char *argv[])
{
    int begin;  //开始时间
    int end;    //结束时间
    int times2 = 0;  //pingpong操作总次数
    

    begin = uptime();   //uptime单位为0.1s
    end = uptime();
    while (end - begin < 100)    //1s内可以执行多少次pingpong操作
    {
        int pid;
        pid = fork();
        if (pid == 0)   //子进程执行pingpong
        {
            int fd1=open("iotest1", O_RDWR |  O_CREATE);
            printf("打开文件%d",fd1);
            char b[2000]={'?'};
            write(fd1,b,2000);
            close(fd1);
            printf("xxx%d\n",times2);
            exit(0);
            

        }
        else if (pid != 0)   //父进程等待子进程执行结束
        {
            wait(0);
            int fd2=open("iotest2", O_RDWR | O_CREATE);
            printf("打开文件%d",fd2);
            times2++;
            write(fd2, "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", 2000);
            char x[2000];
            read(fd2,x,2000);
            close(fd2);
            printf("fff%d\n",times2);
            end = uptime(); //记录当前时间
        }
    }
    printf("开始时间：%dms\n", begin*100);
    printf("结束时间：%dms\n", end*100);
    printf("总次数：%d\n", times2);
    unlink("iotest1");
    unlink("iotest2");
    exit(0);
}