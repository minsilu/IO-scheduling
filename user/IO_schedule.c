#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fcntl.h"

int main(int argc, char *argv[]){
	if(argc!=2)
		printf("Usage: IO_schedule\n need a parameter.\n");
	else{
		char* input=argv[1];
		char* noop="noop";
		char* sstf="sstf";
		char* ddl="ddl";
		char* cfq="cfq";
		char* cscan="cscan";
		if (strcmp(input, noop) == 0){
			IO_schedule(0);
			printf("IO scheduling algorithm switch to NOOP.\n");
		}else if(strcmp(input, cfq) == 0){
			IO_schedule(1);
			printf("IO scheduling algorithm switch to CFQ.\n");
		}else if(strcmp(input, sstf) == 0){
			IO_schedule(2);
			printf("IO scheduling algorithm switch to SSTF(Shortest Seek Time First).\n");
		}else if(strcmp(input, ddl) == 0){
			IO_schedule(3);
			printf("IO scheduling algorithm switch to Deadline.\n");
		}else if(strcmp(input, cscan) == 0){
			IO_schedule(4);
			printf("IO scheduling algorithm switch to Cycle-SCAN.\n");
		}else{
			printf("Usage: elevator. invalid parameter.\n");
		}
		
	}
	exit(0);
}
