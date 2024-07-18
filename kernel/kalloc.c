// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

void freerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run
{
  struct run *next;
};

struct
{
  struct spinlock lock;
  struct run *freelist;
} kmem[NCPU]; // 每个CPU都有一个独立的内存管理单元

void kinit()
{
  char lockname[10];
  for (int i = 0; i < NCPU; i++)
  {
    snprintf(lockname, sizeof(lockname), "kmem%d", i);
    initlock(&kmem[i].lock, lockname);
  }
  freerange(end, (void *)PHYSTOP);
}

void freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char *)PGROUNDUP((uint64)pa_start);
  for (; p + PGSIZE <= (char *)pa_end; p += PGSIZE)
    kfree(p);
}

// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void kfree(void *pa)
{
  struct run *r;

  if (((uint64)pa % PGSIZE) != 0 || (char *)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run *)pa;

  push_off(); // 关闭中断，以确保在获取 CPU ID 之前不会发生中断
  int id = cpuid();
  acquire(&kmem[id].lock); // 获取当前 CPU 的自旋锁
  r->next = kmem[id].freelist;
  kmem[id].freelist = r;
  release(&kmem[id].lock); // 释放自旋锁
  pop_off();               // 恢复中断
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;

  push_off();              // 关闭中断，确保以下操作不被打断
  int id = cpuid();        // 获取当前 CPU 的 ID
  acquire(&kmem[id].lock); // 获取当前 CPU 的自旋锁
  r = kmem[id].freelist;   // 尝试从当前 CPU 的空闲列表中获取内存块
  if (r)
    kmem[id].freelist = r->next;
  else
  {
    int new_id;
    for (new_id = 0; new_id < NCPU; ++new_id)
    {
      if (new_id == id)
        continue;
      acquire(&kmem[new_id].lock);
      r = kmem[new_id].freelist;
      if (r)
      {
        kmem[new_id].freelist = r->next;
        release(&kmem[new_id].lock);
        break;
      }
      release(&kmem[new_id].lock);
    }
  }
  release(&kmem[id].lock); // 释放当前 CPU 的自旋锁
  pop_off();               // 恢复中断

  if (r)
    memset((char *)r, 5, PGSIZE); // fill with junk
  return (void *)r;
}
