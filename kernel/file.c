//
// Support functions for system calls that involve file descriptors.
//

#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "fs.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "file.h"
#include "stat.h"
#include "proc.h"

struct devsw devsw[NDEV];
struct {
  struct spinlock lock;
  struct file file[NFILE];
} ftable;

void
fileinit(void)
{
  initlock(&ftable.lock, "ftable");
}

// Allocate a file structure.
struct file*
filealloc(void)
{
  struct file *f;

  acquire(&ftable.lock);
  for(f = ftable.file; f < ftable.file + NFILE; f++){
    if(f->ref == 0){
      f->ref = 1; //表示文件已经打开
      release(&ftable.lock);
      return f;
    }
  }
  release(&ftable.lock);
  return 0; //若没有空位直接返回0
}

// Increment ref count for file f.
struct file*
filedup(struct file *f)
{
  acquire(&ftable.lock);
  if(f->ref < 1)
    panic("filedup");
  f->ref++; //引用次数+1
  release(&ftable.lock);
  return f;
}

// Close file f.  (Decrement ref count, close when reaches 0.)
void
fileclose(struct file *f)
{
  struct file ff;

  acquire(&ftable.lock);
  if(f->ref < 1)
    panic("fileclose");
  if(--f->ref > 0){ //若引用计数-1大于0
    release(&ftable.lock);
    return; //直接返回
  }
  ff = *f;  //以方便后面获取其内部项
  f->ref = 0; 
  f->type = FD_NONE;
  release(&ftable.lock);

  if(ff.type == FD_PIPE){ //如果是pipe类型
    pipeclose(ff.pipe, ff.writable);  //关闭pipe
  } else if(ff.type == FD_INODE || ff.type == FD_DEVICE){ //如果是设备或inode
    begin_op(); 
    iput(ff.ip);  //减少内存中对ip的引用
    end_op();
  }
}

// Get metadata about file f.
// addr is a user virtual address, pointing to a struct stat.
int
filestat(struct file *f, uint64 addr)
{
  struct proc *p = myproc();
  struct stat st;
  
  if(f->type == FD_INODE || f->type == FD_DEVICE){  //只能读取inode的信息
    ilock(f->ip);
    stati(f->ip, &st);
    iunlock(f->ip);
    if(copyout(p->pagetable, addr, (char *)&st, sizeof(st)) < 0)  //从内核区拷贝到用户区
      return -1; 
    return 0;
  }
  return -1;
}

// Read from file f.
// addr is a user virtual address.
int
fileread(struct file *f, uint64 addr, int n)
{
  int r = 0;

  if(f->readable == 0)  //如果不可读则退出
    return -1;

  if(f->type == FD_PIPE){ //对于pipe，按照pipe方式读取
    r = piperead(f->pipe, addr, n); 
  } else if(f->type == FD_DEVICE){  
    if(f->major < 0 || f->major >= NDEV || !devsw[f->major].read)
      return -1;
    r = devsw[f->major].read(1, addr, n);
  } else if(f->type == FD_INODE){ //对于inode
    ilock(f->ip);
    if((r = readi(f->ip, 1, addr, f->off, n)) > 0)  //读取数据
      f->off += r;  //更新f中的偏移量
    iunlock(f->ip);
  } else {
    panic("fileread");
  }

  return r;
}

// Write to file f.
// addr is a user virtual address.
int
filewrite(struct file *f, uint64 addr, int n)
{
  int r, ret = 0;

  if(f->writable == 0)  //检查是否可写
    return -1;

  if(f->type == FD_PIPE){ //管道
    ret = pipewrite(f->pipe, addr, n);
  } else if(f->type == FD_DEVICE){  //设备
    if(f->major < 0 || f->major >= NDEV || !devsw[f->major].write)
      return -1;
    ret = devsw[f->major].write(1, addr, n);
  } else if(f->type == FD_INODE){
    // write a few blocks at a time to avoid exceeding
    // the maximum log transaction size, including
    // i-node, indirect block, allocation blocks,
    // and 2 blocks of slop for non-aligned writes.
    // this really belongs lower down, since writei()
    // might be writing a device like the console.
    int max = ((MAXOPBLOCKS-1-1-2) / 2) * BSIZE;  //最多写入限制
    int i = 0;
    while(i < n){
      int n1 = n - i;
      if(n1 > max)
        n1 = max;

      begin_op(); //准备提交事务
      ilock(f->ip);  //更新inode
      if ((r = writei(f->ip, 1, addr + i, f->off, n1)) > 0)
        f->off += r;
      iunlock(f->ip);
      end_op(); //提交事务

      if(r != n1){
        // error from writei
        break;
      }
      i += r;
    }
    ret = (i == n ? n : -1);  //写入大小
  } else {
    panic("filewrite");
  }

  return ret;
}

