#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "fs.h"
#include "buf.h"

// Simple logging that allows concurrent FS system calls.
//
// A log transaction contains the updates of multiple FS system
// calls. The logging system only commits when there are
// no FS system calls active. Thus there is never
// any reasoning required about whether a commit might
// write an uncommitted system call's updates to disk.
//
// A system call should call begin_op()/end_op() to mark
// its start and end. Usually begin_op() just increments
// the count of in-progress FS system calls and returns.
// But if it thinks the log is close to running out, it
// sleeps until the last outstanding end_op() commits.
//
// The log is a physical re-do log containing disk blocks.
// The on-disk log format:
//   header block, containing block #s for block A, B, C, ...
//   block A
//   block B
//   block C
//   ...
// Log appends are synchronous.

// Contents of the header block, used for both the on-disk header block
// and to keep track in memory of logged block# before commit.

//在磁盘上的数据结构
struct logheader {
  int n;  // n=0代表当前log不包含完整的磁盘操作
          // n不为0代表有log中有n块需要被写到磁盘上
  int block[LOGSIZE]; //log可以包含30个blocks
};

//在内存中的数据结构
struct log {
  struct spinlock lock;
  int start;  //日志区域开始的磁盘位置
  int size;
  int outstanding; // how many FS sys calls are executing.
                   // 和缓存块的refcnt类似，表示当前执行FS syscalls的线程数
                   // 在begin_op中会加1，在end_op中会减1
                   // 等于0时说明当前没有正在执行的FS sys calls，
                   // 如果在end_op中发现该计数为0，说明这时候可以提交log
  int committing;  // in commit(), please wait. 表示日志系统是否正在加检查点
  int dev;
  struct logheader lh;
};
struct log log;

static void recover_from_log(void);
static void commit();

void
initlog(int dev, struct superblock *sb)
{
  if (sizeof(struct logheader) >= BSIZE)
    panic("initlog: too big logheader");

  initlock(&log.lock, "log");
  log.start = sb->logstart;
  log.size = sb->nlog;
  log.dev = dev;
  recover_from_log();
}

// Copy committed blocks from log to their home location
//从日志区转移到实际位置
static void
install_trans(int recovering)
{
  int tail;

  for (tail = 0; tail < log.lh.n; tail++) {
    struct buf *lbuf = bread(log.dev, log.start+tail+1); // read log block读取日志区上的块
    struct buf *dbuf = bread(log.dev, log.lh.block[tail]); // read dst读取磁盘上的实际位置
    memmove(dbuf->data, lbuf->data, BSIZE);  // copy block to dst从块拷贝到磁盘实际位置
    bwrite(dbuf);  // write dst to disk再重新写入磁盘
    if(recovering == 0)
      bunpin(dbuf); //如果不是重新恢复log，则在这里令引用-1
    brelse(lbuf); //释放缓存lbuf
    brelse(dbuf); //释放缓存dbuf
  }
}

// Read the log header from disk into the in-memory log header
static void
read_head(void)
{
  struct buf *buf = bread(log.dev, log.start);
  struct logheader *lh = (struct logheader *) (buf->data);
  int i;
  log.lh.n = lh->n;
  for (i = 0; i < log.lh.n; i++) {
    log.lh.block[i] = lh->block[i];
  }
  brelse(buf);
}

// Write in-memory log header to disk.
// This is the true point at which the
// current transaction commits.
static void
write_head(void)
{
  struct buf *buf = bread(log.dev, log.start);
  struct logheader *hb = (struct logheader *) (buf->data);//获取logheader缓存块
  int i;
  hb->n = log.lh.n;  //更新磁盘上loghead.n
  for (i = 0; i < log.lh.n; i++) {
    hb->block[i] = log.lh.block[i]; //更新每个log块对应的结果
  }
  bwrite(buf);//将更新后的logheader写回磁盘
  //这里开始完成提交，发生crash可恢复
  brelse(buf);
}

static void
recover_from_log(void)
{
  read_head();  //读出logheader
  install_trans(1); // if committed, copy from log to disk由于缓冲区已被清理，因此这次不需要再减少缓冲块引用次数
  log.lh.n = 0; //后两步同commit，即清除旧log
  write_head(); // clear the log
}

// called at the start of each FS system call.
void
begin_op(void)
{
  acquire(&log.lock);
  while(1){
    if(log.committing){
      //等待当前提交完成
      sleep(&log, &log.lock);
    } else if(log.lh.n + (log.outstanding+1)*MAXOPBLOCKS > LOGSIZE){
      // this op might exhaust log space; wait for commit.
      //如果当前日志区域没有足够空间，先等待
      sleep(&log, &log.lock);
    } else {
      log.outstanding += 1; //当前执行FS syscalls线程数+1
      release(&log.lock);
      break;
    }
  }
}

// called at the end of each FS system call.
// commits if this was the last outstanding operation.
//文件系统调用结束，准备提交事务
void
end_op(void)
{
  int do_commit = 0;

  acquire(&log.lock);
  log.outstanding -= 1;
  if(log.committing)  //如果正在提交，报错
    panic("log.committing");
  if(log.outstanding == 0){ //如果没有系统调用，开始事务提交
    do_commit = 1;
    log.committing = 1;
  } else {
    // begin_op() may be waiting for log space,
    // and decrementing log.outstanding has decreased
    // the amount of reserved space.
    //如果还有其他调用，先不急着提交，留给最后一个系统调用一起提交所有事务
    wakeup(&log);
  }
  release(&log.lock);

  if(do_commit){
    // call commit w/o holding locks, since not allowed
    // to sleep with locks.
    commit(); //修改commit状态
    acquire(&log.lock);
    log.committing = 0;//commiting重新变为0
    wakeup(&log);//唤起一个被挂起的调用
    release(&log.lock);
  }
}

// Copy modified blocks from cache to log.
static void
write_log(void)
{
  int tail;

  for (tail = 0; tail < log.lh.n; tail++) {
    struct buf *to = bread(log.dev, log.start+tail+1); // log block从磁盘上按顺序读出日志区域磁盘块，跳过logheader
    struct buf *from = bread(log.dev, log.lh.block[tail]); // cache block从缓存区中读出更新后的缓存块
    memmove(to->data, from->data, BSIZE);//将数据改为更新后的数据
    bwrite(to);  // write the log将更新后的数据写回log区
    brelse(from);
    brelse(to);
    //两个缓存使用完毕
  }
}

static void
commit()
{
  if (log.lh.n > 0) {
    write_log();     // Write modified blocks from cache to log从缓存写入磁盘
    write_head();    // Write header to disk -- the real commit更新log头写入磁盘
    install_trans(0); // Now install writes to home locations将缓存块从log区移到存储区
    log.lh.n = 0; //重新设块数n=0
    write_head();    // Erase the transaction from the log将更新后的n=0写入磁盘，旧的日志释放
  }
}

// Caller has modified b->data and is done with the buffer.
// Record the block number and pin in the cache by increasing refcnt.
// commit()/write_log() will do the disk write.
//
// log_write() replaces bwrite(); a typical use is:
//   bp = bread(...)
//   modify bp->data[]
//   log_write(bp)
//   brelse(bp)
void
log_write(struct buf *b)
{
  int i;

  acquire(&log.lock);
  if (log.lh.n >= LOGSIZE || log.lh.n >= log.size - 1)  //检查当前log blocks大小是否超过上限
    panic("too big a transaction");
  if (log.outstanding < 1)
    panic("log_write outside of trans");

  for (i = 0; i < log.lh.n; i++) {
    if (log.lh.block[i] == b->blockno)   // log absorption如果之前已经在log中提交该块的更新
      break;  //不用再新加一个block
  }
  log.lh.block[i] = b->blockno; //若没有提交过，则在log中新加一个block
  if (i == log.lh.n) {  // Add new block to log?//如果是新加的block
    bpin(b);  //增加该块的引用次数
    log.lh.n++; //log中块数+1
  }
  release(&log.lock);
}

