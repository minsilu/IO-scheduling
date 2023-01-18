// Buffer cache.
//
// The buffer cache is a linked list of buf structures holding
// cached copies of disk block contents.  Caching disk blocks
// in memory reduces the number of disk reads and also provides
// a synchronization point for disk blocks used by multiple processes.
//
// Interface:
// * To get a buffer for a particular disk block, call bread.
// * After changing buffer data, call bwrite to write it to disk.
// * When done with the buffer, call brelse.
// * Do not use the buffer after calling brelse.
// * Only one process at a time can use a buffer,
//     so do not keep them longer than necessary.


#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "riscv.h"
#include "defs.h"
#include "fs.h"
#include "buf.h"

struct {
  struct spinlock lock; //该自旋锁保护哪些块已经被缓存的信息
  struct buf buf[NBUF]; //NBUF=30

  // Linked list of all buffers, through prev/next.
  // Sorted by how recently the buffer was used.
  // head.next is most recent, head.prev is least.
  struct buf head;
} bcache;

//构件缓存区双向链表
void
binit(void)
{
  struct buf *b;

  initlock(&bcache.lock, "bcache"); 

  // Create linked list of buffers
  //链表头
  bcache.head.prev = &bcache.head;
  bcache.head.next = &bcache.head;
  //构建缓存区链表，不断往head节点后插入b
  for(b = bcache.buf; b < bcache.buf+NBUF; b++){
    b->next = bcache.head.next;
    b->prev = &bcache.head;
    initsleeplock(&b->lock, "buffer");
    bcache.head.next->prev = b;
    bcache.head.next = b;
  }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;
  //多线程访问时，在外面等待
  acquire(&bcache.lock);

  // Is the block already cached?
  //head.next指向最近使用最多的缓存块
  for(b = bcache.head.next; b != &bcache.head; b = b->next){
    if(b->dev == dev && b->blockno == blockno){ //如果命中所查块
      b->refcnt++;  //引用计数—+1
      release(&bcache.lock);  //不能同时持有bcache和b的锁，否则会造成死锁
      acquiresleep(&b->lock); //获得缓存块b的锁
      return b;
    }
  }

  // Not cached.
  // Recycle the least recently used (LRU) unused buffer.
  //head.prev指向最近最少使用的缓存块
  for(b = bcache.head.prev; b != &bcache.head; b = b->prev){  //若未找到该块
    if(b->refcnt == 0) {  //如果没有线程要读这个块，就回收它，然后将新的内容赋给它
      b->dev = dev;
      b->blockno = blockno;
      b->valid = 0;
      b->refcnt = 1;  //本线程要使用它，因此置为1
      release(&bcache.lock);
      acquiresleep(&b->lock); //返回上锁的缓存块
      return b;
    }
  }
  panic("bget: no buffers");
}

// Return a locked buf with the contents of the indicated block.
struct buf*
bread(uint dev, uint blockno)
{
  struct buf *b;

  b = bget(dev, blockno); //通过bget调用指定磁盘块的缓存块
  //若缓存区不包含块的副本，则从磁盘读取到缓存区上
  if(!b->valid) {
    //virtio_disk_rw(b, 0); //0表示读取磁盘
    rw_queue(b,0);
    b->valid = 1; //缓存区已经包含块的副本
  }
  return b;
}

// Write b's contents to disk.  Must be locked.
void
bwrite(struct buf *b)
{
  if(!holdingsleep(&b->lock)) //要保证持有该缓存块的睡眠锁
    panic("bwrite");
  //virtio_disk_rw(b, 1); //1表示写入磁盘块
  rw_queue(b,1);
}

// Release a locked buffer.
// Move to the head of the most-recently-used list.
//当线程使用完了一块缓存块，就用brelse释放它
void
brelse(struct buf *b)
{
  if(!holdingsleep(&b->lock)) //首先需保证持有该缓存块的睡眠锁
    panic("brelse");

  releasesleep(&b->lock); //释放该睡眠锁

  acquire(&bcache.lock);  //获取缓存区
  b->refcnt--; //减少引用次数
  if (b->refcnt == 0) { //若无线程等待读取，则更新该缓存
    // no one is waiting for it.
    b->next->prev = b->prev;  
    b->prev->next = b->next;
    b->next = bcache.head.next; //令其成为最近使用的缓存
    b->prev = &bcache.head;
    bcache.head.next->prev = b;
    bcache.head.next = b;
  }
  
  release(&bcache.lock);
}

//增加缓存块引用计数
void
bpin(struct buf *b) {
  acquire(&bcache.lock);
  b->refcnt++;
  release(&bcache.lock);
}

//减少缓存块引用计数
void
bunpin(struct buf *b) {
  acquire(&bcache.lock);
  b->refcnt--;
  release(&bcache.lock);
}


