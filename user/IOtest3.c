#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fcntl.h"

#define BLOCK_P_FILE 5
#define PROC_NUM  16

char test_1024[1024]={'?'};
char test_10240[10240]={'?'};

int Result_Fd;
int Main_Proc;
int WORK_TYPE;
int ticks[128];

//根据数字建立文件名
void num2fname(char* file_name,int num){
    int len=0;
    strcpy(file_name, "f_IOtest");
    len=8;
    for(int x = 100;x;x/=10){
        if(num>=x){
            file_name[len++]=num/x+'0';
            num%=x;
        }
    }
    strcpy(file_name+len, ".txt");
}


//创建一个文件
void create_file(int num){
    char file_name[200]={0};
    num2fname(file_name, num);
    int fd = open(file_name, O_CREATE|O_WRONLY);
    write(fd, test_10240, 1024*BLOCK_P_FILE);
    close(fd);
}


//将数字转换为字符串
int num2str(int num,char* s){
    int len=0;
    if(num==0)s[len++]='0';
    for(int x=1000000000;x;x/=10)
        if(num>=x){
            s[len++] = num/x+'0';
            num%=x;
        }
    return len;
}

//每个进程读自己的文件
int test_work_RDONLY(int pid){ 
    int s_tick = uptime();
    for(int i=10;i;i--){
        char file_name[200]={0};
        num2fname(file_name, ((pid+i-1)*i)%30+1);   //伪随机数
        for(int j=10;j;j--){
            int fd = open(file_name, O_RDONLY);
            char temp[1024]={0};
            for(int i=0;i<BLOCK_P_FILE*1024/8;i++)
                read(fd, temp, 8);
            close(fd);
        }
    }
    return uptime()-s_tick;
}

//只写
int test_work_WRONLY(int pid){
    int s_tick = uptime();
    char text[1024]={0};
    for (int i=0;i<1024;i++)
        text[i]=(i+pid)%26+'A';
    for(int i=2;i;i--){
        char file_name[200]={0};
        num2fname(file_name, (pid+i)%30+1);
        for(int j=10;j;j--){
            int iswrite;
            int fd = open(file_name, O_WRONLY);
            for(int k=1;k<=BLOCK_P_FILE;k++)
                iswrite=write(fd, text, 1024);
            close(fd);
            if(iswrite>=0)
                printf("true\n");
            else    
                printf("false\n");
        }
    }
    return uptime()-s_tick;
}


int test_work(int pid){
    return test_work_WRONLY(pid);
}






void multi_proc_seg_tree(int L,int R,int nowpid){
    if(L==R){
        int tmp_tick = test_work(1);
        char result_str[20]={0};
        int len = num2str(tmp_tick, result_str);
        result_str[len++]=' ';
        result_str[len]=0;
        
        write(Result_Fd, result_str, len);
        
        printf("    !!! proc%d finish working  time:%d\n", nowpid ,tmp_tick);
        exit(0);
    }
    if(L<R){
        int mid=(L+R)/2,pid,pid2;
        pid=fork();
        if(pid==0){
            multi_proc_seg_tree(L,mid,getpid());
            exit(0);
        }
        else{
            if(mid<R){
                pid2=fork();
                if(pid2==0){
                    multi_proc_seg_tree(mid+1,R,getpid());
                    exit(0);
                }
            }
        }
    }
    while(wait(0)!=-1);
    if(getpid()!=Main_Proc)
        exit(0);
}


int main(int argc, char *argv[]){
	printf(" creating files...\n");
    for(int i=1;i<=PROC_NUM * 2+4;i++){
        create_file(i);
        printf("file%d created\n",i);
    }
    printf(" files created.\n");
    
	printf(" start to test IO 1\n");
    
    Result_Fd = open("testIO_result.txt",O_CREATE|O_RDWR);
    Main_Proc = getpid();
    multi_proc_seg_tree(1,PROC_NUM,Main_Proc);
    close(Result_Fd);

    //int max_tick=0,total_ticks=0;
    //printf("         WORK TIME:");
    //for(int i=0;i<PROC_NUM;i++){
    //    printf("%dms,",ticks[i]*10);
    //    total_ticks+=ticks[i];
    //    if(max_tick<ticks[i]){
    //        max_tick=ticks[i];
    //    }
    //}
    
    

    Result_Fd = open("testIO_result.txt",O_CREATE|O_RDONLY);
    char temp[200]={0};
    int len = read(Result_Fd, temp, 200);
    temp[len]=0;
    printf("\tresult record: %s\n", temp);
    close(Result_Fd);

    int tot_ticks=0, now_tick=0, max_tick=0;
    
    for(int i=0;i<len;i++){
        if(temp[i]==' '){
            tot_ticks+=now_tick;
            if(now_tick>max_tick)
                max_tick = now_tick;
            now_tick=0;
        }
        else now_tick=now_tick*10+temp[i]-'0';
    }
    printf("\ttotal work time: %dms\n\tmax work time:%dms\n", tot_ticks*100, max_tick*100);
    for(int i=1;i<=PROC_NUM * 2+4;i++){
        char file_name[200];
        num2fname(file_name,i);
        unlink(file_name);
    }
    exit(0);
}
