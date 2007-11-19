#ifndef __ASIC3_MMC_H
#define __ASIC3_MMC_H

#define DRIVER_NAME	"asic3_mmc"

#ifdef CONFIG_MMC_DEBUG
#define DBG(x...)       printk(DRIVER_NAME ": " x)
#else
#define DBG(x...)       do { } while (0)
#endif

/* Response types */
#define APP_CMD        0x0040

#define SD_CONFIG_CLKSTOP_ENABLE_ALL 0x1f

#define DONT_CARE_CARD_BITS ( \
      SD_CTRL_INTMASKCARD_SIGNAL_STATE_PRESENT_3 \
    | SD_CTRL_INTMASKCARD_WRITE_PROTECT \
    | SD_CTRL_INTMASKCARD_UNK6 \
    | SD_CTRL_INTMASKCARD_SIGNAL_STATE_PRESENT_0 \
  )
#define DONT_CARE_BUFFER_BITS ( SD_CTRL_INTMASKBUFFER_UNK7 | SD_CTRL_INTMASKBUFFER_CMD_BUSY )

#endif // __ASIC3_MMC_H
