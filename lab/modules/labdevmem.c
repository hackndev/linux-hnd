/* labdevmem.c
 * A devmem module for LAB - Linux As Bootldr.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h> 
#include <linux/ctype.h>
#include <asm/io.h>
#include <linux/lab/lab.h>
#include <linux/lab/commands.h>

#define MAP_SIZE PAGE_SIZE
#define MAP_MASK (MAP_SIZE - 1)

void lab_cmd_devmem2(int argc,const char** argv);

int labdevmem_init(void)
{
	lab_addcommand("devmem2", lab_cmd_devmem2, "devmem2-like command");
	
	return 0;
}

void labdevmem_cleanup(void)
{
	lab_delcommand("devmem2");
}

void lab_cmd_devmem2(int argc,const char** argv)
{
	void *map_base;
	void *virt_addr;
	int access_type = 'w';
	off_t target;
	unsigned long read_result, writeval;
	
	if(argc < 2) {
		lab_printf("\r\nUsage:\t%s { address } [ type [ data ] ]\r\n"
			   "\taddress : memory address to act upon\r\n"
			   "\ttype    : access operation type : [b]yte, [h]alfword, [w]ord\r\n"
			   "\tdata    : data to be written\r\n\n",
			   argv[0]);
		return;
	}
	
	target = simple_strtoul(argv[1], 0, 0);
	
	if (argc > 2)
		access_type = tolower(argv[2][0]);
	
	map_base = ioremap_nocache(target & ~MAP_MASK, MAP_SIZE);
	if (map_base == (void *) 0) {
		lab_puts("Failed to ioremap\r\n");
		return;
	}
	
	virt_addr = map_base + (target & MAP_MASK);
	
	if (argc < 4) {
		switch(access_type) {
		case 'b':
			read_result = *((unsigned char *) virt_addr);
			break;
		case 'h':
			read_result = *((unsigned short *) virt_addr);
			break;
		case 'w':
			read_result = *((unsigned long *) virt_addr);
			break;
		default:
			lab_printf("Illegal data type '%c'.\r\n", access_type);
			return;
		}
		lab_printf("Value at address 0x%X (%p): 0x%X\r\n",
			   target, virt_addr, read_result); 
	}
	
	if(argc > 3) {
		writeval = simple_strtoul(argv[3], 0, 0);
		switch(access_type) {
		case 'b':
			*((unsigned char *) virt_addr) = writeval;
			break;
		case 'h':
			*((unsigned short *) virt_addr) = writeval;
			break;
		case 'w':
			*((unsigned long *) virt_addr) = writeval;
			break;
		}
		lab_printf("Written 0x%X\r\n", (unsigned int)writeval); 
	}
	
	__iounmap(map_base);
}

MODULE_AUTHOR("Joshua Wise");
MODULE_DESCRIPTION("LAB devmem2 Module");
MODULE_LICENSE("GPL");
module_init(labdevmem_init);
module_exit(labdevmem_cleanup);
