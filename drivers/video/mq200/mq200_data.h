/*
 * From ucLinux mq200fb.c and mq200fb.h
 */

#ifndef __MQ200_FB_H__
#define __MQ200_FB_H__

struct mq200_io_regions {
	u32 fb_size;                    /* framebuffer size                  */
	unsigned long phys_mmio_base;   /* physical register memory base     */
	unsigned long virt_mmio_base;    /* virtual start of registers       */
	unsigned long phys_fb_base;     /* physical address of frame buffer  */
	unsigned long virt_fb_base;     /* virtual start of the framebuffer  */
};

#define MQ200_MONITOR_HORI_RES(info)    info->monitor_info.horizontal_res
#define MQ200_MONITOR_VERT_RES(info)    info->monitor_info.vertical_res
#define MQ200_MONITOR_DEPTH(info)       info->monitor_info.depth
#define MQ200_MONITOR_LINE_LENGTH(info) info->monitor_info.line_length

struct mq200_monitor_info {
	unsigned int horizontal_res;
	unsigned int vertical_res;
	unsigned int depth;
	unsigned int refresh;
	unsigned int line_length;
	unsigned long flags;
};


/**
 * Addresses of Module
 */
#define MQ200_FB_BASE  (x) (x + 0x1800000) /* framebuffer */
#define MQ200_FB_SIZE      0x200000 /* framebuffer size in bytes */
#define MQ200_REGS_BASE(x) (x + 0x1e00000) /* start of registers area */
#define MQ200_REGS_SIZE    0x200000 /* registers area size */

#define PMU_OFFSET 0x00000     /* power management */
#define CPU_OFFSET 0x02000     /* CPU interface */
#define MIU_OFFSET 0x04000     /* memory controller */
#define IN_OFFSET  0x08000     /* interrupt controller */
#define GC_OFFSET  0x0a000     /* graphics controller 1&2 */
#define GE_OFFSET  0x0c000     /* graphics engine */
#define FPI_OFFSET 0x0e000     /* flat panel controller */
#define CP1_OFFSET 0x10000     /* color palette 1 */
#define DC_OFFSET  0x14000     /* device configuration */
#define PCI_OFFSET 0x16000     /* PCI configuration */
#define PSF_OFFSET 0x18000     /* ??? */


/****
 * Registers
 */

/* power management unit */
#define PMR(addr) (addr + PCI_OFFSET + 0x40)/* power management
					     register */
#define PMR_VALUE 0x06210001   /* expected read value of PMR register */
#define PM00R(addr) (addr + PMU_OFFSET + 0x00) /* power management unit
						configuration
						register */
#define PM01R(addr) (addr + PMU_OFFSET + 0x04) /* D1 state control */
#define PM02R(addr) (addr + PMU_OFFSET + 0x08) /* d2 state control */
#define PM06R(addr) (addr + PMU_OFFSET + 0x18) /* PLL 2 programming */
#define PM07R(addr) (addr + PMU_OFFSET + 0x1c) /* PLL 3 programming */

#define PMCSR(addr) (addr + PCI_OFFSET + 0x44) /* power management
						  control/status
						  register */

/* memory interface unit */
#define MM00R(addr) (addr + MIU_OFFSET + 0x00)/* MIU interface control
						  0 */
#define MM01R(addr) (addr + MIU_OFFSET + 0x04) /* MIU interface control
						  1 */
#define MM02R(addr) (addr + MIU_OFFSET + 0x08) /* memory interface
						  control 2 */
#define MM03R(addr) (addr + MIU_OFFSET + 0x0c) /* memory interface
						  control 3 */
#define MM04R(addr) (addr + MIU_OFFSET + 0x10) /* memory interface
						  control 4 */
/* graphics controller 1 module */
#define GC00R(addr) (addr + GC_OFFSET + 0x00) /* graphics controller 1
                                                 control */
#define GC01R(addr) (addr + GC_OFFSET + 0x04) /* graphics controller
                                                 CRT control */
#define GC02R(addr) (addr + GC_OFFSET + 0x08) /* horizontal display 1
                                                 control */
#define GC03R(addr) (addr + GC_OFFSET + 0x0c) /* vertical display 1
                                                 control */
#define GC04R(addr) (addr + GC_OFFSET + 0x10) /* horizontal sync 1
                                                 control */
#define GC05R(addr) (addr + GC_OFFSET + 0x14) /* vertical sync 1
                                                 control */
#define GC07R(addr) (addr + GC_OFFSET + 0x1c) /* vertical display 1
                                                 count */
#define GC08R(addr) (addr + GC_OFFSET + 0x20) /* horizontal window 1
                                                 control */
#define GC09R(addr) (addr + GC_OFFSET + 0x24) /* vertical window 1
                                                 control */
#define GC0AR(addr) (addr + GC_OFFSET + 0x28) /* alternate horizontal
                                                 window 1 control */
#define GC0BR(addr) (addr + GC_OFFSET + 0x2c) /* alternate vertical
                                                 window 1 control */
#define GC0CR(addr) (addr + GC_OFFSET + 0x30) /* window 1 
                                                 start address */
#define GC0DR(addr) (addr + GC_OFFSET + 0x34) /* alternate window 1
                                                 start address */
#define GC0ER(addr) (addr + GC_OFFSET + 0x38) /* window 1 stride */
#define GC0FR(addr) (addr + GC_OFFSET + 0x3c) /* reserved */
#define GC10R(addr) (addr + GC_OFFSET + 0x40) /* hardware cursor 1
                                                 position */
#define GC11R(addr) (addr + GC_OFFSET + 0x44) /* hardware cursor 1
                                                 start address and
                                                 offset */
#define GC12R(addr) (addr + GC_OFFSET + 0x48) /* hardware cursor 1
                                                 foreground color */
#define GC13R(addr) (addr + GC_OFFSET + 0x4c) /* hardware cursor 1
                                                 background color */
/* graphics engine */
#define ROP_SRCCOPY     0xCC    /* dest = source */
#define ROP_SRCPAINT    0xEE    /* dest = source OR dest */
#define ROP_SRCAND      0x88    /* dest = source AND dest */
#define ROP_SRCINVERT   0x66    /* dest = source XOR dest */
#define ROP_SRCERASE    0x44    /* dest = source AND (NOT dest) */
#define ROP_NOTSRCCOPY  0x33    /* dest = NOT source */
#define ROP_NOTSRCERASE 0x11    /* dest = (NOT source) AND (NOT dest) */
#define ROP_MERGECOPY   0xC0    /* dest = source AND pattern */
#define ROP_MERGEPAINT  0xBB    /* dest = (NOT source) OR dest */
#define ROP_PATCOPY     0xF0    /* dest = pattern */
#define ROP_PATPAINT    0xFB    /* dest = DPSnoo */
#define ROP_PATINVERT   0x5A    /* dest = pattern XOR dest */
#define ROP_DSTINVERT   0x55    /* dest = NOT dest */
#define ROP_BLACKNESS   0x00    /* dest = BLACK */
#define ROP_WHITENESS   0xFF    /* dest = WHITE */

#define GE00R(addr) (addr + GE_OFFSET + 0x00) /* primary drawing command
						 register */
#define GE01R(addr) (addr + GE_OFFSET + 0x04) /* primary width and
						 height register */
#define GE02R(addr) (addr + GE_OFFSET + 0x08) /* primary destination
						 address register */
#define GE03R(addr) (addr + GE_OFFSET + 0x0c) /* primary source XY
						 register */
#define GE04R(addr) (addr + GE_OFFSET + 0x10) /* primary color compare
						 register */
#define GE05R(addr) (addr + GE_OFFSET + 0x14) /* primary clip left/top
						 register */
#define GE06R(addr) (addr + GE_OFFSET + 0x18) /* primary clip
						 right/bottom register
                                                  */
#define GE07R(addr) (addr + GE_OFFSET + 0x1c) /* primary source and
						 pattern offset
						 register */
#define GE08R(addr) (addr + GE_OFFSET + 0x20) /* primary foreground
						 color
						 register/rectangle
						 fill register */
#define GE09R(addr) (addr + GE_OFFSET + 0x24) /* source stride/offset
						 register */
#define GE0AR(addr) (addr + GE_OFFSET + 0x28) /* destination stride
						 register and color
						 depth */
#define GE0BR(addr) (addr + GE_OFFSET + 0x2c) /* image base address
						 register */
#define GE40R(addr) (addr + GE_OFFSET + 0x100) /* mono pattern register
						  0 */
#define GE41R(addr) (addr + GE_OFFSET + 0x104) /* mono pattern register
						  1 */
#define GE42R(addr) (addr + GE_OFFSET + 0x108) /* foreground color
						  register */
#define GE43R(addr) (addr + GE_OFFSET + 0x10c) /* background color
                                                  register */
/* color palette */
#define C1xxR(addr, regno) \
       (addr + CP1_OFFSET + (regno) * 4) /* graphics controller color
					    palette 1 */
/* device configuration */
#define DC00R(addr) (addr + DC_OFFSET + 0x00)  /* device configuration
						  register 0 */
#define DC_RESET 0x4000
/* PCI configuration space */
#define PC00R(addr) (addr + PCI_OFFSET + 0x00)/* device ID/vendor ID
						 register */
/* Flatpanel Control */
#define FP00R(addr) (addr + FPI_OFFSET + 0x00) /* Flat Panel Control 0       */
#define FP01R(addr) (addr + FPI_OFFSET + 0x04) /* Flat Panel Output Pin      */
#define FP02R(addr) (addr + FPI_OFFSET + 0x08) /* Flat Panel Gener Purpose
						  Outout Control Register    */
#define FP03R(addr) (addr + FPI_OFFSET + 0x0c) /* General Purpose I/O Port
						  Control Register           */
#define FP04R(addr) (addr + FPI_OFFSET + 0x10) /* STN Panel Control Register */
#define FP05R(addr) (addr + FPI_OFFSET + 0x14) /* D-STN Half Frame Buffer
						  Control Register -By Guess */
#define FP0FR(addr) (addr + FPI_OFFSET + 0x3c) /* Pulse Width Modulation
						  Control Register           */
#define FRCTL_PATTERN_COUNT 32
#define FP10R(addr) (addr + FPI_OFFSET + 0x40) /* Frame-Rate Control Pattern
						  Register */
#define FP11R(addr) (addr + FPI_OFFSET + 0x44)
#define FP2FR(addr) (addr + FPI_OFFSET + 0xc0) /* Frame-Rate Control Weight
						  Registers */




/* power management miscellaneous control */
union pm00r {
    struct {
       u32 pll1_n_b5   :1;     /* PLL 1 N parameter bit 5 is 0 */
       u32 reserved_1  :1;
       u32 pll2_enbl   :1;     /* PLL 2 enable */
       u32 pll3_enbl   :1;     /* PLL 3 enable */
       u32 reserved_2  :1;
       u32 pwr_st_ctrl :1;     /* power state status control */
       u32 reserved_3  :2;

       u32 ge_enbl     :1;     /* graphics engine enable */
       u32 ge_bsy_gl   :1;     /* graphics engine force busy (global) */
       u32 ge_bsy_lcl  :1;     /* graphics engine force busy (local) */
       u32 ge_clock    :2;     /* graphics engine clock select */
       u32 ge_cmd_fifo :1;     /* graphics engine command FIFO reset */
       u32 ge_src_fifo :1;     /* graphics engine CPU source FIFO reset */
       u32 miu_pwr_seq :1;     /* memory interface unit power sequencing
                                  enable */

       u32 d3_mem_rfsh :1;     /* D3 memory refresh */
       u32 d4_mem_rfsh :1;     /* D4 memory refresh */
       u32 gpwr_intrvl :2;     /* general power sequencing interval */
       u32 fppwr_intrvl:2;     /* flat panel power sequencing interval */
       u32 gpwr_seq_ctr:1;     /* general power sequencing interval control */
       u32 pmu_tm      :1;     /* PMU test mode */

       u32 pwr_state   :2;     /* power state (read only) */
       u32 pwr_seq_st  :1;     /* power sequencing active status (read
                                  only) */
       u32 reserved_4  :5;
    }  part;
    u32 whole;
};

/* D1 state control */
union pm01r {
    struct {
       u32 osc_enbl    :1;     /* D1 oscillator enable */
       u32 pll1_enbl   :1;     /* D1 PLL 1 enable */
       u32 pll2_enbl   :1;     /* D1 PLL 2 enable */
       u32 pll3_enbl   :1;     /* D1 PLL 3 enable */
       u32 miu_enbl    :1;     /* D1 Memory Interface Unit (MIU) enable */
       u32 mem_rfsh    :1;     /* D1 memory refresh enable */
       u32 ge_enbl     :1;     /* D1 Graphics Engine (GE) enable */
       u32 reserved_1  :1;

       u32 crt_enbl    :1;     /* D1 CRT enable */
       u32 fpd_enbl    :1;     /* D1 Flat Panel enable */
       u32 reserved_2  :6;

       u32 ctl1_enbl   :1;     /* D1 controller 1 enable */
       u32 win1_enbl   :1;     /* D1 window 1 enable */
       u32 awin1_enbl  :1;     /* D1 alternate window 1 enable */
       u32 cur1_enbl   :1;     /* D1 cursor 1 enable */
      u32 reserved_3  :4;

       u32 ctl2_enbl   :1;     /* D1 controller 2 enable */
       u32 win2_enbl   :1;     /* D1 window 2 enable */
       u32 awin2_enbl  :1;     /* D1 alternate window 2 enable */
       u32 cur2_enbl   :1;     /* D1 cursor 2 enable */
       u32 reserved_4  :4;
    }  part;
    u32 whole;
};

/* D2 state control */
union pm02r {
    struct {
       u32 osc_enbl    :1;     /* D2 oscillator enable */
       u32 pll1_enbl   :1;     /* D2 PLL 1 enable */
       u32 pll2_enbl   :1;     /* D2 PLL 2 enable */
       u32 pll3_enbl   :1;     /* D2 PLL 3 enable */
       u32 miu_enbl    :1;     /* D2 Memory Interface Unit (MIU) enable */
       u32 mem_rfsh    :1;     /* D2 memory refresh enable */
       u32 ge_enbl     :1;     /* D2 Graphics Engine (GE) enable */
       u32 reserved_1  :1;

       u32 crt_enbl    :1;     /* D2 CRT enable */
       u32 fpd_enbl    :1;     /* D2 Flat Panel enable */
       u32 reserved_2  :6;

       u32 ctl1_enbl   :1;     /* D2 controller 1 enable */
       u32 win1_enbl   :1;     /* D2 window 1 enable */
       u32 awin1_enbl  :1;     /* D2 alternate window 1 enable */
       u32 cur1_enbl   :1;     /* D2 cursor 1 enable */
       u32 reserved_3  :4;

       u32 ctl2_enbl   :1;     /* D2 controller 2 enable */
       u32 win2_enbl   :1;     /* D2 window 2 enable */
       u32 awin2_enbl  :1;     /* D2 alternate window 2 enable */
       u32 cur2_enbl   :1;     /* D2 cursor 2 enable */
       u32 reserved_4  :4;
    }  part;
    u32 whole;
};

/* PLL 2 programming */
union pm06r {
    struct {
       u32 clk_src     :1;     /* PLL 2 reference clock source */
       u32 bypass      :1;     /* PLL 2 bypass */
       u32 reserved_1  :2;
       u32 p_par       :3;     /* PLL 2 P parameter */
       u32 reserved_2  :1;

       u32 n_par       :5;     /* PLL 2 N parameter */
       u32 reserved_3  :3;

       u32 m_par       :8;     /* PLL 2 M parameter */

       u32 reserved_4  :4;
       u32 trim        :4;     /* PLL 2 trim value */
    }  part;
    u32 whole;
};

/* PLL 3 programming */
union pm07r {
    struct {
       u32 clk_src     :1;     /* PLL 3 reference clock source */
       u32 bypass      :1;     /* PLL 3 bypass */
       u32 reserved_1  :2;
       u32 p_par       :3;     /* PLL 3 P parameter */
       u32 reserved_2  :1;

       u32 n_par       :5;     /* PLL 3 N parameter */
       u32 reserved_3  :3;

       u32 m_par       :8;     /* PLL 3 M parameter */

       u32 reserved_4  :4;
       u32 trim        :4;     /* PLL 3 trim value */
    }  part;
    u32 whole;
};



/* MIU interface control 1 */
union mm00r {
    struct {
       u32 miu_enbl    :1;     /* MIU enable bit */
       u32 mr_dsbl     :1;     /* MIU reset disable bit */
       u32 edr_dsbl    :1;     /* embedded DRAM reset disable bit */
       u32 reserved_1  :29;
    }  part;
    u32 whole;
};

/* MIU interface control 2 */
union mm01r {
    struct {
       u32 mc_src      :1;     /* memory clock source */
       u32 msr_enbl    :1;     /* memory slow refresh enable bit */
       u32 pb_cpu      :1;     /* page break enable for CPU */
       u32 pb_gc1      :1;     /* page break enable for GC1 */
       u32 pb_gc2      :1;     /* page break enable for GC2 */
       u32 pb_stn_r    :1;     /* page break enable for STN read */
       u32 pb_stn_w    :1;     /* page break enable for STN write */
       u32 pb_ge       :1;     /* page break enable for GE */
       u32 reserved_1  :4;
       u32 mr_interval :14;    /* normal memory refresh time interval */
       u32 reserved_2  :4;
       u32 edarm_enbl  :1;     /* embedded DRAM auto-refresh mode enable */
       u32 eds_enbl    :1;     /* EDRAM standby enable for EDRAM normal
                                  mode operation */
    }  part;
    u32 whole;
};

/* memory interface control 3 */
union mm02r {
    struct {
       u32 bs_         :2;
       u32 bs_stnr     :2;     /* burst count for STN read memory cycles */
       u32 bs_stnw     :2;     /* burst count for STN write memroy cycles */
       u32 bs_ge       :2;     /* burst count for graphics engine
                                  read/write memroy cycles */
       u32 bs_cpuw     :2;     /* burst count for CPU write memory cycles */
       u32 fifo_gc1    :4;     /* GC1 display refresh FIFO threshold */
       u32 fifo_gc2    :4;     /* GC2 display refresh FIFO threshold */
       u32 fifo_stnr   :4;     /* STN read FIFO threshold */
       u32 fifo_stnw   :4;     /* STN write FIFO threshold */
       u32 fifo_ge_src :3;     /* GE source read FIFO threshold */
       u32 fifo_ge_dst :3;     /* GE destination read FIFO threshold */
    }  part;
    u32 whole;
};

/* memory interface control 4 */
union mm03r {
    struct {
       u32 rd_late_req :1;     /* read latency request */
       u32 reserved_1  :31;
    }  part;
    u32 whole;
};

/* memory interface control 5 */
union mm04r {
    struct {
       u32 latency     :3;     /* EDRAM latency */
       u32 dmm_cyc     :1;     /* enable for the dummy cycle insertion
                                  between read and write cycles */
       u32 pre_dmm_cyc :1;     /* enable for the dummy cycle insertion
                                  between read/write and precharge cycles
                                  for the same bank */
       u32 reserved_1  :3;
       u32 bnk_act_cls :2;     /* bank activate command to bank close
                                  command timing interval control */
       u32 bnk_act_rw  :1;     /* bank activate command to read/wirte
                                  command timing interval control */
       u32 bnk_cls_act :1;     /* bank close command to bank activate
                                  command timing interval control */
       u32 trc         :1;     /* row cycle time */
       u32 reserved_2  :3;
       u32 delay_r     :2;     /* programmable delay for read clock */
       u32 delay_m     :2;     /* programmable delay for internal memory
                                  clock */
    }  part;
    u32 whole;
};

/* graphics controller 1 register */
union gc00r {
    struct {
       u32 ctl_enbl    :1;     /* Controller 1 Enable */
       u32 hc_reset    :1;     /* Horizontal Counter 1 Reset */
       u32 vc_reset    :1;     /* Vertical Counter 1 Reset */
       u32 iwin_enbl   :1;     /* Image Window 1 Enable */
       u32 gcd         :4;     /* Graphics Color Depth (GCD) */

       u32 hc_enbl     :1;     /* Hardware Cursor 1 Enable */
       u32 reserved_1  :2;
       u32 aiwin_enbl  :1;     /* Alternate Image Window Enable */
       u32 agcd        :4;     /* Alternate Graphics Color Depth (AGCD) */

       u32 g1rclk_src  :2;     /* G1RCLK Source */
       u32 tm0         :1;     /* Test Mode 0 */
       u32 tm1         :1;     /* Test Mode 1 */
       u32 fd          :3;     /* G1MCLK First Clock Divisor (FD1) */
       u32 reserved_2  :1;

       u32 sd          :8;     /* G1MCLK Second Clock Divisor (SD1) */
    }  part;
    u32 whole;
};

/* graphics controller CRT control */
union gc01r {
    struct {
       u32 dac_enbl    :2;     /* CRT DAC enable */
       u32 hsync_out   :1;     /* CRT HSYNC output during power down mode */
       u32 vsync_out   :1;     /* CRT VSYNC output during power down mode */
       u32 hsync_ctl   :2;     /* CRT HSYNC control */
       u32 vsync_ctl   :2;     /* CRT VSYNC control */
       /**/
       u32 hsync_pol   :1;     /* CRT HSYNC polarity */
       u32 vsync_pol   :1;     /* CRT VSYNC polarity */
       u32 sync_p_enbl :1;     /* sync pedestal enable */
       u32 blnk_p_enbl :1;     /* blank pedestal enable */
       u32 c_sync_enbl :1;     /* composite sync enable */
       u32 vref_sel    :1;     /* VREF select */
       u32 mn_sns_enbl :1;     /* monitor sense enable */
       u32 ct_out_enbl :1;     /* constant output enable */
       /**/
       u32 dac_out_lvl :8;     /* monitor sense DAC output level */
       /**/
       u32 blue_dac_r  :1;     /* blue DAC sense result */
       u32 green_dac_r :1;     /* green DAC sense result */
       u32 red_dac_r   :1;     /* red DAC sense result */
       u32 reserved_1  :1;
       u32 mon_col_sel :1;     /* mono/color monitor select */
       u32 reserved_2  :3;
    }  part;
    u32 whole;
};

/* horizontal display 1 control */
union gc02r {
    struct {
       u32 hd1t        :12;    /* horizontal display 1 total */
       u32 reserved_1  :4;

       u32 hd1e        :12;    /* horizontal display 1 end */
       u32 reserved_2  :4;
    }  part;
    u32 whole;
};

/* vertical display 1 control */
union gc03r {
    struct {
       u32 vd1t        :12;    /* vertical display 1 total */
       u32 reserved_1  :4;

       u32 vd1e        :12;    /* vertical display 1 end */
       u32 reserved_2  :4;
    }  part;
    u32 whole;
};

/* horizontal sync 1 control */
union gc04r {
    struct {
       u32 hs1s        :12;    /* horizontal sync 1 start */
       u32 reserved_1  :4;

       u32 hs1e        :12;    /* horizontal sync 1 end */
       u32 reserved_2  :4;
    }  part;
    u32 whole;
};

/* vertical sync 1 control */
union gc05r {
    struct {
       u32 vs1s        :12;    /* vertical sync 1 start */
       u32 reserved_1  :4;

       u32 vs1e        :12;    /* vertical sync 1 end */
       u32 reserved_2  :4;
    }  part;
    u32 whole;
};

/* vertical display 1 count */
union gc07r {
    struct {
       u32 vd_cnt      :12;    /* vertical display 1 count */
       u32 reverved_1  :20;
    }  part;
    u32 whole;
};

/* horizontal window 1 control */
union gc08r {
    struct {
       u32 hw1s        :12;    /* horizontal window 1 start (HW1S) */
       u32 reserved_1  :4;

       u32 hw1w        :12;    /* horizontal window 1 width (HW1W) */
       u32 w1ald       :4;     /* window 1 additional line data */
    }  part;
    u32 whole;
};

/* vertical window 1 control */
union gc09r {
    struct {
       u32 vw1s        :12;    /* vertical window 1 start */
       u32 reserved_1  :4;
       u32 vw1h        :12;    /* vertical window 1 height */
       u32 reserved_2  :4;
    }  part;
    u32 whole;
};

/* window 1 start address */
union gc0cr {
    struct {
       u32 w1sa        :21;    /* window 1 start address */
       u32 reserved_1  :11;
    }  part;
    u32 whole;
};

/* window 1 stride */
union gc0er {
    struct {
       s16 w1st;               /* window 1 stride */
       s16 aw1st;              /* alternate window 1 stride */
    }  part;
    u32 whole;
};

/* hardware cursor 1 position */
union gc10r {
    struct {
       u32 hc1s        :12;    /* horizontal cursor 1 start */
       u32 reserved_1  :4;
       u32 vc1s        :12;    /* vertical cursor 1 start */
       u32 reserved_2  :4;
    }  part;
    u32 whole;
};

/* hardware cursor 1 start address and offset */
union gc11r {
    struct {
       u32 hc1sa       :11;    /* hardware cursor 1 start address */
       u32 reserved_1  :5;
       u32 hc1o        :6;     /* horizontal cursor 1 offset */
       u32 reserved_2  :2;
       u32 vc1o        :6;     /* vertical cursor 1 offset */
       u32 reserved_3  :2;
    }  part;
    u32 whole;
};

/* hardware cursor 1 foreground color */
union gc12r {
    struct {
       u32 hc1fc       :24;    /* hardware cursor 1 foreground color */
       u32 reserved_1  :8;
    }  part;
    u32 whole;
};

/* hardware cursor 1 background color */
union gc13r {
    struct {
       u32 hc1bc       :24;    /* hardware cursor 1 background color */
       u32 reserved_1  :8;
    }  part;
    u32 whole;
};

/* primary drawing command register */
union ge00r {
    struct {
       u32 rop         :8;     /* raster operation */
       /**/
       u32 cmd_typ     :3;     /* command type */
       u32 x_dir       :1;     /* x direction */
       u32 y_dir       :1;     /* y direction */
       u32 src_mem     :1;     /* source memory */
       u32 mon_src     :1;     /* mono source */
       u32 mon_ptn     :1;     /* mono pattern */
       /**/
       u32 dst_trns_e  :1;     /* destination transparency enable */
       u32 dst_trns_p  :1;     /* destination transparency polarity */
       u32 mon_trns_e  :1;     /* mono source or mono pattern transparency
                                  enable */
       u32 mon_trns_p  :1;     /* mono transparency polarity */
       u32 mod_sel     :1;     /* memory to screen or off screen to screen
                                  mode select */
       u32 alpha_sel   :2;     /* Alpha byte mask selection */
       u32 sol_col     :1;     /* solid color */
       /**/
       u32 stride_eq   :1;     /* source stride is equal to destination
                                  stride */
       u32 rop2_sel    :1;     /* ROP2 code selection */
       u32 clipping    :1;     /* enable clipping */
       u32 auto_exec   :1;     /* auto execute */
       u32 reserved_1  :4;
    }  part;
    u32 whole;
};

/* primary width and height register */
union ge01r {
    struct {
       u32 width       :12;    /* source/destination window width */
       u32 reserved_1  :4;

       u32 height      :12;    /* source/destination window height */
       u32 reserved_2  :1;
       u32 reserved_3  :3;
    }  bitblt;
    struct {
       u32 dm          :17;
       u32 axis_major  :12;
       u32 x_y         :1;     /* x-major or y-major */
       u32 last_pix    :1;     /* decision to draw or not to draw the last
                                  pixel of the line */
       u32 reserved_1  :1;
    }  bresenham;
    u32 whole;
};

/* primary destination address register */
union ge02r {
    struct {
       u32 dst_x       :12;    /* destination x position */
       u32 reserved_1  :1;
       u32 h_offset    :3;     /* mono/color pattern horizontal offset */

       u32 dst_y       :12;    /* destination y position */
       u32 reserved_2  :1;
       u32 v_offset    :3;     /* mono/color pattern vertical offset */
    }  window;
    struct {
       u32 x           :12;    /* starting x coordinate */
       u32 dm          :17;    /* 17 bits major-axis delta */
       u32 reserved_1  :3;
    }  line;
    u32 whole;
};

/* source XY register/line draw starting Y coordinate and mintor axis delta */
union ge03r {
    struct {
       u32 src_x       :12;    /* source X position */
       u32 reserved_1  :4;

       u32 src_y       :12;    /* source Y position */
       u32 reserved_2  :4;
    }  window;
    struct {
       u32 start_y     :12;    /* starting Y coordinate */
       u32 dn          :17;    /* 17 bits minor-axis delta */
       u32 reserved_1  :3;
    }  line;
    u32 whole;
};

/* clip left/top register */
union ge05r {
    struct {
       u32 left        :12;    /* left edge of clipping rectangle */
       u32 reserved_1  :4;

       u32 top         :12;    /* top edge of clipping rectangle */
       u32 reserved_2  :4;
    }  part;
    u32 whole;
};

/* source stride/offset register */
union ge09r {
    struct {
       u32 src_strid   :12;    /* source line stride */
       u32 reserved_1  :13;
       u32 strt_bit    :3;     /* initial mono source bit offset */
       u32 strt_byte   :3;     /* initial mono/color source byte offset */
       u32 reserved_2  :1;
    }  line;
    struct {
       u32 strt_bit    :5;     /* initial mono source bit offset */
       u32 reserved_1  :1;
       u32 amount      :10;    /* number of 16 bytes amount that MIU need
                                  to fetch from frame buffer */

       u32 reserved_2  :9;
       u32 bit_spc     :7;     /* bit space between lines */
    }  pack_mono;
    struct {
       u32 strt_bit    :3;     /* initial mono source bit offset */
       u32 strt_byte   :3;     /* initial mono/color source byte offset */
       u32 amount      :10;    /* number of 16 bytes amount that MIU need
                                  to fetch from frame buffer */

       u32 reserved_1  :9;
       u32 bit_spc     :3;     /* bit space between lines */
       u32 byt_spc     :4;     /* byte space between lines */
    }  pack_color;
    u32 whole;
};

/* destination stride register and color depth */
union ge0ar {
    struct {
       u32 dst_strid   :12;    /* destination line stride and color depth */
       u32 reserved_1  :18;
       u32 col_dpth    :2;     /* color depth */
    }  part;
    u32 whole;
};

/* graphics controller color pallete */
union c1xxr {
    struct {
       u8 red;                 /* red color pallete */
       u8 green;               /* green/gray color pallete */
       u8 blue;                /* blue color palette */
       u8 reserved_1;
    }  part;
    u32 whole;
};

/* devicee configuration register 0 */
union dc00r {
    struct {
       u32 osc_bypass  :1;     /* oscillator bypass */
       u32 osc_enbl    :1;     /* oscillator enable */
       u32 pll1_bypass :1;     /* PLL1 bypass */
       u32 pll1_enbl   :1;     /* PLL1 enable */
       u32 pll1_p_par  :3;     /* PLL1 P parameter */
       u32 cpu_div     :1;     /* CPU interface clock divisor */
       u32 pll1_n_par  :5;     /* PLL1 N parameter */
       u32 saisc       :1;     /* StrongARM interface synchronizer control */
       u32 s_chp_reset :1;     /* software chip reset */
       u32 mem_enbl    :1;     /* memory standby enable */
       u32 pll1_m_par  :8;     /* PLL 1 M parameter */
       u32 osc_shaper  :1;     /* oscillator shaper disable */
       u32 fast_pwr    :1;     /* fast power sequencing */
       u32 osc_frq     :2;     /* oscillator frequency select */
       u32 pll1_trim   :4;     /* PLL 1 trim value */
    }  part;
    u32 whole;
};

/* device ID/vendor ID register */
union pc00r {
    struct {
       u16 device;       /* device ID */
       u16 vendor;       /* vendor ID */
    }  part;
    u32 whole;
};

/* Flat Panel Control Register */
union fp00r {
    struct {
	u32  flatp_enbl : 2;   /* Flat Panel Interface Enable */
	u32  flatp_type : 2;   /* Flat Panel Type */
	u32  mono       : 1;   /* Mono/Color Panel Select */
	u32  flatp_intf : 3;   /* Flat Panel Interface */
	u32  dither_pat : 2;   /* Dither Pattern */
	u32  reserved   : 2;   /* Reserved Must Be 0*/
	u32  dither_col : 3;   /* Dither Base Color */
	u32  alt_win_ctl: 1;   /* Alternate Window Control */
	u32  frc_ctl    : 2;   /* FRC Control */
	u32  dither_adj1: 6;   /* Dither Pattern Adjust 1 */
	u32  dither_adj2: 3;   /* Dither Pattern Adjust 2 */
	u32  dither_adj3: 1;   /* Dither Pattern Adjust 3 */
	u32  test_mode0 : 1;   /* Test Mode 0 */
	u32  test_mode1 : 1;   /* Test Mode 1 */
	u32  test_mode2 : 1;   /* Test Mode 2 */
	u32  test_mode3 : 1;   /* Test Mode 3 */
    } part;
    u32 whole;
};

union fp01r {
    struct {
	u32 dummy;
    } part;
    u32 whole;
};

union fp02r {
    struct {
	u32 dummy;
    } part;
    u32 whole;
};

union fp03r {
    struct {
	u32 dummy;
    } part;
    u32 whole;
};

union fp04r {
    struct {
	u32 dummy;
    } part;
    u32 whole;
};

union fp05r {
    struct {
	u32 dummy;
    } part;
    u32 whole;
};

union fp0fr {
    struct {
	u32 dummy;
    } part;
    u32 whole;
};




/****
 * Others
 */

#define CHIPNAME "MQ-200"

extern void mq200_external_setpal(unsigned regno, unsigned long color, unsigned long addr);
extern void mq200_external_setqmode(struct mq200_monitor_info*, unsigned long, spinlock_t *);
extern void mq200_external_offdisplay(unsigned long);
extern void mq200_external_ondisplay (unsigned long);
extern int  mq200_external_probe(unsigned long);



#endif
