#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fcntl.h"

int main(int argc, char *argv[])
{
    int begin;  //开始时间
    int end;    //结束时间
    int times = 0;  //pingpong操作总次数
    begin = uptime();   //uptime单位为0.1s
    fprintf(1, "开始时间：%d\n", begin);
    end = uptime();
    while (end - begin < 10)    //1s内可以执行多少次pingpong操作
    {
        int pid;
        pid = fork();
        if (pid == 0)   //子进程执行pingpong
        {
            exec("pingpong", argv);
        }
        else if (pid > 0)   //父进程等待子进程执行结束
        {
            pid = wait(0);
            end = uptime(); //记录当前时间
            times++;
        }
    }
    fprintf(1, "结束时间：%d\n", end);
    fprintf(1, "总次数：%d\n", times);
    exit(0);
}