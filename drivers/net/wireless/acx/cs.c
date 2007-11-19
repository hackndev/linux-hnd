/***********************************************************************
** Copyright (C) 2003  ACX100 Open Source Project
**
** The contents of this file are subject to the Mozilla Public
** License Version 1.1 (the "License"); you may not use this file
** except in compliance with the License. You may obtain a copy of
** the License at http://www.mozilla.org/MPL/
**
** Software distributed under the License is distributed on an "AS
** IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
** implied. See the License for the specific language governing
** rights and limitations under the License.
**
** Alternatively, the contents of this file may be used under the
** terms of the GNU Public License version 2 (the "GPL"), in which
** case the provisions of the GPL are applicable instead of the
** above.  If you wish to allow the use of your version of this file
** only under the terms of the GPL and not to allow others to use
** your version of this file under the MPL, indicate your decision
** by deleting the provisions above and replace them with the notice
** and other provisions required by the GPL.  If you do not delete
** the provisions above, a recipient may use your version of this
** file under either the MPL or the GPL.
** ---------------------------------------------------------------------
** Inquiries regarding the ACX100 Open Source Project can be
** made directly to:
**
** acx100-users@lists.sf.net
** http://acx100.sf.net
** ---------------------------------------------------------------------
**
** Slave memory interface support:
**
** Todd Blumer - SDG Systems
** Bill Reese - HP
** Eric McCorkle - Shadowsun
**
** CF support, (c) Fabrice Crohas, Paul Sokolovsky
*/
#define ACX_MEM 1

/*
 * non-zero makes it dump the ACX memory to the console then
 * panic when you cat /proc/driver/acx_wlan0_diag
 */
#define DUMP_MEM_DEFINED 1

#define DUMP_MEM_DURING_DIAG 0
#define DUMP_IF_SLOW 0

#define PATCH_AROUND_BAD_SPOTS 1
#define HX4700_FIRMWARE_CHECKSUM 0x0036862e
#define HX4700_ALTERNATE_FIRMWARE_CHECKSUM 0x00368a75

#include <linux/version.h>
#if LINUX_VERSION_CODE <= KERNEL_VERSION(2, 6, 18)
#include <linux/config.h>
#endif

/* Linux 2.6.18+ uses <linux/utsrelease.h> */
#ifndef UTS_RELEASE
#include <linux/utsrelease.h>
#endif

#include <linux/compiler.h> /* required for Lx 2.6.8 ?? */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/sched.h>
#include <linux/types.h>
#include <linux/skbuff.h>
#include <linux/slab.h>
#include <linux/if_arp.h>
#include <linux/irq.h>
#include <linux/rtnetlink.h>
#include <linux/wireless.h>
#include <net/iw_handler.h>
#include <linux/netdevice.h>
#include <linux/ioport.h>
#include <linux/pci.h>
#include <linux/platform_device.h>
#include <linux/pm.h>
#include <linux/vmalloc.h>
#include <linux/delay.h>
#include <linux/workqueue.h>
#include <linux/inetdevice.h>

#define PCMCIA_DEBUG 1

/*
   All the PCMCIA modules use PCMCIA_DEBUG to control debugging.  If
   you do not define PCMCIA_DEBUG at all, all the debug code will be
   left out.  If you compile with PCMCIA_DEBUG=0, the debug code will
   be present but disabled -- but it can then be enabled for specific
   modules at load time with a 'pc_debug=#' option to insmod.
	
*/
#include <pcmcia/cs_types.h>
#include <pcmcia/cs.h>
#include <pcmcia/cistpl.h>
#include <pcmcia/cisreg.h>
#include <pcmcia/ds.h>
#include "acx.h"
#include "acx_hw.h"

#ifdef PCMCIA_DEBUG
static int pc_debug = PCMCIA_DEBUG;
module_param(pc_debug, int, 0);
static char *version = "$Revision: 1.10 $";
#define DEBUG(n, args...) if (pc_debug>(n)) printk(KERN_DEBUG args);
#else
#define DEBUG(n, args...)
#endif


static win_req_t memwin;

typedef struct local_info_t {
        dev_node_t      node;
        struct net_device *ndev;
} local_info_t;

static struct net_device *resume_ndev;


/***********************************************************************
*/

#define CARD_EEPROM_ID_SIZE 6

#include <asm/io.h>

#define REG_ACX_VENDOR_ID 0x900
/*
 * This is the vendor id on the HX4700, anyway
 */
#define ACX_VENDOR_ID 0x8400104c

typedef enum {
	ACX_SOFT_RESET = 0,

	ACX_SLV_REG_ADDR,
	ACX_SLV_REG_DATA,
	ACX_SLV_REG_ADATA,

	ACX_SLV_MEM_CP,
	ACX_SLV_MEM_ADDR,
	ACX_SLV_MEM_DATA,
	ACX_SLV_MEM_CTL,
} acxreg_t;

/***********************************************************************
*/
static void acxmem_i_tx_timeout(struct net_device *ndev);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 19)
static irqreturn_t acxmem_i_interrupt(int irq, void *dev_id);
#else
static irqreturn_t acxmem_i_interrupt(int irq, void *dev_id, struct pt_regs *regs);
#endif
static void acxmem_i_set_multicast_list(struct net_device *ndev);

static int acxmem_e_open(struct net_device *ndev);
static int acxmem_e_close(struct net_device *ndev);
static void acxmem_s_up(struct net_device *ndev);
static void acxmem_s_down(struct net_device *ndev);

static void dump_acxmem (acx_device_t *adev, u32 start, int length);
static int acxmem_complete_hw_reset (acx_device_t *adev);
static void acxmem_s_delete_dma_regions(acx_device_t *adev);

static int
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 11)
acxmem_e_suspend( struct net_device *ndev, pm_message_t state);
#else
acxmem_e_suspend( struct net_device *ndev, u32 state);
#endif
static void
fw_resumer(struct work_struct *notused);
//fw_resumer( void *data );

static int acx_netdev_event(struct notifier_block *this, unsigned long event, void *ptr)
{
  struct net_device *ndev = ptr;
  acx_device_t *adev = ndev2adev(ndev);

  /*
   * Upper level ioctl() handlers send a NETDEV_CHANGEADDR if the MAC address changes.
   */

  if (NETDEV_CHANGEADDR == event) {
    /*
     * the upper layers put the new MAC address in ndev->dev_addr; we just copy
     * it over and update the ACX with it.
     */
    MAC_COPY(adev->dev_addr, adev->ndev->dev_addr);
    adev->set_mask |= GETSET_STATION_ID;
    acx_s_update_card_settings (adev);
  }

  return 0;
}

static struct notifier_block acx_netdev_notifier = {
        .notifier_call = acx_netdev_event,
};

/***********************************************************************
** Register access
*/

/* Pick one */
/* #define INLINE_IO static */
#define INLINE_IO static inline

INLINE_IO u32
read_id_register (acx_device_t *adev)
{
  writel (0x24, &adev->iobase[ACX_SLV_REG_ADDR]);
  return readl (&adev->iobase[ACX_SLV_REG_DATA]);
}

INLINE_IO u32
read_reg32(acx_device_t *adev, unsigned int offset)
{
        u32 val;
	u32 addr;

        if (offset > IO_ACX_ECPU_CTRL)
	  addr = offset;
	else
	  addr = adev->io[offset];

	if (addr < 0x20) {
	  return readl(((u8*)adev->iobase) + addr);
	}

	writel( addr, &adev->iobase[ACX_SLV_REG_ADDR] );
	val = readl( &adev->iobase[ACX_SLV_REG_DATA] );

	return val;
}

INLINE_IO u16
read_reg16(acx_device_t *adev, unsigned int offset)
{
	u16 lo;
	u32 addr;

        if (offset > IO_ACX_ECPU_CTRL)
	  addr = offset;
	else
	  addr = adev->io[offset];

	if (addr < 0x20) {
	    return readw(((u8 *) adev->iobase) + addr);
	}

	writel( addr, &adev->iobase[ACX_SLV_REG_ADDR] );
	lo = readw( (u16 *)&adev->iobase[ACX_SLV_REG_DATA] );

	return lo;
}

INLINE_IO u8
read_reg8(acx_device_t *adev, unsigned int offset)
{
	u8 lo;
	u32 addr;

        if (offset > IO_ACX_ECPU_CTRL)
	  addr = offset;
	else
	  addr = adev->io[offset];

	if (addr < 0x20)
	    return readb(((u8 *)adev->iobase) + addr);

	writel( addr, &adev->iobase[ACX_SLV_REG_ADDR] );
	lo = readw( (u8 *)&adev->iobase[ACX_SLV_REG_DATA] );

	return (u8)lo;
}

INLINE_IO void
write_reg32(acx_device_t *adev, unsigned int offset, u32 val)
{
	u32 addr;

        if (offset > IO_ACX_ECPU_CTRL)
	  addr = offset;
	else
	  addr = adev->io[offset];

	if (addr < 0x20) {
	    writel(val, ((u8*)adev->iobase) + addr);
	    return;
	}

	writel( addr, &adev->iobase[ACX_SLV_REG_ADDR] );
	writel( val, &adev->iobase[ACX_SLV_REG_DATA] );
}

INLINE_IO void
write_reg16(acx_device_t *adev, unsigned int offset, u16 val)
{
	u32 addr;

        if (offset > IO_ACX_ECPU_CTRL)
	  addr = offset;
	else
	  addr = adev->io[offset];

	if (addr < 0x20) {
	    writew(val, ((u8 *)adev->iobase) + addr);
	    return;
	}
	writel( addr, &adev->iobase[ACX_SLV_REG_ADDR] );
	writew( val, (u16 *) &adev->iobase[ACX_SLV_REG_DATA] );
}

INLINE_IO void
write_reg8(acx_device_t *adev, unsigned int offset, u8 val)
{
	u32 addr;

        if (offset > IO_ACX_ECPU_CTRL)
	  addr = offset;
	else
	  addr = adev->io[offset];

	if (addr < 0x20) {
	    writeb(val, ((u8 *) adev->iobase) + addr);
	    return;
	}
	writel( addr, &adev->iobase[ACX_SLV_REG_ADDR] );
	writeb( val, (u8 *)&adev->iobase[ACX_SLV_REG_DATA] );
}

/* Handle PCI posting properly:
 * Make sure that writes reach the adapter in case they require to be executed
 * *before* the next write, by reading a random (and safely accessible) register.
 * This call has to be made if there is no read following (which would flush the data
 * to the adapter), yet the written data has to reach the adapter immediately. */
INLINE_IO void
write_flush(acx_device_t *adev)
{
	/* readb(adev->iobase + adev->io[IO_ACX_INFO_MAILBOX_OFFS]); */
	/* faster version (accesses the first register, IO_ACX_SOFT_RESET,
	 * which should also be safe): */
	(void) readl(adev->iobase);
}

INLINE_IO void
set_regbits (acx_device_t *adev, unsigned int offset, u32 bits) {
  u32 tmp;

  tmp = read_reg32 (adev, offset);
  tmp = tmp | bits;
  write_reg32 (adev, offset, tmp);
  write_flush (adev);
}

INLINE_IO void
clear_regbits (acx_device_t *adev, unsigned int offset, u32 bits) {
  u32 tmp;

  tmp = read_reg32 (adev, offset);
  tmp = tmp & ~bits;
  write_reg32 (adev, offset, tmp);
  write_flush (adev);
}

/*
 * Copy from PXA memory to the ACX memory.  This assumes both the PXA and ACX
 * addresses are 32 bit aligned.  Count is in bytes.
 */
INLINE_IO void
write_slavemem32 (acx_device_t *adev, u32 slave_address, u32 val)
{
  write_reg32 (adev, IO_ACX_SLV_MEM_CTL, 0x0);
  write_reg32 (adev, IO_ACX_SLV_MEM_ADDR, slave_address);
  udelay (10);
  write_reg32 (adev, IO_ACX_SLV_MEM_DATA, val);
}

INLINE_IO u32
read_slavemem32 (acx_device_t *adev, u32 slave_address)
{
  u32 val;

  write_reg32 (adev, IO_ACX_SLV_MEM_CTL, 0x0);
  write_reg32 (adev, IO_ACX_SLV_MEM_ADDR, slave_address);
  udelay (10);
  val = read_reg32 (adev, IO_ACX_SLV_MEM_DATA);

  return val;
}

INLINE_IO void
write_slavemem8 (acx_device_t *adev, u32 slave_address, u8 val)
{
  u32 data;
  u32 base;
  int offset;

  /*
   * Get the word containing the target address and the byte offset in that word.
   */
  base = slave_address & ~3;
  offset = (slave_address & 3) * 8;

  data = read_slavemem32 (adev, base);
  data &= ~(0xff << offset);
  data |= val << offset;
  write_slavemem32 (adev, base, data);
}

INLINE_IO u8
read_slavemem8 (acx_device_t *adev, u32 slave_address)
{
  u8 val;
  u32 base;
  u32 data;
  int offset;

  base = slave_address & ~3;
  offset = (slave_address & 3) * 8;

  data = read_slavemem32 (adev, base);
 
  val = (data >> offset) & 0xff;

  return val;
}

/*
 * doesn't split across word boundaries
 */
INLINE_IO void
write_slavemem16 (acx_device_t *adev, u32 slave_address, u16 val)
{
  u32 data;
  u32 base;
  int offset;

  /*
   * Get the word containing the target address and the byte offset in that word.
   */
  base = slave_address & ~3;
  offset = (slave_address & 3) * 8;

  data = read_slavemem32 (adev, base);
  data &= ~(0xffff << offset);
  data |= val << offset;
  write_slavemem32 (adev, base, data);
}

/*
 * doesn't split across word boundaries
 */
INLINE_IO u16
read_slavemem16 (acx_device_t *adev, u32 slave_address)
{
  u16 val;
  u32 base;
  u32 data;
  int offset;

  base = slave_address & ~3;
  offset = (slave_address & 3) * 8;

  data = read_slavemem32 (adev, base);
 
  val = (data >> offset) & 0xffff;

  return val;
}

/*
 * Copy from slave memory
 *
 * TODO - rewrite using address autoincrement, handle partial words
 */
void
copy_from_slavemem (acx_device_t *adev, u8 *destination, u32 source, int count) {
  u32 tmp = 0;
  u8 *ptmp = (u8 *) &tmp;

  /*
   * Right now I'm making the assumption that the destination is aligned, but
   * I'd better check.
   */
  if ((u32) destination & 3) {
    printk ("acx copy_from_slavemem: warning!  destination not word-aligned!\n");
  }

  while (count >= 4) {
    write_reg32 (adev, IO_ACX_SLV_MEM_ADDR, source);
    udelay (10);
    *((u32 *) destination) = read_reg32 (adev, IO_ACX_SLV_MEM_DATA);
    count -= 4;
    source += 4;
    destination += 4;
  }

  /*
   * If the word reads above didn't satisfy the count, read one more word
   * and transfer a byte at a time until the request is satisfied.
   */
  if (count) {
    write_reg32 (adev, IO_ACX_SLV_MEM_ADDR, source);
    udelay (10);
    tmp = read_reg32 (adev, IO_ACX_SLV_MEM_DATA);
    while (count--) {
      *destination++ = *ptmp++;
    }
  }
}

/*
 * Copy to slave memory
 *
 * TODO - rewrite using autoincrement, handle partial words
 */
void
copy_to_slavemem (acx_device_t *adev, u32 destination, u8 *source, int count)
{
  u32 tmp = 0;
  u8* ptmp = (u8 *) &tmp;
  static u8 src[512];	/* make static to avoid huge stack objects */

  /*
   * For now, make sure the source is word-aligned by copying it to a word-aligned
   * buffer.  Someday rewrite to avoid the extra copy.
   */
  if (count > sizeof (src)) {
    printk ("acx copy_to_slavemem: Warning! buffer overflow!\n");
    count = sizeof (src);
  }
  memcpy (src, source, count);
  source = src;

  while (count >= 4) {
    write_reg32 (adev, IO_ACX_SLV_MEM_ADDR, destination);
    udelay (10);
    write_reg32 (adev, IO_ACX_SLV_MEM_DATA, *((u32 *) source));
    count -= 4;
    source += 4;
    destination += 4;
  }

  /*
   * If there are leftovers read the next word from the acx and merge in
   * what they want to write.
   */
  if (count) {
    write_reg32 (adev, IO_ACX_SLV_MEM_ADDR, destination);
    udelay (10);
    tmp = read_reg32 (adev, IO_ACX_SLV_MEM_DATA);
    while (count--) {
      *ptmp++ = *source++;
    }
    /*
     * reset address in case we're currently in auto-increment mode
     */
    write_reg32 (adev, IO_ACX_SLV_MEM_ADDR, destination);
    udelay (10);
    write_reg32 (adev, IO_ACX_SLV_MEM_DATA, tmp);
    udelay (10);
  }
  
}

/*
 * Block copy to slave buffers using memory block chain mode.  Copies to the ACX
 * transmit buffer structure with minimal intervention on our part.
 * Interrupts should be disabled when calling this.
 */
void
chaincopy_to_slavemem (acx_device_t *adev, u32 destination, u8 *source, int count)
{
  u32 val;
  u32 *data = (u32 *) source;
  static u8 aligned_source[WLAN_A4FR_MAXLEN_WEP_FCS];

  /*
   * Warn if the pointers don't look right.  Destination must fit in [23:5] with
   * zero elsewhere and source should be 32 bit aligned.
   * This should never happen since we're in control of both, but I want to know about
   * it if it does.
   */
  if ((destination & 0x00ffffe0) != destination) {
    printk ("acx chaincopy: destination block 0x%04x not aligned!\n", destination);
  }
  if (count > sizeof aligned_source) {
      printk( KERN_ERR "chaincopy_to_slavemem overflow!\n" );
      count = sizeof aligned_source;
  }
  if ((u32) source & 3) {
    memcpy (aligned_source, source, count);
    data = (u32 *) aligned_source;
  }

  /*
   * SLV_MEM_CTL[17:16] = memory block chain mode with auto-increment
   * SLV_MEM_CTL[5:2] = offset to data portion = 1 word
   */
  val = 2 << 16 | 1 << 2;
  writel (val, &adev->iobase[ACX_SLV_MEM_CTL]);

  /*
   * SLV_MEM_CP[23:5] = start of 1st block
   * SLV_MEM_CP[3:2] = offset to memblkptr = 0
   */
  val = destination & 0x00ffffe0;
  writel (val, &adev->iobase[ACX_SLV_MEM_CP]);

  /*
   * SLV_MEM_ADDR[23:2] = SLV_MEM_CTL[5:2] + SLV_MEM_CP[23:5]
   */
  val = (destination & 0x00ffffe0) + (1<<2);
  writel (val, &adev->iobase[ACX_SLV_MEM_ADDR]);

  /*
   * Write the data to the slave data register, rounding up to the end
   * of the word containing the last byte (hence the > 0)
   */
  while (count > 0) {
    writel (*data++, &adev->iobase[ACX_SLV_MEM_DATA]);
    count -= 4;
  }
}


/*
 * Block copy from slave buffers using memory block chain mode.  Copies from the ACX
 * receive buffer structures with minimal intervention on our part.
 * Interrupts should be disabled when calling this.
 */
void
chaincopy_from_slavemem (acx_device_t *adev, u8 *destination, u32 source, int count)
{
  u32 val;
  u32 *data = (u32 *) destination;
  static u8 aligned_destination[WLAN_A4FR_MAXLEN_WEP_FCS];
  int saved_count = count;

  /*
   * Warn if the pointers don't look right.  Destination must fit in [23:5] with
   * zero elsewhere and source should be 32 bit aligned.
   * Turns out the network stack sends unaligned things, so fix them before
   * copying to the ACX.
   */
  if ((source & 0x00ffffe0) != source) {
    printk ("acx chaincopy: source block 0x%04x not aligned!\n", source);
    dump_acxmem (adev, 0, 0x10000);
  }
  if ((u32) destination & 3) {
    //printk ("acx chaincopy: data destination not word aligned!\n");
    data = (u32 *) aligned_destination;
    if (count > sizeof aligned_destination) {
	printk( KERN_ERR "chaincopy_from_slavemem overflow!\n" );
	count = sizeof aligned_destination;
    }
  }

  /*
   * SLV_MEM_CTL[17:16] = memory block chain mode with auto-increment
   * SLV_MEM_CTL[5:2] = offset to data portion = 1 word
   */
  val = (2 << 16) | (1 << 2);
  writel (val, &adev->iobase[ACX_SLV_MEM_CTL]);

  /*
   * SLV_MEM_CP[23:5] = start of 1st block
   * SLV_MEM_CP[3:2] = offset to memblkptr = 0
   */
  val = source & 0x00ffffe0;
  writel (val, &adev->iobase[ACX_SLV_MEM_CP]);

  /*
   * SLV_MEM_ADDR[23:2] = SLV_MEM_CTL[5:2] + SLV_MEM_CP[23:5]
   */
  val = (source & 0x00ffffe0) + (1<<2);
  writel (val, &adev->iobase[ACX_SLV_MEM_ADDR]);

  /*
   * Read the data from the slave data register, rounding up to the end
   * of the word containing the last byte (hence the > 0)
   */
  while (count > 0) {
    *data++ = readl (&adev->iobase[ACX_SLV_MEM_DATA]);
    count -= 4;
  }

  /*
   * If the destination wasn't aligned, we would have saved it in
   * the aligned buffer, so copy it where it should go.
   */
  if ((u32) destination & 3) {
    memcpy (destination, aligned_destination, saved_count);
  }
}

char
printable (char c) 
{
  return ((c >= 20) && (c < 127)) ? c : '.';
}

#if DUMP_MEM_DEFINED > 0
static void
dump_acxmem (acx_device_t *adev, u32 start, int length)
{
  int i;
  u8 buf[16];

  while (length > 0) {
    printk ("%04x ", start);
    copy_from_slavemem (adev, buf, start, 16);
    for (i = 0; (i < 16) && (i < length); i++) {
      printk ("%02x ", buf[i]);
    }
    for (i = 0; (i < 16) && (i < length); i++) {
      printk ("%c", printable (buf[i]));
    }
    printk ("\n");
    start += 16;
    length -= 16;
  }
}
#endif

static void
enable_acx_irq(acx_device_t *adev);
static void
disable_acx_irq(acx_device_t *adev);

/*
 * Return an acx pointer to the next transmit data block.
 */
u32
allocate_acx_txbuf_space (acx_device_t *adev, int count) {
  u32 block, next, last_block;
  int blocks_needed;
  unsigned long flags;

  spin_lock_irqsave(&adev->txbuf_lock, flags);
  /*
   * Take 4 off the memory block size to account for the reserved word at the start of
   * the block.
   */
  blocks_needed = count / (adev->memblocksize - 4);
  if (count % (adev->memblocksize - 4))
    blocks_needed++;

  if (blocks_needed <= adev->acx_txbuf_blocks_free) {
    /*
     * Take blocks at the head of the free list.
     */
    last_block = block = adev->acx_txbuf_free;

    /*
     * Follow block pointers through the requested number of blocks both to
     * find the new head of the free list and to set the flags for the blocks
     * appropriately.
     */
    while (blocks_needed--) {
      /*
       * Keep track of the last block of the allocation
       */
      last_block = adev->acx_txbuf_free;

      /*
       * Make sure the end control flag is not set.
       */
      next = read_slavemem32 (adev, adev->acx_txbuf_free) & 0x7ffff;
      write_slavemem32 (adev, adev->acx_txbuf_free, next);

      /*
       * Update the new head of the free list
       */
      adev->acx_txbuf_free = next << 5;
      adev->acx_txbuf_blocks_free--;

    }

    /*
     * Flag the last block both by clearing out the next pointer
     * and marking the control field.
     */
    write_slavemem32 (adev, last_block, 0x02000000);

    /*
     * If we're out of buffers make sure the free list pointer is NULL
     */
    if (!adev->acx_txbuf_blocks_free) {
      adev->acx_txbuf_free = 0;
    }
  }
  else {
    block = 0;
  }
  spin_unlock_irqrestore (&adev->txbuf_lock, flags);
  return block;
}

/*
 * Return buffer space back to the pool by following the next pointers until we find
 * the block marked as the end.  Point the last block to the head of the free list,
 * then update the head of the free list to point to the newly freed memory.
 * This routine gets called in interrupt context, so it shouldn't block to protect
 * the integrity of the linked list.  The ISR already holds the lock.
 */
void
reclaim_acx_txbuf_space (acx_device_t *adev, u32 blockptr) {
  u32 cur, last, next;
  unsigned long flags;

  spin_lock_irqsave (&adev->txbuf_lock, flags);
  if ((blockptr >= adev->acx_txbuf_start) &&
      (blockptr <= adev->acx_txbuf_start +
       (adev->acx_txbuf_numblocks - 1) * adev->memblocksize)) {
    cur = blockptr;
    do {
      last = cur;
      next = read_slavemem32 (adev, cur);
      
      /*
       * Advance to the next block in this allocation
       */
      cur = (next & 0x7ffff) << 5;
      
      /*
       * This block now counts as free.
       */
      adev->acx_txbuf_blocks_free++;
    } while (!(next & 0x02000000));
    
    /*
     * last now points to the last block of that allocation.  Update the pointer
     * in that block to point to the free list and reset the free list to the
     * first block of the free call.  If there were no free blocks, make sure
     * the new end of the list marks itself as truly the end.
     */
    if (adev->acx_txbuf_free) {
      write_slavemem32 (adev, last, adev->acx_txbuf_free >> 5);
    }
    else {
      write_slavemem32 (adev, last, 0x02000000);
    }
    adev->acx_txbuf_free = blockptr;
  } 
  spin_unlock_irqrestore(&adev->txbuf_lock, flags);
}

/*
 * Initialize the pieces managing the transmit buffer pool on the ACX.  The transmit
 * buffer is a circular queue with one 32 bit word reserved at the beginning of each
 * block.  The upper 13 bits are a control field, of which only 0x02000000 has any
 * meaning.  The lower 19 bits are the address of the next block divided by 32.
 */
void
init_acx_txbuf (acx_device_t *adev) {
  
  /*
   * acx100_s_init_memory_pools set up txbuf_start and txbuf_numblocks for us.
   * All we need to do is reset the rest of the bookeeping.
   */
  
  adev->acx_txbuf_free = adev->acx_txbuf_start;
  adev->acx_txbuf_blocks_free = adev->acx_txbuf_numblocks;
  
  /*
   * Initialization leaves the last transmit pool block without a pointer back to
   * the head of the list, but marked as the end of the list.  That's how we want
   * to see it, too, so leave it alone.  This is only ever called after a firmware
   * reset, so the ACX memory is in the state we want.
   */
	
}

INLINE_IO int
adev_present(acx_device_t *adev)
{
	/* fast version (accesses the first register, IO_ACX_SOFT_RESET,
	 * which should be safe): */
	return readl(adev->iobase) != 0xffffffff;
}

/***********************************************************************
*/
static inline txdesc_t*
get_txdesc(acx_device_t *adev, int index)
{
	return (txdesc_t*) (((u8*)adev->txdesc_start) + index * adev->txdesc_size);
}

static inline txdesc_t*
advance_txdesc(acx_device_t *adev, txdesc_t* txdesc, int inc)
{
	return (txdesc_t*) (((u8*)txdesc) + inc * adev->txdesc_size);
}

static txhostdesc_t*
get_txhostdesc(acx_device_t *adev, txdesc_t* txdesc)
{
	int index = (u8*)txdesc - (u8*)adev->txdesc_start;
	if (unlikely(ACX_DEBUG && (index % adev->txdesc_size))) {
		printk("bad txdesc ptr %p\n", txdesc);
		return NULL;
	}
	index /= adev->txdesc_size;
	if (unlikely(ACX_DEBUG && (index >= TX_CNT))) {
		printk("bad txdesc ptr %p\n", txdesc);
		return NULL;
	}
	return &adev->txhostdesc_start[index*2];
}

static inline client_t*
get_txc(acx_device_t *adev, txdesc_t* txdesc)
{
	int index = (u8*)txdesc - (u8*)adev->txdesc_start;
	if (unlikely(ACX_DEBUG && (index % adev->txdesc_size))) {
		printk("bad txdesc ptr %p\n", txdesc);
		return NULL;
	}
	index /= adev->txdesc_size;
	if (unlikely(ACX_DEBUG && (index >= TX_CNT))) {
		printk("bad txdesc ptr %p\n", txdesc);
		return NULL;
	}
	return adev->txc[index];
}

static inline u16
get_txr(acx_device_t *adev, txdesc_t* txdesc)
{
	int index = (u8*)txdesc - (u8*)adev->txdesc_start;
	index /= adev->txdesc_size;
	return adev->txr[index];
}

static inline void
put_txcr(acx_device_t *adev, txdesc_t* txdesc, client_t* c, u16 r111)
{
	int index = (u8*)txdesc - (u8*)adev->txdesc_start;
	if (unlikely(ACX_DEBUG && (index % adev->txdesc_size))) {
		printk("bad txdesc ptr %p\n", txdesc);
		return;
	}
	index /= adev->txdesc_size;
	if (unlikely(ACX_DEBUG && (index >= TX_CNT))) {
		printk("bad txdesc ptr %p\n", txdesc);
		return;
	}
	adev->txc[index] = c;
	adev->txr[index] = r111;
}


/***********************************************************************
** EEPROM and PHY read/write helpers
*/
/***********************************************************************
** acxmem_read_eeprom_byte
**
** Function called to read an octet in the EEPROM.
**
** This function is used by acxmem_e_probe to check if the
** connected card is a legal one or not.
**
** Arguments:
**	adev		ptr to acx_device structure
**	addr		address to read in the EEPROM
**	charbuf		ptr to a char. This is where the read octet
**			will be stored
*/
int
acxmem_read_eeprom_byte(acx_device_t *adev, u32 addr, u8 *charbuf)
{
	int result;
	int count;

	write_reg32(adev, IO_ACX_EEPROM_CFG, 0);
	write_reg32(adev, IO_ACX_EEPROM_ADDR, addr);
	write_flush(adev);
	write_reg32(adev, IO_ACX_EEPROM_CTL, 2);

	count = 0xffff;
	while (read_reg16(adev, IO_ACX_EEPROM_CTL)) {
		/* scheduling away instead of CPU burning loop
		 * doesn't seem to work here at all:
		 * awful delay, sometimes also failure.
		 * Doesn't matter anyway (only small delay). */
		if (unlikely(!--count)) {
			printk("%s: timeout waiting for EEPROM read\n",
							adev->ndev->name);
			result = NOT_OK;
			goto fail;
		}
		cpu_relax();
	}

	*charbuf = read_reg8(adev, IO_ACX_EEPROM_DATA);
	log(L_DEBUG, "EEPROM at 0x%04X = 0x%02X\n", addr, *charbuf);
	result = OK;

fail:
	return result;
}


/***********************************************************************
** We don't lock hw accesses here since we never r/w eeprom in IRQ
** Note: this function sleeps only because of GFP_KERNEL alloc
*/
#ifdef UNUSED
int
acxmem_s_write_eeprom(acx_device_t *adev, u32 addr, u32 len, const u8 *charbuf)
{
	u8 *data_verify = NULL;
	unsigned long flags;
	int count, i;
	int result = NOT_OK;
	u16 gpio_orig;

	printk("acx: WARNING! I would write to EEPROM now. "
		"Since I really DON'T want to unless you know "
		"what you're doing (THIS CODE WILL PROBABLY "
		"NOT WORK YET!), I will abort that now. And "
		"definitely make sure to make a "
		"/proc/driver/acx_wlan0_eeprom backup copy first!!! "
		"(the EEPROM content includes the PCI config header!! "
		"If you kill important stuff, then you WILL "
		"get in trouble and people DID get in trouble already)\n");
	return OK;

	FN_ENTER;

	data_verify = kmalloc(len, GFP_KERNEL);
	if (!data_verify) {
		goto end;
	}

	/* first we need to enable the OE (EEPROM Output Enable) GPIO line
	 * to be able to write to the EEPROM.
	 * NOTE: an EEPROM writing success has been reported,
	 * but you probably have to modify GPIO_OUT, too,
	 * and you probably need to activate a different GPIO
	 * line instead! */
	gpio_orig = read_reg16(adev, IO_ACX_GPIO_OE);
	write_reg16(adev, IO_ACX_GPIO_OE, gpio_orig & ~1);
	write_flush(adev);

	/* ok, now start writing the data out */
	for (i = 0; i < len; i++) {
		write_reg32(adev, IO_ACX_EEPROM_CFG, 0);
		write_reg32(adev, IO_ACX_EEPROM_ADDR, addr + i);
		write_reg32(adev, IO_ACX_EEPROM_DATA, *(charbuf + i));
		write_flush(adev);
		write_reg32(adev, IO_ACX_EEPROM_CTL, 1);

		count = 0xffff;
		while (read_reg16(adev, IO_ACX_EEPROM_CTL)) {
			if (unlikely(!--count)) {
				printk("WARNING, DANGER!!! "
					"Timeout waiting for EEPROM write\n");
				goto end;
			}
			cpu_relax();
		}
	}

	/* disable EEPROM writing */
	write_reg16(adev, IO_ACX_GPIO_OE, gpio_orig);
	write_flush(adev);

	/* now start a verification run */
	for (i = 0; i < len; i++) {
		write_reg32(adev, IO_ACX_EEPROM_CFG, 0);
		write_reg32(adev, IO_ACX_EEPROM_ADDR, addr + i);
		write_flush(adev);
		write_reg32(adev, IO_ACX_EEPROM_CTL, 2);

		count = 0xffff;
		while (read_reg16(adev, IO_ACX_EEPROM_CTL)) {
			if (unlikely(!--count)) {
				printk("timeout waiting for EEPROM read\n");
				goto end;
			}
			cpu_relax();
		}

		data_verify[i] = read_reg16(adev, IO_ACX_EEPROM_DATA);
	}

	if (0 == memcmp(charbuf, data_verify, len))
		result = OK; /* read data matches, success */

end:
	kfree(data_verify);
	FN_EXIT1(result);
	return result;
}
#endif /* UNUSED */


/***********************************************************************
** acxmem_s_read_phy_reg
**
** Messing with rx/tx disabling and enabling here
** (write_reg32(adev, IO_ACX_ENABLE, 0b000000xx)) kills traffic
*/
int
acxmem_s_read_phy_reg(acx_device_t *adev, u32 reg, u8 *charbuf)
{
	int result = NOT_OK;
	int count;

	FN_ENTER;

	write_reg32(adev, IO_ACX_PHY_ADDR, reg);
	write_flush(adev);
	write_reg32(adev, IO_ACX_PHY_CTL, 2);

	count = 0xffff;
	while (read_reg32(adev, IO_ACX_PHY_CTL)) {
		/* scheduling away instead of CPU burning loop
		 * doesn't seem to work here at all:
		 * awful delay, sometimes also failure.
		 * Doesn't matter anyway (only small delay). */
		if (unlikely(!--count)) {
			printk("%s: timeout waiting for phy read\n",
							adev->ndev->name);
			*charbuf = 0;
			goto fail;
		}
		cpu_relax();
	}

	log(L_DEBUG, "count was %u\n", count);
	*charbuf = read_reg8(adev, IO_ACX_PHY_DATA);

	log(L_DEBUG, "radio PHY at 0x%04X = 0x%02X\n", *charbuf, reg);
	result = OK;
	goto fail; /* silence compiler warning */
fail:
	FN_EXIT1(result);
	return result;
}


/***********************************************************************
*/
int
acxmem_s_write_phy_reg(acx_device_t *adev, u32 reg, u8 value)
{
        int count;
	FN_ENTER;

	/* mprusko said that 32bit accesses result in distorted sensitivity
	 * on his card. Unconfirmed, looks like it's not true (most likely since we
	 * now properly flush writes). */
	write_reg32(adev, IO_ACX_PHY_DATA, value);
	write_reg32(adev, IO_ACX_PHY_ADDR, reg);
	write_flush(adev);
	write_reg32(adev, IO_ACX_PHY_CTL, 1);
	write_flush(adev);

	count = 0xffff;
	while (read_reg32(adev, IO_ACX_PHY_CTL)) {
		/* scheduling away instead of CPU burning loop
		 * doesn't seem to work here at all:
		 * awful delay, sometimes also failure.
		 * Doesn't matter anyway (only small delay). */
		if (unlikely(!--count)) {
			printk("%s: timeout waiting for phy read\n",
							adev->ndev->name);
			goto fail;
		}
		cpu_relax();
	}

	log(L_DEBUG, "radio PHY write 0x%02X at 0x%04X\n", value, reg);
 fail:
	FN_EXIT1(OK);
	return OK;
}


#define NO_AUTO_INCREMENT	1

/***********************************************************************
** acxmem_s_write_fw
**
** Write the firmware image into the card.
**
** Arguments:
**	adev		wlan device structure
**	fw_image	firmware image.
**
** Returns:
**	1	firmware image corrupted
**	0	success
*/
static int
acxmem_s_write_fw(acx_device_t *adev, const firmware_image_t *fw_image, u32 offset)
{
	int len, size, checkMismatch = -1;
	u32 sum, v32, tmp, id;
	/* we skip the first four bytes which contain the control sum */
	const u8 *p = (u8*)fw_image + 4;

	/* start the image checksum by adding the image size value */
	sum = p[0]+p[1]+p[2]+p[3];
	p += 4;

#ifdef NOPE
#if NO_AUTO_INCREMENT
	write_reg32(adev, IO_ACX_SLV_MEM_CTL, 0); /* use basic mode */
#else
	write_reg32(adev, IO_ACX_SLV_MEM_CTL, 1); /* use autoincrement mode */
	write_reg32(adev, IO_ACX_SLV_MEM_ADDR, offset); /* configure start address */
	write_flush(adev);
#endif
#endif
	len = 0;
	size = le32_to_cpu(fw_image->size) & (~3);

	while (likely(len < size)) {
		v32 = be32_to_cpu(*(u32*)p);
		sum += p[0]+p[1]+p[2]+p[3];
		p += 4;
		len += 4;

#ifdef NOPE
#if NO_AUTO_INCREMENT 
		write_reg32(adev, IO_ACX_SLV_MEM_ADDR, offset + len - 4);
		write_flush(adev);
#endif
		write_reg32(adev, IO_ACX_SLV_MEM_DATA, v32);
		write_flush(adev);
#endif
		write_slavemem32 (adev, offset + len - 4, v32);

		id = read_id_register (adev);
		
		/*
		 * check the data written
		 */
		tmp = read_slavemem32 (adev, offset + len - 4);
		if (checkMismatch && (tmp != v32)) {
		  printk ("first data mismatch at 0x%08x good 0x%08x bad 0x%08x id 0x%08x\n",
			  offset + len - 4, v32, tmp, id);
		  checkMismatch = 0;
		}
	}
	log(L_DEBUG, "firmware written, size:%d sum1:%x sum2:%x\n",
			size, sum, le32_to_cpu(fw_image->chksum));

	/* compare our checksum with the stored image checksum */
	return (sum != le32_to_cpu(fw_image->chksum));
}


/***********************************************************************
** acxmem_s_validate_fw
**
** Compare the firmware image given with
** the firmware image written into the card.
**
** Arguments:
**	adev		wlan device structure
**	fw_image	firmware image.
**
** Returns:
**	NOT_OK	firmware image corrupted or not correctly written
**	OK	success
*/
static int
acxmem_s_validate_fw(acx_device_t *adev, const firmware_image_t *fw_image,
				u32 offset)
{
	u32 sum, v32, w32;
	int len, size;
	int result = OK;
	/* we skip the first four bytes which contain the control sum */
	const u8 *p = (u8*)fw_image + 4;

	/* start the image checksum by adding the image size value */
	sum = p[0]+p[1]+p[2]+p[3];
	p += 4;

	write_reg32(adev, IO_ACX_SLV_END_CTL, 0);

#if NO_AUTO_INCREMENT
	write_reg32(adev, IO_ACX_SLV_MEM_CTL, 0); /* use basic mode */
#else
	write_reg32(adev, IO_ACX_SLV_MEM_CTL, 1); /* use autoincrement mode */
	write_reg32(adev, IO_ACX_SLV_MEM_ADDR, offset); /* configure start address */
#endif

	len = 0;
	size = le32_to_cpu(fw_image->size) & (~3);

	while (likely(len < size)) {
		v32 = be32_to_cpu(*(u32*)p);
		p += 4;
		len += 4;

#ifdef NOPE
#if NO_AUTO_INCREMENT
		write_reg32(adev, IO_ACX_SLV_MEM_ADDR, offset + len - 4);
#endif
		udelay(10);
		w32 = read_reg32(adev, IO_ACX_SLV_MEM_DATA);
#endif
		w32 = read_slavemem32 (adev, offset + len - 4);

		if (unlikely(w32 != v32)) {
			printk("acx: FATAL: firmware upload: "
			"data parts at offset %d don't match\n(0x%08X vs. 0x%08X)!\n"
			"I/O timing issues or defective memory, with DWL-xx0+? "
			"ACX_IO_WIDTH=16 may help. Please report\n",
				len, v32, w32);
			result = NOT_OK;
			break;
		}

		sum += (u8)w32 + (u8)(w32>>8) + (u8)(w32>>16) + (u8)(w32>>24);
	}

	/* sum control verification */
	if (result != NOT_OK) {
		if (sum != le32_to_cpu(fw_image->chksum)) {
			printk("acx: FATAL: firmware upload: "
				"checksums don't match!\n");
			result = NOT_OK;
		}
	}

	return result;
}


/***********************************************************************
** acxmem_s_upload_fw
**
** Called from acx_reset_dev
*/
static int
acxmem_s_upload_fw(acx_device_t *adev)
{
	firmware_image_t *fw_image = NULL;
	int res = NOT_OK;
	int try;
	u32 file_size;
	char *filename = "WLANGEN.BIN";
#ifdef PATCH_AROUND_BAD_SPOTS
	u32 offset;
	int i;
        /*
         * arm-linux-objdump -d patch.bin, or
         * od -Ax -t x4 patch.bin after finding the bounds
         * of the .text section with arm-linux-objdump -s patch.bin
         */
        u32 patch[] = {
	  0xe584c030, 0xe59fc008,
	  0xe92d1000, 0xe59fc004, 0xe8bd8000, 0x0000080c,
	  0x0000aa68, 0x605a2200, 0x2c0a689c, 0x2414d80a,
	  0x2f00689f, 0x1c27d007, 0x06241e7c, 0x2f000e24,
	  0xe000d1f6, 0x602e6018, 0x23036468, 0x480203db,
	  0x60ca6003, 0xbdf0750a, 0xffff0808
	};
#endif

	FN_ENTER;
	/* No combined image; tell common we need the radio firmware, too */
	adev->need_radio_fw = 1;

	fw_image = acx_s_read_fw(adev->dev, filename, &file_size);
	if (!fw_image) {
	  FN_EXIT1(NOT_OK);
	  return NOT_OK;
	}

	for (try = 1; try <= 5; try++) {
		res = acxmem_s_write_fw(adev, fw_image, 0);
		log(L_DEBUG|L_INIT, "acx_write_fw (main): %d\n", res);
		if (OK == res) {
			res = acxmem_s_validate_fw(adev, fw_image, 0);
			log(L_DEBUG|L_INIT, "acx_validate_fw "
					"(main): %d\n", res);
		}

		if (OK == res) {
			SET_BIT(adev->dev_state_mask, ACX_STATE_FW_LOADED);
			break;
		}
		printk("acx: firmware upload attempt #%d FAILED, "
			"retrying...\n", try);
		acx_s_msleep(1000); /* better wait for a while... */
	}

#ifdef PATCH_AROUND_BAD_SPOTS
	/*
	 * Only want to do this if the firmware is exactly what we expect for an
	 * iPaq 4700; otherwise, bad things would ensue.
	 */
	if ((HX4700_FIRMWARE_CHECKSUM == fw_image->chksum) ||
	    (HX4700_ALTERNATE_FIRMWARE_CHECKSUM == fw_image->chksum)) {
	  /*
	   * Put the patch after the main firmware image.  0x950c contains
	   * the ACX's idea of the end of the firmware.  Use that location to
	   * load ours (which depends on that location being 0xab58) then
	   * update that location to point to after ours.
	   */

	  offset = read_slavemem32 (adev, 0x950c);
	  
	  log (L_DEBUG, "acx: patching in at 0x%04x\n", offset);

	  for (i = 0; i < sizeof(patch) / sizeof(patch[0]); i++) {
	    write_slavemem32 (adev, offset, patch[i]);
	    offset += sizeof(u32);
	  }

	  /*
	   * Patch the instruction at 0x0804 to branch to our ARM patch at 0xab58
	   */
	  write_slavemem32 (adev, 0x0804, 0xea000000 + (0xab58-0x0804-8)/4);

	  /*
	   * Patch the instructions at 0x1f40 to branch to our Thumb patch at 0xab74
	   *
	   * 4a00 ldr r2, [pc, #0]
	   * 4710 bx  r2
	   * .data 0xab74+1
	   */
	  write_slavemem32 (adev, 0x1f40, 0x47104a00);
	  write_slavemem32 (adev, 0x1f44, 0x0000ab74+1);

	  /*
	   * Bump the end of the firmware up to beyond our patch.
	   */
	  write_slavemem32 (adev, 0x950c, offset);
	
	}
#endif

	vfree(fw_image);

	FN_EXIT1(res);
	return res;
}


/***********************************************************************
** acxmem_s_upload_radio
**
** Uploads the appropriate radio module firmware into the card.
*/
int
acxmem_s_upload_radio(acx_device_t *adev)
{
	acx_ie_memmap_t mm;
	firmware_image_t *radio_image;
	acx_cmd_radioinit_t radioinit;
	int res = NOT_OK;
	int try;
	u32 offset;
	u32 size;
	char filename[sizeof("RADIONN.BIN")];

	if (!adev->need_radio_fw) return OK;

	FN_ENTER;

	acx_s_interrogate(adev, &mm, ACX1xx_IE_MEMORY_MAP);
	offset = le32_to_cpu(mm.CodeEnd);

	snprintf(filename, sizeof(filename), "RADIO%02x.BIN",
		adev->radio_type);
	radio_image = acx_s_read_fw(adev->dev, filename, &size);
	if (!radio_image) {
		printk("acx: can't load radio module '%s'\n", filename);
		goto fail;
	}

	acx_s_issue_cmd(adev, ACX1xx_CMD_SLEEP, NULL, 0);

	for (try = 1; try <= 5; try++) {
		res = acxmem_s_write_fw(adev, radio_image, offset);
		log(L_DEBUG|L_INIT, "acx_write_fw (radio): %d\n", res);
		if (OK == res) {
			res = acxmem_s_validate_fw(adev, radio_image, offset);
			log(L_DEBUG|L_INIT, "acx_validate_fw (radio): %d\n", res);
		}

		if (OK == res)
			break;
		printk("acx: radio firmware upload attempt #%d FAILED, "
			"retrying...\n", try);
		acx_s_msleep(1000); /* better wait for a while... */
	}

	acx_s_issue_cmd(adev, ACX1xx_CMD_WAKE, NULL, 0);
	radioinit.offset = cpu_to_le32(offset);

	/* no endian conversion needed, remains in card CPU area: */
	radioinit.len = radio_image->size;

	vfree(radio_image);

	if (OK != res)
		goto fail;

	/* will take a moment so let's have a big timeout */
	acx_s_issue_cmd_timeo(adev, ACX1xx_CMD_RADIOINIT,
		&radioinit, sizeof(radioinit), CMD_TIMEOUT_MS(1000));

	res = acx_s_interrogate(adev, &mm, ACX1xx_IE_MEMORY_MAP);

fail:
	FN_EXIT1(res);
	return res;
}

/***********************************************************************
** acxmem_l_reset_mac
**
** MAC will be reset
** Call context: reset_dev
*/
static void
acxmem_l_reset_mac(acx_device_t *adev)
{
  int count;
	FN_ENTER;

	/* halt eCPU */
	set_regbits (adev, IO_ACX_ECPU_CTRL, 0x1);

	/* now do soft reset of eCPU, set bit */
	set_regbits (adev, IO_ACX_SOFT_RESET, 0x1);
	log(L_DEBUG, "%s: enable soft reset...\n", __func__);

	/* Windows driver sleeps here for a while with this sequence */
	for (count = 0; count < 200; count++) {
	  udelay (50);
	}

	/* now clear bit again: deassert eCPU reset */
	log(L_DEBUG, "%s: disable soft reset and go to init mode...\n", __func__);
	clear_regbits (adev, IO_ACX_SOFT_RESET, 0x1);

	/* now start a burst read from initial EEPROM */
	set_regbits (adev, IO_ACX_EE_START, 0x1);

	/*
	 * Windows driver sleeps here for a while with this sequence
	 */
	for (count = 0; count < 200; count++) {
	  udelay (50);
	}

	/* Windows driver writes 0x10000 to register 0x808 here */
	
	write_reg32 (adev, 0x808, 0x10000);

	FN_EXIT0;
}


/***********************************************************************
** acxmem_s_verify_init
*/
static int
acxmem_s_verify_init(acx_device_t *adev)
{
	int result = NOT_OK;
	unsigned long timeout;

	FN_ENTER;

	timeout = jiffies + 2*HZ;
	for (;;) {
		u32 irqstat = read_reg32(adev, IO_ACX_IRQ_STATUS_NON_DES);
		if ((irqstat != 0xFFFFFFFF) && (irqstat & HOST_INT_FCS_THRESHOLD)) {
			result = OK;
			write_reg32(adev, IO_ACX_IRQ_ACK, HOST_INT_FCS_THRESHOLD);
			break;
		}
		if (time_after(jiffies, timeout))
			break;
		/* Init may take up to ~0.5 sec total */
		acx_s_msleep(50);
	}

	FN_EXIT1(result);
	return result;
}


/***********************************************************************
** A few low-level helpers
**
** Note: these functions are not protected by lock
** and thus are never allowed to be called from IRQ.
** Also they must not race with fw upload which uses same hw regs
*/

/***********************************************************************
** acxmem_write_cmd_type_status
*/

static inline void
acxmem_write_cmd_type_status(acx_device_t *adev, u16 type, u16 status)
{
  write_slavemem32 (adev, (u32) adev->cmd_area, type | (status << 16));
  write_flush(adev);
}


/***********************************************************************
** acxmem_read_cmd_type_status
*/
static u32
acxmem_read_cmd_type_status(acx_device_t *adev)
{
	u32 cmd_type, cmd_status;

	cmd_type = read_slavemem32 (adev, (u32) adev->cmd_area);

	cmd_status = (cmd_type >> 16);
	cmd_type = (u16)cmd_type;

	log(L_CTL, "cmd_type:%04X cmd_status:%04X [%s]\n",
		cmd_type, cmd_status,
		acx_cmd_status_str(cmd_status));

	return cmd_status;
}


/***********************************************************************
** acxmem_s_reset_dev
**
** Arguments:
**	netdevice that contains the adev variable
** Returns:
**	NOT_OK on fail
**	OK on success
** Side effects:
**	device is hard reset
** Call context:
**	acxmem_e_probe
** Comment:
**	This resets the device using low level hardware calls
**	as well as uploads and verifies the firmware to the card
*/

static inline void
init_mboxes(acx_device_t *adev)
{
	u32 cmd_offs, info_offs;

	cmd_offs = read_reg32(adev, IO_ACX_CMD_MAILBOX_OFFS);
	info_offs = read_reg32(adev, IO_ACX_INFO_MAILBOX_OFFS);
	adev->cmd_area = (u8*) cmd_offs;
	adev->info_area = (u8*) info_offs;
	/*
	log(L_DEBUG, "iobase2=%p\n"
	*/
	log( L_DEBUG, "cmd_mbox_offset=%X cmd_area=%p\n"
		"info_mbox_offset=%X info_area=%p\n",
		cmd_offs, adev->cmd_area,
		info_offs, adev->info_area);
}


static inline void
read_eeprom_area(acx_device_t *adev)
{
#if ACX_DEBUG > 1
	int offs;
	u8 tmp;

	for (offs = 0x8c; offs < 0xb9; offs++)
		acxmem_read_eeprom_byte(adev, offs, &tmp);
#endif
}

static int
acxmem_s_reset_dev(acx_device_t *adev)
{
	const char* msg = "";
	unsigned long flags;
	int result = NOT_OK;
	u16 hardware_info;
	u16 ecpu_ctrl;
	int count;
	u32 tmp;

	FN_ENTER;
	/*
	write_reg32 (adev, IO_ACX_SLV_MEM_CP, 0);
	*/
	/* reset the device to make sure the eCPU is stopped
	 * to upload the firmware correctly */

	acx_lock(adev, flags);

	/* Windows driver does some funny things here */
	/*
	 * clear bit 0x200 in register 0x2A0
	 */
	clear_regbits (adev, 0x2A0, 0x200);

	/*
	 * Set bit 0x200 in ACX_GPIO_OUT
	 */
	set_regbits (adev, IO_ACX_GPIO_OUT, 0x200);

	/*
	 * read register 0x900 until its value is 0x8400104C, sleeping
	 * in between reads if it's not immediate
	 */
	tmp = read_reg32 (adev, REG_ACX_VENDOR_ID);
	count = 500;
	while (count-- && (tmp != ACX_VENDOR_ID)) {
	  mdelay (10);
	  tmp = read_reg32 (adev, REG_ACX_VENDOR_ID);
	}

	/* end what Windows driver does */

	acxmem_l_reset_mac(adev);

	ecpu_ctrl = read_reg32(adev, IO_ACX_ECPU_CTRL) & 1;
	if (!ecpu_ctrl) {
		msg = "eCPU is already running. ";
		goto end_unlock;
	}

#ifdef WE_DONT_NEED_THAT_DO_WE
	if (read_reg16(adev, IO_ACX_SOR_CFG) & 2) {
		/* eCPU most likely means "embedded CPU" */
		msg = "eCPU did not start after boot from flash. ";
		goto end_unlock;
	}

	/* check sense on reset flags */
	if (read_reg16(adev, IO_ACX_SOR_CFG) & 0x10) {
		printk("%s: eCPU did not start after boot (SOR), "
			"is this fatal?\n", adev->ndev->name);
	}
#endif
	/* scan, if any, is stopped now, setting corresponding IRQ bit */
	adev->irq_status |= HOST_INT_SCAN_COMPLETE;

	acx_unlock(adev, flags);
	
	/* need to know radio type before fw load */
	/* Need to wait for arrival of this information in a loop,
	 * most probably since eCPU runs some init code from EEPROM
	 * (started burst read in reset_mac()) which also
	 * sets the radio type ID */

	count = 0xffff;
	do {
		hardware_info = read_reg16(adev, IO_ACX_EEPROM_INFORMATION);
		if (!--count) {
			msg = "eCPU didn't indicate radio type";
			goto end_fail;
		}
		cpu_relax();
	} while (!(hardware_info & 0xff00)); /* radio type still zero? */
	printk("ACX radio type 0x%02x\n", (hardware_info >> 8) & 0xff);
	/* printk("DEBUG: count %d\n", count); */
	adev->form_factor = hardware_info & 0xff;
	adev->radio_type = hardware_info >> 8;

	/* load the firmware */
	if (OK != acxmem_s_upload_fw(adev))
		goto end_fail;

	/* acx_s_msleep(10);	this one really shouldn't be required */

	/* now start eCPU by clearing bit */
	clear_regbits (adev, IO_ACX_ECPU_CTRL, 0x1);
	log(L_DEBUG, "booted eCPU up and waiting for completion...\n");

	/* Windows driver clears bit 0x200 in register 0x2A0 here */
	clear_regbits (adev, 0x2A0, 0x200);

	/* Windows driver sets bit 0x200 in ACX_GPIO_OUT here */
	set_regbits (adev, IO_ACX_GPIO_OUT, 0x200);
	/* wait for eCPU bootup */
	if (OK != acxmem_s_verify_init(adev)) {
		msg = "timeout waiting for eCPU. ";
		goto end_fail;
	}
	log(L_DEBUG, "eCPU has woken up, card is ready to be configured\n");
	init_mboxes(adev);
	acxmem_write_cmd_type_status(adev, ACX1xx_CMD_RESET, 0);

	/* test that EEPROM is readable */
	read_eeprom_area(adev);

	result = OK;
	goto end;

/* Finish error message. Indicate which function failed */
end_unlock:
	acx_unlock(adev, flags);
end_fail:
	printk("acx: %sreset_dev() FAILED\n", msg);
end:
	FN_EXIT1(result);
	return result;
}


/***********************************************************************
** acxmem_s_issue_cmd_timeo
**
** Sends command to fw, extract result
**
** NB: we do _not_ take lock inside, so be sure to not touch anything
** which may interfere with IRQ handler operation
**
** TODO: busy wait is a bit silly, so:
** 1) stop doing many iters - go to sleep after first
** 2) go to waitqueue based approach: wait, not poll!
*/
#undef FUNC
#define FUNC "issue_cmd"

#if !ACX_DEBUG
int
acxmem_s_issue_cmd_timeo(
	acx_device_t *adev,
	unsigned int cmd,
	void *buffer,
	unsigned buflen,
	unsigned cmd_timeout)
{
#else
int
acxmem_s_issue_cmd_timeo_debug(
	acx_device_t *adev,
	unsigned cmd,
	void *buffer,
	unsigned buflen,
	unsigned cmd_timeout,
	const char* cmdstr)
{
	unsigned long start = jiffies;
#endif
	const char *devname;
	unsigned counter;
	u16 irqtype;
	int i, j;
	u8 *p;
	u16 cmd_status;
	unsigned long timeout;

	FN_ENTER;

	devname = adev->ndev->name;
	if (!devname || !devname[0] || devname[4]=='%')
		devname = "acx";

	log(L_CTL, FUNC"(cmd:%s,buflen:%u,timeout:%ums,type:0x%04X)\n",
		cmdstr, buflen, cmd_timeout,
		buffer ? le16_to_cpu(((acx_ie_generic_t *)buffer)->type) : -1);

	if (!(adev->dev_state_mask & ACX_STATE_FW_LOADED)) {
		printk("%s: "FUNC"(): firmware is not loaded yet, "
			"cannot execute commands!\n", devname);
		goto bad;
	}

	if ((acx_debug & L_DEBUG) && (cmd != ACX1xx_CMD_INTERROGATE)) {
		printk("input buffer (len=%u):\n", buflen);
		acx_dump_bytes(buffer, buflen);
	}

	/* wait for firmware to become idle for our command submission */
	timeout = HZ/5;
	counter = (timeout * 1000 / HZ) - 1; /* in ms */
	timeout += jiffies;
	do {
		cmd_status = acxmem_read_cmd_type_status(adev);
		/* Test for IDLE state */
		if (!cmd_status)
			break;
		if (counter % 8 == 0) {
			if (time_after(jiffies, timeout)) {
				counter = 0;
				break;
			}
			/* we waited 8 iterations, no luck. Sleep 8 ms */
			acx_s_msleep(8);
		}
	} while (likely(--counter));

	if (!counter) {
		/* the card doesn't get idle, we're in trouble */
		printk("%s: "FUNC"(): cmd_status is not IDLE: 0x%04X!=0\n",
			devname, cmd_status);
#if DUMP_IF_SLOW > 0
		dump_acxmem (adev, 0, 0x10000);
		panic ("not idle");
#endif
		goto bad;
	} else if (counter < 190) { /* if waited >10ms... */
		log(L_CTL|L_DEBUG, FUNC"(): waited for IDLE %dms. "
			"Please report\n", 199 - counter);
	}

	/* now write the parameters of the command if needed */
	if (buffer && buflen) {
		/* if it's an INTERROGATE command, just pass the length
		 * of parameters to read, as data */
#if CMD_DISCOVERY
		if (cmd == ACX1xx_CMD_INTERROGATE)
			memset_io(adev->cmd_area + 4, 0xAA, buflen);
#endif
		/*
		 * slave memory version
		 */
		copy_to_slavemem (adev, (u32) (adev->cmd_area + 4), buffer, 
			       (cmd == ACX1xx_CMD_INTERROGATE) ? 4 : buflen);
	}
	/* now write the actual command type */
	acxmem_write_cmd_type_status(adev, cmd, 0);

	/* clear CMD_COMPLETE bit. can be set only by IRQ handler: */
	adev->irq_status &= ~HOST_INT_CMD_COMPLETE;

	/* execute command */
	write_reg16(adev, IO_ACX_INT_TRIG, INT_TRIG_CMD);
	write_flush(adev);

	/* wait for firmware to process command */

	/* Ensure nonzero and not too large timeout.
	** Also converts e.g. 100->99, 200->199
	** which is nice but not essential */
	cmd_timeout = (cmd_timeout-1) | 1;
	if (unlikely(cmd_timeout > 1199))
		cmd_timeout = 1199;

	/* we schedule away sometimes (timeout can be large) */
	counter = cmd_timeout;
	timeout = jiffies + cmd_timeout * HZ / 1000;
	do {
		if (!adev->irqs_active) { /* IRQ disabled: poll */
			irqtype = read_reg16(adev, IO_ACX_IRQ_STATUS_NON_DES);
			if (irqtype & HOST_INT_CMD_COMPLETE) {
				write_reg16(adev, IO_ACX_IRQ_ACK,
						HOST_INT_CMD_COMPLETE);
				break;
			}
		} else { /* Wait when IRQ will set the bit */
			irqtype = adev->irq_status;
			if (irqtype & HOST_INT_CMD_COMPLETE)
				break;
		}

		if (counter % 8 == 0) {
			if (time_after(jiffies, timeout)) {
				counter = 0;
				break;
			}
			/* we waited 8 iterations, no luck. Sleep 8 ms */
			acx_s_msleep(8);
		}
	} while (likely(--counter));

	/* save state for debugging */
	cmd_status = acxmem_read_cmd_type_status(adev);

	/* put the card in IDLE state */
	acxmem_write_cmd_type_status(adev, ACX1xx_CMD_RESET, 0);

	if (!counter) {	/* timed out! */
		printk("%s: "FUNC"(): timed out %s for CMD_COMPLETE. "
			"irq bits:0x%04X irq_status:0x%04X timeout:%dms "
			"cmd_status:%d (%s)\n",
			devname, (adev->irqs_active) ? "waiting" : "polling",
			irqtype, adev->irq_status, cmd_timeout,
			cmd_status, acx_cmd_status_str(cmd_status));
		printk("%s: "FUNC"(): device irq status 0x%04x\n",
		       devname, read_reg16(adev, IO_ACX_IRQ_STATUS_NON_DES));
		printk("%s: "FUNC"(): IO_ACX_IRQ_MASK 0x%04x IO_ACX_FEMR 0x%04x\n",
		       devname,
		       read_reg16 (adev, IO_ACX_IRQ_MASK),
		       read_reg16 (adev, IO_ACX_FEMR));
		if (read_reg16 (adev, IO_ACX_IRQ_MASK) == 0xffff) {
			printk ("acxmem: firmware probably hosed - reloading\n");
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 11)
                	{
				pm_message_t state;
				/* acxmem_e_suspend (resume_pdev, state); */
				acxmem_e_suspend (adev->ndev , state);
			}
#else
			acxmem_e_suspend (adev, 0);
#endif
			{
				resume_ndev = adev->ndev;
				fw_resumer (NULL);
			}
		}

		goto bad;
	} else if (cmd_timeout - counter > 30) { /* if waited >30ms... */
		log(L_CTL|L_DEBUG, FUNC"(): %s for CMD_COMPLETE %dms. "
			"count:%d. Please report\n",
			(adev->irqs_active) ? "waited" : "polled",
			cmd_timeout - counter, counter);
	}

	if (1 != cmd_status) { /* it is not a 'Success' */
		printk("%s: "FUNC"(): cmd_status is not SUCCESS: %d (%s). "
			"Took %dms of %d\n",
			devname, cmd_status, acx_cmd_status_str(cmd_status),
			cmd_timeout - counter, cmd_timeout);
		/* zero out result buffer
		 * WARNING: this will trash stack in case of illegally large input
		 * length! */
		if (buflen > 388) {
		  /*
		   * 388 is maximum command length
		   */
		  printk ("invalid length 0x%08x\n", buflen);
		  buflen = 388;
		}
		p = (u8 *) buffer;
		for (i = 0; i < buflen; i+= 16) {
		  printk ("%04x:", i);
		  for (j = 0; (j < 16) && (i+j < buflen); j++) {
		    printk (" %02x", *p++);
		  }
		  printk ("\n");
		}
		if (buffer && buflen)
			memset(buffer, 0, buflen);
		goto bad;
	}

	/* read in result parameters if needed */
	if (buffer && buflen && (cmd == ACX1xx_CMD_INTERROGATE)) {
		copy_from_slavemem (adev, buffer, (u32) (adev->cmd_area + 4), buflen);
		if (acx_debug & L_DEBUG) {
			printk("output buffer (len=%u): ", buflen);
			acx_dump_bytes(buffer, buflen);
		}
	}

/* ok: */
	log(L_CTL, FUNC"(%s): took %ld jiffies to complete\n",
			 cmdstr, jiffies - start);
	FN_EXIT1(OK);
	return OK;

bad:
	/* Give enough info so that callers can avoid
	** printing their own diagnostic messages */
#if ACX_DEBUG
	printk("%s: "FUNC"(cmd:%s) FAILED\n", devname, cmdstr);
#else
	printk("%s: "FUNC"(cmd:0x%04X) FAILED\n", devname, cmd);
#endif
	dump_stack();
	FN_EXIT1(NOT_OK);
	return NOT_OK;
}


/***********************************************************************
*/
#if defined(NONESSENTIAL_FEATURES)
typedef struct device_id {
	unsigned char id[6];
	char *descr;
	char *type;
} device_id_t;

static const device_id_t
device_ids[] =
{
	{
		{'G', 'l', 'o', 'b', 'a', 'l'},
		NULL,
		NULL,
	},
	{
		{0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
		"uninitialized",
		"SpeedStream SS1021 or Gigafast WF721-AEX"
	},
	{
		{0x80, 0x81, 0x82, 0x83, 0x84, 0x85},
		"non-standard",
		"DrayTek Vigor 520"
	},
	{
		{'?', '?', '?', '?', '?', '?'},
		"non-standard",
		"Level One WPC-0200"
	},
	{
		{0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
		"empty",
		"DWL-650+ variant"
	}
};

static void
acx_show_card_eeprom_id(acx_device_t *adev)
{
	unsigned char buffer[CARD_EEPROM_ID_SIZE];
	int i;

	memset(&buffer, 0, CARD_EEPROM_ID_SIZE);
	/* use direct EEPROM access */
	for (i = 0; i < CARD_EEPROM_ID_SIZE; i++) {
		if (OK != acxmem_read_eeprom_byte(adev,
					 ACX100_EEPROM_ID_OFFSET + i,
					 &buffer[i])) {
			printk("acx: reading EEPROM FAILED\n");
			break;
		}
	}

	for (i = 0; i < VEC_SIZE(device_ids); i++) {
		if (!memcmp(&buffer, device_ids[i].id, CARD_EEPROM_ID_SIZE)) {
			if (device_ids[i].descr) {
				printk("acx: EEPROM card ID string check "
					"found %s card ID: is this %s?\n",
					device_ids[i].descr, device_ids[i].type);
			}
			break;
		}
	}
	if (i == VEC_SIZE(device_ids)) {
		printk("acx: EEPROM card ID string check found "
			"unknown card: expected 'Global', got '%.*s\'. "
			"Please report\n", CARD_EEPROM_ID_SIZE, buffer);
	}
}
#endif /* NONESSENTIAL_FEATURES */

/***********************************************************************
** acxmem_free_desc_queues
**
** Releases the queues that have been allocated, the
** others have been initialised to NULL so this
** function can be used if only part of the queues were allocated.
*/

void
acxmem_free_desc_queues(acx_device_t *adev)
{
#define ACX_FREE_QUEUE(size, ptr, phyaddr) \
        if (ptr) { \
                kfree(ptr); \
                ptr = NULL; \
                size = 0; \
        }

	FN_ENTER;

	ACX_FREE_QUEUE(adev->txhostdesc_area_size, adev->txhostdesc_start, adev->txhostdesc_startphy);
	ACX_FREE_QUEUE(adev->txbuf_area_size, adev->txbuf_start, adev->txbuf_startphy);

	adev->txdesc_start = NULL;

	ACX_FREE_QUEUE(adev->rxhostdesc_area_size, adev->rxhostdesc_start, adev->rxhostdesc_startphy);
	ACX_FREE_QUEUE(adev->rxbuf_area_size, adev->rxbuf_start, adev->rxbuf_startphy);

	adev->rxdesc_start = NULL;

	FN_EXIT0;
}


/***********************************************************************
** acxmem_s_delete_dma_regions
*/
static void
acxmem_s_delete_dma_regions(acx_device_t *adev)
{
	unsigned long flags;

	FN_ENTER;
	/* disable radio Tx/Rx. Shouldn't we use the firmware commands
	 * here instead? Or are we that much down the road that it's no
	 * longer possible here? */
	/*
	 * slave memory interface really doesn't like this.
	 */
	/*
	write_reg16(adev, IO_ACX_ENABLE, 0);
	*/

	acx_s_msleep(100);

	acx_lock(adev, flags);
	acxmem_free_desc_queues(adev);
	acx_unlock(adev, flags);

	FN_EXIT0;
}


/***********************************************************************
** acxmem_e_probe
**
** Probe routine called when a PCI device w/ matching ID is found.
** Here's the sequence:
**   - Allocate the PCI resources.
**   - Read the PCMCIA attribute memory to make sure we have a WLAN card
**   - Reset the MAC
**   - Initialize the dev and wlan data
**   - Initialize the MAC
**
** pdev	- ptr to pci device structure containing info about pci configuration
** id	- ptr to the device id entry that matched this device
*/
static const u16
IO_ACX100[] =
{
	0x0000, /* IO_ACX_SOFT_RESET */

	0x0014, /* IO_ACX_SLV_MEM_ADDR */
	0x0018, /* IO_ACX_SLV_MEM_DATA */
	0x001c, /* IO_ACX_SLV_MEM_CTL */
	0x0020, /* IO_ACX_SLV_END_CTL */

	0x0034, /* IO_ACX_FEMR */

	0x007c, /* IO_ACX_INT_TRIG */
	0x0098, /* IO_ACX_IRQ_MASK */
	0x00a4, /* IO_ACX_IRQ_STATUS_NON_DES */
	0x00a8, /* IO_ACX_IRQ_STATUS_CLEAR */
	0x00ac, /* IO_ACX_IRQ_ACK */
	0x00b0, /* IO_ACX_HINT_TRIG */

	0x0104, /* IO_ACX_ENABLE */

	0x0250, /* IO_ACX_EEPROM_CTL */
	0x0254, /* IO_ACX_EEPROM_ADDR */
	0x0258, /* IO_ACX_EEPROM_DATA */
	0x025c, /* IO_ACX_EEPROM_CFG */

	0x0268, /* IO_ACX_PHY_ADDR */
	0x026c, /* IO_ACX_PHY_DATA */
	0x0270, /* IO_ACX_PHY_CTL */

	0x0290, /* IO_ACX_GPIO_OE */

	0x0298, /* IO_ACX_GPIO_OUT */

	0x02a4, /* IO_ACX_CMD_MAILBOX_OFFS */
	0x02a8, /* IO_ACX_INFO_MAILBOX_OFFS */
	0x02ac, /* IO_ACX_EEPROM_INFORMATION */

	0x02d0, /* IO_ACX_EE_START */
	0x02d4, /* IO_ACX_SOR_CFG */
	0x02d8 /* IO_ACX_ECPU_CTRL */
};

static const u16
IO_ACX111[] =
{
	0x0000, /* IO_ACX_SOFT_RESET */

	0x0014, /* IO_ACX_SLV_MEM_ADDR */
	0x0018, /* IO_ACX_SLV_MEM_DATA */
	0x001c, /* IO_ACX_SLV_MEM_CTL */
	0x0020, /* IO_ACX_SLV_MEM_CP */

	0x0034, /* IO_ACX_FEMR */

	0x00b4, /* IO_ACX_INT_TRIG */
	0x00d4, /* IO_ACX_IRQ_MASK */
	/* we do mean NON_DES (0xf0), not NON_DES_MASK which is at 0xe0: */
	0x00f0, /* IO_ACX_IRQ_STATUS_NON_DES */
	0x00e4, /* IO_ACX_IRQ_STATUS_CLEAR */
	0x00e8, /* IO_ACX_IRQ_ACK */
	0x00ec, /* IO_ACX_HINT_TRIG */

	0x01d0, /* IO_ACX_ENABLE */

	0x0338, /* IO_ACX_EEPROM_CTL */
	0x033c, /* IO_ACX_EEPROM_ADDR */
	0x0340, /* IO_ACX_EEPROM_DATA */
	0x0344, /* IO_ACX_EEPROM_CFG */

	0x0350, /* IO_ACX_PHY_ADDR */
	0x0354, /* IO_ACX_PHY_DATA */
	0x0358, /* IO_ACX_PHY_CTL */

	0x0374, /* IO_ACX_GPIO_OE */

	0x037c, /* IO_ACX_GPIO_OUT */

	0x0388, /* IO_ACX_CMD_MAILBOX_OFFS */
	0x038c, /* IO_ACX_INFO_MAILBOX_OFFS */
	0x0390, /* IO_ACX_EEPROM_INFORMATION */

	0x0100, /* IO_ACX_EE_START */
	0x0104, /* IO_ACX_SOR_CFG */
	0x0108, /* IO_ACX_ECPU_CTRL */
};

static void
dummy_netdev_init(struct net_device *ndev) {}

/*
 * Most of the acx specific pieces of hardware reset.
 */
static int
acxmem_complete_hw_reset (acx_device_t *adev)
{
	acx111_ie_configoption_t co;

	/* NB: read_reg() reads may return bogus data before reset_dev(),
	 * since the firmware which directly controls large parts of the I/O
	 * registers isn't initialized yet.
	 * acx100 seems to be more affected than acx111 */
	if (OK != acxmem_s_reset_dev (adev))
	  return -1;

	if (IS_ACX100(adev)) {
		/* ACX100: configopt struct in cmd mailbox - directly after reset */
		copy_from_slavemem (adev, (u8*) &co, (u32) adev->cmd_area, sizeof (co));
	}

	if (OK != acx_s_init_mac(adev))
	  return -3;

	if (IS_ACX111(adev)) {
		/* ACX111: configopt struct needs to be queried after full init */
		acx_s_interrogate(adev, &co, ACX111_IE_CONFIG_OPTIONS);
	}

	/*
	 * Set up transmit buffer administration
	 */
	init_acx_txbuf (adev);

	/*
	 * Windows driver writes 0x01000000 to register 0x288, RADIO_CTL, if the form factor
	 * is 3.  It also write protects the EEPROM by writing 1<<9 to GPIO_OUT
	 */
	if (adev->form_factor == 3) {
	  set_regbits (adev, 0x288, 0x01000000);
	  set_regbits (adev, 0x298, 1<<9);
	}

/* TODO: merge them into one function, they are called just once and are the same for pci & usb */
	if (OK != acxmem_read_eeprom_byte(adev, 0x05, &adev->eeprom_version))
	  return -2;

	acx_s_parse_configoption(adev, &co);
	acx_s_get_firmware_version(adev); /* needs to be after acx_s_init_mac() */
	acx_display_hardware_details(adev);

	return 0;
}

static int acx_init_netdev(struct net_device *ndev, struct device *dev, int base_addr, int addr_size, int irq)
{
	const char *chip_name;
	int result = -EIO;
	int err;
	u8 chip_type;
	acx_device_t *adev = NULL;
 
	FN_ENTER;

	/* FIXME: prism54 calls pci_set_mwi() here,
	 * should we do/support the same? */

	/* chiptype is u8 but id->driver_data is ulong
	** Works for now (possible values are 1 and 2) */
	chip_type = CHIPTYPE_ACX100;
	/* acx100 and acx111 have different PCI memory regions */
	if (chip_type == CHIPTYPE_ACX100) {
		chip_name = "ACX100";
	} else if (chip_type == CHIPTYPE_ACX111) {
		chip_name = "ACX111";
	} else {
		printk("acx: unknown chip type 0x%04X\n", chip_type);
		goto fail_unknown_chiptype;
	}

	printk("acx: found %s-based wireless network card\n", chip_name);
	log(L_ANY, "initial debug setting is 0x%04X\n", acx_debug);


	dev_set_drvdata(dev, ndev);

	ether_setup(ndev);

	ndev->irq = irq;

	ndev->base_addr = base_addr;
printk (KERN_INFO "memwinbase=%lx memwinsize=%u\n",memwin.Base,memwin.Size);
	if (addr_size == 0 || ndev->irq == 0)
		goto fail_hw_params;
	ndev->open = &acxmem_e_open;
	ndev->stop = &acxmem_e_close;
	//pdev->dev.release = &acxmem_e_release;
	ndev->hard_start_xmit = &acx_i_start_xmit;
	ndev->get_stats = &acx_e_get_stats;
#if IW_HANDLER_VERSION <= 5
	ndev->get_wireless_stats = &acx_e_get_wireless_stats;
#endif
	ndev->wireless_handlers = (struct iw_handler_def *)&acx_ioctl_handler_def;
	ndev->set_multicast_list = &acxmem_i_set_multicast_list;
	ndev->tx_timeout = &acxmem_i_tx_timeout;
	ndev->change_mtu = &acx_e_change_mtu;
	ndev->watchdog_timeo = 4 * HZ;

	adev = ndev2adev(ndev);
	spin_lock_init(&adev->lock);	/* initial state: unlocked */
	spin_lock_init(&adev->txbuf_lock);
	/* We do not start with downed sem: we want PARANOID_LOCKING to work */
	sema_init(&adev->sem, 1);	/* initial state: 1 (upped) */
	/* since nobody can see new netdev yet, we can as well
	** just _presume_ that we're under sem (instead of actually taking it): */
	/* acx_sem_lock(adev); */
	adev->dev = dev;
	adev->ndev = ndev;
	adev->dev_type = DEVTYPE_MEM;
	adev->chip_type = chip_type;
	adev->chip_name = chip_name;
	adev->io = (CHIPTYPE_ACX100 == chip_type) ? IO_ACX100 : IO_ACX111;
	adev->membase = (volatile u32 *) ndev->base_addr;
	adev->iobase = (volatile u32 *) ioremap_nocache (ndev->base_addr, addr_size);
	/* to find crashes due to weird driver access
	 * to unconfigured interface (ifup) */
	adev->mgmt_timer.function = (void (*)(unsigned long))0x0000dead;

#if defined(NONESSENTIAL_FEATURES)
	acx_show_card_eeprom_id(adev);
#endif /* NONESSENTIAL_FEATURES */

#ifdef SET_MODULE_OWNER
	SET_MODULE_OWNER(ndev);
#endif
	// need to fix that @@
	SET_NETDEV_DEV(ndev, dev);

	log(L_IRQ|L_INIT, "using IRQ %d\n", ndev->irq);

	/* ok, pci setup is finished, now start initializing the card */

	if (OK != acxmem_complete_hw_reset (adev))
	  goto fail_reset;

	/*
	 * Set up default things for most of the card settings.
	 */
	acx_s_set_defaults(adev);

	/* Register the card, AFTER everything else has been set up,
	 * since otherwise an ioctl could step on our feet due to
	 * firmware operations happening in parallel or uninitialized data */
	err = register_netdev(ndev);
	if (OK != err) {
		printk("acx: register_netdev() FAILED: %d\n", err);
		goto fail_register_netdev;
	}

	acx_proc_register_entries(ndev);

	/* Now we have our device, so make sure the kernel doesn't try
	 * to send packets even though we're not associated to a network yet */
	acx_stop_queue(ndev, "on probe");
	acx_carrier_off(ndev, "on probe");

	/*
	 * Set up a default monitor type so that poor combinations of initialization
	 * sequences in monitor mode don't end up destroying the hardware type.
	 */
	adev->monitor_type = ARPHRD_ETHER;

	/*
	 * Register to receive inetaddr notifier changes.  This will allow us to
	 * catch if the user changes the MAC address of the interface.
	 */
	register_netdevice_notifier(&acx_netdev_notifier);

	/* after register_netdev() userspace may start working with dev
	 * (in particular, on other CPUs), we only need to up the sem */
	/* acx_sem_unlock(adev); */

	printk("acx "ACX_RELEASE": net device %s, driver compiled "
		"against wireless extensions %d and Linux %s\n",
		ndev->name, WIRELESS_EXT, UTS_RELEASE);

#if CMD_DISCOVERY
	great_inquisitor(adev);
#endif

	result = OK;
	goto done;

	/* error paths: undo everything in reverse order... */

fail_register_netdev:

	acxmem_s_delete_dma_regions(adev);

fail_reset:
fail_hw_params:
	free_netdev(ndev);
fail_unknown_chiptype:


done:
	FN_EXIT1(result);
	return result;
}


/***********************************************************************
** acxmem_e_remove
**
** Shut device down (if not hot unplugged)
** and deallocate PCI resources for the acx chip.
**
** pdev - ptr to PCI device structure containing info about pci configuration
*/
static int __devexit
acxmem_e_remove(struct pcmcia_device *link)
{
	struct net_device *ndev;
	acx_device_t *adev;
	unsigned long flags;

	FN_ENTER;

	ndev = ((local_info_t*)link->priv)->ndev;
	if (!ndev) {
		log(L_DEBUG, "%s: card is unused. Skipping any release code\n",
			__func__);
		goto end;
	}

	adev = ndev2adev(ndev);

	/* If device wasn't hot unplugged... */
	if (adev_present(adev)) {

		acx_sem_lock(adev);

		/* disable both Tx and Rx to shut radio down properly */
		acx_s_issue_cmd(adev, ACX1xx_CMD_DISABLE_TX, NULL, 0);
		acx_s_issue_cmd(adev, ACX1xx_CMD_DISABLE_RX, NULL, 0);

#ifdef REDUNDANT
		/* put the eCPU to sleep to save power
		 * Halting is not possible currently,
		 * since not supported by all firmware versions */
		acx_s_issue_cmd(adev, ACX100_CMD_SLEEP, NULL, 0);
#endif
		acx_lock(adev, flags);

		/* disable power LED to save power :-) */
		log(L_INIT, "switching off power LED to save power\n");
		acxmem_l_power_led(adev, 0);

		/* stop our eCPU */
		if (IS_ACX111(adev)) {
			/* FIXME: does this actually keep halting the eCPU?
			 * I don't think so...
			 */
			acxmem_l_reset_mac(adev);
		} else {
			u16 temp;

			/* halt eCPU */
			temp = read_reg16(adev, IO_ACX_ECPU_CTRL) | 0x1;
			write_reg16(adev, IO_ACX_ECPU_CTRL, temp);
			write_flush(adev);
		}

		acx_unlock(adev, flags);

		acx_sem_unlock(adev);
	}


	/*
	 * Unregister the notifier chain
	 */
	unregister_netdevice_notifier(&acx_netdev_notifier);

	/* unregister the device to not let the kernel
	 * (e.g. ioctls) access a half-deconfigured device
	 * NB: this will cause acxmem_e_close() to be called,
	 * thus we shouldn't call it under sem! */
	log(L_INIT, "removing device %s\n", ndev->name);
	unregister_netdev(ndev);

	/* unregister_netdev ensures that no references to us left.
	 * For paranoid reasons we continue to follow the rules */
	acx_sem_lock(adev);

	if (adev->dev_state_mask & ACX_STATE_IFACE_UP) {
		acxmem_s_down(ndev);
		CLEAR_BIT(adev->dev_state_mask, ACX_STATE_IFACE_UP);
	}

	acx_proc_unregister_entries(ndev);

	acxmem_s_delete_dma_regions(adev);

	/* finally, clean up PCI bus state */
	if (adev->iobase) iounmap((void *)adev->iobase);

	acx_sem_unlock(adev);

	/* Free netdev (quite late,
	 * since otherwise we might get caught off-guard
	 * by a netdev timeout handler execution
	 * expecting to see a working dev...) */
	free_netdev(ndev);

	printk ("e_remove done\n");
end:
	FN_EXIT0;

	return 0;
}


/***********************************************************************
** TODO: PM code needs to be fixed / debugged / tested.
*/
#ifdef CONFIG_PM
static int
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 11)
acxmem_e_suspend( struct net_device *ndev, pm_message_t state)
#else
acxmem_e_suspend( struct net_device *ndev, u32 state)
#endif
{
	FN_ENTER;
	acx_device_t *adev;
	printk("acx: suspend handler is experimental!\n");
	printk("sus: dev %p\n", ndev);

	if (!netif_running(ndev))
		goto end;
	// @@ need to get it from link or something like that
	adev = ndev2adev(ndev);
	printk("sus: adev %p\n", adev);

	acx_sem_lock(adev);

	netif_device_detach(adev->ndev);	/* this one cannot sleep */
	acxmem_s_down(adev->ndev);
	/* down() does not set it to 0xffff, but here we really want that */
	write_reg16(adev, IO_ACX_IRQ_MASK, 0xffff);
	write_reg16(adev, IO_ACX_FEMR, 0x0);
	acxmem_s_delete_dma_regions(adev);

	/*
	 * Turn the ACX chip off.
	 */

	acx_sem_unlock(adev);
end:
	FN_EXIT0;
	return OK;
}


static void
fw_resumer(struct work_struct *notused)
{
	acx_device_t *adev;
	struct net_device *ndev = resume_ndev;

	printk("acx: resume handler is experimental!\n");
	printk("rsm: got dev %p\n", ndev);

	if (!netif_running(ndev))
		return;

	adev = ndev2adev(ndev);
	printk("rsm: got adev %p\n", adev);

	acx_sem_lock(adev);

	/*
	 * Turn on the ACX.
	 */

	acxmem_complete_hw_reset (adev);

	/*
	 * done by acx_s_set_defaults for initial startup
	 */
	acxmem_set_interrupt_mask(adev);

	printk ("rsm: bringing up interface\n");
	SET_BIT (adev->set_mask, GETSET_ALL);
	acxmem_s_up(ndev);
	printk("rsm: acx up done\n");

	/* now even reload all card parameters as they were before suspend,
	 * and possibly be back in the network again already :-) 
	 */
	/* - most settings updated in acxmem_s_up()
	if (ACX_STATE_IFACE_UP & adev->dev_state_mask) {
		adev->set_mask = GETSET_ALL;
		acx_s_update_card_settings(adev);
		printk("rsm: settings updated\n");
	}
	*/
	netif_device_attach(ndev);
	printk("rsm: device attached\n");

	acx_sem_unlock(adev);
}

DECLARE_WORK( fw_resume_work, fw_resumer );

static int
acxmem_e_resume(struct pcmcia_device *link)
{
	FN_ENTER;

	//resume_pdev = pdev;
	schedule_work( &fw_resume_work );

	FN_EXIT0;
	return OK;
}
#endif /* CONFIG_PM */


/***********************************************************************
** acxmem_s_up
**
** This function is called by acxmem_e_open (when ifconfig sets the device as up)
**
** Side effects:
** - Enables on-card interrupt requests
** - calls acx_s_start
*/

static void
enable_acx_irq(acx_device_t *adev)
{
	FN_ENTER;
	write_reg16(adev, IO_ACX_IRQ_MASK, adev->irq_mask);
	write_reg16(adev, IO_ACX_FEMR, 0x8000);
	adev->irqs_active = 1;
	FN_EXIT0;
}

static void
acxmem_s_up(struct net_device *ndev)
{
	acx_device_t *adev = ndev2adev(ndev);
	unsigned long flags;

	FN_ENTER;

	acx_lock(adev, flags);
	enable_acx_irq(adev);
	acx_unlock(adev, flags);

	/* acx fw < 1.9.3.e has a hardware timer, and older drivers
	** used to use it. But we don't do that anymore, our OS
	** has reliable software timers */
	init_timer(&adev->mgmt_timer);
	adev->mgmt_timer.function = acx_i_timer;
	adev->mgmt_timer.data = (unsigned long)adev;

	/* Need to set ACX_STATE_IFACE_UP first, or else
	** timer won't be started by acx_set_status() */
	SET_BIT(adev->dev_state_mask, ACX_STATE_IFACE_UP);
	switch (adev->mode) {
	case ACX_MODE_0_ADHOC:
	case ACX_MODE_2_STA:
		/* actual scan cmd will happen in start() */
		acx_set_status(adev, ACX_STATUS_1_SCANNING); break;
	case ACX_MODE_3_AP:
	case ACX_MODE_MONITOR:
		acx_set_status(adev, ACX_STATUS_4_ASSOCIATED); break;
	}

	acx_s_start(adev);

	FN_EXIT0;
}


/***********************************************************************
** acxmem_s_down
**
** This disables the netdevice
**
** Side effects:
** - disables on-card interrupt request
*/

static void
disable_acx_irq(acx_device_t *adev)
{
	FN_ENTER;

	/* I guess mask is not 0xffff because acx100 won't signal
	** cmd completion then (needed for ifup).
	** Someone with acx100 please confirm */
	write_reg16(adev, IO_ACX_IRQ_MASK, adev->irq_mask_off);
	write_reg16(adev, IO_ACX_FEMR, 0x0);
	adev->irqs_active = 0;
	FN_EXIT0;
}

static void
acxmem_s_down(struct net_device *ndev)
{
	acx_device_t *adev = ndev2adev(ndev);
	unsigned long flags;

	FN_ENTER;

	/* Disable IRQs first, so that IRQs cannot race with us */
	/* then wait until interrupts have finished executing on other CPUs */
	acx_lock(adev, flags);
	disable_acx_irq(adev);
	synchronize_irq(adev->pdev->irq);
	acx_unlock(adev, flags);

	/* we really don't want to have an asynchronous tasklet disturb us
	** after something vital for its job has been shut down, so
	** end all remaining work now.
	**
	** NB: carrier_off (done by set_status below) would lead to
	** not yet fully understood deadlock in FLUSH_SCHEDULED_WORK().
	** That's why we do FLUSH first.
	**
	** NB2: we have a bad locking bug here: FLUSH_SCHEDULED_WORK()
	** waits for acx_e_after_interrupt_task to complete if it is running
	** on another CPU, but acx_e_after_interrupt_task
	** will sleep on sem forever, because it is taken by us!
	** Work around that by temporary sem unlock.
	** This will fail miserably if we'll be hit by concurrent
	** iwconfig or something in between. TODO! */
	acx_sem_unlock(adev);
	FLUSH_SCHEDULED_WORK();
	acx_sem_lock(adev);

	/* This is possible:
	** FLUSH_SCHEDULED_WORK -> acx_e_after_interrupt_task ->
	** -> set_status(ASSOCIATED) -> wake_queue()
	** That's why we stop queue _after_ FLUSH_SCHEDULED_WORK
	** lock/unlock is just paranoia, maybe not needed */
	acx_lock(adev, flags);
	acx_stop_queue(ndev, "on ifdown");
	acx_set_status(adev, ACX_STATUS_0_STOPPED);
	acx_unlock(adev, flags);

	/* kernel/timer.c says it's illegal to del_timer_sync()
	** a timer which restarts itself. We guarantee this cannot
	** ever happen because acx_i_timer() never does this if
	** status is ACX_STATUS_0_STOPPED */
	del_timer_sync(&adev->mgmt_timer);

	FN_EXIT0;
}


/***********************************************************************
** acxmem_e_open
**
** Called as a result of SIOCSIFFLAGS ioctl changing the flags bit IFF_UP
** from clear to set. In other words: ifconfig up.
**
** Returns:
**	0	success
**	>0	f/w reported error
**	<0	driver reported error
*/
static int
acxmem_e_open(struct net_device *ndev)
{
	acx_device_t *adev = ndev2adev(ndev);
	int result = OK;

	FN_ENTER;

	acx_sem_lock(adev);

	acx_init_task_scheduler(adev);

/* TODO: pci_set_power_state(pdev, PCI_D0); ? */

#if 0
	/* request shared IRQ handler */
	if (request_irq(ndev->irq, acxmem_i_interrupt, SA_INTERRUPT, ndev->name, ndev)) {
		printk("%s: request_irq FAILED\n", ndev->name);
		result = -EAGAIN;
		goto done;
	}
	set_irq_type (ndev->irq, IRQT_FALLING);
	log(L_DEBUG|L_IRQ, "request_irq %d successful\n", ndev->irq);
#endif

	/* ifup device */
	acxmem_s_up(ndev);

	/* We don't currently have to do anything else.
	 * The setup of the MAC should be subsequently completed via
	 * the mlme commands.
	 * Higher layers know we're ready from dev->start==1 and
	 * dev->tbusy==0.  Our rx path knows to pass up received/
	 * frames because of dev->flags&IFF_UP is true.
	 */
done:
	acx_sem_unlock(adev);

	FN_EXIT1(result);
	return result;
}


/***********************************************************************
** acxmem_e_close
**
** Called as a result of SIOCSIIFFLAGS ioctl changing the flags bit IFF_UP
** from set to clear. I.e. called by "ifconfig DEV down"
**
** Returns:
**	0	success
**	>0	f/w reported error
**	<0	driver reported error
*/
static int
acxmem_e_close(struct net_device *ndev)
{
	acx_device_t *adev = ndev2adev(ndev);

	FN_ENTER;

	acx_sem_lock(adev);

	/* ifdown device */
	CLEAR_BIT(adev->dev_state_mask, ACX_STATE_IFACE_UP);
	if (netif_device_present(ndev)) {
		acxmem_s_down(ndev);
	}

	/* disable all IRQs, release shared IRQ handler */
	write_reg16(adev, IO_ACX_IRQ_MASK, 0xffff);
	write_reg16(adev, IO_ACX_FEMR, 0x0);
	free_irq(ndev->irq, ndev);

/* TODO: pci_set_power_state(pdev, PCI_D3hot); ? */

	/* We currently don't have to do anything else.
	 * Higher layers know we're not ready from dev->start==0 and
	 * dev->tbusy==1.  Our rx path knows to not pass up received
	 * frames because of dev->flags&IFF_UP is false.
	 */
	acx_sem_unlock(adev);

	log(L_INIT, "closed device\n");
	FN_EXIT0;
	return OK;
}


/***********************************************************************
** acxmem_i_tx_timeout
**
** Called from network core. Must not sleep!
*/
static void
acxmem_i_tx_timeout(struct net_device *ndev)
{
	acx_device_t *adev = ndev2adev(ndev);
	unsigned long flags;
	unsigned int tx_num_cleaned;

	FN_ENTER;

	acx_lock(adev, flags);

	/* clean processed tx descs, they may have been completely full */
	tx_num_cleaned = acxmem_l_clean_txdesc(adev);

	/* nothing cleaned, yet (almost) no free buffers available?
	 * --> clean all tx descs, no matter which status!!
	 * Note that I strongly suspect that doing emergency cleaning
	 * may confuse the firmware. This is a last ditch effort to get
	 * ANYTHING to work again...
	 *
	 * TODO: it's best to simply reset & reinit hw from scratch...
	 */
	if ((adev->tx_free <= TX_EMERG_CLEAN) && (tx_num_cleaned == 0)) {
		printk("%s: FAILED to free any of the many full tx buffers. "
			"Switching to emergency freeing. "
			"Please report!\n", ndev->name);
		acxmem_l_clean_txdesc_emergency(adev);
	}

	if (acx_queue_stopped(ndev) && (ACX_STATUS_4_ASSOCIATED == adev->status))
		acx_wake_queue(ndev, "after tx timeout");

	/* stall may have happened due to radio drift, so recalib radio */
	acx_schedule_task(adev, ACX_AFTER_IRQ_CMD_RADIO_RECALIB);

	/* do unimportant work last */
	printk("%s: tx timeout!\n", ndev->name);
	adev->stats.tx_errors++;

	acx_unlock(adev, flags);

	FN_EXIT0;
}


/***********************************************************************
** acxmem_i_set_multicast_list
** FIXME: most likely needs refinement
*/
static void
acxmem_i_set_multicast_list(struct net_device *ndev)
{
	acx_device_t *adev = ndev2adev(ndev);
	unsigned long flags;

	FN_ENTER;

	acx_lock(adev, flags);

	/* firmwares don't have allmulti capability,
	 * so just use promiscuous mode instead in this case. */
	if (ndev->flags & (IFF_PROMISC|IFF_ALLMULTI)) {
		SET_BIT(adev->rx_config_1, RX_CFG1_RCV_PROMISCUOUS);
		CLEAR_BIT(adev->rx_config_1, RX_CFG1_FILTER_ALL_MULTI);
		SET_BIT(adev->set_mask, SET_RXCONFIG);
		/* let kernel know in case *we* needed to set promiscuous */
		ndev->flags |= (IFF_PROMISC|IFF_ALLMULTI);
	} else {
		CLEAR_BIT(adev->rx_config_1, RX_CFG1_RCV_PROMISCUOUS);
		SET_BIT(adev->rx_config_1, RX_CFG1_FILTER_ALL_MULTI);
		SET_BIT(adev->set_mask, SET_RXCONFIG);
		ndev->flags &= ~(IFF_PROMISC|IFF_ALLMULTI);
	}

	/* cannot update card settings directly here, atomic context */
	acx_schedule_task(adev, ACX_AFTER_IRQ_UPDATE_CARD_CFG);

	acx_unlock(adev, flags);

	FN_EXIT0;
}


/***************************************************************
** acxmem_l_process_rxdesc
**
** Called directly and only from the IRQ handler
*/

#if !ACX_DEBUG
static inline void log_rxbuffer(const acx_device_t *adev) {}
#else
static void
log_rxbuffer(const acx_device_t *adev)
{
	register const struct rxhostdesc *rxhostdesc;
	int i;
	/* no FN_ENTER here, we don't want that */

	rxhostdesc = adev->rxhostdesc_start;
	if (unlikely(!rxhostdesc)) return;
	for (i = 0; i < RX_CNT; i++) {
		if ((rxhostdesc->Ctl_16 & cpu_to_le16(DESC_CTL_HOSTOWN))
		 && (rxhostdesc->Status & cpu_to_le32(DESC_STATUS_FULL)))
			printk("rx: buf %d full\n", i);
		rxhostdesc++;
	}
}
#endif

static void
acxmem_l_process_rxdesc(acx_device_t *adev)
{
	register rxhostdesc_t *hostdesc;
	register rxdesc_t *rxdesc;
	unsigned count, tail;
	u32 addr;
	u8 Ctl_8;

	FN_ENTER;

	if (unlikely(acx_debug & L_BUFR))
		log_rxbuffer(adev);

	/* First, have a loop to determine the first descriptor that's
	 * full, just in case there's a mismatch between our current
	 * rx_tail and the full descriptor we're supposed to handle. */
	tail = adev->rx_tail;
	count = RX_CNT;
	while (1) {
		hostdesc = &adev->rxhostdesc_start[tail];
		rxdesc = &adev->rxdesc_start[tail];
		/* advance tail regardless of outcome of the below test */
		tail = (tail + 1) % RX_CNT;

		/*
		 * Unlike the PCI interface, where the ACX can write directly to
		 * the host descriptors, on the slave memory interface we have to
		 * pull these.  All we really need to do is check the Ctl_8 field
		 * in the rx descriptor on the ACX, which should be 0x11000000 if
		 * we should process it.
		 */
		Ctl_8 = hostdesc->Ctl_16 = read_slavemem8 (adev, (u32) &(rxdesc->Ctl_8));
                if ((Ctl_8 & DESC_CTL_HOSTOWN) &&
		    (Ctl_8 & DESC_CTL_ACXDONE))
		  break;		/* found it! */

		if (unlikely(!--count))	/* hmm, no luck: all descs empty, bail out */
			goto end;
	}

	/* now process descriptors, starting with the first we figured out */
	while (1) {
		log(L_BUFR, "rx: tail=%u Ctl_8=%02X\n",	tail, Ctl_8);
		/*
		 * If the ACX has CTL_RECLAIM set on this descriptor there
		 * is no buffer associated; it just wants us to tell it to
		 * reclaim the memory.
		 */
		if (!(Ctl_8 & DESC_CTL_RECLAIM)) {

		  /*
		 * slave interface - pull data now
		 */
		hostdesc->length = read_slavemem16 (adev, (u32) &(rxdesc->total_length));

		/*
		 * hostdesc->data is an rxbuffer_t, which includes header information,
		 * but the length in the data packet doesn't.  The header information
		 * takes up an additional 12 bytes, so add that to the length we copy.
		 */
		addr = read_slavemem32 (adev, (u32) &(rxdesc->ACXMemPtr));
		  if (addr) {
		    /*
		     * How can &(rxdesc->ACXMemPtr) above ever be zero?  Looks like we
		     * get that now and then - try to trap it for debug.
		     */
		    if (addr & 0xffff0000) {
		      printk("rxdesc 0x%08x\n", (u32) rxdesc);
		      dump_acxmem (adev, 0, 0x10000);
		      panic ("Bad access!");
		    }
		  chaincopy_from_slavemem (adev, (u8 *) hostdesc->data, addr,
					   hostdesc->length +
					   (u32) &((rxbuffer_t *)0)->hdr_a3);
		acx_l_process_rxbuf(adev, hostdesc->data);
		  }
		}
		else {
		  printk ("rx reclaim only!\n");
		}

		hostdesc->Status = 0;

		/*
		 * Let the ACX know we're done.
		 */
		CLEAR_BIT (Ctl_8, DESC_CTL_HOSTOWN);
                SET_BIT (Ctl_8, DESC_CTL_HOSTDONE);
		SET_BIT (Ctl_8, DESC_CTL_RECLAIM);
		write_slavemem8 (adev, (u32) &rxdesc->Ctl_8, Ctl_8);

		/*
		 * Now tell the ACX we've finished with the receive buffer so 
		 * it can finish the reclaim.
		 */
		write_reg16 (adev, IO_ACX_INT_TRIG, INT_TRIG_RXPRC);

		/* ok, descriptor is handled, now check the next descriptor */
		hostdesc = &adev->rxhostdesc_start[tail];
		rxdesc = &adev->rxdesc_start[tail];

		Ctl_8 = hostdesc->Ctl_16 = read_slavemem8 (adev, (u32) &(rxdesc->Ctl_8));

		/* if next descriptor is empty, then bail out */
		if (!(Ctl_8 & DESC_CTL_HOSTOWN) || !(Ctl_8 & DESC_CTL_ACXDONE))
			break;

		tail = (tail + 1) % RX_CNT;
	}
end:
	adev->rx_tail = tail;
	FN_EXIT0;
}


/***********************************************************************
** acxmem_i_interrupt
**
** IRQ handler (atomic context, must not sleep, blah, blah)
*/

/* scan is complete. all frames now on the receive queue are valid */
#define INFO_SCAN_COMPLETE      0x0001
#define INFO_WEP_KEY_NOT_FOUND  0x0002
/* hw has been reset as the result of a watchdog timer timeout */
#define INFO_WATCH_DOG_RESET    0x0003
/* failed to send out NULL frame from PS mode notification to AP */
/* recommended action: try entering 802.11 PS mode again */
#define INFO_PS_FAIL            0x0004
/* encryption/decryption process on a packet failed */
#define INFO_IV_ICV_FAILURE     0x0005

/* Info mailbox format:
2 bytes: type
2 bytes: status
more bytes may follow
    rumors say about status:
	0x0000 info available (set by hw)
	0x0001 information received (must be set by host)
	0x1000 info available, mailbox overflowed (messages lost) (set by hw)
    but in practice we've seen:
	0x9000 when we did not set status to 0x0001 on prev message
	0x1001 when we did set it
	0x0000 was never seen
    conclusion: this is really a bitfield:
    0x1000 is 'info available' bit
    'mailbox overflowed' bit is 0x8000, not 0x1000
    value of 0x0000 probably means that there are no messages at all
    P.S. I dunno how in hell hw is supposed to notice that messages are lost -
    it does NOT clear bit 0x0001, and this bit will probably stay forever set
    after we set it once. Let's hope this will be fixed in firmware someday
*/

static void
handle_info_irq(acx_device_t *adev)
{
#if ACX_DEBUG
	static const char * const info_type_msg[] = {
		"(unknown)",
		"scan complete",
		"WEP key not found",
		"internal watchdog reset was done",
		"failed to send powersave (NULL frame) notification to AP",
		"encrypt/decrypt on a packet has failed",
		"TKIP tx keys disabled",
		"TKIP rx keys disabled",
		"TKIP rx: key ID not found",
		"???",
		"???",
		"???",
		"???",
		"???",
		"???",
		"???",
		"TKIP IV value exceeds thresh"
	};
#endif
	u32 info_type, info_status;

	info_type = read_slavemem32 (adev, (u32) adev->info_area);

	info_status = (info_type >> 16);
	info_type = (u16)info_type;

	/* inform fw that we have read this info message */
	write_slavemem32(adev, (u32) adev->info_area, info_type | 0x00010000);
	write_reg16(adev, IO_ACX_INT_TRIG, INT_TRIG_INFOACK);
	write_flush(adev);

	log(L_CTL, "info_type:%04X info_status:%04X\n",
			info_type, info_status);

	log(L_IRQ, "got Info IRQ: status %04X type %04X: %s\n",
		info_status, info_type,
		info_type_msg[(info_type >= VEC_SIZE(info_type_msg)) ?
				0 : info_type]
	);
}


static void
log_unusual_irq(u16 irqtype) {
	/*
	if (!printk_ratelimit())
		return;
	*/

	printk("acx: got");
	if (irqtype & HOST_INT_TX_XFER) {
		printk(" Tx_Xfer");
	}
	if (irqtype & HOST_INT_RX_COMPLETE) {
		printk(" Rx_Complete");
	}
	if (irqtype & HOST_INT_DTIM) {
		printk(" DTIM");
	}
	if (irqtype & HOST_INT_BEACON) {
		printk(" Beacon");
	}
	if (irqtype & HOST_INT_TIMER) {
		log(L_IRQ, " Timer");
	}
	if (irqtype & HOST_INT_KEY_NOT_FOUND) {
		printk(" Key_Not_Found");
	}
	if (irqtype & HOST_INT_IV_ICV_FAILURE) {
		printk(" IV_ICV_Failure (crypto)");
	}
		/* HOST_INT_CMD_COMPLETE  */
		/* HOST_INT_INFO          */
	if (irqtype & HOST_INT_OVERFLOW) {
		printk(" Overflow");
	}
	if (irqtype & HOST_INT_PROCESS_ERROR) {
		printk(" Process_Error");
	}
		/* HOST_INT_SCAN_COMPLETE */
	if (irqtype & HOST_INT_FCS_THRESHOLD) {
		printk(" FCS_Threshold");
	}
	if (irqtype & HOST_INT_UNKNOWN) {
		printk(" Unknown");
	}
	printk(" IRQ(s)\n");
}


static void
update_link_quality_led(acx_device_t *adev)
{
	int qual;

	qual = acx_signal_determine_quality(adev->wstats.qual.level, adev->wstats.qual.noise);
	if (qual > adev->brange_max_quality)
		qual = adev->brange_max_quality;

	if (time_after(jiffies, adev->brange_time_last_state_change +
				(HZ/2 - HZ/2 * (unsigned long)qual / adev->brange_max_quality ) )) {
		acxmem_l_power_led(adev, (adev->brange_last_state == 0));
		adev->brange_last_state ^= 1; /* toggle */
		adev->brange_time_last_state_change = jiffies;
	}
}


#define MAX_IRQLOOPS_PER_JIFFY  (20000/HZ) /* a la orinoco.c */

static irqreturn_t
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 19)
acxmem_i_interrupt(int irq, void *dev_id)
#else
acxmwm_i_interrupt(int irq, void *dev_id, struct pt_regs *regs)
#endif
{
	acx_device_t *adev;
	unsigned long flags;
	unsigned int irqcount = MAX_IRQLOOPS_PER_JIFFY;
	register u16 irqtype;
	u16 unmasked;

	adev = ndev2adev((struct net_device*)dev_id);

	/* LOCKING: can just spin_lock() since IRQs are disabled anyway.
	 * I am paranoid */
	acx_lock(adev, flags);

	unmasked = read_reg16(adev, IO_ACX_IRQ_STATUS_CLEAR);
	if (unlikely(0xffff == unmasked)) {
		/* 0xffff value hints at missing hardware,
		 * so don't do anything.
		 * Not very clean, but other drivers do the same... */
		log(L_IRQ, "IRQ type:FFFF - device removed? IRQ_NONE\n");
		goto none;
	}

	/* We will check only "interesting" IRQ types */
	irqtype = unmasked & ~adev->irq_mask;
	if (!irqtype) {
		/* We are on a shared IRQ line and it wasn't our IRQ */
		log(L_IRQ, "IRQ type:%04X, mask:%04X - all are masked, IRQ_NONE\n",
			unmasked, adev->irq_mask);
		goto none;
	}

	/* Done here because IRQ_NONEs taking three lines of log
	** drive me crazy */
	FN_ENTER;

#define IRQ_ITERATE 1
#if IRQ_ITERATE
if (jiffies != adev->irq_last_jiffies) {
	adev->irq_loops_this_jiffy = 0;
	adev->irq_last_jiffies = jiffies;
}

/* safety condition; we'll normally abort loop below
 * in case no IRQ type occurred */
while (likely(--irqcount)) {
#endif
	/* ACK all IRQs ASAP */
	write_reg16(adev, IO_ACX_IRQ_ACK, 0xffff);

	log(L_IRQ, "IRQ type:%04X, mask:%04X, type & ~mask:%04X\n",
				unmasked, adev->irq_mask, irqtype);

	/* Handle most important IRQ types first */
	if (irqtype & HOST_INT_RX_DATA) {
		log(L_IRQ, "got Rx_Data IRQ\n");
		acxmem_l_process_rxdesc(adev);
	}
	if (irqtype & HOST_INT_TX_COMPLETE) {
		log(L_IRQ, "got Tx_Complete IRQ\n");
		/* don't clean up on each Tx complete, wait a bit
		 * unless we're going towards full, in which case
		 * we do it immediately, too (otherwise we might lockup
		 * with a full Tx buffer if we go into
		 * acxmem_l_clean_txdesc() at a time when we won't wakeup
		 * the net queue in there for some reason...) */
		if (adev->tx_free <= TX_START_CLEAN) {
#if TX_CLEANUP_IN_SOFTIRQ
			acx_schedule_task(adev, ACX_AFTER_IRQ_TX_CLEANUP);
#else
			acxmem_l_clean_txdesc(adev);
#endif
		}
	}

	/* Less frequent ones */
	if (irqtype & (0
		| HOST_INT_CMD_COMPLETE
		| HOST_INT_INFO
		| HOST_INT_SCAN_COMPLETE
	)) {
		if (irqtype & HOST_INT_CMD_COMPLETE) {
			log(L_IRQ, "got Command_Complete IRQ\n");
			/* save the state for the running issue_cmd() */
			SET_BIT(adev->irq_status, HOST_INT_CMD_COMPLETE);
		}
		if (irqtype & HOST_INT_INFO) {
			handle_info_irq(adev);
		}
		if (irqtype & HOST_INT_SCAN_COMPLETE) {
			log(L_IRQ, "got Scan_Complete IRQ\n");
			/* need to do that in process context */
			acx_schedule_task(adev, ACX_AFTER_IRQ_COMPLETE_SCAN);
			/* remember that fw is not scanning anymore */
			SET_BIT(adev->irq_status, HOST_INT_SCAN_COMPLETE);
		}
	}

	/* These we just log, but either they happen rarely
	 * or we keep them masked out */
	if (irqtype & (0
		/* | HOST_INT_RX_DATA */
		/* | HOST_INT_TX_COMPLETE   */
		| HOST_INT_TX_XFER
		| HOST_INT_RX_COMPLETE
		| HOST_INT_DTIM
		| HOST_INT_BEACON
		| HOST_INT_TIMER
		| HOST_INT_KEY_NOT_FOUND
		| HOST_INT_IV_ICV_FAILURE
		/* | HOST_INT_CMD_COMPLETE  */
		/* | HOST_INT_INFO          */
		| HOST_INT_OVERFLOW
		| HOST_INT_PROCESS_ERROR
		/* | HOST_INT_SCAN_COMPLETE */
		| HOST_INT_FCS_THRESHOLD
		| HOST_INT_UNKNOWN
	)) {
		log_unusual_irq(irqtype);
	}

#if IRQ_ITERATE
	unmasked = read_reg16(adev, IO_ACX_IRQ_STATUS_CLEAR);
	irqtype = unmasked & ~adev->irq_mask;
	/* Bail out if no new IRQ bits or if all are masked out */
	if (!irqtype)
		break;

	if (unlikely(++adev->irq_loops_this_jiffy > MAX_IRQLOOPS_PER_JIFFY)) {
		printk(KERN_ERR "acx: too many interrupts per jiffy!\n");
		/* Looks like card floods us with IRQs! Try to stop that */
		write_reg16(adev, IO_ACX_IRQ_MASK, 0xffff);
		/* This will short-circuit all future attempts to handle IRQ.
		 * We cant do much more... */
		adev->irq_mask = 0;
		break;
	}
}
#endif
	/* Routine to perform blink with range */
	if (unlikely(adev->led_power == 2))
		update_link_quality_led(adev);

/* handled: */
	/* write_flush(adev); - not needed, last op was read anyway */
	acx_unlock(adev, flags);
	FN_EXIT0;
	return IRQ_HANDLED;

none:
	acx_unlock(adev, flags);
	return IRQ_NONE;
}


/***********************************************************************
** acxmem_l_power_led
*/
void
acxmem_l_power_led(acx_device_t *adev, int enable)
{
	u16 gpio_pled =	IS_ACX111(adev) ? 0x0040 : 0x0800;

	/* A hack. Not moving message rate limiting to adev->xxx
	 * (it's only a debug message after all) */
	static int rate_limit = 0;

	if (rate_limit++ < 3)
		log(L_IOCTL, "Please report in case toggling the power "
				"LED doesn't work for your card!\n");
	if (enable)
		write_reg16(adev, IO_ACX_GPIO_OUT,
			read_reg16(adev, IO_ACX_GPIO_OUT) & ~gpio_pled);
	else
		write_reg16(adev, IO_ACX_GPIO_OUT,
			read_reg16(adev, IO_ACX_GPIO_OUT) | gpio_pled);
}


/***********************************************************************
** Ioctls
*/

/***********************************************************************
*/
int
acx111pci_ioctl_info(
	struct net_device *ndev,
	struct iw_request_info *info,
	struct iw_param *vwrq,
	char *extra)
{
#if ACX_DEBUG > 1
	acx_device_t *adev = ndev2adev(ndev);
	rxdesc_t *rxdesc;
	txdesc_t *txdesc;
	rxhostdesc_t *rxhostdesc;
	txhostdesc_t *txhostdesc;
	struct acx111_ie_memoryconfig memconf;
	struct acx111_ie_queueconfig queueconf;
	unsigned long flags;
	int i;
	char memmap[0x34];
	char rxconfig[0x8];
	char fcserror[0x8];
	char ratefallback[0x5];

	if ( !(acx_debug & (L_IOCTL|L_DEBUG)) )
		return OK;
	/* using printk() since we checked debug flag already */

	acx_sem_lock(adev);

	if (!IS_ACX111(adev)) {
		printk("acx111-specific function called "
			"with non-acx111 chip, aborting\n");
		goto end_ok;
	}

	/* get Acx111 Memory Configuration */
	memset(&memconf, 0, sizeof(memconf));
	/* BTW, fails with 12 (Write only) error code.
	** Retained for easy testing of issue_cmd error handling :) */
	printk ("Interrogating queue config\n");
	acx_s_interrogate(adev, &memconf, ACX1xx_IE_QUEUE_CONFIG);
	printk ("done with queue config\n");

	/* get Acx111 Queue Configuration */
	memset(&queueconf, 0, sizeof(queueconf));
	printk ("Interrogating mem config options\n");
	acx_s_interrogate(adev, &queueconf, ACX1xx_IE_MEMORY_CONFIG_OPTIONS);
	printk ("done with mem config options\n");

	/* get Acx111 Memory Map */
	memset(memmap, 0, sizeof(memmap));
	printk ("Interrogating mem map\n");
	acx_s_interrogate(adev, &memmap, ACX1xx_IE_MEMORY_MAP);
	printk ("done with mem map\n");

	/* get Acx111 Rx Config */
	memset(rxconfig, 0, sizeof(rxconfig));
	printk ("Interrogating rxconfig\n");
	acx_s_interrogate(adev, &rxconfig, ACX1xx_IE_RXCONFIG);
	printk ("done with queue rxconfig\n");

	/* get Acx111 fcs error count */
	memset(fcserror, 0, sizeof(fcserror));
	printk ("Interrogating fcs err count\n");
	acx_s_interrogate(adev, &fcserror, ACX1xx_IE_FCS_ERROR_COUNT);
	printk ("done with err count\n");

	/* get Acx111 rate fallback */
	memset(ratefallback, 0, sizeof(ratefallback));
	printk ("Interrogating rate fallback\n");
	acx_s_interrogate(adev, &ratefallback, ACX1xx_IE_RATE_FALLBACK);
	printk ("done with rate fallback\n");

	/* force occurrence of a beacon interrupt */
	/* TODO: comment why is this necessary */
	write_reg16(adev, IO_ACX_HINT_TRIG, HOST_INT_BEACON);

	/* dump Acx111 Mem Configuration */
	printk("dump mem config:\n"
		"data read: %d, struct size: %d\n"
		"Number of stations: %1X\n"
		"Memory block size: %1X\n"
		"tx/rx memory block allocation: %1X\n"
		"count rx: %X / tx: %X queues\n"
		"options %1X\n"
		"fragmentation %1X\n"
		"Rx Queue 1 Count Descriptors: %X\n"
		"Rx Queue 1 Host Memory Start: %X\n"
		"Tx Queue 1 Count Descriptors: %X\n"
		"Tx Queue 1 Attributes: %X\n",
		memconf.len, (int) sizeof(memconf),
		memconf.no_of_stations,
		memconf.memory_block_size,
		memconf.tx_rx_memory_block_allocation,
		memconf.count_rx_queues, memconf.count_tx_queues,
		memconf.options,
		memconf.fragmentation,
		memconf.rx_queue1_count_descs,
	acx2cpu(memconf.rx_queue1_host_rx_start),
		memconf.tx_queue1_count_descs,
		memconf.tx_queue1_attributes);

	/* dump Acx111 Queue Configuration */
	printk("dump queue head:\n"
		"data read: %d, struct size: %d\n"
		"tx_memory_block_address (from card): %X\n"
		"rx_memory_block_address (from card): %X\n"
		"rx1_queue address (from card): %X\n"
		"tx1_queue address (from card): %X\n"
		"tx1_queue attributes (from card): %X\n",
		queueconf.len, (int) sizeof(queueconf),
		queueconf.tx_memory_block_address,
		queueconf.rx_memory_block_address,
		queueconf.rx1_queue_address,
		queueconf.tx1_queue_address,
		queueconf.tx1_attributes);

	/* dump Acx111 Mem Map */
	printk("dump mem map:\n"
		"data read: %d, struct size: %d\n"
		"Code start: %X\n"
		"Code end: %X\n"
		"WEP default key start: %X\n"
		"WEP default key end: %X\n"
		"STA table start: %X\n"
		"STA table end: %X\n"
		"Packet template start: %X\n"
		"Packet template end: %X\n"
		"Queue memory start: %X\n"
		"Queue memory end: %X\n"
		"Packet memory pool start: %X\n"
		"Packet memory pool end: %X\n"
		"iobase: %p\n"
		"iobase2: %p\n",
		*((u16 *)&memmap[0x02]), (int) sizeof(memmap),
		*((u32 *)&memmap[0x04]),
		*((u32 *)&memmap[0x08]),
		*((u32 *)&memmap[0x0C]),
		*((u32 *)&memmap[0x10]),
		*((u32 *)&memmap[0x14]),
		*((u32 *)&memmap[0x18]),
		*((u32 *)&memmap[0x1C]),
		*((u32 *)&memmap[0x20]),
		*((u32 *)&memmap[0x24]),
		*((u32 *)&memmap[0x28]),
		*((u32 *)&memmap[0x2C]),
		*((u32 *)&memmap[0x30]),
		adev->iobase,
		adev->iobase2);

	/* dump Acx111 Rx Config */
	printk("dump rx config:\n"
		"data read: %d, struct size: %d\n"
		"rx config: %X\n"
		"rx filter config: %X\n",
		*((u16 *)&rxconfig[0x02]), (int) sizeof(rxconfig),
		*((u16 *)&rxconfig[0x04]),
		*((u16 *)&rxconfig[0x06]));

	/* dump Acx111 fcs error */
	printk("dump fcserror:\n"
		"data read: %d, struct size: %d\n"
		"fcserrors: %X\n",
		*((u16 *)&fcserror[0x02]), (int) sizeof(fcserror),
		*((u32 *)&fcserror[0x04]));

	/* dump Acx111 rate fallback */
	printk("dump rate fallback:\n"
		"data read: %d, struct size: %d\n"
		"ratefallback: %X\n",
		*((u16 *)&ratefallback[0x02]), (int) sizeof(ratefallback),
		*((u8 *)&ratefallback[0x04]));

	/* protect against IRQ */
	acx_lock(adev, flags);

	/* dump acx111 internal rx descriptor ring buffer */
	rxdesc = adev->rxdesc_start;

	/* loop over complete receive pool */
	if (rxdesc) for (i = 0; i < RX_CNT; i++) {
		printk("\ndump internal rxdesc %d:\n"
			"mem pos %p\n"
			"next 0x%X\n"
			"acx mem pointer (dynamic) 0x%X\n"
			"CTL (dynamic) 0x%X\n"
			"Rate (dynamic) 0x%X\n"
			"RxStatus (dynamic) 0x%X\n"
			"Mod/Pre (dynamic) 0x%X\n",
			i,
			rxdesc,
			acx2cpu(rxdesc->pNextDesc),
			acx2cpu(rxdesc->ACXMemPtr),
			rxdesc->Ctl_8,
			rxdesc->rate,
			rxdesc->error,
			rxdesc->SNR);
		rxdesc++;
	}

	/* dump host rx descriptor ring buffer */

	rxhostdesc = adev->rxhostdesc_start;

	/* loop over complete receive pool */
	if (rxhostdesc) for (i = 0; i < RX_CNT; i++) {
		printk("\ndump host rxdesc %d:\n"
			"mem pos %p\n"
			"buffer mem pos 0x%X\n"
			"buffer mem offset 0x%X\n"
			"CTL 0x%X\n"
			"Length 0x%X\n"
			"next 0x%X\n"
			"Status 0x%X\n",
			i,
			rxhostdesc,
			acx2cpu(rxhostdesc->data_phy),
			rxhostdesc->data_offset,
			le16_to_cpu(rxhostdesc->Ctl_16),
			le16_to_cpu(rxhostdesc->length),
			acx2cpu(rxhostdesc->desc_phy_next),
			rxhostdesc->Status);
		rxhostdesc++;
	}

	/* dump acx111 internal tx descriptor ring buffer */
	txdesc = adev->txdesc_start;

	/* loop over complete transmit pool */
	if (txdesc) for (i = 0; i < TX_CNT; i++) {
		printk("\ndump internal txdesc %d:\n"
			"size 0x%X\n"
			"mem pos %p\n"
			"next 0x%X\n"
			"acx mem pointer (dynamic) 0x%X\n"
			"host mem pointer (dynamic) 0x%X\n"
			"length (dynamic) 0x%X\n"
			"CTL (dynamic) 0x%X\n"
			"CTL2 (dynamic) 0x%X\n"
			"Status (dynamic) 0x%X\n"
			"Rate (dynamic) 0x%X\n",
			i,
			(int) sizeof(struct txdesc),
			txdesc,
			acx2cpu(txdesc->pNextDesc),
			acx2cpu(txdesc->AcxMemPtr),
			acx2cpu(txdesc->HostMemPtr),
			le16_to_cpu(txdesc->total_length),
			txdesc->Ctl_8,
			txdesc->Ctl2_8, txdesc->error,
			txdesc->u.r1.rate);
		txdesc = advance_txdesc(adev, txdesc, 1);
	}

	/* dump host tx descriptor ring buffer */

	txhostdesc = adev->txhostdesc_start;

	/* loop over complete host send pool */
	if (txhostdesc) for (i = 0; i < TX_CNT * 2; i++) {
		printk("\ndump host txdesc %d:\n"
			"mem pos %p\n"
			"buffer mem pos 0x%X\n"
			"buffer mem offset 0x%X\n"
			"CTL 0x%X\n"
			"Length 0x%X\n"
			"next 0x%X\n"
			"Status 0x%X\n",
			i,
			txhostdesc,
			acx2cpu(txhostdesc->data_phy),
			txhostdesc->data_offset,
			le16_to_cpu(txhostdesc->Ctl_16),
			le16_to_cpu(txhostdesc->length),
			acx2cpu(txhostdesc->desc_phy_next),
			le32_to_cpu(txhostdesc->Status));
		txhostdesc++;
	}

	/* write_reg16(adev, 0xb4, 0x4); */

	acx_unlock(adev, flags);
end_ok:

	acx_sem_unlock(adev);
#endif /* ACX_DEBUG */
	return OK;
}


/***********************************************************************
*/
int
acx100mem_ioctl_set_phy_amp_bias(
	struct net_device *ndev,
	struct iw_request_info *info,
	struct iw_param *vwrq,
	char *extra)
{
	acx_device_t *adev = ndev2adev(ndev);
	unsigned long flags;
	u16 gpio_old;

	if (!IS_ACX100(adev)) {
		/* WARNING!!!
		 * Removing this check *might* damage
		 * hardware, since we're tweaking GPIOs here after all!!!
		 * You've been warned...
		 * WARNING!!! */
		printk("acx: sorry, setting bias level for non-acx100 "
			"is not supported yet\n");
		return OK;
	}

	if (*extra > 7)	{
		printk("acx: invalid bias parameter, range is 0-7\n");
		return -EINVAL;
	}

	acx_sem_lock(adev);

	/* Need to lock accesses to [IO_ACX_GPIO_OUT]:
	 * IRQ handler uses it to update LED */
	acx_lock(adev, flags);
	gpio_old = read_reg16(adev, IO_ACX_GPIO_OUT);
	write_reg16(adev, IO_ACX_GPIO_OUT, (gpio_old & 0xf8ff) | ((u16)*extra << 8));
	acx_unlock(adev, flags);

	log(L_DEBUG, "gpio_old: 0x%04X\n", gpio_old);
	printk("%s: PHY power amplifier bias: old:%d, new:%d\n",
		ndev->name,
		(gpio_old & 0x0700) >> 8, (unsigned char)*extra);

	acx_sem_unlock(adev);

	return OK;
}

/***************************************************************
** acxmem_l_alloc_tx
** Actually returns a txdesc_t* ptr
**
** FIXME: in case of fragments, should allocate multiple descrs
** after figuring out how many we need and whether we still have
** sufficiently many.
*/
tx_t*
acxmem_l_alloc_tx(acx_device_t *adev)
{
	struct txdesc *txdesc;
	unsigned head;
	u8 ctl8;
	static int txattempts = 0;

	FN_ENTER;

	if (unlikely(!adev->tx_free)) {
		printk("acx: BUG: no free txdesc left\n");
		/*
		 * Probably the ACX ignored a transmit attempt and now there's a packet
		 * sitting in the queue we think should be transmitting but the ACX doesn't
		 * know about.
		 * On the first pass, send the ACX a TxProc interrupt to try moving
		 * things along, and if that doesn't work (ie, we get called again) completely
		 * flush the transmit queue.
		 */
		if (txattempts < 10) {
		  txattempts++;
		  printk ("acx: trying to wake up ACX\n");
		  write_reg16(adev, IO_ACX_INT_TRIG, INT_TRIG_TXPRC);
		  write_flush(adev);		}
		else  {
		  txattempts = 0;
		  printk ("acx: flushing transmit queue.\n");
		  acxmem_l_clean_txdesc_emergency (adev);
		}
		txdesc = NULL;
		goto end;
	}

	/*
	 * Make a quick check to see if there is transmit buffer space on
	 * the ACX.  This can't guarantee there is enough space for the packet
	 * since we don't yet know how big it is, but it will prevent at least some
	 * annoyances.
	 */
	if (!adev->acx_txbuf_blocks_free) {
	  txdesc = NULL;
	  goto end;
	}

	head = adev->tx_head;
	/*
	 * txdesc points to ACX memory
	 */
	txdesc = get_txdesc(adev, head);
	ctl8 = read_slavemem8 (adev, (u32) &(txdesc->Ctl_8));

	/* 
	 * If we don't own the buffer (HOSTOWN) it is certainly not free; however,
	 * we may have previously thought we had enough memory to send
	 * a packet, allocated the buffer then gave up when we found not enough
	 * transmit buffer space on the ACX. In that case, HOSTOWN and
	 * ACXDONE will both be set.
	 */
	if (unlikely(DESC_CTL_HOSTOWN != (ctl8 & DESC_CTL_HOSTOWN))) {
		/* whoops, descr at current index is not free, so probably
		 * ring buffer already full */
                printk("acx: BUG: tx_head:%d Ctl8:0x%02X - failed to find "
                        "free txdesc\n", head, ctl8);
		txdesc = NULL;
		goto end;
	}

	/* Needed in case txdesc won't be eventually submitted for tx */
	write_slavemem8 (adev, (u32) &(txdesc->Ctl_8), DESC_CTL_ACXDONE_HOSTOWN);

	adev->tx_free--;
	log(L_BUFT, "tx: got desc %u, %u remain\n",
			head, adev->tx_free);
	/* Keep a few free descs between head and tail of tx ring.
	** It is not absolutely needed, just feels safer */
	if (adev->tx_free < TX_STOP_QUEUE) {
		log(L_BUF, "stop queue (%u tx desc left)\n",
				adev->tx_free);
		acx_stop_queue(adev->ndev, NULL);
	}

	/* returning current descriptor, so advance to next free one */
	adev->tx_head = (head + 1) % TX_CNT;
end:
	FN_EXIT0;

	return (tx_t*)txdesc;
}


/***************************************************************
** acxmem_l_dealloc_tx
** Clears out a previously allocatedvoid acxmem_l_dealloc_tx(tx_t *tx_opaque);
 transmit descriptor.  The ACX
** can get confused if we skip transmit descriptors in the queue,
** so when we don't need a descriptor return it to its original
** state and move the queue head pointer back.
**
*/
void
acxmem_l_dealloc_tx(acx_device_t *adev, tx_t *tx_opaque)
{
  /*
   * txdesc is the address of the descriptor on the ACX.
   */
  txdesc_t *txdesc = (txdesc_t*)tx_opaque;
  txdesc_t tmptxdesc;
  int index;

  memset (&tmptxdesc, 0, sizeof(tmptxdesc));
  tmptxdesc.Ctl_8 = DESC_CTL_HOSTOWN | DESC_CTL_FIRSTFRAG;
  tmptxdesc.u.r1.rate = 0x0a;

  /*
   * Clear out all of the transmit descriptor except for the next pointer
   */
  copy_to_slavemem (adev, (u32) &(txdesc->HostMemPtr),
		    (u8 *) &(tmptxdesc.HostMemPtr),
		    sizeof (tmptxdesc) - sizeof(tmptxdesc.pNextDesc));
  
  /*
   * This is only called immediately after we've allocated, so we should
   * be able to set the head back to this descriptor.
   */
  index = ((u8*) txdesc - (u8*)adev->txdesc_start) / adev->txdesc_size;
  printk ("acx_dealloc: moving head from %d to %d\n", adev->tx_head, index);
  adev->tx_head = index;
}


/***********************************************************************
*/
void*
acxmem_l_get_txbuf(acx_device_t *adev, tx_t* tx_opaque)
{
	return get_txhostdesc(adev, (txdesc_t*)tx_opaque)->data;
}


/***********************************************************************
** acxmem_l_tx_data
**
** Can be called from IRQ (rx -> (AP bridging or mgmt response) -> tx).
** Can be called from acx_i_start_xmit (data frames from net core).
**
** FIXME: in case of fragments, should loop over the number of
** pre-allocated tx descrs, properly setting up transfer data and
** CTL_xxx flags according to fragment number.
*/
void
acxmem_update_queue_indicator (acx_device_t *adev, int txqueue)
{
#ifdef USING_MORE_THAN_ONE_TRANSMIT_QUEUE
  u32 indicator;
  unsigned long flags;
  int count;

  /*
   * Can't handle an interrupt while we're fiddling with the ACX's lock,
   * according to TI.  The ACX is supposed to hold fw_lock for at most
   * 500ns.
   */
  local_irq_save (flags);

  /*
   * Wait for ACX to release the lock (at most 500ns).
   */
  count = 0;
  while (read_slavemem16 (adev, (u32) &(adev->acx_queue_indicator->fw_lock))
	 && (count++ < 50)) {
    ndelay (10);
  }
  if (count < 50) {

    /*
     * Take out the host lock - anything non-zero will work, so don't worry about
     * be/le
     */
    write_slavemem16 (adev, (u32) &(adev->acx_queue_indicator->host_lock), 1);

    /*
     * Avoid a race condition
     */
    count = 0;
    while (read_slavemem16 (adev, (u32) &(adev->acx_queue_indicator->fw_lock))
	   && (count++ < 50)) {
      ndelay (10);
    }

    if (count < 50) {
      /*
       * Mark the queue active
       */
      indicator = read_slavemem32 (adev, (u32) &(adev->acx_queue_indicator->indicator));
      indicator |= cpu_to_le32 (1 << txqueue);
      write_slavemem32 (adev, (u32) &(adev->acx_queue_indicator->indicator), indicator);
    }

    /*
     * Release the host lock
     */
    write_slavemem16 (adev, (u32) &(adev->acx_queue_indicator->host_lock), 0);

  }

  /*
   * Restore interrupts
   */
  local_irq_restore (flags);
#endif
}

void
acxmem_l_tx_data(acx_device_t *adev, tx_t* tx_opaque, int len)
{
  /*
   * txdesc is the address on the ACX
   */
	txdesc_t *txdesc = (txdesc_t*)tx_opaque;
	txhostdesc_t *hostdesc1, *hostdesc2;
	client_t *clt;
	u16 rate_cur;
	u8 Ctl_8, Ctl2_8;
	u32 addr;

	FN_ENTER;
	/* fw doesn't tx such packets anyhow */
	if (unlikely(len < WLAN_HDR_A3_LEN))
		goto end;

	hostdesc1 = get_txhostdesc(adev, txdesc);
	/* modify flag status in separate variable to be able to write it back
	 * in one big swoop later (also in order to have less device memory
	 * accesses) */
	Ctl_8 = read_slavemem8 (adev, (u32) &(txdesc->Ctl_8));
	Ctl2_8 = 0; /* really need to init it to 0, not txdesc->Ctl2_8, it seems */

	hostdesc2 = hostdesc1 + 1;

	/* DON'T simply set Ctl field to 0 here globally,
	 * it needs to maintain a consistent flag status (those are state flags!!),
	 * otherwise it may lead to severe disruption. Only set or reset particular
	 * flags at the exact moment this is needed... */

	/* let chip do RTS/CTS handshaking before sending
	 * in case packet size exceeds threshold */
	if (len > adev->rts_threshold)
		SET_BIT(Ctl2_8, DESC_CTL2_RTS);
	else
		CLEAR_BIT(Ctl2_8, DESC_CTL2_RTS);

	switch (adev->mode) {
	case ACX_MODE_0_ADHOC:
	case ACX_MODE_3_AP:
		clt = acx_l_sta_list_get(adev, ((wlan_hdr_t*)hostdesc1->data)->a1);
		break;
	case ACX_MODE_2_STA:
		clt = adev->ap_client;
		break;
#if 0
/* testing was done on acx111: */
	case ACX_MODE_MONITOR:
		SET_BIT(Ctl2_8, 0
/* sends CTS to self before packet */
			+ DESC_CTL2_SEQ		/* don't increase sequence field */
/* not working (looks like good fcs is still added) */
			+ DESC_CTL2_FCS		/* don't add the FCS */
/* not tested */
			+ DESC_CTL2_MORE_FRAG
/* not tested */
			+ DESC_CTL2_RETRY	/* don't increase retry field */
/* not tested */
			+ DESC_CTL2_POWER	/* don't increase power mgmt. field */
/* no effect */
			+ DESC_CTL2_WEP		/* encrypt this frame */
/* not tested */
			+ DESC_CTL2_DUR		/* don't increase duration field */
			);
		/* fallthrough */
#endif
	default: /* ACX_MODE_OFF, ACX_MODE_MONITOR */
		clt = NULL;
		break;
	}

	rate_cur = clt ? clt->rate_cur : adev->rate_bcast;
	if (unlikely(!rate_cur)) {
		printk("acx: driver bug! bad ratemask\n");
		goto end;
	}

	/* used in tx cleanup routine for auto rate and accounting: */
	put_txcr(adev, txdesc, clt, rate_cur);

	write_slavemem16 (adev, (u32) &(txdesc->total_length), cpu_to_le16(len));
	hostdesc2->length = cpu_to_le16(len - WLAN_HDR_A3_LEN);
	if (IS_ACX111(adev)) {
		/* note that if !txdesc->do_auto, txrate->cur
		** has only one nonzero bit */
		txdesc->u.r2.rate111 = cpu_to_le16(
			rate_cur
			/* WARNING: I was never able to make it work with prism54 AP.
			** It was falling down to 1Mbit where shortpre is not applicable,
			** and not working at all at "5,11 basic rates only" setting.
			** I even didn't see tx packets in radio packet capture.
			** Disabled for now --vda */
			/*| ((clt->shortpre && clt->cur!=RATE111_1) ? RATE111_SHORTPRE : 0) */
			);
#ifdef TODO_FIGURE_OUT_WHEN_TO_SET_THIS
			/* should add this to rate111 above as necessary */
			| (clt->pbcc511 ? RATE111_PBCC511 : 0)
#endif
		hostdesc1->length = cpu_to_le16(len);
	} else { /* ACX100 */
		u8 rate_100 = clt ? clt->rate_100 : adev->rate_bcast100;
		write_slavemem8 (adev, (u32) &(txdesc->u.r1.rate), rate_100);
#ifdef TODO_FIGURE_OUT_WHEN_TO_SET_THIS
		if (clt->pbcc511) {
			if (n == RATE100_5 || n == RATE100_11)
				n |= RATE100_PBCC511;
		}

		if (clt->shortpre && (clt->cur != RATE111_1))
			SET_BIT(Ctl_8, DESC_CTL_SHORT_PREAMBLE); /* set Short Preamble */
#endif
		/* set autodma and reclaim and 1st mpdu */
		SET_BIT(Ctl_8, DESC_CTL_FIRSTFRAG);

#if ACX_FRAGMENTATION
		/* SET_BIT(Ctl2_8, DESC_CTL2_MORE_FRAG); cannot set it unconditionally, needs to be set for all non-last fragments */
#endif
		hostdesc1->length = cpu_to_le16(WLAN_HDR_A3_LEN);

		/*
		 * Since we're not using autodma copy the packet data to the acx now.
		 * Even host descriptors point to the packet header, and the odd indexed
		 * descriptor following points to the packet data.
		 *
		 * The first step is to find free memory in the ACX transmit buffers.
		 * They don't necessarily map one to one with the transmit queue entries,
		 * so search through them starting just after the last one used.
		 */
		addr = allocate_acx_txbuf_space (adev, len);
		if (addr) {
		  chaincopy_to_slavemem (adev, addr, hostdesc1->data, len);
		}
		else {
		  /*
		   * Bummer.  We thought we might have enough room in the transmit
		   * buffers to send this packet, but it turns out we don't.  alloc_tx
		   * has already marked this transmit descriptor as HOSTOWN and ACXDONE,
		   * which means the ACX will hang when it gets to this descriptor unless
		   * we do something about it.  Having a bubble in the transmit queue just
		   * doesn't seem to work, so we have to reset this transmit queue entry's
		   * state to its original value and back up our head pointer to point
		   * back to this entry.
		   */
		  hostdesc1->length = 0;
		  hostdesc2->length = 0;
		  write_slavemem16 (adev, (u32) &(txdesc->total_length), 0);
		  write_slavemem8 (adev, (u32) &(txdesc->Ctl_8), DESC_CTL_HOSTOWN | DESC_CTL_FIRSTFRAG);
		  adev->tx_head  = ((u8*) txdesc - (u8*) adev->txdesc_start) / adev->txdesc_size;
		  goto end;
		}
		/*
		 * Tell the ACX where the packet is.
		 */
		write_slavemem32 (adev, (u32) &(txdesc->AcxMemPtr), addr);

	}
	/* don't need to clean ack/rts statistics here, already
	 * done on descr cleanup */

	/* clears HOSTOWN and ACXDONE bits, thus telling that the descriptors
	 * are now owned by the acx100; do this as LAST operation */
	CLEAR_BIT(Ctl_8, DESC_CTL_ACXDONE_HOSTOWN);
	/* flush writes before we release hostdesc to the adapter here */
	//wmb();

	/* write back modified flags */
	/*
	 * At this point Ctl_8 should just be FIRSTFRAG
	 */
	write_slavemem8 (adev, (u32) &(txdesc->Ctl2_8),Ctl2_8);
	write_slavemem8 (adev, (u32) &(txdesc->Ctl_8), Ctl_8);
	/* unused: txdesc->tx_time = cpu_to_le32(jiffies); */

	/*
	 * Update the queue indicator to say there's data on the first queue.
	 */
	acxmem_update_queue_indicator (adev, 0);

	/* flush writes before we tell the adapter that it's its turn now */
	mmiowb();
	write_reg16(adev, IO_ACX_INT_TRIG, INT_TRIG_TXPRC);
	write_flush(adev);

	/* log the packet content AFTER sending it,
	 * in order to not delay sending any further than absolutely needed
	 * Do separate logs for acx100/111 to have human-readable rates */
	if (unlikely(acx_debug & (L_XFER|L_DATA))) {
		u16 fc = ((wlan_hdr_t*)hostdesc1->data)->fc;
		if (IS_ACX111(adev))
			printk("tx: pkt (%s): len %d "
				"rate %04X%s status %u\n",
				acx_get_packet_type_string(le16_to_cpu(fc)), len,
				le16_to_cpu(txdesc->u.r2.rate111),
				(le16_to_cpu(txdesc->u.r2.rate111) & RATE111_SHORTPRE) ? "(SPr)" : "",
				adev->status);
		else
			printk("tx: pkt (%s): len %d rate %03u%s status %u\n",
				acx_get_packet_type_string(fc), len,
				read_slavemem8 (adev, (u32) &(txdesc->u.r1.rate)),
				(Ctl_8 & DESC_CTL_SHORT_PREAMBLE) ? "(SPr)" : "",
				adev->status);

		if (acx_debug & L_DATA) {
			printk("tx: 802.11 [%d]: ", len);
			acx_dump_bytes(hostdesc1->data, len);
		}
	}
end:
	FN_EXIT0;
}


/***********************************************************************
** acxmem_l_clean_txdesc
**
** This function resets the txdescs' status when the ACX100
** signals the TX done IRQ (txdescs have been processed), starting with
** the pool index of the descriptor which we would use next,
** in order to make sure that we can be as fast as possible
** in filling new txdescs.
** Everytime we get called we know where the next packet to be cleaned is.
*/

#if !ACX_DEBUG
static inline void log_txbuffer(const acx_device_t *adev) {}
#else
static void
log_txbuffer(acx_device_t *adev)
{
	txdesc_t *txdesc;
	int i;
	u8 Ctl_8;

	/* no FN_ENTER here, we don't want that */
	/* no locks here, since it's entirely non-critical code */
	txdesc = adev->txdesc_start;
	if (unlikely(!txdesc)) return;
	printk("tx: desc->Ctl8's:");
	for (i = 0; i < TX_CNT; i++) {
	  Ctl_8 = read_slavemem8 (adev, (u32) &(txdesc->Ctl_8));
		printk(" %02X", Ctl_8);
		txdesc = advance_txdesc(adev, txdesc, 1);
	}
	printk("\n");
}
#endif


static void
handle_tx_error(acx_device_t *adev, u8 error, unsigned int finger)
{
	const char *err = "unknown error";

	/* hmm, should we handle this as a mask
	 * of *several* bits?
	 * For now I think only caring about
	 * individual bits is ok... */
	switch (error) {
	case 0x01:
		err = "no Tx due to error in other fragment";
		adev->wstats.discard.fragment++;
		break;
	case 0x02:
		err = "Tx aborted";
		adev->stats.tx_aborted_errors++;
		break;
	case 0x04:
		err = "Tx desc wrong parameters";
		adev->wstats.discard.misc++;
		break;
	case 0x08:
		err = "WEP key not found";
		adev->wstats.discard.misc++;
		break;
	case 0x10:
		err = "MSDU lifetime timeout? - try changing "
				"'iwconfig retry lifetime XXX'";
		adev->wstats.discard.misc++;
		break;
	case 0x20:
		err = "excessive Tx retries due to either distance "
			"too high or unable to Tx or Tx frame error - "
			"try changing 'iwconfig txpower XXX' or "
			"'sens'itivity or 'retry'";
		adev->wstats.discard.retries++;
		/* Tx error 0x20 also seems to occur on
		 * overheating, so I'm not sure whether we
		 * actually want to do aggressive radio recalibration,
		 * since people maybe won't notice then that their hardware
		 * is slowly getting cooked...
		 * Or is it still a safe long distance from utter
		 * radio non-functionality despite many radio recalibs
		 * to final destructive overheating of the hardware?
		 * In this case we really should do recalib here...
		 * I guess the only way to find out is to do a
		 * potentially fatal self-experiment :-\
		 * Or maybe only recalib in case we're using Tx
		 * rate auto (on errors switching to lower speed
		 * --> less heat?) or 802.11 power save mode?
		 *
		 * ok, just do it. */
		if (++adev->retry_errors_msg_ratelimit % 4 == 0) {
			if (adev->retry_errors_msg_ratelimit <= 20) {
				printk("%s: several excessive Tx "
					"retry errors occurred, attempting "
					"to recalibrate radio. Radio "
					"drift might be caused by increasing "
					"card temperature, please check the card "
					"before it's too late!\n",
					adev->ndev->name);
				if (adev->retry_errors_msg_ratelimit == 20)
					printk("disabling above message\n");
			}

			acx_schedule_task(adev,	ACX_AFTER_IRQ_CMD_RADIO_RECALIB);
		}
		break;
	case 0x40:
		err = "Tx buffer overflow";
		adev->stats.tx_fifo_errors++;
		break;
	case 0x80:
		err = "DMA error";
		adev->wstats.discard.misc++;
		break;
	}
	adev->stats.tx_errors++;
	if (adev->stats.tx_errors <= 20)
		printk("%s: tx error 0x%02X, buf %02u! (%s)\n",
				adev->ndev->name, error, finger, err);
	else
		printk("%s: tx error 0x%02X, buf %02u!\n",
				adev->ndev->name, error, finger);
}


unsigned int
acxmem_l_clean_txdesc(acx_device_t *adev)
{
	txdesc_t *txdesc;
	unsigned finger;
	int num_cleaned;
	u16 r111;
	u8 error, ack_failures, rts_failures, rts_ok, r100, Ctl_8;
	u32 acxmem;
	txdesc_t tmptxdesc;

	FN_ENTER;

	/*
	 * Set up a template descriptor for re-initialization.  The only
	 * things that get set are Ctl_8 and the rate, and the rate defaults
	 * to 1Mbps.
	 */
	memset (&tmptxdesc, 0, sizeof (tmptxdesc));
	tmptxdesc.Ctl_8 = DESC_CTL_HOSTOWN | DESC_CTL_FIRSTFRAG;
	tmptxdesc.u.r1.rate = 0x0a;

	if (unlikely(acx_debug & L_DEBUG))
		log_txbuffer(adev);

	log(L_BUFT, "tx: cleaning up bufs from %u\n", adev->tx_tail);

	/* We know first descr which is not free yet. We advance it as far
	** as we see correct bits set in following descs (if next desc
	** is NOT free, we shouldn't advance at all). We know that in
	** front of tx_tail may be "holes" with isolated free descs.
	** We will catch up when all intermediate descs will be freed also */

	finger = adev->tx_tail;
	num_cleaned = 0;
	while (likely(finger != adev->tx_head)) {
		txdesc = get_txdesc(adev, finger);

		/* If we allocated txdesc on tx path but then decided
		** to NOT use it, then it will be left as a free "bubble"
		** in the "allocated for tx" part of the ring.
		** We may meet it on the next ring pass here. */

		/* stop if not marked as "tx finished" and "host owned" */
		Ctl_8 = read_slavemem8 (adev, (u32) &(txdesc->Ctl_8));
		if ((Ctl_8 & DESC_CTL_ACXDONE_HOSTOWN)
					!= DESC_CTL_ACXDONE_HOSTOWN) {
			if (unlikely(!num_cleaned)) { /* maybe remove completely */
				log(L_BUFT, "clean_txdesc: tail isn't free. "
					"tail:%d head:%d\n",
					adev->tx_tail, adev->tx_head);
			}
			break;
		}

		/* remember desc values... */
		error = read_slavemem8 (adev, (u32) &(txdesc->error));
		ack_failures = read_slavemem8 (adev, (u32) &(txdesc->ack_failures));
		rts_failures = read_slavemem8 (adev, (u32) &(txdesc->u.rts.rts_failures));
		rts_ok = read_slavemem8 (adev, (u32) &(txdesc->u.rts.rts_ok));
		r100 = read_slavemem8 (adev, (u32) &(txdesc->u.r1.rate));
		r111 = le16_to_cpu(read_slavemem16 (adev, (u32) &(txdesc->u.r2.rate111)));

		/* need to check for certain error conditions before we
		 * clean the descriptor: we still need valid descr data here */
		if (unlikely(0x30 & error)) {
			/* only send IWEVTXDROP in case of retry or lifetime exceeded;
			 * all other errors mean we screwed up locally */
			union iwreq_data wrqu;
			wlan_hdr_t *hdr;
			txhostdesc_t *hostdesc;

			hostdesc = get_txhostdesc(adev, txdesc);
			hdr = (wlan_hdr_t *)hostdesc->data;
			MAC_COPY(wrqu.addr.sa_data, hdr->a1);
			wireless_send_event(adev->ndev, IWEVTXDROP, &wrqu, NULL);
		}

		/*
		 * Free up the transmit data buffers
		 */
		acxmem = read_slavemem32 (adev, (u32) &(txdesc->AcxMemPtr));
		if (acxmem) {
		  reclaim_acx_txbuf_space (adev, acxmem);
		}

		/* ...and free the desc by clearing all the fields 
		   except the next pointer */
		copy_to_slavemem (adev,
				  (u32) &(txdesc->HostMemPtr), 
				  (u8 *) &(tmptxdesc.HostMemPtr),
				  sizeof (tmptxdesc) - sizeof(tmptxdesc.pNextDesc)
				  );

		adev->tx_free++;
		num_cleaned++;

		if ((adev->tx_free >= TX_START_QUEUE)
		 && (adev->status == ACX_STATUS_4_ASSOCIATED)
		 && (acx_queue_stopped(adev->ndev))
		) {
			log(L_BUF, "tx: wake queue (avail. Tx desc %u)\n",
					adev->tx_free);
			acx_wake_queue(adev->ndev, NULL);
		}

		/* do error checking, rate handling and logging
		 * AFTER having done the work, it's faster */

		/* do rate handling */
		if (adev->rate_auto) {
			struct client *clt = get_txc(adev, txdesc);
			if (clt) {
				u16 cur = get_txr(adev, txdesc);
				if (clt->rate_cur == cur) {
					acx_l_handle_txrate_auto(adev, clt,
						cur, /* intended rate */
						r100, r111, /* actually used rate */
						(error & 0x30), /* was there an error? */
						TX_CNT + TX_CLEAN_BACKLOG - adev->tx_free);
				}
			}
		}

		if (unlikely(error))
			handle_tx_error(adev, error, finger);

		if (IS_ACX111(adev))
			log(L_BUFT, "tx: cleaned %u: !ACK=%u !RTS=%u RTS=%u r111=%04X\n",
				finger, ack_failures, rts_failures, rts_ok, r111);
		else
			log(L_BUFT, "tx: cleaned %u: !ACK=%u !RTS=%u RTS=%u rate=%u\n",
				finger, ack_failures, rts_failures, rts_ok, r100);

		/* update pointer for descr to be cleaned next */
		finger = (finger + 1) % TX_CNT;
	}

	/* remember last position */
	adev->tx_tail = finger;
/* end: */
	FN_EXIT1(num_cleaned);
	return num_cleaned;
}

/* clean *all* Tx descriptors, and regardless of their previous state.
 * Used for brute-force reset handling. */
void
acxmem_l_clean_txdesc_emergency(acx_device_t *adev)
{
	txdesc_t *txdesc;
	int i;
	u32 acxmem;

	FN_ENTER;

	for (i = 0; i < TX_CNT; i++) {
		txdesc = get_txdesc(adev, i);

		/* free it */
		write_slavemem8 (adev, (u32) &(txdesc->ack_failures), 0);
		write_slavemem8 (adev, (u32) &(txdesc->u.rts.rts_failures), 0);
		write_slavemem8 (adev, (u32) &(txdesc->u.rts.rts_ok), 0);
		write_slavemem8 (adev, (u32) &(txdesc->error), 0);
		write_slavemem8 (adev, (u32) &(txdesc->Ctl_8), DESC_CTL_HOSTOWN);

		/*
		 * Clean up the memory allocated on the ACX for this transmit descriptor.
		 */
		acxmem = read_slavemem32 (adev, (u32) &(txdesc->AcxMemPtr));
		if (acxmem) {
		  reclaim_acx_txbuf_space (adev, acxmem);
		}		
		
		write_slavemem32 (adev, (u32) &(txdesc->AcxMemPtr), 0);
	}

	adev->tx_free = TX_CNT;

	FN_EXIT0;
}


/***********************************************************************
** acxmem_s_create_tx_host_desc_queue
*/

static void*
allocate(acx_device_t *adev, size_t size, dma_addr_t *phy, const char *msg)
{
	void *ptr;
        ptr = kmalloc (size, GFP_KERNEL);
	/*
	 * The ACX can't use the physical address, so we'll have to fake it
	 * later and it might be handy to have the virtual address.
	 */
	*phy = (dma_addr_t) NULL;

	if (ptr) {
		log(L_DEBUG, "%s sz=%d adr=0x%p phy=0x%08llx\n",
				msg, (int)size, ptr, (unsigned long long)*phy);
		memset(ptr, 0, size);
		return ptr;
	}
	printk(KERN_ERR "acx: %s allocation FAILED (%d bytes)\n",
					msg, (int)size);
	return NULL;
}


/*
 * In the generic slave memory access mode, most of the stuff in
 * the txhostdesc_t is unused.  It's only here because the rest of
 * the ACX driver expects it to be since the PCI version uses indirect
 * host memory organization with DMA.  Since we're not using DMA the
 * only use we have for the host descriptors is to store the packets
 * on the way out.
 */
static int
acxmem_s_create_tx_host_desc_queue(acx_device_t *adev)
{
	txhostdesc_t *hostdesc;
	u8 *txbuf;
	int i;

	FN_ENTER;

	/* allocate TX buffer */
	adev->txbuf_area_size = TX_CNT * WLAN_A4FR_MAXLEN_WEP_FCS;

	adev->txbuf_start = allocate(adev, adev->txbuf_area_size,
				     &adev->txbuf_startphy, "txbuf_start");
	if (!adev->txbuf_start)
	  goto fail;

	/* allocate the TX host descriptor queue pool */
	adev->txhostdesc_area_size = TX_CNT * 2*sizeof(*hostdesc);

	adev->txhostdesc_start = allocate(adev, adev->txhostdesc_area_size,
					  &adev->txhostdesc_startphy, "txhostdesc_start");
	if (!adev->txhostdesc_start)
	  goto fail;

	/* check for proper alignment of TX host descriptor pool */
	if ((long) adev->txhostdesc_start & 3) {
		printk("acx: driver bug: dma alloc returns unaligned address\n");
		goto fail;
	}

	hostdesc = adev->txhostdesc_start;
	txbuf = adev->txbuf_start;

#if 0
/* Each tx buffer is accessed by hardware via
** txdesc -> txhostdesc(s) -> txbuffer(s).
** We use only one txhostdesc per txdesc, but it looks like
** acx111 is buggy: it accesses second txhostdesc
** (via hostdesc.desc_phy_next field) even if
** txdesc->length == hostdesc->length and thus
** entire packet was placed into first txhostdesc.
** Due to this bug acx111 hangs unless second txhostdesc
** has le16_to_cpu(hostdesc.length) = 3 (or larger)
** Storing NULL into hostdesc.desc_phy_next
** doesn't seem to help.
**
** Update: although it worked on Xterasys XN-2522g
** with len=3 trick, WG311v2 is even more bogus, doesn't work.
** Keeping this code (#ifdef'ed out) for documentational purposes.
*/
	for (i = 0; i < TX_CNT*2; i++) {
		hostdesc_phy += sizeof(*hostdesc);
		if (!(i & 1)) {
			hostdesc->data_phy = cpu2acx(txbuf_phy);
			/* hostdesc->data_offset = ... */
			/* hostdesc->reserved = ... */
			hostdesc->Ctl_16 = cpu_to_le16(DESC_CTL_HOSTOWN);
			/* hostdesc->length = ... */
			hostdesc->desc_phy_next = cpu2acx(hostdesc_phy);
			hostdesc->pNext = ptr2acx(NULL);
			/* hostdesc->Status = ... */
			/* below: non-hardware fields */
			hostdesc->data = txbuf;

			txbuf += WLAN_A4FR_MAXLEN_WEP_FCS;
			txbuf_phy += WLAN_A4FR_MAXLEN_WEP_FCS;
		} else {
			/* hostdesc->data_phy = ... */
			/* hostdesc->data_offset = ... */
			/* hostdesc->reserved = ... */
			/* hostdesc->Ctl_16 = ... */
			hostdesc->length = cpu_to_le16(3); /* bug workaround */
			/* hostdesc->desc_phy_next = ... */
			/* hostdesc->pNext = ... */
			/* hostdesc->Status = ... */
			/* below: non-hardware fields */
			/* hostdesc->data = ... */
		}
		hostdesc++;
	}
#endif
/* We initialize two hostdescs so that they point to adjacent
** memory areas. Thus txbuf is really just a contiguous memory area */
	for (i = 0; i < TX_CNT*2; i++) {
		/* ->data is a non-hardware field: */
		hostdesc->data = txbuf;

		if (!(i & 1)) {
			txbuf += WLAN_HDR_A3_LEN;
		} else {
			txbuf += WLAN_A4FR_MAXLEN_WEP_FCS - WLAN_HDR_A3_LEN;
		}
		hostdesc++;
	}
	hostdesc--;

	FN_EXIT1(OK);
	return OK;
fail:
	printk("acx: create_tx_host_desc_queue FAILED\n");
	/* dealloc will be done by free function on error case */
	FN_EXIT1(NOT_OK);
	return NOT_OK;
}


/***************************************************************
** acxmem_s_create_rx_host_desc_queue
*/
/* the whole size of a data buffer (header plus data body)
 * plus 32 bytes safety offset at the end */
#define RX_BUFFER_SIZE (sizeof(rxbuffer_t) + 32)

static int
acxmem_s_create_rx_host_desc_queue(acx_device_t *adev)
{
	rxhostdesc_t *hostdesc;
	rxbuffer_t *rxbuf;
	int i;

	FN_ENTER;

	/* allocate the RX host descriptor queue pool */
	adev->rxhostdesc_area_size = RX_CNT * sizeof(*hostdesc);

	adev->rxhostdesc_start = allocate(adev, adev->rxhostdesc_area_size,
					  &adev->rxhostdesc_startphy, "rxhostdesc_start");
	if (!adev->rxhostdesc_start)
	  goto fail;

	/* check for proper alignment of RX host descriptor pool */
	if ((long) adev->rxhostdesc_start & 3) {
		printk("acx: driver bug: dma alloc returns unaligned address\n");
		goto fail;
	}

	/* allocate Rx buffer pool which will be used by the acx
	 * to store the whole content of the received frames in it */
	adev->rxbuf_area_size = RX_CNT * RX_BUFFER_SIZE;

	adev->rxbuf_start = allocate(adev, adev->rxbuf_area_size,
				     &adev->rxbuf_startphy, "rxbuf_start");
	if (!adev->rxbuf_start)
	  goto fail;

	rxbuf = adev->rxbuf_start;
	hostdesc = adev->rxhostdesc_start;

	/* don't make any popular C programming pointer arithmetic mistakes
	 * here, otherwise I'll kill you...
	 * (and don't dare asking me why I'm warning you about that...) */
	for (i = 0; i < RX_CNT; i++) {
		hostdesc->data = rxbuf;
		hostdesc->length = cpu_to_le16(RX_BUFFER_SIZE);
		rxbuf++;
		hostdesc++;
	}
	hostdesc--;
	FN_EXIT1(OK);
	return OK;
fail:
	printk("acx: create_rx_host_desc_queue FAILED\n");
	/* dealloc will be done by free function on error case */
	FN_EXIT1(NOT_OK);
	return NOT_OK;
}


/***************************************************************
** acxmem_s_create_hostdesc_queues
*/
int
acxmem_s_create_hostdesc_queues(acx_device_t *adev)
{
	int result;
	result = acxmem_s_create_tx_host_desc_queue(adev);
	if (OK != result) return result;
	result = acxmem_s_create_rx_host_desc_queue(adev);
	return result;
}


/***************************************************************
** acxmem_create_tx_desc_queue
*/
static void
acxmem_create_tx_desc_queue(acx_device_t *adev, u32 tx_queue_start)
{
	txdesc_t *txdesc;
	u32 clr;
	int i;

	FN_ENTER;

	if (IS_ACX100(adev))
		adev->txdesc_size = sizeof(*txdesc);
	else
		/* the acx111 txdesc is 4 bytes larger */
		adev->txdesc_size = sizeof(*txdesc) + 4;

	/*
	 * This refers to an ACX address, not one of ours
	 */
	adev->txdesc_start = (txdesc_t *) tx_queue_start;

	log(L_DEBUG, "adev->txdesc_start=%p\n",
			adev->txdesc_start);

	adev->tx_free = TX_CNT;
	/* done by memset: adev->tx_head = 0; */
	/* done by memset: adev->tx_tail = 0; */
	txdesc = adev->txdesc_start;

	if (IS_ACX111(adev)) {
		/* ACX111 has a preinitialized Tx buffer! */
		/* loop over whole send pool */
		/* FIXME: do we have to do the hostmemptr stuff here?? */
		for (i = 0; i < TX_CNT; i++) {
			txdesc->Ctl_8 = DESC_CTL_HOSTOWN;
			/* reserve two (hdr desc and payload desc) */
			txdesc = advance_txdesc(adev, txdesc, 1);
		}
	} else {
		/* ACX100 Tx buffer needs to be initialized by us */
		/* clear whole send pool. sizeof is safe here (we are acx100) */

		/*
		 * adev->txdesc_start refers to device memory, so we can't write
		 * directly to it.
		 */
		clr = (u32) adev->txdesc_start;
		while (clr < (u32) adev->txdesc_start + (TX_CNT * sizeof(*txdesc))) {
		  write_slavemem32 (adev, clr, 0);
		  clr += 4;
		}

		/* loop over whole send pool */
		for (i = 0; i < TX_CNT; i++) {
			log(L_DEBUG, "configure card tx descriptor: 0x%p, "
				"size: 0x%X\n", txdesc, adev->txdesc_size);

			/* initialise ctl */
			/*
			 * No auto DMA here
			 */
			write_slavemem8 (adev, (u32) &(txdesc->Ctl_8),
					(u8) (DESC_CTL_HOSTOWN | DESC_CTL_FIRSTFRAG));
			/* done by memset(0): txdesc->Ctl2_8 = 0; */

			/* point to next txdesc */
			write_slavemem32 (adev, (u32) &(txdesc->pNextDesc),
					  (u32) cpu_to_le32 ((u8 *) txdesc + adev->txdesc_size));

			/* go to the next one */
			/* ++ is safe here (we are acx100) */
			txdesc++;
		}
		/* go back to the last one */
		txdesc--;
		/* and point to the first making it a ring buffer */
		write_slavemem32 (adev, (u32) &(txdesc->pNextDesc),
				  (u32) cpu_to_le32 (tx_queue_start));
	}
	FN_EXIT0;
}


/***************************************************************
** acxmem_create_rx_desc_queue
*/
static void
acxmem_create_rx_desc_queue(acx_device_t *adev, u32 rx_queue_start)
{
	rxdesc_t *rxdesc;
	u32 mem_offs;
	int i;

	FN_ENTER;

	/* done by memset: adev->rx_tail = 0; */

	/* ACX111 doesn't need any further config: preconfigures itself.
	 * Simply print ring buffer for debugging */
	if (IS_ACX111(adev)) {
		/* rxdesc_start already set here */

		adev->rxdesc_start = (rxdesc_t *) rx_queue_start;

		rxdesc = adev->rxdesc_start;
		for (i = 0; i < RX_CNT; i++) {
			log(L_DEBUG, "rx descriptor %d @ 0x%p\n", i, rxdesc);
			rxdesc = adev->rxdesc_start = (rxdesc_t *)
			  acx2cpu(rxdesc->pNextDesc);
		}
	} else {
		/* we didn't pre-calculate rxdesc_start in case of ACX100 */
		/* rxdesc_start should be right AFTER Tx pool */
		adev->rxdesc_start = (rxdesc_t *)
			((u8 *) adev->txdesc_start + (TX_CNT * sizeof(txdesc_t)));
		/* NB: sizeof(txdesc_t) above is valid because we know
		** we are in if (acx100) block. Beware of cut-n-pasting elsewhere!
		** acx111's txdesc is larger! */

		mem_offs = (u32) adev->rxdesc_start;
		while (mem_offs < (u32) adev->rxdesc_start + (RX_CNT * sizeof (*rxdesc))) {
		  write_slavemem32 (adev, mem_offs, 0);
		  mem_offs += 4;
		}

		/* loop over whole receive pool */
		rxdesc = adev->rxdesc_start;
		for (i = 0; i < RX_CNT; i++) {
			log(L_DEBUG, "rx descriptor @ 0x%p\n", rxdesc);
			/* point to next rxdesc */
			write_slavemem32 (adev, (u32) &(rxdesc->pNextDesc),
					  (u32) cpu_to_le32 ((u8 *) rxdesc + sizeof(*rxdesc)));
			/* go to the next one */
			rxdesc++;
		}
		/* go to the last one */
		rxdesc--;

		/* and point to the first making it a ring buffer */
		write_slavemem32 (adev, (u32) &(rxdesc->pNextDesc),
				  (u32) cpu_to_le32 (rx_queue_start));
	}
	FN_EXIT0;
}


/***************************************************************
** acxmem_create_desc_queues
*/
void
acxmem_create_desc_queues(acx_device_t *adev, u32 tx_queue_start, u32 rx_queue_start)
{
  u32 *p;
  int i;

	acxmem_create_tx_desc_queue(adev, tx_queue_start);
	acxmem_create_rx_desc_queue(adev, rx_queue_start);
	p = (u32 *) adev->acx_queue_indicator;
	for (i = 0; i < 4; i++) {
	  write_slavemem32 (adev, (u32) p, 0);
	  p++;
	}
}


/***************************************************************
** acxmem_s_proc_diag_output
*/
char*
acxmem_s_proc_diag_output(char *p, acx_device_t *adev)
{
	const char *rtl, *thd, *ttl;
	txdesc_t *txdesc;
	u8 Ctl_8;
	rxdesc_t *rxdesc;
	int i;
	u32 tmp;
	txdesc_t txd;
	u8 buf[0x200];
	int j, k;

	FN_ENTER;

#if DUMP_MEM_DURING_DIAG > 0
	dump_acxmem (adev, 0, 0x10000);
	panic ("dump finished");
#endif
	
	p += sprintf(p, "** Rx buf **\n");
	rxdesc = adev->rxdesc_start;
	if (rxdesc) for (i = 0; i < RX_CNT; i++) {
		rtl = (i == adev->rx_tail) ? " [tail]" : "";
		Ctl_8 = read_slavemem8 (adev, (u32) &(rxdesc->Ctl_8));
		if (Ctl_8 & DESC_CTL_HOSTOWN)
			p += sprintf(p, "%02u (%02x) FULL%s\n", i, Ctl_8, rtl);
		else
			p += sprintf(p, "%02u (%02x) empty%s\n", i, Ctl_8, rtl);
		rxdesc++;
	}
	p += sprintf(p, "** Tx buf (free %d, Linux netqueue %s) **\n", adev->tx_free,
				acx_queue_stopped(adev->ndev) ? "STOPPED" : "running");
	
	p += sprintf(p, "** Tx buf %d blocks total, %d available, free list head %04x\n",
		     adev->acx_txbuf_numblocks, adev->acx_txbuf_blocks_free, adev->acx_txbuf_free);
	txdesc = adev->txdesc_start;
	if (txdesc) {
	  for (i = 0; i < TX_CNT; i++) {
	    thd = (i == adev->tx_head) ? " [head]" : "";
	    ttl = (i == adev->tx_tail) ? " [tail]" : "";
	    copy_from_slavemem (adev, (u8 *) &txd, (u32) txdesc, sizeof (txd));
	    Ctl_8 = read_slavemem8 (adev, (u32) &(txdesc->Ctl_8));
	    if (Ctl_8 & DESC_CTL_ACXDONE)
	      p += sprintf(p, "%02u ready to free (%02X)%s%s", i, Ctl_8, thd, ttl);
	    else if (Ctl_8 & DESC_CTL_HOSTOWN)
	      p += sprintf(p, "%02u available     (%02X)%s%s", i, Ctl_8, thd, ttl);
	    else
	      p += sprintf(p, "%02u busy          (%02X)%s%s", i, Ctl_8, thd, ttl);
	    tmp = read_slavemem32 (adev, (u32) &(txdesc->AcxMemPtr));
	    if (tmp) {
	      p += sprintf (p, " %04x", tmp);
	      while ((tmp = read_slavemem32 (adev, (u32) tmp)) != 0x02000000) {
		tmp <<= 5;
		p += sprintf (p, " %04x", tmp);
	      }
	    }
	    p += sprintf (p, "\n");
	    p += sprintf (p, "  %04x: %04x %04x %04x %04x %04x %04x %04x %04x %04x %04x %02x %02x %02x %02x\n"
			     "%02x %02x %02x %02x %04x\n",
			  (u32) txdesc,
			  txd.pNextDesc.v, txd.HostMemPtr.v, txd.AcxMemPtr.v, txd.tx_time,
			  txd.total_length, txd.Reserved,
			  txd.dummy[0], txd.dummy[1], txd.dummy[2], txd.dummy[3],
			  txd.Ctl_8, txd.Ctl2_8, txd.error, txd.ack_failures,
			  txd.u.rts.rts_failures, txd.u.rts.rts_ok, txd.u.r1.rate, txd.u.r1.queue_ctrl,
			  txd.queue_info
			  );
	    if (txd.AcxMemPtr.v) {
	      copy_from_slavemem (adev, buf, txd.AcxMemPtr.v, sizeof (buf));
	      for (j = 0; (j < txd.total_length) && (j<(sizeof(buf)-4)); j+=16) {
		p += sprintf (p, "    ");
		for (k = 0; (k < 16) && (j+k < txd.total_length); k++) {
		  p += sprintf (p, " %02x", buf[j+k+4]);
		}
		p += sprintf (p, "\n");
	      }
	    }
	    txdesc = advance_txdesc(adev, txdesc, 1);
	  }
	}
	
	p += sprintf(p,
		"\n"
		"** Generic slave data **\n"
                "irq_mask 0x%04x irq_status 0x%04x irq on acx 0x%04x\n"
		"txbuf_start 0x%p, txbuf_area_size %u\n"
		"txdesc_size %u, txdesc_start 0x%p\n"
		"txhostdesc_start 0x%p, txhostdesc_area_size %u\n"
		"txbuf start 0x%04x, txbuf size %d\n"
		"rxdesc_start 0x%p\n"
		"rxhostdesc_start 0x%p, rxhostdesc_area_size %u\n"
		"rxbuf_start 0x%p, rxbuf_area_size %u\n",
		adev->irq_mask, adev->irq_status, read_reg32(adev, IO_ACX_IRQ_STATUS_NON_DES),
		adev->txbuf_start, adev->txbuf_area_size,
		adev->txdesc_size, adev->txdesc_start,
		adev->txhostdesc_start, adev->txhostdesc_area_size,
                adev->acx_txbuf_start, adev->acx_txbuf_numblocks * adev->memblocksize,
		adev->rxdesc_start,
		adev->rxhostdesc_start, adev->rxhostdesc_area_size,
		adev->rxbuf_start, adev->rxbuf_area_size);
	FN_EXIT0;
	return p;
}


/***********************************************************************
*/
int
acxmem_proc_eeprom_output(char *buf, acx_device_t *adev)
{
	char *p = buf;
	int i;

	FN_ENTER;

	for (i = 0; i < 0x400; i++) {
		acxmem_read_eeprom_byte(adev, i, p++);
	}

	FN_EXIT1(p - buf);
	return p - buf;
}


/***********************************************************************
*/
void
acxmem_set_interrupt_mask(acx_device_t *adev)
{
	if (IS_ACX111(adev)) {
		adev->irq_mask = (u16) ~(0
				| HOST_INT_RX_DATA       
				| HOST_INT_TX_COMPLETE
				/* | HOST_INT_TX_XFER        */
				/* | HOST_INT_RX_COMPLETE    */
				/* | HOST_INT_DTIM           */
				/* | HOST_INT_BEACON         */
				/* | HOST_INT_TIMER          */
				/* | HOST_INT_KEY_NOT_FOUND  */
				| HOST_INT_IV_ICV_FAILURE
				| HOST_INT_CMD_COMPLETE
				| HOST_INT_INFO
				| HOST_INT_OVERFLOW    
				/* | HOST_INT_PROCESS_ERROR  */
				| HOST_INT_SCAN_COMPLETE
				| HOST_INT_FCS_THRESHOLD
				| HOST_INT_UNKNOWN
				);
		/* Or else acx100 won't signal cmd completion, right? */
		adev->irq_mask_off = (u16)~( HOST_INT_CMD_COMPLETE ); /* 0xfdff */
	} else {
		adev->irq_mask = (u16) ~(0
				| HOST_INT_RX_DATA 
				| HOST_INT_TX_COMPLETE
				/* | HOST_INT_TX_XFER        */
				/* | HOST_INT_RX_COMPLETE    */
				/* | HOST_INT_DTIM           */
				/* | HOST_INT_BEACON         */
				/* | HOST_INT_TIMER          */
				/* | HOST_INT_KEY_NOT_FOUND  */
				/* | HOST_INT_IV_ICV_FAILURE */
				| HOST_INT_CMD_COMPLETE
				| HOST_INT_INFO
				/* | HOST_INT_OVERFLOW       */
				/* | HOST_INT_PROCESS_ERROR  */
				| HOST_INT_SCAN_COMPLETE
				/* | HOST_INT_FCS_THRESHOLD  */
				/* | HOST_INT_BEACON_MISSED        */
				);
		adev->irq_mask_off = (u16)~( HOST_INT_UNKNOWN ); /* 0x7fff */
	}
}


/***********************************************************************
*/
int
acx100mem_s_set_tx_level(acx_device_t *adev, u8 level_dbm)
{
	struct acx111_ie_tx_level tx_level;

	/* since it can be assumed that at least the Maxim radio has a
	 * maximum power output of 20dBm and since it also can be
	 * assumed that these values drive the DAC responsible for
	 * setting the linear Tx level, I'd guess that these values
	 * should be the corresponding linear values for a dBm value,
	 * in other words: calculate the values from that formula:
	 * Y [dBm] = 10 * log (X [mW])
	 * then scale the 0..63 value range onto the 1..100mW range (0..20 dBm)
	 * and you're done...
	 * Hopefully that's ok, but you never know if we're actually
	 * right... (especially since Windows XP doesn't seem to show
	 * actual Tx dBm values :-P) */

	/* NOTE: on Maxim, value 30 IS 30mW, and value 10 IS 10mW - so the
	 * values are EXACTLY mW!!! Not sure about RFMD and others,
	 * though... */
	static const u8 dbm2val_maxim[21] = {
		63, 63, 63, 62,
		61, 61, 60, 60,
		59, 58, 57, 55,
		53, 50, 47, 43,
		38, 31, 23, 13,
		0
	};
	static const u8 dbm2val_rfmd[21] = {
		 0,  0,  0,  1,
		 2,  2,  3,  3,
		 4,  5,  6,  8,
		10, 13, 16, 20,
		25, 32, 41, 50,
		63
	};
	const u8 *table;

	switch (adev->radio_type) {
	case RADIO_MAXIM_0D:
		table = &dbm2val_maxim[0];
		break;
	case RADIO_RFMD_11:
	case RADIO_RALINK_15:
		table = &dbm2val_rfmd[0];
		break;
	default:
		printk("%s: unknown/unsupported radio type, "
			"cannot modify tx power level yet!\n",
				adev->ndev->name);
		return NOT_OK;
	}
	/*
	 * The hx4700 EEPROM, at least, only supports 1 power setting.  The configure
	 * routine matches the PA bias with the gain, so just use its default value.
	 * The values are: 0x2b for the gain and 0x03 for the PA bias.  The firmware
	 * writes the gain level to the Tx gain control DAC and the PA bias to the Maxim
	 * radio's PA bias register.  The firmware limits itself to 0 - 64 when writing to the
	 * gain control DAC.
	 *
	 * Physically between the ACX and the radio, higher Tx gain control DAC values result
	 * in less power output; 0 volts to the Maxim radio results in the highest output power
	 * level, which I'm assuming matches up with 0 in the Tx Gain DAC register.
	 *
	 * Although there is only the 1 power setting, one of the radio firmware functions adjusts
	 * the transmit power level up and down.  That function is called by the ACX FIQ handler
	 * under certain conditions.
	 */
	tx_level.level = 1;
	//return acx_s_configure(adev, &tx_level, ACX1xx_IE_DOT11_TX_POWER_LEVEL);

	printk("%s: changing radio power level to %u dBm (%u)\n",
			adev->ndev->name, level_dbm, table[level_dbm]);
	acxmem_s_write_phy_reg(adev, 0x11, table[level_dbm]);

	return 0;
}

void acxmem_e_release(struct device *dev) {
}

/***********************************************************************
** acx_cs part
**
** called by pcmcia card service
*/

/*
   The event() function is this driver's Card Services event handler.
   It will be called by Card Services when an appropriate card status
   event is received.  The config() and release() entry points are
   used to configure or release a socket, in response to card
   insertion and ejection events.  They are invoked from the acx_cs
   event handler. 
*/

static int acx_cs_config(struct pcmcia_device *link);
static void acx_cs_release(struct pcmcia_device *link);

/*
   The attach() and detach() entry points are used to create and destroy
   "instances" of the driver, where each instance represents everything
   needed to manage one actual PCMCIA card.
*/

static void acx_cs_detach(struct pcmcia_device *p_dev);

/*
   You'll also need to prototype all the functions that will actually
   be used to talk to your device.  See 'pcmem_cs' for a good example
   of a fully self-sufficient driver; the other drivers rely more or
   less on other parts of the kernel.
*/

/*
   A linked list of "instances" of the  acxnet device.  Each actual
   PCMCIA card corresponds to one device instance, and is described
   by one struct pcmcia_device structure (defined in ds.h).

   You may not want to use a linked list for this -- for example, the
   memory card driver uses an array of struct pcmcia_device pointers, where minor
   device numbers are used to derive the corresponding array index.
*/

/*
   A driver needs to provide a dev_node_t structure for each device
   on a card.  In some cases, there is only one device per card (for
   example, ethernet cards, modems).  In other cases, there may be
   many actual or logical devices (SCSI adapters, memory cards with
   multiple partitions).  The dev_node_t structures need to be kept
   in a linked list starting at the 'dev' field of a struct pcmcia_device
   structure.  We allocate them in the card's private data structure,
   because they generally shouldn't be allocated dynamically.

   In this case, we also provide a flag to indicate if a device is
   "stopped" due to a power management event, or card ejection.  The
   device IO routines can use a flag like this to throttle IO to a
   card that is not ready to accept it.
*/
   

/*======================================================================
  
  acx_attach() creates an "instance" of the driver, allocating
  local data structures for one device.  The device is registered
  with Card Services.
  
  The dev_link structure is initialized, but we don't actually
  configure the card at this point -- we wait until we receive a
  card insertion event.
  
  ======================================================================*/

static int acx_cs_probe(struct pcmcia_device *link)
{
	local_info_t *local;
	struct net_device *ndev;

	DEBUG(0, "acx_attach()\n");

        ndev = alloc_netdev(sizeof(acx_device_t), "wlan%d", dummy_netdev_init);
        if (!ndev) {
                printk("acx: no memory for netdevice struct\n");
                return -ENOMEM;
        }

        /* Interrupt setup */
        link->irq.Attributes = IRQ_TYPE_EXCLUSIVE | IRQ_HANDLE_PRESENT;
        link->irq.IRQInfo1 = IRQ_LEVEL_ID;
        link->irq.Handler = acxmem_i_interrupt;
        link->irq.Instance = ndev;

	/*
	  General socket configuration defaults can go here.  In this
	  client, we assume very little, and rely on the CIS for almost
	  everything.  In most clients, many details (i.e., number, sizes,
	  and attributes of IO windows) are fixed by the nature of the
	  device, and can be hard-wired here.
	*/
	link->conf.Attributes = CONF_ENABLE_IRQ;
	link->conf.IntType = INT_MEMORY_AND_IO;
	link->conf.Present = PRESENT_OPTION | PRESENT_COPY;
	
	/* Allocate space for private device-specific data */
	local = kzalloc(sizeof(local_info_t), GFP_KERNEL);
	if (!local) {
		printk(KERN_ERR "acx_cs: no memory for new device\n");
		return -ENOMEM;
	}
	local->ndev = ndev;
	
	link->priv = local;

	return acx_cs_config(link);
} /* acx_attach */

/*======================================================================
  
  This deletes a driver "instance".  The device is de-registered
  with Card Services.  If it has been released, all local data
  structures are freed.  Otherwise, the structures will be freed
  when the device is released.
  
  ======================================================================*/

static void acx_cs_detach(struct pcmcia_device *link)
{
	DEBUG(0, "acx_detach(0x%p)\n", link);


	if ( ((local_info_t*)link->priv)->ndev ) {
		acxmem_e_close( ((local_info_t*)link->priv)->ndev );
	}

	acx_cs_release(link);

	((local_info_t*)link->priv)->ndev = NULL;

	kfree(link->priv);
} /* acx_detach */

/*======================================================================
  
  acx_config() is scheduled to run after a CARD_INSERTION event
  is received, to configure the PCMCIA socket, and to make the
  device available to the system.
  
  ======================================================================*/

#define CS_CHECK(fn, ret) \
do { last_fn = (fn); if ((last_ret = (ret)) != 0) goto cs_failed; } while (0)

static int acx_cs_config(struct pcmcia_device *link)
{
	tuple_t tuple;
	cisparse_t parse;
	local_info_t *local = link->priv;
	int last_fn, last_ret;
	u_char buf[64];
	win_req_t req;
	memreq_t map;
//	int i;
//        acx_device_t *adev;

//	adev = (acx_device_t *)link->priv;

	DEBUG(0, "acx_cs_config(0x%p)\n", link);

	/*
	  In this loop, we scan the CIS for configuration table entries,
	  each of which describes a valid card configuration, including
	  voltage, IO window, memory window, and interrupt settings.
	  
	  We make no assumptions about the card to be configured: we use
	  just the information available in the CIS.  In an ideal world,
	  this would work for any PCMCIA card, but it requires a complete
	  and accurate CIS.  In practice, a driver usually "knows" most of
	  these things without consulting the CIS, and most client drivers
	  will only use the CIS to fill in implementation-defined details.
	*/
    tuple.Attributes = 0;
    tuple.TupleData = (cisdata_t *)buf;
    tuple.TupleDataMax = sizeof(buf);
    tuple.TupleOffset = 0;
    tuple.DesiredTuple = CISTPL_CFTABLE_ENTRY;

    /* don't trust the CIS on this; Linksys got it wrong */
    //link->conf.Present = 0x63;

    CS_CHECK(GetFirstTuple, pcmcia_get_first_tuple(link, &tuple));
        while (1) {
                cistpl_cftable_entry_t dflt = { 0 };
                cistpl_cftable_entry_t *cfg = &(parse.cftable_entry);
                if (pcmcia_get_tuple_data(link, &tuple) != 0 ||
                                pcmcia_parse_tuple(link, &tuple, &parse) != 0)
                        goto next_entry;

                if (cfg->flags & CISTPL_CFTABLE_DEFAULT) dflt = *cfg;
                if (cfg->index == 0) goto next_entry;
                link->conf.ConfigIndex = cfg->index;

                /* Does this card need audio output? */
                if (cfg->flags & CISTPL_CFTABLE_AUDIO) {
                        link->conf.Attributes |= CONF_ENABLE_SPKR;
                        link->conf.Status = CCSR_AUDIO_ENA;
                }

                /* Use power settings for Vcc and Vpp if present */
                /*  Note that the CIS values need to be rescaled */
                if (cfg->vpp1.present & (1<<CISTPL_POWER_VNOM))
                        link->conf.Vpp =
                                cfg->vpp1.param[CISTPL_POWER_VNOM]/10000;
                else if (dflt.vpp1.present & (1<<CISTPL_POWER_VNOM))
                        link->conf.Vpp =
                                dflt.vpp1.param[CISTPL_POWER_VNOM]/10000;

                /* Do we need to allocate an interrupt? */
                if (cfg->irq.IRQInfo1 || dflt.irq.IRQInfo1)
                        link->conf.Attributes |= CONF_ENABLE_IRQ;
                if ((cfg->mem.nwin > 0) || (dflt.mem.nwin > 0)) {
                        cistpl_mem_t *mem =
                                (cfg->mem.nwin) ? &cfg->mem : &dflt.mem;
//                        req.Attributes = WIN_DATA_WIDTH_16|WIN_MEMORY_TYPE_AM|WIN_ENABLE|WIN_USE_WAIT;
                      req.Attributes = WIN_DATA_WIDTH_16|WIN_MEMORY_TYPE_CM|WIN_ENABLE|WIN_USE_WAIT;
                        req.Base = mem->win[0].host_addr;
                        req.Size = mem->win[0].len;
                        req.Size=0x1000;
                        req.AccessSpeed = 0;
                        if (pcmcia_request_window(&link, &req, &link->win) != 0)
                                goto next_entry;
                        map.Page = 0; map.CardOffset = mem->win[0].card_addr;
                        if (pcmcia_map_mem_page(link->win, &map) != 0)
                                goto next_entry;
                        else
                           printk(KERN_INFO "MEMORY WINDOW FOUND!!!\n");
                }
                /* If we got this far, we're cool! */
                break;

        next_entry:
                CS_CHECK(GetNextTuple, pcmcia_get_next_tuple(link, &tuple));
        }

       if (link->conf.Attributes & CONF_ENABLE_IRQ) {
                printk(KERN_INFO "requesting Irq...\n");
                CS_CHECK(RequestIRQ, pcmcia_request_irq(link, &link->irq));
       }

        /*
          This actually configures the PCMCIA socket -- setting up
          the I/O windows and the interrupt mapping, and putting the
          card and host interface into "Memory and IO" mode.
        */
        CS_CHECK(RequestConfiguration, pcmcia_request_configuration(link, &link->conf));
	DEBUG(0,"RequestConfiguration OK\n");


          memwin.Base=req.Base;
          memwin.Size=req.Size;

	acx_init_netdev(local->ndev, &link->dev, memwin.Base, memwin.Size, link->irq.AssignedIRQ);

#if 1	
	/*
	  At this point, the dev_node_t structure(s) need to be
	  initialized and arranged in a linked list at link->dev_node.
	*/
	strcpy(local->node.dev_name, local->ndev->name );
	local->node.major = local->node.minor = 0;
	link->dev_node = &local->node;
	
	/* Finally, report what we've done */
	printk(KERN_INFO "%s: index 0x%02x: ",
	       local->ndev->name, link->conf.ConfigIndex);
#endif
	if (link->conf.Attributes & CONF_ENABLE_IRQ)
		printk("irq %d", link->irq.AssignedIRQ);
	if (link->io.NumPorts1)
		printk(", io 0x%04x-0x%04x", link->io.BasePort1,
		       link->io.BasePort1+link->io.NumPorts1-1);
	if (link->io.NumPorts2)
		printk(" & 0x%04x-0x%04x", link->io.BasePort2,
		       link->io.BasePort2+link->io.NumPorts2-1);
	if (link->win)
		printk(", mem 0x%06lx-0x%06lx\n", req.Base,
		       req.Base+req.Size-1);
	return 0;

 cs_failed:
	cs_error(link, last_fn, last_ret);
	acx_cs_release(link);
	return -ENODEV;
} /* acx_config */

/*======================================================================
  
  After a card is removed, acx_release() will unregister the
  device, and release the PCMCIA configuration.  If the device is
  still open, this will be postponed until it is closed.
  
  ======================================================================*/

static void acx_cs_release(struct pcmcia_device *link)
{
	DEBUG(0, "acx_release(0x%p)\n", link);
	acxmem_e_remove(link);
	pcmcia_disable_device(link);
}

static int acx_cs_suspend(struct pcmcia_device *link)
{
	local_info_t *local = link->priv;

	pm_message_t state;
	acxmem_e_suspend ( local->ndev, state);
	/* Already done in suspend
	 * netif_device_detach(local->ndev); */

	return 0;
}

static int acx_cs_resume(struct pcmcia_device *link)
{
	local_info_t *local = link->priv;
	
	FN_ENTER;
	resume_ndev = local->ndev;

	schedule_work( &fw_resume_work );
	
	/* Already done in suspend
	if (link->open) {
	  // do we need reset for ACX, if so what function nane is ?
		//reset_acx_card(local->eth_dev);
		netif_device_attach(local->ndev);
	} */
	
	FN_EXIT0;
	return 0;
}

static struct pcmcia_device_id acx_ids[] = {
	PCMCIA_DEVICE_MANF_CARD(0x0097, 0x8402),
        PCMCIA_DEVICE_MANF_CARD(0x0250, 0xb001),
	PCMCIA_DEVICE_NULL,
};
MODULE_DEVICE_TABLE(pcmcia, acx_ids);

static struct pcmcia_driver acx_driver = {
	.owner		= THIS_MODULE,
	.drv		= {
		.name	= "acx_cs",
	},
	.probe		= acx_cs_probe,
	.remove		= acx_cs_detach,
	.id_table = acx_ids,
	.suspend	= acx_cs_suspend,
	.resume		= acx_cs_resume,
};

int acx_cs_init(void)
{
        /* return success if at least one succeeded */
	DEBUG(0, "acxcs_init()\n");
	return pcmcia_register_driver(&acx_driver);
}

void acx_cs_cleanup(void)
{
	pcmcia_unregister_driver(&acx_driver);
}

/*
    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License
    as published by the Free Software Foundation; either version 2
    of the License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    In addition:

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions
    are met:

    1. Redistributions of source code must retain the above copyright
       notice, this list of conditions and the following disclaimer.
    2. Redistributions in binary form must reproduce the above copyright
       notice, this list of conditions and the following disclaimer in the
       documentation and/or other materials provided with the distribution.
    3. The name of the author may not be used to endorse or promote
       products derived from this software without specific prior written
       permission.

    THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
    IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
    WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
    ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
    INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
    (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
    SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
    HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
    STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
    IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
    POSSIBILITY OF SUCH DAMAGE.    
*/

MODULE_DESCRIPTION( "ACX Cardbus Driver" );
MODULE_LICENSE( "GPL" );

