#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "date.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"

uint64
sys_exit(void)
{
  int n;
  if (argint(0, &n) < 0)
    return -1;
  exit(n);
  return 0; // not reached
}

uint64
sys_getpid(void)
{
  return myproc()->pid;
}

uint64
sys_fork(void)
{
  return fork();
}

uint64
sys_wait(void)
{
  uint64 p;
  if (argaddr(0, &p) < 0)
    return -1;
  return wait(p);
}

uint64
sys_sbrk(void)
{
  int addr;
  int n;

  if (argint(0, &n) < 0)
    return -1;
  addr = myproc()->sz;
  if (growproc(n) < 0)
    return -1;
  return addr;
}

uint64
sys_sleep(void)
{
  int n;
  uint ticks0;

  if (argint(0, &n) < 0)
    return -1;
  acquire(&tickslock);
  ticks0 = ticks;
  while (ticks - ticks0 < n)
  {
    if (myproc()->killed)
    {
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

uint64
sys_kill(void)
{
  int pid;

  if (argint(0, &pid) < 0)
    return -1;
  return kill(pid);
}

// return how many clock tick interrupts have occurred
// since start.
uint64
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}

//获取系统信息sysinfo
uint64
sys_sysinfo(void)
{
  uint64 addr; //空闲内存变量的地址,用户区的虚拟地址
//若获取用户区的虚拟地址失败，则退出
  if (argaddr(0, &addr) < 0)
  {
    return -1;
  }

  struct proc *p = myproc();
  //计算进程中的空余内存
  int temp_free_mem;
  temp_free_mem = kfree_memory();
  //将空余内存数从内核拷贝回用户空间
  if (copyout(p->pagetable, addr, (char *)&temp_free_mem, sizeof(temp_free_mem)) < 0)
  {
    return -1;
  }
  return 0;
}


//IO调度切换
uint64
sys_IO_schedule(void)
{
  int new_type;

  if(argint(0, &new_type) < 0)
    return -1;
  return IO_switch(new_type);
}