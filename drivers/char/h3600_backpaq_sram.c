/*
*
* Driver for the Compaq iPAQ Mercury Backpaq sram
*
* Copyright 2001 Compaq Computer Corporation.
*
* Use consistent with the GNU GPL is permitted,
* provided that this copyright notice is
* preserved in its entirety in all copies and derived works.
*
* COMPAQ COMPUTER CORPORATION MAKES NO WARRANTIES, EXPRESSED OR IMPLIED,
* AS TO THE USEFULNESS OR CORRECTNESS OF THIS CODE OR ITS
* FITNESS FOR ANY PARTICULAR PURPOSE.
*                   
* Author: Jamey Hicks
*         <jamey@handhelds.org>
*/

#include <linux/module.h>
#include <linux/version.h>

#include <linux/init.h>
#include <asm/uaccess.h>         /* get_user,copy_to_user*/

#include <asm/arch-sa1100/backpaq.h>
#include <linux/delay.h>
#include <linux/devfs_fs_kernel.h>
#include <linux/kmod.h>


#define MODULE_NAME      "h3600_backpaq_sram"

#define SRAM_MINOR        0
#define SRAM_DEVICE_NAME "backpaq/sram"

/* Global variables */

static devfs_handle_t           devfs_sram;
static int    h3600_backpaq_sram_major_num = 0;

struct h3600_sram_struct {
    /* General parameters common to all camera types */	
    unsigned long max_size;
    wait_queue_head_t       sramq;                /* Wait queue for sram transaction */
    struct semaphore        lock;          /* Force single access */
};
static struct h3600_sram_struct g_sram;     /* We only have a single sram*/

//#define BACKPAQ_SRAM_DEBUG
#undef BACKPAQ_SRAM_DEBUG

#ifdef BACKPAQ_SRAM_DEBUG
#define SRAMDEBUG(format,args...)  printk(KERN_ALERT __FILE__ format, ## args)
#define PRINTDEBUG(format,args...)  printk(__FUNCTION__ format, ## args)
#else
#define SRAMDEBUG(format,args...)
#define PRINTDEBUG(format,args...)
#endif

#define SRAMERROR(format,args...)  printk(KERN_ERR  __FILE__ format, ## args)

/* 512*1024 - 2 since we have 256Kx16 bytes*/
#define BACKPAQ_SRAM_MAX_ADDR 524286
#define SRAM_CHUNK_SIZE 128

/* Useful externals */

int h3600_backpaq_fpga_status(void);





static int __h3600_sram_startup( struct h3600_sram_struct *sram )
{
        init_MUTEX(&sram->lock);
        init_waitqueue_head(&sram->sramq);
        return 0;
        
}



/*
 * low level read functions
 *
 * 
 */

ssize_t __h3600_backpaq_sram_read_hw(struct h3600_sram_struct *sram, loff_t addr,unsigned short *data)
{
        

        int retval;
	wait_queue_t  wait;
	signed long   timeout;

        PRINTDEBUG(" entered\n" );
        
        // check for an error
        if (((BackpaqFPGASramStatus & FPGA_SRAM_STATUS_BITS_MASK) & BACKPAQ_SRAM_ERROR)) 
                retval = -EFAULT;

        // make sure we are within our address space
        if (addr > BACKPAQ_SRAM_MAX_ADDR)
                return -ENOMEM;
        
        
	/* We're basically using interruptible_sleep_on_timeout */
	/* and waiting for the xaction to finish or time to run out */
	init_waitqueue_entry(&wait,current);
	add_wait_queue(&(sram->sramq), &wait);
	timeout = 10 * HZ / 1000;    /* 1 milliseconds (a guess :-)) */
        // wait for a pending xaction to complete if there is one
	while ( timeout > 0 ) {
		set_current_state( TASK_INTERRUPTIBLE );
		if (!((BackpaqFPGASramStatus & FPGA_SRAM_STATUS_BITS_MASK) & BACKPAQ_SRAM_BUSY)) 
		    break;
		
		if ( signal_pending(current) ){
                        PRINTDEBUG(" left  1 \n" );                        
                        return -ERESTARTSYS;
                }
                
		timeout = schedule_timeout( timeout );
		if ( timeout <= 0 ) {
                        //PRINTDEBUG(" timeout 1 \n" );
			/* timeouts are ok since the action will have completed by then       */
		}
	}
	set_current_state( TASK_RUNNING );
	remove_wait_queue(&(sram->sramq), &wait);

        // do our xaction:
        BackpaqFPGASramReadAddr = addr;


        /* we need to wait for the read transaction to complete so ourdata is avail*/        
	/* We're basically using interruptible_sleep_on_timeout */
	/* and waiting for the xaction to finish or time to run out */
	init_waitqueue_entry(&wait,current);
	add_wait_queue(&(sram->sramq), &wait);
	timeout = 10 * HZ / 1000;    /* 1 milliseconds (a guess :-)) */
        // wait for a pending xaction to complete if there is one
	while ( timeout > 0 ) {
		set_current_state( TASK_INTERRUPTIBLE );
		if (!((BackpaqFPGASramStatus & FPGA_SRAM_STATUS_BITS_MASK) & BACKPAQ_SRAM_BUSY)) 
		    break;
		
		if ( signal_pending(current) ) {
                        PRINTDEBUG(" left 3 \n" );
                        return -ERESTARTSYS;
                }
                
		timeout = schedule_timeout( timeout );
		if ( timeout <= 0 ) {
                        //PRINTDEBUG(" timeout 2 \n" );
			/* timeouts are ok since the action will have completed by then       */
		}
	}
	set_current_state( TASK_RUNNING );
	remove_wait_queue(&(sram->sramq), &wait);

        
        *data = BackpaqFPGASramReadData & FPGA_SRAM_STATUS_MASK ;
        PRINTDEBUG(" data = 0x%x addr = 0x%lx\n",*data,(unsigned long) addr );
        
        // check for an error
        if (((BackpaqFPGASramStatus & FPGA_SRAM_STATUS_BITS_MASK) & BACKPAQ_SRAM_ERROR)) {
                PRINTDEBUG(" left 5 \n" );
                retval = -EFAULT;
        }
        
        return 0;        

}


// this one need to deal with alignment
ssize_t __h3600_backpaq_sram_read(struct h3600_sram_struct *sram, loff_t *f_pos,unsigned char *kbuf,size_t size)
{
        size_t lcount=0;
        unsigned short data;
        int ret = 0;

        PRINTDEBUG(" size = 0x%x offset = 0x%lx\n",size,(unsigned long) *f_pos );        
        while (lcount < size){
                if ( (ret = h3600_backpaq_fpga_status()) != 0)
                        return (lcount>0)?lcount:ret;
                if ((ret = __h3600_backpaq_sram_read_hw(sram, *f_pos,&data))){
                        PRINTDEBUG(" bad read_hw ret = %d \n",ret );        
                        return (lcount>0)?lcount:ret;
                }

                if ((size-lcount) == 1){ //a last byte write requires a read and rewrite of a halfword
                        PRINTDEBUG(" (size-lcount) == 1\n");                        
                        kbuf[lcount] = (data & 0x00ff);
                        *f_pos+=1;
                        lcount+=1;

                }
                else{
                        kbuf[lcount] = (data & 0x00ff);
                        kbuf[lcount+1] = ((data & 0xff00)>>8);
                        *f_pos+=2;
                        lcount+=2;

                }
                PRINTDEBUG(" lcount = 0x%x offset = 0x%lx\n",lcount,(unsigned long) *f_pos );
        }
        return lcount;
        
}


/*
 * low level write functions
 *
 * 
 */

ssize_t __h3600_backpaq_sram_write_hw(struct h3600_sram_struct *sram, loff_t addr,unsigned short data)
{
        
        int retval;
	wait_queue_t  wait;
	signed long   timeout;

        PRINTDEBUG(" data = 0x%x addr = 0x%lx\n",data,(unsigned long) addr );
        
        // check for an error
        if (((BackpaqFPGASramStatus & FPGA_SRAM_STATUS_BITS_MASK) & BACKPAQ_SRAM_ERROR)) 
                retval = -EFAULT;

        // make sure we are within our address space
        if (addr > BACKPAQ_SRAM_MAX_ADDR)
                return -ENOMEM;
        

	/* We're basically using interruptible_sleep_on_timeout */
	/* and waiting for the xaction to finish or time to run out */
	init_waitqueue_entry(&wait,current);
	add_wait_queue(&(sram->sramq), &wait);
	timeout = 10 * HZ / 1000;    /* 1 milliseconds (a guess :-)) */
        // wait for a pending xaction to complete if there is one
	while ( timeout > 0 ) {
		set_current_state( TASK_INTERRUPTIBLE );
		if (!((BackpaqFPGASramStatus & FPGA_SRAM_STATUS_BITS_MASK) & BACKPAQ_SRAM_BUSY)) 
		    break;
		
		if ( signal_pending(current) ) 
		    return -ERESTARTSYS;

		timeout = schedule_timeout( timeout );
		if ( timeout <= 0 ) {
                        /* timeouts are ok since any action will have completed by then */                        
		}
	}
	set_current_state( TASK_RUNNING );
	remove_wait_queue(&(sram->sramq), &wait);

        // do our xaction:
        BackpaqFPGASramWriteAddr = addr;
        BackpaqFPGASramWriteData = data;
        // note, we don;t need to wait for the xaction to complete but any next xaction needs to wait
        // for a pending xaction to complete by checking the busybit before doing anything
        // check for an error
        if (((BackpaqFPGASramStatus & FPGA_SRAM_STATUS_BITS_MASK) & BACKPAQ_SRAM_ERROR)) 
                retval = -EFAULT;        
        return 0;        

}

// this one need to deal with alignment
ssize_t __h3600_backpaq_sram_write(struct h3600_sram_struct *sram, loff_t *f_pos,unsigned char *kbuf,size_t size)
{
        size_t lcount=0;
        unsigned short data;
        int ret = 0;

        PRINTDEBUG(" size = 0x%x offset = 0x%lx\n",size,(unsigned long) *f_pos );
        while (lcount < size){
                if ( (ret = h3600_backpaq_fpga_status()) != 0)
                        return (lcount>0)?lcount:ret;
                if ((size - lcount) == 1){ //a last byte write requires a read and rewrite of a halfword
                        PRINTDEBUG(" size-lcount == 1\n" );                        
                        if ((ret = __h3600_backpaq_sram_read_hw(sram, *f_pos,&data))){
                                return (lcount>0)?lcount:ret;
                        }
                        data &= 0xff00;                        
                        data |= kbuf[lcount];                        
                }
                else{
                        data = (kbuf[lcount+1] << 8) | kbuf[lcount];
                }
                // now do the write
                if (!(ret = __h3600_backpaq_sram_write_hw(sram, *f_pos,data))){
                        if ((size - lcount) == 1){ 
                                *f_pos+=1;
                                lcount+=1;
                        }
                        else{
                                *f_pos+=2;
                                lcount+=2;
                        }
                        
                }
                else // some kinda error either return num bytes succ written or the error if none written
                        return (lcount>0)?lcount:ret;
        }
        return lcount;
        
}


/**************************************************************/
static int h3600_backpaq_sram_ioctl(struct inode *inode, struct file * filp,
                    unsigned int cmd, unsigned long arg)
{
	return -ENOIOCTLCMD;
}


static long long h3600_backpaq_sram_llseek(struct file *filp, loff_t off, int whence)
{
        loff_t newpos;

        PRINTDEBUG(" origin = %d\toffset = 0x%ld\n",whence,(long) off );
        switch(whence) {
                case 0: /* SEEK_SET */
                        newpos = off;
                        break;

                case 1: /* SEEK_CUR */
                        newpos = filp->f_pos + off;
                        break;

                case 2: /* SEEK_END */
                        newpos =  BACKPAQ_SRAM_MAX_ADDR + off;
                        break;

                default: /* can't happen */
                        return -EINVAL;
    }
        if (newpos<0) return -EINVAL;
        if (newpos>BACKPAQ_SRAM_MAX_ADDR) return -EINVAL;
        if(newpos%2) return -EINVAL;
        filp->f_pos = newpos;
        return newpos;
}

ssize_t h3600_backpaq_sram_read( struct file *filp, char *buf, 
				  size_t count, loff_t *f_pos )
{
        unsigned char kbuf[SRAM_CHUNK_SIZE];
        ssize_t ret = -ENOMEM; /* value used in "goto out" statements */        
        size_t lcount = 0;
        struct h3600_sram_struct *sram = &g_sram;
        size_t size;        
        
        PRINTDEBUG(" count = 0x%x\toffset = 0x%lx\n",count,(unsigned long) *f_pos );
        // aligned access nly
        if ((*f_pos)%2)
                return -EINVAL;
        
        if (down_interruptible(&sram->lock))
                return -ERESTARTSYS;
        if ( (ret = h3600_backpaq_fpga_status()) != 0)
		return ret;
        PRINTDEBUG(" count = 0x%x\toffset = 0x%lx\n",count,(unsigned long)*f_pos );

        while (lcount < count){
                size = (count<SRAM_CHUNK_SIZE) ? count : SRAM_CHUNK_SIZE;                
                ret = __h3600_backpaq_sram_read(sram,f_pos,kbuf,size);                

                if (ret > 0){
                        if (copy_to_user(buf+lcount, kbuf, ret)) {
                                ret = -EFAULT;
                                goto out;
                        }
                        lcount += ret;
                }
                else
                        goto out;                
        }
        
                
  out:
        if (lcount > 0)
                ret=lcount;        
        up(&sram->lock);
	return ret;
}

/*
 * Write function.
 *
 * 
 */
ssize_t h3600_backpaq_sram_write( struct file *filp, const char *buf, 
				   size_t count, loff_t *f_pos )
{
        unsigned char kbuf[SRAM_CHUNK_SIZE];
        ssize_t ret = -ENOMEM; /* value used in "goto out" statements */        
        size_t lcount = 0;
        struct h3600_sram_struct *sram = &g_sram;
        size_t size;

        PRINTDEBUG(" count = 0x%x\toffset = 0x%lx\n",count,(unsigned long)*f_pos );
        if ((*f_pos)%2)
                return -EINVAL;
        
        if (down_interruptible(&sram->lock))
                return -ERESTARTSYS;
        if ( (ret = h3600_backpaq_fpga_status()) != 0)
		return ret;


        while (lcount < count){
                size = (count<SRAM_CHUNK_SIZE) ? count : SRAM_CHUNK_SIZE;                
                if (copy_from_user(kbuf, buf+lcount, size)) {
                        ret = -EFAULT;
                        goto out;
                }
                PRINTDEBUG(" lcount = 0x%x size = 0x%lx\n",lcount,(unsigned long)size );                
                if ((ret = __h3600_backpaq_sram_write(sram,f_pos,kbuf,size))  > 0)
                        lcount += ret;
        }
        
                
  out:
        if (lcount > 0)
                ret=lcount;        
        up(&sram->lock);
	return ret;
}
  
int h3600_backpaq_sram_open( struct inode *inode, struct file *filp )
{
        PRINTDEBUG("\n" );
        // force an error clear on open.  if the device errors during use, you will need to close and reopen it.
        BackpaqFPGASramStatusClear = 1;
        
	return 0;    /* Success */
}

int h3600_backpaq_sram_release( struct inode *inode, struct file *filp )
{
	int result = 0;
        PRINTDEBUG("\n" );
        // force an error clear on close in case some fpga code is relying on it.
        BackpaqFPGASramStatusClear = 1;

	return result;
}


struct file_operations h3600_backpaq_sram_fops = {
	owner:		THIS_MODULE,
	read:    h3600_backpaq_sram_read,
	write:   h3600_backpaq_sram_write,
	open:    h3600_backpaq_sram_open,
	release: h3600_backpaq_sram_release,
	llseek:	 h3600_backpaq_sram_llseek,        
	ioctl:   h3600_backpaq_sram_ioctl,
};



/***********************************************************************************/

/***********************************************************************************/

#ifdef MODULE
#define FPGA_MODULE "h3600_backpaq_fpga"

int __init h3600_backpaq_sram_init_module(void)    
{    
	int result;    

	/* This module only makes sense if h3600_backpaq_fpga is loaded */
	result = request_module(FPGA_MODULE);
	if ( result ) {
		printk(KERN_ALERT __FILE__ ": unable to load " FPGA_MODULE "\n");
		return result;
	}

	/* Register my device driver */
	printk(KERN_ALERT __FILE__ ": registering char device");    
	result = devfs_register_chrdev(0,MODULE_NAME, &h3600_backpaq_sram_fops);    
	if ( result <= 0 ) {
		printk(" can't get major number\n");
		return result;    
	}    

	if ( h3600_backpaq_sram_major_num == 0 )
		h3600_backpaq_sram_major_num = result;
	printk(" %d\n", h3600_backpaq_sram_major_num);


        result = __h3600_sram_startup( &g_sram );
        if ( result )
		return result;


#ifdef CONFIG_DEVFS_FS
	devfs_sram = devfs_register( NULL, SRAM_DEVICE_NAME, DEVFS_FL_DEFAULT,
                                         h3600_backpaq_sram_major_num, SRAM_MINOR,
                                         S_IFCHR | S_IRUSR | S_IWUSR, 
                                         &h3600_backpaq_sram_fops, NULL );
	if ( !devfs_sram ) {
		printk(KERN_ALERT __FILE__ ": unable to register %s\n", SRAM_DEVICE_NAME );
		devfs_unregister_chrdev( h3600_backpaq_sram_major_num, MODULE_NAME );
		return -ENODEV;
	}
#endif

	return 0;    
} 

void __exit h3600_backpaq_sram_exit_module(void)
{
	printk(KERN_ALERT __FILE__ ": exit\n");

	devfs_unregister( devfs_sram );
	devfs_unregister_chrdev( h3600_backpaq_sram_major_num, MODULE_NAME );
}


MODULE_AUTHOR("Jamey Hicks <jamey.hicks@hp.com>");
MODULE_DESCRIPTION("SRAM driver for Compaq Mercury BackPAQ");
MODULE_LICENSE("Dual BSD/GPL");

module_init(h3600_backpaq_sram_init_module);
module_exit(h3600_backpaq_sram_exit_module);

#endif /* MODULE */
