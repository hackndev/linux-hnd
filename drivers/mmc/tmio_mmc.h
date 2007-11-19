/* Definitons for use with the tmio_mmc.c
 *
 * (c) 2005 Ian Molton <spyro@f2s.com>
 *
 */

#define TMIO_CONF_COMMAND_REG        0x04  // This is NOT the SD command reg.
#define   SDCREN 0x2   // Enable access to MMC CTL regs. (flag in COMMAND_REG)
#define TMIO_CONF_SD_CTRL_BASE_ADDRESS_REG 0x10
#define TMIO_CONF_STOP_CLOCK_CONTROL_REG   0x40
#define TMIO_CONF_CLOCK_MODE_REG     0x42
#define TMIO_CONF_POWER_CNTL_REG_2   0x49
#define TMIO_CONF_POWER_CNTL_REG_3   0x4a


#define TMIO_CTRL_CMD_REG                         0x000
#define TMIO_CTRL_ARG_REG_BASE                    0x004
#define TMIO_CTRL_STOP_INTERNAL_ACTION_REG        0x008
#define TMIO_CTRL_TRANSFER_BLOCK_COUNT_REG        0x00a
#define TMIO_CTRL_RESP_REG_BASE                   0x00c
#define TMIO_CTRL_STATUS_REG_BASE                 0x01c
#define TMIO_CTRL_IRQMASK_REG_BASE                0x020
#define TMIO_CTRL_SD_CARD_CLOCK_CONTROL_REG       0x024
#define TMIO_CTRL_DATALENGTH_REG                  0x026
#define TMIO_CTRL_SD_MEMORY_CARD_OPTION_SETUP_REG 0x028
#define TMIO_CTRL_DATA_REG                        0x030
#define TMIO_CTRL_CLOCK_AND_WAIT_CONTROL_REG      0x138
#define TMIO_CTRL_SD_SOFT_RESET_REG               0x0e0
#define TMIO_CTRL_SDIO_SOFT_RESET_REG             0x1e0

/* Note that the ARG, CMD_RESP, STATUS, and IRQMASK registers are
 * really two registers which we acess simultaneously for convienience.
 * See the read/write_long_reg macros below. 
 */

//FIXME - how convienient this really is Im not sure. I may change it.

/* Definitions for values the CTRL_STATUS register can take. */
#define TMIO_STAT_CMDRESPEND    0x00000001
#define TMIO_STAT_DATAEND       0x00000004
#define TMIO_STAT_CARD_REMOVE   0x00000008
#define TMIO_STAT_CARD_INSERT   0x00000010
#define TMIO_STAT_SIGSTATE      0x00000020
#define TMIO_STAT_WRPROTECT     0x00000080
#define TMIO_STAT_CARD_REMOVE_A 0x00000100
#define TMIO_STAT_CARD_INSERT_A 0x00000200
#define TMIO_STAT_SIGSTATE_A    0x00000400
#define TMIO_STAT_CMD_IDX_ERR   0x00010000
#define TMIO_STAT_CRCFAIL       0x00020000
#define TMIO_STAT_STOPBIT_ERR   0x00040000
#define TMIO_STAT_DATATIMEOUT   0x00080000
#define TMIO_STAT_RXOVERFLOW    0x00100000
#define TMIO_STAT_TXUNDERRUN    0x00200000
#define TMIO_STAT_CMDTIMEOUT    0x00400000
#define TMIO_STAT_RXRDY         0x01000000
#define TMIO_STAT_TXRQ          0x02000000
#define TMIO_STAT_ILL_FUNC      0x20000000
#define TMIO_STAT_CMD_BUSY      0x40000000
#define TMIO_STAT_ILL_ACCESS    0x80000000

/* Define some IRQ masks */
/* This is the mask used at reset by the chip */
#define TMIO_MASK_ALL           0x837f031d
#define TMIO_MASK_READOP  (TMIO_STAT_RXRDY | TMIO_STAT_DATAEND | \
                           TMIO_STAT_CARD_REMOVE | TMIO_STAT_CARD_INSERT)
#define TMIO_MASK_WRITEOP (TMIO_STAT_TXRQ | TMIO_STAT_DATAEND | \
                           TMIO_STAT_CARD_REMOVE | TMIO_STAT_CARD_INSERT)
#define TMIO_MASK_CMD     (TMIO_STAT_CMDRESPEND | TMIO_STAT_CMDTIMEOUT | \
                           TMIO_STAT_CARD_REMOVE | TMIO_STAT_CARD_INSERT)
#define TMIO_MASK_IRQ     (TMIO_MASK_READOP | TMIO_MASK_WRITEOP | TMIO_MASK_CMD)

#define read_long_reg(reg) (u32)(readw((reg)) | (readw((reg)+2) << 16))

#define write_long_reg(value, reg) \
	do { \
		writew((u32)(value) & 0xffff, (u32)(reg)); \
		writew((u32)(value) >> 16, (u32)(reg)+2); \
	} while(0)

#define enable_mmc_irqs(h, i) \
	do { \
		unsigned int mask;\
		mask  = read_long_reg((h)->ctl_base + TMIO_CTRL_IRQMASK_REG_BASE); \
		mask &= ~((i) & TMIO_MASK_IRQ); \
		write_long_reg(mask, (h)->ctl_base + TMIO_CTRL_IRQMASK_REG_BASE); \
	} while (0)

#define disable_mmc_irqs(h, i) \
	do { \
		unsigned int mask;\
		mask  = read_long_reg((h)->ctl_base + TMIO_CTRL_IRQMASK_REG_BASE); \
		mask |= ((i) & TMIO_MASK_IRQ); \
		write_long_reg(mask, (h)->ctl_base + TMIO_CTRL_IRQMASK_REG_BASE); \
	} while (0)

#define ack_mmc_irqs(h, i) \
	do { \
		unsigned int mask;\
		mask  = read_long_reg((h)->ctl_base + TMIO_CTRL_STATUS_REG_BASE); \
		mask &= ~((i) & TMIO_MASK_IRQ); \
		write_long_reg(mask, (h)->ctl_base + TMIO_CTRL_STATUS_REG_BASE); \
	} while (0)


struct tmio_mmc_host {
	void                    *ctl_base;
	void                    *cnf_base;
	struct mmc_command      *cmd;
	struct mmc_request      *mrq;
	struct mmc_data         *data;
	struct mmc_host         *mmc;
	int                     irq;

	/* pio related stuff */
	struct scatterlist      *sg_ptr;
	unsigned int            sg_len;
	unsigned int            sg_off;
};


static inline void tmio_mmc_init_sg(struct tmio_mmc_host *host, struct mmc_data *data)
{
	host->sg_len = data->sg_len;
	host->sg_ptr = data->sg;
	host->sg_off = 0;
}

static inline int tmio_mmc_next_sg(struct tmio_mmc_host *host)
{
	host->sg_ptr++;
	host->sg_off = 0;
	return --host->sg_len;
}

static inline char *tmio_mmc_kmap_atomic(struct tmio_mmc_host *host, unsigned long *flags)
{
	struct scatterlist *sg = host->sg_ptr;

	local_irq_save(*flags);
	return kmap_atomic(sg->page, KM_BIO_SRC_IRQ) + sg->offset;
}

static inline void tmio_mmc_kunmap_atomic(struct tmio_mmc_host *host, unsigned long *flags)
{
	kunmap_atomic(host->sg_ptr->page, KM_BIO_SRC_IRQ);
	local_irq_restore(*flags);
}

#ifdef CONFIG_MMC_DEBUG
#define DBG(args...)    printk(args)

void debug_status(u32 status){
	printk("status: %08x = ", status);
	if(status & TMIO_STAT_CARD_REMOVE) printk("Card_removed ");
	if(status & TMIO_STAT_CARD_INSERT) printk("Card_insert ");
	if(status & TMIO_STAT_SIGSTATE) printk("Sigstate ");
	if(status & TMIO_STAT_WRPROTECT) printk("Write_protect ");
	if(status & TMIO_STAT_CARD_REMOVE_A) printk("Card_remove_A ");
	if(status & TMIO_STAT_CARD_INSERT_A) printk("Card_insert_A ");
	if(status & TMIO_STAT_SIGSTATE_A) printk("Sigstate_A ");
	if(status & TMIO_STAT_CMD_IDX_ERR) printk("Cmd_IDX_Err ");
	if(status & TMIO_STAT_STOPBIT_ERR) printk("Stopbit_ERR ");
	if(status & TMIO_STAT_ILL_FUNC) printk("ILLEGAL_FUNC ");
	if(status & TMIO_STAT_CMD_BUSY) printk("CMD_BUSY ");
	if(status & TMIO_STAT_CMDRESPEND)  printk("Response_end ");
	if(status & TMIO_STAT_DATAEND)     printk("Data_end ");
	if(status & TMIO_STAT_CRCFAIL)     printk("CRC_failure ");
	if(status & TMIO_STAT_DATATIMEOUT) printk("Data_timeout ");
	if(status & TMIO_STAT_CMDTIMEOUT)  printk("Command_timeout ");
	if(status & TMIO_STAT_RXOVERFLOW)  printk("RX_OVF ");
	if(status & TMIO_STAT_TXUNDERRUN)  printk("TX_UND ");
	if(status & TMIO_STAT_RXRDY)       printk("RX_rdy ");
	if(status & TMIO_STAT_TXRQ)        printk("TX_req ");
	if(status & TMIO_STAT_ILL_ACCESS)  printk("ILLEGAL_ACCESS ");
	printk("\n");
}
#else
#define DBG(fmt,args...)        do { } while (0)
#endif

