/*
 *  linux/include/asm-arm/mach/serial_h3800.h
 *
 *  Author: Andrew Christian
 */

struct uart_port;
struct uart_info;

/*
 * This is a temporary structure for registering these
 * functions; it is intended to be discarded after boot.
 */
struct serial_h3800_fns {
	void	(*pm)(struct uart_port *, u_int, u_int);
	int	(*open)(struct uart_port *, struct uart_info *);
	void	(*close)(struct uart_port *, struct uart_info *);
};

#if defined(CONFIG_SERIAL_SA1100) && (defined(CONFIG_SERIAL_H3800_ASIC) || defined(CONFIG_SERIAL_H3800_ASIC_MODULE))
int  register_serial_h3800(int index);
void unregister_serial_h3800(int line);
void register_serial_h3800_fns(struct serial_h3800_fns *fns);
#else
#define register_serial_h3800(index) do { } while (0)
#define unregister_serial_h3800(line) do { } while (0)
#define register_serial_h3800_fns(fns) do {} while (0)
#endif

