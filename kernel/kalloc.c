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

#define INDEX(pa) (((char *)pa - (char *)PGROUNDUP((uint64)end)) >> 12)
struct
{
  struct spinlock lock;
  struct run *freelist;
  // add ref
  struct spinlock ref_lock;
  uint *ref_count;
} kmem;

void kinit()
{
  initlock(&kmem.lock, "kmem");
  initlock(&kmem.ref_lock, "ref");

  // total physical pages
  uint64 physical_pages = ((PHYSTOP - (uint64)end) >> 12) + 1;
  physical_pages = ((physical_pages * sizeof(uint)) >> 12) + 1;
  kmem.ref_count = (uint *)end;
  uint64 offset = physical_pages << 12;
  freerange(end + offset, (void *)PHYSTOP);
}

void freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char *)PGROUNDUP((uint64)pa_start);
  for (; p + PGSIZE <= (char *)pa_end; p += PGSIZE)
  {
    kmem.ref_count[INDEX((void *)p)] = 1;
    kfree(p);
  }
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
  // check if the number reaches 0
  acquire(&kmem.lock);
  if (--kmem.ref_count[INDEX(pa)])
  {
    release(&kmem.lock);
    return;
  }
  release(&kmem.lock);
  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run *)pa;

  acquire(&kmem.lock);
  r->next = kmem.freelist;
  kmem.freelist = r;
  release(&kmem.lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;

  acquire(&kmem.lock);
  r = kmem.freelist;
  if (r)
  {
    kmem.freelist = r->next;
    kmem.ref_count[INDEX((void *)r)] = 1;
  }
  release(&kmem.lock);

  if (r)
    memset((char *)r, 5, PGSIZE); // fill with junk
  return (void *)r;
}
int get_kmem_ref(void *pa)
{
  return kmem.ref_count[INDEX(pa)];
}

void add_kmem_ref(void *pa)
{
  kmem.ref_count[INDEX(pa)]++;
}

void acquire_ref_lock()
{
  acquire(&kmem.ref_lock);
}

void release_ref_lock()
{
  release(&kmem.ref_lock);
}