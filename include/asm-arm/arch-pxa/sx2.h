/*
 * linux/include/asm-arm/arch-pxa/sx2.h
 *
 * This supports machine-specific differences in how the Cypress SX2
 * USB Device Controller (UDC) is wired.
 *
 */

struct sx2_udc_mach_info {
        u32	virt_base;

	int	ready_pin;
	int	int_pin;
	int	reset_pin;
	int	power_pin;

	int	sloe_pin;
	int	slrd_pin;
	int	slwr_pin;
};

