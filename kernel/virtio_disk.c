//
// driver for qemu's virtio disk device.
// uses qemu's mmio interface to virtio.
// qemu presents a "legacy" virtio interface.
//
// qemu ... -drive file=fs.img,if=none,format=raw,id=x0 -device virtio-blk-device,drive=x0,bus=virtio-mmio-bus.0
//

#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "fs.h"
#include "buf.h"
#include "virtio.h"

// the address of virtio mmio register r //virtio mmio寄存器r的地址。
#define R(r) ((volatile uint32 *)(VIRTIO0 + (r)))

#define NULL 0
#define TREESIZE 128 
#define RED 0
#define BLACK 1 


struct req{
  struct buf* b;
  int write;
};

struct HNode{
    struct req data[128];//表示堆的数组 大小要在用户输入的元素个数上+1
    int Size;//数组里已有的元素(不包含a[0]) 
    int Capacity; //数组的数量上限
};
typedef struct HNode * MinHeap;//结构体指针

 
int     IO_type=0; //IO调度方式，默认为noop



static struct req r;
static struct buf temp;
struct spinlock queue_lock;
struct spinlock output;




extern uint ticks;

typedef struct Node {
    int rank;           //the position in the RBTree blocks
    int color;          //0 stands for red and 1 stands for black
    struct req data;
    struct Node* p[3]; //p[2] for the parent, p[0] for the left child, p[1] for the right child
    struct Section* pSection;
} Node;

typedef struct RBTree {
    Node node[TREESIZE];
    Node sen;
    Node* root;
    int pos;
    int avail[TREESIZE];
    int size; // the previous number of nodes
    int capacity;
} RBTree;

typedef struct Section {
    int rank;
    int time;
    struct req data;
    struct Section* pFront;
    struct Section* pBack;
    Node* pNode;
} Section;

typedef struct Ring {
    Section section[TREESIZE];
    Section sen;
    Section* head;
    Section* rear;
    int pos;
    int avail[TREESIZE];
    int timeLimit;
    int size;
    int capacity;
} Ring;





static struct disk {
  // the virtio driver and device mostly communicate through a set of
  // structures in RAM. pages[] allocates that memory. pages[] is a
  // global (instead of calls to kalloc()) because it must consist of
  // two contiguous pages of page-aligned physical memory.
  //virtio驱动程序和设备主要通过一组
  //RAM中的结构进行通信。pages[]分配该内存。pages[]是
  //全局的（而不是调用kalloc（）），因为它必须由
  //物理内存的两个连续页组成
  char pages[2*PGSIZE];

  // pages[] is divided into three regions (descriptors, avail, and
  // used), as explained in Section 2.6 of the virtio specification
  // for the legacy interface.
  // https://docs.oasis-open.org/virtio/virtio/v1.1/virtio-v1.1.pdf
  //pages[]被划分为三个区域（DMA描述符，希望处理的描述符，和已经使用的描述符）

  
  // the first region of pages[] is a set (not a ring) of DMA
  // descriptors, with which the driver tells the device where to read
  // and write individual disk operations. there are NUM descriptors.
  // most commands consist of a "chain" (a linked list) of a couple of
  // these descriptors.
  // points into pages[].
  //页[]的第一个区域是DMA的set（不是ring）
  //描述符，驱动程序用它告诉设备在何处读取
  //和写入单个磁盘操作。一共有NUM个描述符。
  //大多数命令都由一个“链”（链表）和几个
  //这些描述符组成。
  //指向页面[]。
  struct virtq_desc *desc;

  // next is a ring in which the driver writes descriptor numbers
  // that the driver would like the device to process.  it only
  // includes the head descriptor of each chain. the ring has
  // NUM elements.
  // points into pages[].
  //接下来是一个环，驱动程序在其中写入驱动程序希望设备处理
  //的描述符编号。它只
  //包括每个链的头描述符。这个ring有
  //NUM个元素。
  //指向页面[]。
  struct virtq_avail *avail;

  // finally a ring in which the device writes descriptor numbers that
  // the device has finished processing (just the head of each chain).
  // there are NUM used ring entries.
  // points into pages[].
  //最后是一个环，设备在其中写入
  //设备已完成处理（仅每个链条的头部）的描述符编号。
  //有NUM个已使用的环条目。
  //指向页面[]。
  struct virtq_used *used;

  // our own book-keeping.
  char free[NUM];  // is a descriptor free?用来表示是否每个描述符是否空闲
  uint16 used_idx; // we've looked this far in used[2..NUM].

  // track info about in-flight operations,
  // for use when completion interrupt arrives.
  // indexed by first descriptor index of chain.
  //关于飞行中操作的跟踪信息，供完成中断到达时使用。由链的第一个描述符索引。
  struct {
    struct buf *b;
    char status;
  } info[NUM];

  // disk command headers.
  // one-for-one with descriptors, for convenience.
  //磁盘命令头。
  //为了方便起见，一对一使用描述符。
  struct virtio_blk_req ops[NUM];
  
  struct spinlock vdisk_lock;


  //链表
  struct HNode hnode;
  MinHeap heap;


  //红黑树
  RBTree tree[2];

  Ring ring[2];

  struct req readyArea;

  
} __attribute__ ((aligned (PGSIZE))) disk;


//cfq算法
void insertMinHeap(MinHeap heap, struct req x){
    //判断是否满了 
    if (heap->Size == heap->Capacity){
        panic("queue out of capacity");
    }

    int p = ++heap->Size;
    //printf("dafda%d\n",p);
    for (; heap->data[p/2].b->blockno>x.b->blockno; p/=2) {
//这里是最小堆  所以在a[0]位置的岗哨保存了比数组中所有元素都小的元素
        heap->data[p] = heap->data[p/2];
        //printf("dafda%d\n",p);
    }

    heap->data[p] = x;
    
}

void deleteFromMinHeap(MinHeap heap) {
    //struct req top = heap->data[1];
    struct req last = heap->data[heap->Size--];
    int parent, child;
    for (parent = 1; parent * 2 <= heap->Size; parent = child) {
        child = parent * 2;
        //注意这里是存在右子节点 并且 右子节点比左子节点小    
        if (child != heap->Size && heap->data[child].b->blockno > heap->data[child + 1].b->blockno) {
            child++;
        }
        //如果比右子节点还小
        if (heap->data[child].b->blockno > last.b->blockno) {
            break;
        }
        else {//下滤
            heap->data[parent] = heap->data[child];
        }
      
    }
    heap->data[parent] = last;
    return;
}












uint64
Nowtime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}


//sstf算法
struct Queue {//创建一个队列，该队列存放缓存区送来的IO请求
	struct req data[128];
	int size;
	int capacity;
};

struct Queue sstfqnode;
struct Queue* sstfq;

int distance(struct req x,struct req y)
{
	int delta = x.b->blockno - y.b->blockno;
	if(delta>=0)
		return delta;
	else{
		delta = - delta;
		return delta;
	}
}

void SSTF(struct Queue* qp)//SSTF算法实现，头尾指针改变
{
	for(int i=1 ;i< qp->size; i++)
	{
		if (distance(qp->data[0], qp->data[i]) < distance(qp->data[0], qp->data[1])){
			struct req cur = qp->data[i];
			qp->data[i] = qp->data[1];
			qp->data[1] = cur;
    }
	}
}


void enqueue(struct Queue* qp,struct req x){
	if (qp->size == qp->capacity){
        panic("queue out of capacity");
    }
	qp->data[qp->size++] = x;
	SSTF(qp);
}


void dequeue(struct Queue* qp){
	//if ( x>size-1 || x<0)
		//panic("queue overflow");
	for(int i=0 ; i<qp->size-1; i++)//有无问题
		qp->data[i]=qp->data[i+1];
	qp->size--;
	SSTF(qp);
}






//ddl
void
Rotate(Node* node, int dir) { //when dir == 0, left rotate, otherwise right
    Node* newP = node->p[1 - dir];
    if (node == node->p[2]->p[0]) {
        node->p[2]->p[0] = newP;
    }
    else {
        node->p[2]->p[1] = newP;
    }
    newP->p[2] = node->p[2];        //rearrange the new father
    node->p[1 - dir] = newP->p[dir];
    newP->p[1 - dir]->p[2] = newP; //rearrange the grandson
    node->p[2] = newP;
    newP->p[dir] = node; //rearrange the node itself
}

void
BalanceAfterAddition(Node* node, RBTree* tree) {
    while (node->p[2]->color == RED) {
        Node* p = node->p[2], * g = node->p[2]->p[2];
        int dir = 1;
        if (p == g->p[0]) {
            dir = 0;
        }
        Node* u = g->p[1 - dir];
        if (u->color == RED) {
            p->color = BLACK;
            u->color = BLACK;
            g->color = RED;
            node = g;
        }
        else if (node == p->p[1]) {
            node = p;
            Rotate(node, dir); //when dir == 0, left rotate, otherwise right  
       }
        else {
            p->color = BLACK;
            g->color = RED;
            Rotate(g, 1 - dir);
        }
    }
    if (tree->root != NULL)
        tree->root->color = BLACK;
}

void BalanceAfterDeletion(Node* temp, RBTree* tree) {
    int dir = 0;
    for (; ; dir++) {
        if (temp->p[dir] == NULL) //reveal the direction of node deletion
            break;
    }
    if (temp->p[2] == &tree->sen || temp->p[2]->color == RED) {
        temp->color = BLACK;
        return;
    }
    Node* p = temp->p[2], * b = p->p[1 - dir], * cin = b->p[dir], * cout = b->p[1 - dir];
    if (b->color == BLACK) { //parent, brother, inner consin and outer cousin
        if (cin != NULL && cout == NULL) {
            cin->color = BLACK;
            b->color = RED;
            Rotate(b, 1 - dir);
            cout = b;
            b = cin;
            cin = NULL;
        }
        if (cout != NULL) {
            b->color = p->color;
            cout->color = BLACK;
            p->color = BLACK; 
            Rotate(p, dir);
        }
        else {
            p->color = BLACK;
            b->color = RED;
            BalanceAfterDeletion(p, tree);
        }
    }    
    else {
        b->color = BLACK;
        cin->color = RED;
        Rotate(p, dir); //four different situations after the deletion
    }
}

struct req GetFromRBTree(RBTree* tree) {
    if (tree == &disk.tree[0] && tree->size == 0) {
        return GetFromRBTree(&disk.tree[1]);  //delete from tree[1] only if tree[0] is empty
    }
    Node* temp = &tree->sen;
    while (temp->p[0] != NULL) {
        temp = temp->p[0];
    } //reach the mininum of the nodes
    return temp->data;
}

void PrimRBTree(RBTree* tree) {
    if (tree == &disk.tree[0] && tree->size == 0) {
        PrimRBTree(&disk.tree[1]);           //delete from tree[1] only if tree[0] is empty
        return;
    }
    else if (tree->size != 0) {
        tree->size--;
    }
    Node* temp = &tree->sen;
    while (temp->p[0] != NULL) {
        temp = temp->p[0];
    }
    tree->pos++;
    tree->avail[tree->pos] = temp->rank;//delete and free the node
    temp->p[2]->p[0] = NULL;
    Section* pSection = temp->pSection; //delete and free the corresponding section in the ring
    Ring* pRing = disk.ring + temp->data.write;
    pRing->pos++;
    pRing->avail[pRing->pos] = pSection->rank;        
    pRing->size--;
    pSection->pFront->pBack = pSection->pBack;
    if(pSection->pBack != NULL)
        pSection->pBack->pFront = pSection->pFront;
    if (temp->p[1] != NULL) {
        temp->p[2]->p[0] = temp->p[1];
    }
    else {
        BalanceAfterDeletion(temp, tree);
    }
}

void DeleteFromRBTree(Node* node, RBTree* tree) {
    int dir = 0;
    for (; ; dir++) 
        if (node->p[2]->p[dir] == node)
            break; //searching
    node->p[2]->p[dir] = NULL;
    tree->pos++;
    tree->avail[tree->pos] = node->rank;//deletion and freeing
    node->p[2]->p[0] = NULL;
    BalanceAfterDeletion(node->p[2], tree); //rebalance
}

Section* InsertToRing(Ring* ring,struct req x) {
    if (ring->size == ring->capacity) {
        panic("ring out of capacity");
    }
    ring->size++;
    Section* pSection= ring->section + ring->avail[ring->pos];
    ring->pos--;
    pSection->data = x;
    pSection->time = Nowtime();
    pSection->pFront = ring->rear;
    ring->rear->pBack = pSection;
    ring->rear = pSection;
    return pSection;
}

void RenewReadyArea() {
    for (int i = 0; i < 2; i++) {
        uint64 timecost = 0;
        Ring* pRing = &disk.ring[i];  //following the order of read queue, write queue, 
        if (pRing->sen.pBack != NULL) //read RB tree and write Rb tree
            if ((timecost = (Nowtime() - pRing->sen.pBack->time)) > pRing->timeLimit) {
                disk.readyArea = pRing->sen.pBack->data;
                DeleteFromRBTree(pRing->sen.pBack->pNode, disk.tree + pRing->sen.pBack->data.write);
                pRing->size--;
                pRing->pos++;
                pRing->avail[pRing->pos] = pRing->sen.pBack->rank;
                pRing->sen.pBack = pRing->sen.pBack->pBack;
                pRing->sen.pBack->pFront = &pRing->sen;
             }  //selective information
                //printf("timecost: %d\n", timecost);
    }
    disk.readyArea = GetFromRBTree(&disk.tree[0]);
}

Node* InsertToRBTree(RBTree* tree, struct req x) {
    if(tree->size == tree->capacity) {
        panic("queue out of capacity");
    }
    tree->size++;
    Node* hot = &tree->sen;
    Node* tmp = tree->root;
    int dir = 0;
    while (tmp != NULL) {
        hot = tmp;
        if (tmp->data.b->blockno > x.b->blockno) {
            dir = 0;
            tmp = tmp->p[0];
        }
        else {
            dir = 1;
            tmp = tmp->p[1];
        }
    }
    tmp = tree->node + tree->avail[tree->pos];
    tree->pos--;
    tmp->color = RED;
    tmp->data = x;
    tmp->p[2] = hot;  //initiation of the newly inserted nodes
    hot->p[dir] = tmp;
    if (hot == &tree->sen) {
        tmp->color = BLACK;    
    }
    else {
        hot->p[1] = tmp;
    }
    BalanceAfterAddition(tmp, tree);
    RenewReadyArea();
    return tmp;//return the pointer for the link between nodes and sections
}








//调度函数
void rw_queue(struct buf*b,int write){
  if(IO_type==0){
    //printf("io-0");
    virtio_disk_rw(b, write);
  }
  else if (IO_type==1){
    //printf("io-1");
    acquire(&queue_lock);
    struct req r;
    //printf("test2");
    r.b=b;
    r.write=write;
//printf("test3");
    
//printf("test4");
    insertMinHeap(disk.heap,r);
    
    //printf("队列长度%d\n",disk.heap->Size);
    release(&queue_lock);
    virtio_disk_rw(disk.heap->data[1].b, disk.heap->data[1].write);
    
    //if(disk.heap->Size>2){
    //  printf("合并情况");
    //  
    //  for(int i=1;i<=disk.heap->Size;i++){
    //    virtio_disk_rw(disk.heap->data[i].b, disk.heap->data[i].write);
    //  }
//
    //}
    //else{
    //  uint64 begin=Nowtime();
    //  uint64 end=Nowtime();
    //  while(end-begin<1){
    //    end=Nowtime();
    //  }
    //  printf("队列长度%d\n",disk.heap->Size);
    //  for(int i=1;i<=disk.heap->Size;i++){
    //    //printf("%d\n",disk.heap->data[i].b->blockno);
    //    virtio_disk_rw(disk.heap->data[i].b, disk.heap->data[i].write);
    //  }
//
    //}
    //
  }
  else if(IO_type==2){
    acquire(&queue_lock);
    struct req r;
    r.b=b;
    r.write=write;
    enqueue(sstfq,r);
    release(&queue_lock);
    virtio_disk_rw(sstfq->data[0].b, sstfq->data[0].write);
  }
  else if(IO_type==3){
    acquire(&queue_lock);
    struct req r;
    r.b=b;
    r.write=write;
    Section* pSection = InsertToRing(&disk.ring[write], r);
    Node* pNode = InsertToRBTree(&disk.tree[write], r);
    pSection->pNode = pNode;
    pNode->pSection = pSection;
    release(&queue_lock);
    virtio_disk_rw(disk.readyArea.b, disk.readyArea.write);
  }
  
 
}




//请求的初始化，操作系统启动时，main()函数会调用该函数进行初始化，初始化函数中会初始化vdisk_lock锁，设定磁盘中断控制，并检查是否存在第二个磁盘。
void
virtio_disk_init(void)
{
  uint32 status = 0;

  initlock(&disk.vdisk_lock, "virtio_disk");
  initlock(&queue_lock, "queue_lock");

  if(*R(VIRTIO_MMIO_MAGIC_VALUE) != 0x74726976 ||
     *R(VIRTIO_MMIO_VERSION) != 1 ||
     *R(VIRTIO_MMIO_DEVICE_ID) != 2 ||
     *R(VIRTIO_MMIO_VENDOR_ID) != 0x554d4551){
    panic("could not find virtio disk");
  }
  
  status |= VIRTIO_CONFIG_S_ACKNOWLEDGE;
  *R(VIRTIO_MMIO_STATUS) = status;

  status |= VIRTIO_CONFIG_S_DRIVER;
  *R(VIRTIO_MMIO_STATUS) = status;

  // negotiate features
  uint64 features = *R(VIRTIO_MMIO_DEVICE_FEATURES);
  features &= ~(1 << VIRTIO_BLK_F_RO);
  features &= ~(1 << VIRTIO_BLK_F_SCSI);
  features &= ~(1 << VIRTIO_BLK_F_CONFIG_WCE);
  features &= ~(1 << VIRTIO_BLK_F_MQ);
  features &= ~(1 << VIRTIO_F_ANY_LAYOUT);
  features &= ~(1 << VIRTIO_RING_F_EVENT_IDX);
  features &= ~(1 << VIRTIO_RING_F_INDIRECT_DESC);
  *R(VIRTIO_MMIO_DRIVER_FEATURES) = features;

  // tell device that feature negotiation is complete.
  status |= VIRTIO_CONFIG_S_FEATURES_OK;
  *R(VIRTIO_MMIO_STATUS) = status;

  // tell device we're completely ready.
  status |= VIRTIO_CONFIG_S_DRIVER_OK;
  *R(VIRTIO_MMIO_STATUS) = status;

  *R(VIRTIO_MMIO_GUEST_PAGE_SIZE) = PGSIZE;

  // initialize queue 0.
  *R(VIRTIO_MMIO_QUEUE_SEL) = 0;  //被选中队列，只写
  uint32 max = *R(VIRTIO_MMIO_QUEUE_NUM_MAX); //队列容量，只读
  if(max == 0)
    panic("virtio disk has no queue 0");
  if(max < NUM) //队列容量必须大于指示符数量
    panic("virtio disk max queue too short");
  *R(VIRTIO_MMIO_QUEUE_NUM) = NUM;  //当前队列大小
  memset(disk.pages, 0, sizeof(disk.pages));  //先将页面初始化
  *R(VIRTIO_MMIO_QUEUE_PFN) = ((uint64)disk.pages) >> PGSHIFT;

  // desc = pages -- num * virtq_desc
  // avail = pages + 0x40 -- 2 * uint16, then num * uint16
  // used = pages + 4096 -- 2 * uint16, then num * vRingUsedElem
  //PGSIZE 4096bytes，desc为8*16=1024

  disk.desc = (struct virtq_desc *) disk.pages;
  //printf("1:%x\n",disk.desc);
  disk.avail = (struct virtq_avail *)(disk.pages + NUM*sizeof(struct virtq_desc));
  //printf("2:%x\n",disk.avail);
  //printf("2:%x\n",sizeof(struct virtq_desc));
  disk.used = (struct virtq_used *) (disk.pages + PGSIZE);
  //printf("3:%x\n",disk.used);

  // all NUM descriptors start out unused.
  for(int i = 0; i < NUM; i++)
    disk.free[i] = 1;


  //最小堆
 
  disk.heap=(MinHeap) &disk.hnode;
  //printf("1213421341\n");
  disk.heap->Size = 0;
  disk.heap->Capacity=128;
  
  r.write=0;
  temp.blockno=0;
  r.b=&temp;
  disk.heap->data[0] = r;//岗哨 



  //sstf

  sstfq=&sstfqnode;
  sstfq->capacity=128;
  sstfq->size=0;



  //ddl
  int i, j;
  for (i = 0; i < 2; i++) {
      disk.tree[i].root = NULL;
      disk.tree[i].sen.p[0] = NULL;
      disk.tree[i].sen.color = BLACK;
      disk.tree[i].size = 0;
      disk.tree[i].capacity = TREESIZE;
      for (j = 0; j < TREESIZE; j++) {
          disk.tree->avail[j] = j;
      }
      disk.tree[i].pos = TREESIZE - 1;
      disk.ring[i].head = &disk.ring[i].sen;
      disk.ring[i].rear = &disk.ring[i].sen;
      disk.ring[i].timeLimit = 22;
      disk.ring[i].size = 0;
      disk.ring[i].capacity= TREESIZE;
      for (j = 0; j < TREESIZE; j++) {
          disk.ring->avail[j] = j;
      }
      disk.ring[i].pos = TREESIZE - 1;
  }
  // plic.c and trap.c arrange for interrupts from VIRTIO0_IRQ.
}

// find a free descriptor, mark it non-free, return its index.
//找到一个描述符，标记其为不空闲，返回其索引（顺着找）
static int
alloc_desc()
{
  for(int i = 0; i < NUM; i++){
    if(disk.free[i]){
      disk.free[i] = 0;
      return i;
    }
  }
  return -1;
}

// mark a descriptor as free.
//标记一个描述符为空闲
static void
free_desc(int i)
{
  if(i >= NUM)  //如果超出NUM范围，报错
    panic("free_desc 1");
  if(disk.free[i])  //如果该描述符本身为空闲，报错
    panic("free_desc 2");
  disk.desc[i].addr = 0;
  disk.desc[i].len = 0;
  disk.desc[i].flags = 0;
  disk.desc[i].next = 0;
  disk.free[i] = 1;
  wakeup(&disk.free[0]);  //唤起被挂起的调用
}

// free a chain of descriptors.
//释放一个描述符链表的空间
static void
free_chain(int i)
{
  while(1){
    int flag = disk.desc[i].flags;  
    int nxt = disk.desc[i].next;
    free_desc(i);
    if(flag & VRING_DESC_F_NEXT)  //是否valid
      i = nxt;
    else
      break;
  }
}

// allocate three descriptors (they need not be contiguous).
// disk transfers always use three descriptors.
//分配三个描述符（它们不必是连续的）。
//磁盘传输始终使用三个描述符。
static int
alloc3_desc(int *idx)
{
  for(int i = 0; i < 3; i++){
    idx[i] = alloc_desc();
    if(idx[i] < 0){ //若申请失败，意味着当前空间不够
      for(int j = 0; j < i; j++)
        free_desc(idx[j]);  //释放之前申请的几个描述符，返回申请失败
      return -1;
    }
  }
  return 0;
}



//磁盘读写
void
virtio_disk_rw(struct buf *b, int write)
{
  uint64 sector = b->blockno * (BSIZE / 512); //扇区=块号*2

  acquire(&disk.vdisk_lock);

  // the spec's Section 5.2 says that legacy block operations use
  // three descriptors: one for type/reserved/sector, one for the
  // data, one for a 1-byte status result.

  // allocate the three descriptors.
  //若能申请到三个描述符，继续下一步
  int idx[3];
  while(1){
    if(alloc3_desc(idx) == 0) {
      break;
    }
    printf("等待\n");
    sleep(&disk.free[0], &disk.vdisk_lock); //如果申请不到，进入睡眠状态，当释放描述符时唤醒
  }

  // format the three descriptors.
  // qemu's virtio-blk.c reads them.

  struct virtio_blk_req *buf0 = &disk.ops[idx[0]];

  if(write)
    buf0->type = VIRTIO_BLK_T_OUT; // write the disk
  else
    buf0->type = VIRTIO_BLK_T_IN; // read the disk
  buf0->reserved = 0;
  buf0->sector = sector;

  // 第一个描述符表示格式
  disk.desc[idx[0]].addr = (uint64) buf0; //地址为磁盘请求
  disk.desc[idx[0]].len = sizeof(struct virtio_blk_req);  //表示磁盘请求的长度
  disk.desc[idx[0]].flags = VRING_DESC_F_NEXT;  //连接下一个描述符
  disk.desc[idx[0]].next = idx[1];  //下一个描述符为idx[1]

  //第二个描述符用来表示块
  disk.desc[idx[1]].addr = (uint64) b->data;  //地址为块
  disk.desc[idx[1]].len = BSIZE;  //长度为块大小
  if(write)
    disk.desc[idx[1]].flags = 0; // device reads b->data
  else
    disk.desc[idx[1]].flags = VRING_DESC_F_WRITE; // device writes b->data
  disk.desc[idx[1]].flags |= VRING_DESC_F_NEXT; //读为01，写为11
  disk.desc[idx[1]].next = idx[2];

  //第三个描述符为1个单字节状态
  disk.info[idx[0]].status = 0xff; // device writes 0 on success 设备成功写入0
  disk.desc[idx[2]].addr = (uint64) &disk.info[idx[0]].status; //设备写入状态的地址
  disk.desc[idx[2]].len = 1;
  disk.desc[idx[2]].flags = VRING_DESC_F_WRITE; // device writes the status
  disk.desc[idx[2]].next = 0;

  // record struct buf for virtio_disk_intr().
  b->disk = 1;  //缓存区内容已经提交给磁盘为0，未完成为1
  disk.info[idx[0]].b = b;

  // tell the device the first index in our chain of descriptors.
  //告诉磁盘队列中等待的请求的第一个描述符
  disk.avail->ring[disk.avail->idx % NUM] = idx[0];
  //printf("1\n");  
  __sync_synchronize();

  // tell the device another avail ring entry is available.
  //告诉设备另一个可用环条目可用。
  disk.avail->idx += 1; // not % NUM ...
  //printf("2\n");  
  __sync_synchronize();

  *R(VIRTIO_MMIO_QUEUE_NOTIFY) = 0; // value is queue number

  // Wait for virtio_disk_intr() to say request has finished.
  while(b->disk == 1) { //当还没有读取或写入完成时
    sleep(b, &disk.vdisk_lock);
  }
  //printf("块号%d\n",disk.info[idx[0]].b->blockno);  
  disk.info[idx[0]].b = 0;  //跟踪的块信息清零
  free_chain(idx[0]); //释放这3个描述符

  release(&disk.vdisk_lock);  //释放磁盘锁
}

void
virtio_disk_intr()
{
  acquire(&disk.vdisk_lock);

  // the device won't raise another interrupt until we tell it
  // we've seen this interrupt, which the following line does.
  // this may race with the device writing new entries to
  // the "used" ring, in which case we may process the new
  // completion entries in this interrupt, and have nothing to do
  // in the next interrupt, which is harmless.
  //在我们告诉它之前，该设备不会引发另一个中断
  //我们已经看到了这个中断，下一行就是这样做的。
  //这可能与设备写入新条目的情况发生冲突
  //“used”环，在这种情况下，我们可以处理新的
  //完成此中断中的条目，与
  //下一个中断中的条目无关，这是无害的。
  *R(VIRTIO_MMIO_INTERRUPT_ACK) = *R(VIRTIO_MMIO_INTERRUPT_STATUS) & 0x3;

  __sync_synchronize();

  // the device increments disk.used->idx when it
  // adds an entry to the used ring.
  //当将条目添加到已使用的环中时,该设备增加了disk.used->idx
  while(disk.used_idx != disk.used->idx){
    __sync_synchronize();
    int id = disk.used->ring[disk.used_idx % NUM].id;

    if(disk.info[id].status != 0)
      panic("virtio_disk_intr status");

    struct buf *b = disk.info[id].b;
    //printf("首地址字符：%d\n",b->data[0]);
    b->disk = 0;   // disk is done with buf
    //printf("4\n");  
    wakeup(b);
    //printf("5\n");    
    disk.used_idx += 1;
    //printf("已使用idx数目%d\n",disk.used_idx);  //进行测试
  }




  if(IO_type==1){
    acquire(&queue_lock);
    deleteFromMinHeap(disk.heap);
    release(&queue_lock);
  }
  if(IO_type==2){
    acquire(&queue_lock);
    dequeue(sstfq);
    release(&queue_lock);
  }
  if(IO_type==3){
    acquire(&queue_lock);
    PrimRBTree(&disk.tree[0]); //delete from tree[0], or tree[1] when tree[0] is empty
    release(&queue_lock);
  }
 
  


  release(&disk.vdisk_lock);
}
