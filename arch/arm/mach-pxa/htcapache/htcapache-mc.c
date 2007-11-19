/*
 * Support for the "mysterious" micro-controller attached to gpio pins
 * 56/57 on the HTC Apache phone.  This controller controls the lcd
 * power, the front keypad, and front LEDs.
 *
 * (c) Copyright 2006 Kevin O'Connor <kevin@koconnor.net>
 *
 * This file may be distributed under the terms of the GNU GPL license.
 */

#include <linux/kernel.h>
#include <linux/module.h> // EXPORT_SYMBOL

#include <asm/arch/hardware.h> // __REG
#include <asm/arch/pxa-regs.h> // GPLR1
#include <asm/arch/htcapache-gpio.h>

// The HTC Apache phone has a device attached to gpio pins 56 and 57.
// This device appears to follow the i2c system with some minor
// deviations.  It uses a 4bit register number instead of a 7 bit
// device-id address.

#define dprintf1(fmt, args...) printk(KERN_DEBUG fmt "\n" , ##args)
#define dprintf(fmt, args...)

#define GPLR_BIT(n) (GPLR((n)) & GPIO_bit((n)))
#define GPSR_BIT(n) (GPSR((n)) = GPIO_bit((n)))
#define GPCR_BIT(n) (GPCR((n)) = GPIO_bit((n)))

#define GET_SDA() GPLR_BIT(GPIO_NR_HTCAPACHE_MC_SDA)
#define SET_SDA() GPSR_BIT(GPIO_NR_HTCAPACHE_MC_SDA)
#define CLR_SDA() GPCR_BIT(GPIO_NR_HTCAPACHE_MC_SDA)
#define DIR_IN_SDA() (GPDR(GPIO_NR_HTCAPACHE_MC_SDA) &= ~GPIO_bit(GPIO_NR_HTCAPACHE_MC_SDA))
#define DIR_OUT_SDA() (GPDR(GPIO_NR_HTCAPACHE_MC_SDA) |= GPIO_bit(GPIO_NR_HTCAPACHE_MC_SDA))

#define SET_SCL() GPSR_BIT(GPIO_NR_HTCAPACHE_MC_SCL)
#define CLR_SCL() GPCR_BIT(GPIO_NR_HTCAPACHE_MC_SCL)


// Read 'count' bits from SDA pin.  The caller must ensure the SDA pin
// is set to input mode before calling.  The SCL pin must be low.
static inline u32
read_bits(int count)
{
	u32 data = 0;
	int i = count;

	for (; i>0; i--) {
		data <<= 1;

		SET_SCL();

		if (GET_SDA())
			data |= 1;
		dprintf(2, "Read bit of %d", data & 1);

		CLR_SCL();
	}
	dprintf(1, "Read %d bits: 0x%08x", count, data);
	return data;
}

// Write 'count' bits to SDA pin.  The caller must ensure the SDA pin
// is set to output mode before calling.  The SCL pin must be low.
static inline void
write_bits(u32 data, int count)
{
	u32 bit = 1<<(count-1);
	dprintf(1, "Writing %d bits: 0x%08x", count, data);

	for (; count>0; count--) {
		if (data & bit)
			SET_SDA();
		else
			CLR_SDA();
		dprintf(2, "Write bit of %d", !!(data & bit));

		SET_SCL();
		CLR_SCL();

		data <<= 1;
	}
}

// Set the SDA line to be input/output.
static inline void
set_sda_output(int isOutput)
{
	dprintf(2, "Set SDA output to %d", isOutput);
	if (isOutput)
		DIR_OUT_SDA();
	else
		DIR_IN_SDA();
}

// Init the gpio lines, and send a stop followed by a start
static inline void
stopstart(void)
{
	dprintf(1, "Stop Start");

	// Init lines
	set_sda_output(1);
	CLR_SCL();
	CLR_SDA();

	// Send stop
	SET_SCL();
	SET_SDA();

	// Send start
	CLR_SDA();
	CLR_SCL();
}

// Read a register from the bit-banging interface.
u32
htcapache_read_mc_reg(u32 reg)
{
	stopstart();

	// Set read indicator as low order bit.
	reg <<= 1;
	reg |= 1;

	// Send register
	write_bits(reg, 5);

	// Get ack.
	set_sda_output(0);
	reg = read_bits(1);
	if (! reg) {
		printk(KERN_WARNING "Missing ack on register send\n");
		return 0;
	}

	// Read register value.
	reg = read_bits(9);
	if (!(reg & 1)) {
		printk(KERN_WARNING "Missing ack on register read\n");
		return 0;
	}
	reg >>= 1;

	return reg;
}
EXPORT_SYMBOL(htcapache_read_mc_reg);

// Write a register on the bit-banging interface.
void
htcapache_write_mc_reg(u32 reg, u32 data)
{
	stopstart();

	// Set write indicator as low order bit.
	reg <<= 1;

	// Send register
	write_bits(reg, 5);

	// Get ack.
	set_sda_output(0);
	reg = read_bits(1);
	if (! reg) {
		printk(KERN_WARNING "Missing ack on register send\n");
		return;
	}
	set_sda_output(1);

	// write register value.
	write_bits(data, 8);
	set_sda_output(0);
	reg = read_bits(1);
	if (!reg) {
		printk(KERN_WARNING "Missing ack on register write\n");
		return;
	}
}
EXPORT_SYMBOL(htcapache_write_mc_reg);
