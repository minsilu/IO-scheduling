// File system implementation.  Five layers:
//   + Blocks: allocator for raw disk blocks.
//   + Log: crash recovery for multi-step updates.
//   + Files: inode allocator, reading, writing, metadata.
//   + Directories: inode with special contents (list of other inodes!)
//   + Names: paths like /usr/rtm/xv6/fs.c for convenient naming.
//
// This file contains the low-level file system manipulation
// routines.  The (higher-level) system call implementations
// are in sysfile.c.

#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "stat.h"
#include "spinlock.h"
#include "proc.h"
#include "sleeplock.h"
#include "fs.h"
#include "buf.h"
#include "file.h"

#define min(a, b) ((a) < (b) ? (a) : (b))
// there should be one superblock per disk device, but we run with
// only one device
struct superblock sb;

// Read the super block.
static void
readsb(int dev, struct superblock *sb)
{
  struct buf *bp;

  bp = bread(dev, 1);
  memmove(sb, bp->data, sizeof(*sb));
  brelse(bp);
}

// Init fs
void fsinit(int dev)
{
  readsb(dev, &sb);
  if (sb.magic != FSMAGIC)
    panic("invalid file system");
  initlog(dev, &sb);
}

// Zero a block.
static void
bzero(int dev, int bno)
{
  struct buf *bp;

  bp = bread(dev, bno);
  memset(bp->data, 0, BSIZE);
  log_write(bp);
  brelse(bp);
}

// Blocks.

// Allocate a zeroed disk block.
//分配一块新的磁盘块
static uint
balloc(uint dev)
{
  int b, bi, m;
  struct buf *bp;

  bp = 0;
  for (b = 0; b < sb.size; b += BPB)      
  {                                 //BPB=8192,表示一个块的bit数
    bp = bread(dev, BBLOCK(b, sb)); //BBLOCK计算b块应该在bitmap的哪个块上
    for (bi = 0; bi < BPB && b + bi < sb.size; bi++)
    {                    //内层循环检查bitmapblock里的每一个位
      m = 1 << (bi % 8); //bi表示8192个位里的哪一个，m表示在一个字节内部8位的哪一位上
      if ((bp->data[bi / 8] & m) == 0)
      {                        // Is block free?如果该块空闲
        bp->data[bi / 8] |= m; // Mark block in use.将该磁盘设置为已分配
        log_write(bp);
        brelse(bp);
        bzero(dev, b + bi); //清零新分配的磁盘块
        return b + bi;
      }
    }
    brelse(bp);
  }
  panic("balloc: out of blocks");
}

// Free a disk block.
//空出一块磁盘块
static void
bfree(int dev, uint b)
{
  struct buf *bp;
  int bi, m;

  bp = bread(dev, BBLOCK(b, sb));
  bi = b % BPB;                    //块中位置
  m = 1 << (bi % 8);               //字节中位置
  if ((bp->data[bi / 8] & m) == 0) //如果该位置本来就是空闲块，标记为0
    panic("freeing free block");   //报错
  bp->data[bi / 8] &= ~m;          //将该位置标记为0
  log_write(bp);
  brelse(bp);
}

// Inodes.
//
// An inode describes a single unnamed file.
// The inode disk structure holds metadata: the file's type,
// its size, the number of links referring to it, and the
// list of blocks holding the file's content.
//
// The inodes are laid out sequentially on disk at
// sb.startinode. Each inode has a number, indicating its
// position on the disk.
//
// The kernel keeps a table of in-use inodes in memory
// to provide a place for synchronizing access
// to inodes used by multiple processes. The in-memory
// inodes include book-keeping information that is
// not stored on disk: ip->ref and ip->valid.
//
// An inode and its in-memory representation go through a
// sequence of states before they can be used by the
// rest of the file system code.
//
// * Allocation: an inode is allocated if its type (on disk)
//   is non-zero. ialloc() allocates, and iput() frees if
//   the reference and link counts have fallen to zero.
//
// * Referencing in table: an entry in the inode table
//   is free if ip->ref is zero. Otherwise ip->ref tracks
//   the number of in-memory pointers to the entry (open
//   files and current directories). iget() finds or
//   creates a table entry and increments its ref; iput()
//   decrements ref.
//
// * Valid: the information (type, size, &c) in an inode
//   table entry is only correct when ip->valid is 1.
//   ilock() reads the inode from
//   the disk and sets ip->valid, while iput() clears
//   ip->valid if ip->ref has fallen to zero.
//
// * Locked: file system code may only examine and modify
//   the information in an inode and its content if it
//   has first locked the inode.
//
// Thus a typical sequence is:
//   ip = iget(dev, inum)
//   ilock(ip)
//   ... examine and modify ip->xxx ...
//   iunlock(ip)
//   iput(ip)
//
// ilock() is separate from iget() so that system calls can
// get a long-term reference to an inode (as for an open file)
// and only lock it for short periods (e.g., in read()).
// The separation also helps avoid deadlock and races during
// pathname lookup. iget() increments ip->ref so that the inode
// stays in the table and pointers to it remain valid.
//
// Many internal file system functions expect the caller to
// have locked the inodes involved; this lets callers create
// multi-step atomic operations.
//
// The itable.lock spin-lock protects the allocation of itable
// entries. Since ip->ref indicates whether an entry is free,
// and ip->dev and ip->inum indicate which i-node an entry
// holds, one must hold itable.lock while using any of those fields.
//
// An ip->lock sleep-lock protects all ip-> fields other than ref,
// dev, and inum.  One must hold ip->lock in order to
// read or write that inode's ip->valid, ip->size, ip->type, &c.

struct
{
  struct spinlock lock;       //保护itable缓存区
  struct inode inode[NINODE]; //最多50个INODE被缓存
} itable;

void iinit()
{
  int i = 0;

  initlock(&itable.lock, "itable");
  for (i = 0; i < NINODE; i++)
  {
    initsleeplock(&itable.inode[i].lock, "inode");
  }
}

static struct inode *iget(uint dev, uint inum);

// Allocate an inode on device dev.
// Mark it as allocated by  giving it type type.
// Returns an unlocked but allocated and referenced inode.
struct inode *
ialloc(uint dev, short type)
{
  int inum;
  struct buf *bp;
  struct dinode *dip;

  for (inum = 1; inum < sb.ninodes; inum++)
  {
    bp = bread(dev, IBLOCK(inum, sb));            //IBLOCK(inum,sb)计算给定的inode inum在哪个块上
    dip = (struct dinode *)bp->data + inum % IPB; //IPB=16,每个块有16个inode,dip指向该dinode实际位置
    if (dip->type == 0)
    { // a free inode
      memset(dip, 0, sizeof(*dip));
      dip->type = type;
      log_write(bp); // mark it allocated on the disk
      brelse(bp);
      return iget(dev, inum);
    }
    brelse(bp);
  }
  panic("ialloc: no inodes"); //如果所有inode都不是空余的，则报错
}

// Copy a modified in-memory inode to disk.
// Must be called after every change to an ip->xxx field
// that lives on disk.
// Caller must hold ip->lock.
void iupdate(struct inode *ip)
{
  //每次更新内存中inode副本，都需要调用iupdate写入磁盘
  struct buf *bp;
  struct dinode *dip;

  bp = bread(ip->dev, IBLOCK(ip->inum, sb));        //获取磁盘所在的块
  dip = (struct dinode *)bp->data + ip->inum % IPB; //获取inode的位置
  dip->type = ip->type;
  dip->major = ip->major;
  dip->minor = ip->minor;
  dip->nlink = ip->nlink;
  dip->size = ip->size;
  memmove(dip->addrs, ip->addrs, sizeof(ip->addrs));
  log_write(bp); //写入日志层
  brelse(bp);
}

// Find the inode with number inum on device dev
// and return the in-memory copy. Does not lock
// the inode and does not read it from disk.
static struct inode *
iget(uint dev, uint inum)
{
  struct inode *ip, *empty;

  acquire(&itable.lock);

  // Is the inode already in the table?
  empty = 0;
  for (ip = &itable.inode[0]; ip < &itable.inode[NINODE]; ip++)
  { //在itable中查找该inode是否已经被缓存
    if (ip->ref > 0 && ip->dev == dev && ip->inum == inum)
    {
      ip->ref++;
      release(&itable.lock);
      return ip; //如果已被缓存，就直接返回该指针
    }
    if (empty == 0 && ip->ref == 0) // Remember empty slot.
      empty = ip;                   //找到itable中第一个空的位置
  }

  // Recycle an inode entry.
  if (empty == 0)
    panic("iget: no inodes"); //没有空的位置

  ip = empty;
  ip->dev = dev;
  ip->inum = inum;
  ip->ref = 1;   //被引用1次
  ip->valid = 0; //还没有从磁盘读取有效内容
  release(&itable.lock);

  return ip;
}

// Increment reference count for ip.
// Returns ip to enable ip = idup(ip1) idiom.
struct inode *
idup(struct inode *ip)
{
  acquire(&itable.lock);
  ip->ref++;
  release(&itable.lock);
  return ip;
}

// Lock the given inode.
// Reads the inode from disk if necessary.
//在读取或修改inode的数据之前，需要先调用ilock对inode上锁
void ilock(struct inode *ip)
{
  struct buf *bp;
  struct dinode *dip;

  if (ip == 0 || ip->ref < 1) //若没有引用或inode为空，则报错
    panic("ilock");

  acquiresleep(&ip->lock);

  if (ip->valid == 0)
  { //刚从iget得到的指针，还未从磁盘读取内容
    bp = bread(ip->dev, IBLOCK(ip->inum, sb));
    dip = (struct dinode *)bp->data + ip->inum % IPB; //找到磁盘上存放inode的位置
    ip->type = dip->type;
    ip->major = dip->major;
    ip->minor = dip->minor;
    ip->nlink = dip->nlink;
    ip->size = dip->size;
    memmove(ip->addrs, dip->addrs, sizeof(ip->addrs));
    brelse(bp);
    ip->valid = 1;
    if (ip->type == 0)
      panic("ilock: no type");
  }
  //此时还没有释放ip->lock
}

// Unlock the given inode.
//如果持有锁，则释放
void iunlock(struct inode *ip)
{
  if (ip == 0 || !holdingsleep(&ip->lock) || ip->ref < 1)
    panic("iunlock");

  releasesleep(&ip->lock);
}

// Drop a reference to an in-memory inode.
// If that was the last reference, the inode table entry can
// be recycled.
// If that was the last reference and the inode has no links
// to it, free the inode (and its content) on disk.
// All calls to iput() must be inside a transaction in
// case it has to free the inode.
//删除内存中对inode的引用
void iput(struct inode *ip)
{
  acquire(&itable.lock);

  if (ip->ref == 1 && ip->valid && ip->nlink == 0)
  {
    // inode has no links and no other references: truncate and free.
    //只有当ref=1（只有本线程指针引用）且没有目录条目引用该inode且已经从磁盘上读取数据后，才可以释放
    // ip->ref == 1 means no other process can have ip locked,
    // so this acquiresleep() won't block (or deadlock).
    acquiresleep(&ip->lock);

    release(&itable.lock);

    itrunc(ip); //释放给定inode的所有数据块，并且将inode长度截断为0
    ip->type = 0;
    iupdate(ip); //将更新后的inode写回磁盘
    ip->valid = 0;

    releasesleep(&ip->lock);

    acquire(&itable.lock);
  }

  ip->ref--; //如果不是最后一次，则减少引用次数
  release(&itable.lock);
}

// Common idiom: unlock, then put.
void iunlockput(struct inode *ip)
{
  iunlock(ip);
  iput(ip);
}

// Inode content
//
// The content (data) associated with each inode is stored
// in blocks on the disk. The first NDIRECT block numbers
// are listed in ip->addrs[].  The next NINDIRECT blocks are
// listed in block ip->addrs[NDIRECT].

// Return the disk block address of the nth block in inode ip.
// If there is no such block, bmap allocates one.
static uint
bmap(struct inode *ip, uint bn)
{
  uint addr, *a;
  struct buf *bp;
  //先查找直接块
  if (bn < NDIRECT)
  {
    if ((addr = ip->addrs[bn]) == 0)          //若该直接块还没有被分配
      ip->addrs[bn] = addr = balloc(ip->dev); //则新分配一个数据块给它
    return addr;
  }
  bn -= NDIRECT; //减去直接块

  if (bn < NINDIRECT) //若bn在间接块范围内
  {
    // Load indirect block, allocating if necessary.
    if ((addr = ip->addrs[NDIRECT]) == 0)          //如果间接块索引为空
      ip->addrs[NDIRECT] = addr = balloc(ip->dev); //先分配给间接块索引一个块
    bp = bread(ip->dev, addr);                     //读取间接块索引所在的块
    a = (uint *)bp->data;
    if ((addr = a[bn]) == 0) //如果要读取的间接块没有分配
    {
      a[bn] = addr = balloc(ip->dev); //则为该间接块分配一个块
      log_write(bp);                  //由于此时间接块索引也被更新，所以要另外调用写入磁盘
    }
    brelse(bp);
    return addr;
  }

  panic("bmap: out of range");
}

// Truncate inode (discard contents).
// Caller must hold ip->lock.
void itrunc(struct inode *ip)
{
  int i, j;
  struct buf *bp;
  uint *a;
  //释放所有直接块
  for (i = 0; i < NDIRECT; i++)
  {
    if (ip->addrs[i]) //如果该块号不为0
    {
      bfree(ip->dev, ip->addrs[i]);
      ip->addrs[i] = 0;
    }
  }
  //释放间接块
  if (ip->addrs[NDIRECT]) //如果有间接块
  {
    bp = bread(ip->dev, ip->addrs[NDIRECT]);
    a = (uint *)bp->data;
    for (j = 0; j < NINDIRECT; j++)
    {
      if (a[j])
        bfree(ip->dev, a[j]); //释放所有间接块数据
    }
    brelse(bp);
    bfree(ip->dev, ip->addrs[NDIRECT]); //释放间接块
    ip->addrs[NDIRECT] = 0;
  }

  ip->size = 0; //更改inode大小
  iupdate(ip);  //更新到磁盘上
}

// Copy stat information from inode.
// Caller must hold ip->lock.
void stati(struct inode *ip, struct stat *st)
{
  st->dev = ip->dev;
  st->ino = ip->inum;
  st->type = ip->type;
  st->nlink = ip->nlink;
  st->size = ip->size;
}

// Read data from inode.
// Caller must hold ip->lock.
// If user_dst==1, then dst is a user virtual address;
// otherwise, dst is a kernel address.
int readi(struct inode *ip, int user_dst, uint64 dst, uint off, uint n)
{
  uint tot, m;
  struct buf *bp;

  if (off > ip->size || off + n < off) //读取不合法
    return 0;
  if (off + n > ip->size) //读取合法，但字节数超过文件大小
    n = ip->size - off;   //修改字节数为文件范围内

  for (tot = 0; tot < n; tot += m, off += m, dst += m)
  {
    bp = bread(ip->dev, bmap(ip, off / BSIZE));
    m = min(n - tot, BSIZE - off % BSIZE);                                //读取非整块的数据
    if (either_copyout(user_dst, dst, bp->data + (off % BSIZE), m) == -1) //拷贝到内存
    {
      brelse(bp);
      tot = -1;
      break;
    }
    brelse(bp);
  }
  return tot;
}

// Write data to inode.
// Caller must hold ip->lock.
// If user_src==1, then src is a user virtual address;
// otherwise, src is a kernel address.
// Returns the number of bytes successfully written.
// If the return value is less than the requested n,
// there was an error of some kind.
int writei(struct inode *ip, int user_src, uint64 src, uint off, uint n)
{
  uint tot, m;
  struct buf *bp;

  if (off > ip->size || off + n < off) //不合法
    return -1;
  if (off + n > MAXFILE * BSIZE) //文件总大小不能超过规定
    return -1;

  for (tot = 0; tot < n; tot += m, off += m, src += m)
  {
    bp = bread(ip->dev, bmap(ip, off / BSIZE));
    m = min(n - tot, BSIZE - off % BSIZE);
    if (either_copyin(bp->data + (off % BSIZE), user_src, src, m) == -1) //从用户区写入内核区
    {
      brelse(bp);
      break;
    }
    log_write(bp); //将块更新写入log
    brelse(bp);
  }

  if (off > ip->size) //如果写入大于ip->size，则更新
    ip->size = off;

  // write the i-node back to disk even if the size didn't change
  // because the loop above might have called bmap() and added a new
  // block to ip->addrs[].
  iupdate(ip);

  return tot;
}

// Directories

int namecmp(const char *s, const char *t)
{
  return strncmp(s, t, DIRSIZ);
}

// Look for a directory entry in a directory.
// If found, set *poff to byte offset of entry.
struct inode *
dirlookup(struct inode *dp, char *name, uint *poff)
{
  uint off, inum;
  struct dirent de;

  if (dp->type != T_DIR) //所查找的不是目录
    panic("dirlookup not DIR");

  for (off = 0; off < dp->size; off += sizeof(de))
  { //在该inode下查找
    if (readi(dp, 0, (uint64)&de, off, sizeof(de)) != sizeof(de)) //读取失败
      panic("dirlookup read");
    if (de.inum == 0) //该目录条目空闲
      continue;
    if (namecmp(name, de.name) == 0)  //找到该条目
    {
      // entry matches path element
      if (poff)
        *poff = off;  //设置偏移量为当前偏移量
      inum = de.inum;
      return iget(dp->dev, inum); //返回该条目所指向的inode
    }
  }

  return 0;
}

// Write a new directory entry (name, inum) into the directory dp.
int dirlink(struct inode *dp, char *name, uint inum)
{
  int off;
  struct dirent de;
  struct inode *ip;

  // Check that name is not present.
  //如果该名字已经在条目当中出现
  if ((ip = dirlookup(dp, name, 0)) != 0)
  {
    iput(ip); //删除内存中的引用
    return -1;  //表示错误
  }

  // Look for an empty dirent.
  for (off = 0; off < dp->size; off += sizeof(de))
  {
    if (readi(dp, 0, (uint64)&de, off, sizeof(de)) != sizeof(de)) //读取错误
      panic("dirlink read");
    if (de.inum == 0) //找到空闲条目
      break;
  }
  //如果没找到空的，off=dp->size，inode->size会增大
  strncpy(de.name, name, DIRSIZ);
  de.inum = inum;
  if (writei(dp, 0, (uint64)&de, off, sizeof(de)) != sizeof(de))
    panic("dirlink");

  return 0;
}

// Paths

// Copy the next path element from path into name.
// Return a pointer to the element following the copied one.
// The returned path has no leading slashes,
// so the caller can check *path=='\0' to see if the name is the last one.
// If no name to remove, return 0.
//
// Examples:
//   skipelem("a/bb/c", name) = "bb/c", setting name = "a"
//   skipelem("///a//bb", name) = "bb", setting name = "a"
//   skipelem("a", name) = "", setting name = "a"
//   skipelem("", name) = skipelem("////", name) = 0
//
static char *
skipelem(char *path, char *name)
{
  char *s;
  int len;

  while (*path == '/')
    path++;
  if (*path == 0) //如果path是'\0'，返回0
    return 0;
  s = path; //记录起始位置
  while (*path != '/' && *path != 0)
    path++;
  len = path - s; //记录某一个条目的长度
  if (len >= DIRSIZ)  //如果长度比DIRSIZ大，则只移动DIRSIZ个
    memmove(name, s, DIRSIZ);
  else
  {
    memmove(name, s, len);  //正常情况下将条目赋给name，并在末尾补0
    name[len] = 0;
  }
  while (*path == '/')
    path++;
  return path;  //返回后面的路径
}

// Look up and return the inode for a path name.
// If parent != 0, return the inode for the parent and copy the final
// path element into name, which must have room for DIRSIZ bytes.
// Must be called inside a transaction since it calls iput().
static struct inode *
namex(char *path, int nameiparent, char *name)
{
  struct inode *ip, *next;

  if (*path == '/') //从根目录开始查找
    ip = iget(ROOTDEV, ROOTINO);
  else
    ip = idup(myproc()->cwd); //从当前目录开始查找

  while ((path = skipelem(path, name)) != 0)  //skipelem将下个path element拷贝到name中，返回跟在下个path element的后续路径
  {
    ilock(ip);
    if (ip->type != T_DIR)  //检查当前ip是否为目录类型
    {
      iunlockput(ip);
      return 0;
    }
    if (nameiparent && *path == '\0') //如果已经是最后一个元素且为nameparent情况
    {
      // Stop one level early.
      iunlock(ip);
      return ip;  //返回父目录
    }
    if ((next = dirlookup(ip, name, 0)) == 0) //没有找到name
    {
      iunlockput(ip);
      return 0;
    }
    iunlockput(ip);
    // 释放ip，为下一次循环做准备
    // 当查找'.'时，next和ip相同，若在释放ip的锁之前给next上锁就会发生死锁
    // 因此namex在下一个循环中获取next的锁之前，在这里要先释放ip的锁，从而避免死锁
    ip = next; //ip变为找到的name的i节点
  }
  if (nameiparent)
  //正常情况下nameiparent应该在主循环中就返回
  //如果运行到了这里，说明nameiparent失败，因此namex返回0
  {
    iput(ip);
    return 0;
  }
  return ip;
}

//返回子元素
struct inode *
namei(char *path)
{
  char name[DIRSIZ];
  return namex(path, 0, name);
}

//返回父目录
struct inode *
nameiparent(char *path, char *name)
{
  return namex(path, 1, name);
}
