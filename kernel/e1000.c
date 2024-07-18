#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"
#include "e1000_dev.h"
#include "net.h"

#define TX_RING_SIZE 16
static struct tx_desc tx_ring[TX_RING_SIZE] __attribute__((aligned(16)));
static struct mbuf *tx_mbufs[TX_RING_SIZE];

#define RX_RING_SIZE 16
static struct rx_desc rx_ring[RX_RING_SIZE] __attribute__((aligned(16)));
static struct mbuf *rx_mbufs[RX_RING_SIZE];

// remember where the e1000's registers live.
static volatile uint32 *regs;

struct spinlock e1000_lock;

// called by pci_init().
// xregs is the memory address at which the
// e1000's registers are mapped.
void e1000_init(uint32 *xregs)
{
  int i;

  initlock(&e1000_lock, "e1000");

  regs = xregs;

  // Reset the device
  regs[E1000_IMS] = 0; // disable interrupts
  regs[E1000_CTL] |= E1000_CTL_RST;
  regs[E1000_IMS] = 0; // redisable interrupts
  __sync_synchronize();

  // [E1000 14.5] Transmit initialization
  memset(tx_ring, 0, sizeof(tx_ring));
  for (i = 0; i < TX_RING_SIZE; i++)
  {
    tx_ring[i].status = E1000_TXD_STAT_DD;
    tx_mbufs[i] = 0;
  }
  regs[E1000_TDBAL] = (uint64)tx_ring;
  if (sizeof(tx_ring) % 128 != 0)
    panic("e1000");
  regs[E1000_TDLEN] = sizeof(tx_ring);
  regs[E1000_TDH] = regs[E1000_TDT] = 0;

  // [E1000 14.4] Receive initialization
  memset(rx_ring, 0, sizeof(rx_ring));
  for (i = 0; i < RX_RING_SIZE; i++)
  {
    rx_mbufs[i] = mbufalloc(0);
    if (!rx_mbufs[i])
      panic("e1000");
    rx_ring[i].addr = (uint64)rx_mbufs[i]->head;
  }
  regs[E1000_RDBAL] = (uint64)rx_ring;
  if (sizeof(rx_ring) % 128 != 0)
    panic("e1000");
  regs[E1000_RDH] = 0;
  regs[E1000_RDT] = RX_RING_SIZE - 1;
  regs[E1000_RDLEN] = sizeof(rx_ring);

  // filter by qemu's MAC address, 52:54:00:12:34:56
  regs[E1000_RA] = 0x12005452;
  regs[E1000_RA + 1] = 0x5634 | (1 << 31);
  // multicast table
  for (int i = 0; i < 4096 / 32; i++)
    regs[E1000_MTA + i] = 0;

  // transmitter control bits.
  regs[E1000_TCTL] = E1000_TCTL_EN |                 // enable
                     E1000_TCTL_PSP |                // pad short packets
                     (0x10 << E1000_TCTL_CT_SHIFT) | // collision stuff
                     (0x40 << E1000_TCTL_COLD_SHIFT);
  regs[E1000_TIPG] = 10 | (8 << 10) | (6 << 20); // inter-pkt gap

  // receiver control bits.
  regs[E1000_RCTL] = E1000_RCTL_EN |      // enable receiver
                     E1000_RCTL_BAM |     // enable broadcast
                     E1000_RCTL_SZ_2048 | // 2048-byte rx buffers
                     E1000_RCTL_SECRC;    // strip CRC

  // ask e1000 for receive interrupts.
  regs[E1000_RDTR] = 0;       // interrupt after every received packet (no timer)
  regs[E1000_RADV] = 0;       // interrupt after every packet (no timer)
  regs[E1000_IMS] = (1 << 7); // RXDW -- Receiver Descriptor Write Back
}

int e1000_transmit(struct mbuf *m)
{
  //
  // Your code here.
  //
  // the mbuf contains an ethernet frame; program it into
  // the TX descriptor ring so that the e1000 sends it. Stash
  // a pointer so that it can be freed after sending.
  //
  acquire(&e1000_lock);
  // 获取 e1000_lock 锁，这个锁用于同步对 e1000 网络设备的访问，以防止并发访问带来的问题。

  uint32 next_index = regs[E1000_TDT];
  // 从寄存器中读取当前可用的 TX（发送）描述符环的下一个索引。

  if ((tx_ring[next_index].status & E1000_TXD_STAT_DD) == 0)
  {
    // 检查当前下一个描述符的状态是否为 "E1000_TXD_STAT_DD"（表示描述符是否可用）。
    // 如果不可用，则说明发送队列已满，无法继续发送新的数据包，所以需要释放锁并返回-1，表示发送失败。
    release(&e1000_lock);
    return -1;
  }

  if (tx_mbufs[next_index])
    mbuffree(tx_mbufs[next_index]);
  // 检查下一个描述符的 mbuf 指针是否非空，如果非空，则释放之前可能存储在该描述符中的 mbuf。

  tx_ring[next_index].addr = (uint64)m->head;
  tx_ring[next_index].length = (uint16)m->len;
  // 将 mbuf 中的数据包内容的头部地址和长度存储到下一个描述符中，以便将数据包发送。

  tx_ring[next_index].cmd = E1000_TXD_CMD_EOP | E1000_TXD_CMD_RS;
  // 设置下一个描述符的命令字段。其中 EOP 表示该描述符为一个完整数据包的结束描述符，
  // RS 表示发送时将报告状态（Report Status），即在数据包发送完成后触发中断。

  tx_mbufs[next_index] = m;
  // 将当前 mbuf 存储到下一个描述符对应的缓冲区中，以便在发送完成后能够释放 mbuf。

  regs[E1000_TDT] = (next_index + 1) % TX_RING_SIZE;
  // 更新寄存器中的 TDT（Transmit Descriptor Tail）指针，使其指向下一个可用的描述符。
  // 这样做后，该描述符就准备好发送数据包了。

  release(&e1000_lock);
  // 释放 e1000_lock 锁，允许其他线程再次访问 e1000 网络设备。

  return 0;
  // 返回0表示数据包发送成功。
}

static void
e1000_recv(void)
{
  uint32 next_index = (regs[E1000_RDT] + 1) % RX_RING_SIZE;
  // 从寄存器中读取当前可用的 RX（接收）描述符环的下一个索引。

  while (rx_ring[next_index].status & E1000_RXD_STAT_DD)
  {
    // 循环检查下一个描述符的状态是否为 "E1000_RXD_STAT_DD"（表示描述符中有新的数据包到达）。

    if (rx_ring[next_index].length > MBUF_SIZE)
    {
      panic("MBUF_SIZE OVERFLOW!");
    }
    // 检查数据包的长度是否超过了 mbuf 的最大大小（MBUF_SIZE）。如果超过了，就触发 panic（内核恐慌）。

    rx_mbufs[next_index]->len = rx_ring[next_index].length;
    // 将接收到的数据包长度存储到对应的 mbuf 中。

    net_rx(rx_mbufs[next_index]);
    // 调用 net_rx() 函数，将 mbuf 传递给网络栈处理，以进行后续的数据包处理和解析。

    rx_mbufs[next_index] = mbufalloc(0);
    // 分配一个新的 mbuf，并将其存储到接收描述符的缓冲区中，以准备接收下一个数据包。

    rx_ring[next_index].addr = (uint64)rx_mbufs[next_index]->head;
    // 将新的 mbuf 的头部地址存储到接收描述符中，以便接收数据包时能够正确写入数据。

    rx_ring[next_index].status = 0;
    // 将接收描述符的状态字段清零，表示该描述符已被处理完毕，可以用于接收新的数据包。

    next_index = (next_index + 1) % RX_RING_SIZE;
    // 更新下一个可用接收描述符的索引，准备处理下一个接收到的数据包。
  }

  regs[E1000_RDT] = (next_index - 1) % RX_RING_SIZE;
  // 更新寄存器中的 RDT（Receive Descriptor Tail）指针，使其指向最后一个处理过的接收描述符。
  //
  // Your code here.
  //
  // Check for packets that have arrived from the e1000
  // Create and deliver an mbuf for each packet (using net_rx()).
  //
}

void e1000_intr(void)
{
  // tell the e1000 we've seen this interrupt;
  // without this the e1000 won't raise any
  // further interrupts.
  regs[E1000_ICR] = 0xffffffff;

  e1000_recv();
}
