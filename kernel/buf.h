struct buf {
  int valid;   // has data been read from disk? 若缓存区包含块的副本为1，否则为0
  int disk;    // does disk "own" buf? 缓存区内容已经提交给磁盘为0，未完成为1
  uint dev;
  uint blockno; //块号
  struct sleeplock lock;  //缓存块睡眠锁保护对该块内容的读与写
  uint refcnt;  //当前有多少个内核线程在排队等待读缓存块
  struct buf *prev; // LRU cache list
  struct buf *next;
  uchar data[BSIZE];//BSIZE为1024，表示块大小
};

