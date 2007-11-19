#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/i2c.h>
#include <asm/hardware.h>
#include <asm/types.h>
#include <linux/delay.h>
#include <asm/arch/palmz72-gpio.h>

#include "i2c-ov9640.h"
#include "ov9640_hw.h"

#define DEBUG 1
#define DPRINTK(fmt,args...)	do { if (DEBUG) printk("in function %s "fmt,__FUNCTION__,##args);} while(0)
extern int i2c_adapter_id(struct i2c_adapter *adap);

void i2c_ov9640_cleanup(void);
static int  i2c_ov9640_attach_adapter(struct i2c_adapter *adapter);
static int  i2c_ov9640_detect_client(struct i2c_adapter *, int, int);
static int  i2c_ov9640_detach_client(struct i2c_client *client);

struct i2c_driver ov9640_driver  = 
{
	.driver = {
		    .name = "i2c-ov9640",	            /* name           */
	},
	.attach_adapter = 	&i2c_ov9640_attach_adapter,       /* attach_adapter */
	.detach_client = 	&i2c_ov9640_detach_client,        /* detach_client  */
};

extern  struct i2c_adapter *i2cdev_adaps[];
struct i2c_client *g_client = NULL;
static unsigned short normal_i2c[] = {OV9640_SLAVE_ADDR ,I2C_CLIENT_END };
/* static unsigned short normal_i2c_range[] = { I2C_CLIENT_END }; */
I2C_CLIENT_INSMOD;

char ov9640_read(u8 addr, u8 *pvalue)
{
	int	res = 0;
	char 	buf = 0;
	struct i2c_msg msgs[2];
	struct ov9640_data * p;
	
	if( g_client == NULL )	
		return -1;
	p = i2c_get_clientdata(g_client);

	msgs[0].addr = g_client->addr;
	msgs[0].flags = 0;
	msgs[0].len = 1;
	msgs[0].buf = &addr;

	msgs[1].addr = g_client->addr;
	msgs[1].flags = I2C_M_RD;
	msgs[1].len = 1;
	msgs[1].buf = &buf;
	
	down(&p->update_lock);
	res=i2c_transfer(g_client->adapter,&msgs[0],1);
	if (res<=0) 
		goto out;
	res=i2c_transfer(g_client->adapter,&msgs[1],1);
	if (res<=0) 
		goto out;
	*pvalue = buf;
	up(&p->update_lock);
out:
	DPRINTK(KERN_INFO "In funtion %s addr:%x,value=%x\n", __FUNCTION__, addr,*pvalue);
	if (res<=0) DPRINTK("res = %d \n",res);
	return res;
}	

int ov9640_write(u8 addr, u8 value)
{
	int res = 0;
	char buf[2] = {addr, value};

	struct ov9640_data * p;
	if( g_client == NULL )
		return -1;
	p = i2c_get_clientdata(g_client);
	if(!p)
	    return -1;
	down(&p->update_lock);
	res = i2c_master_send(g_client, buf, 2);
	up(&p->update_lock);
	if (res >0) res =0;
	else res =-1;
	DPRINTK(KERN_INFO "In funtion %s addr:%x value:%xreturn %d \n", __FUNCTION__, addr,value,res);
	return res;
}	


int i2c_ov9640_read(struct i2c_client *client, u8 reg)
{
	unsigned char msgbuf=0;
	DPRINTK("in function %s\n",__FUNCTION__);
	i2c_master_recv(client,&msgbuf,1);
	return msgbuf;
}

int i2c_ov9640_write(struct i2c_client *client, u8 reg, u16 value)
{
    return i2c_smbus_write_word_data(client,reg,value);
}


int i2c_ov9640_attach_adapter(struct i2c_adapter *adap)
{
	DPRINTK("In function %s.\n", __FUNCTION__);
	return i2c_probe(adap,&addr_data,i2c_ov9640_detect_client);
}

#if 0
static int a780_camera_adapter_attach(struct i2c_adapter *adap)
{
	if(! (a780_camera_client = kmalloc(sizeof(struct i2c_client),GFP_KERNEL)))
		return -ENOMEM;
	memcpy(a780_camera_client,&client_template,sizeof(struct i2c_client));
	a780_camera_client->adapter = adap;
        
	a780_camera_client->addr = 0x5D;
	
	i2c_attach_client(a780_camera_client);
	return 0;
}	

#endif

static int i2c_ov9640_detect_client(struct i2c_adapter *adapter, int address,  int kind)
{
    struct i2c_client *new_client;
    int err = 0;
    char res = -1;
    struct ov9640_data *data;
    
    /*check if */
    if(g_client != NULL) {
      err = -ENXIO;
      goto ERROR0;
    }
 

	DPRINTK(KERN_INFO "In funtion %s. address=0X%X\n", __FUNCTION__, address);
    /* Let's see whether this adapter can support what we need.
       Please substitute the things you need here!  */
	if ( !i2c_check_functionality(adapter,I2C_FUNC_SMBUS_WORD_DATA) ) {
		DPRINTK(KERN_INFO "Word op is not permited.\n");
		goto ERROR0;
	}

    /* OK. For now, we presume we have a valid client. We now create the
       client structure, even though we cannot fill it completely yet.
       But it allows us to access several i2c functions safely */
    
    /* Note that we reserve some space for ov9640_data too. If you don't
       need it, remove it. We do it here to help to lessen memory
       fragmentation. */

    new_client=kzalloc(sizeof(struct i2c_client), GFP_KERNEL);

    if ( !new_client )  {
      err = -ENOMEM;
      goto ERROR0;
    }
    data=kzalloc(sizeof(struct ov9640_data), GFP_KERNEL );

    if ( !new_client )  {
      err = -ENOMEM;
      goto ERROR0;
    }

	new_client->addr = address;	
	new_client->adapter = adapter;
	new_client->driver = &ov9640_driver;
	new_client->flags = 0;
	init_MUTEX(&data->update_lock); 
	i2c_set_clientdata(new_client, data);

	g_client = new_client;
	ov9640_power_down(0);
	mdelay(1);

    /* Now, we do the remaining detection. If no `force' parameter is used. */

    /* First, the generic detection (if any), that is skipped if any force
       parameter was used. */
	mdelay(2000);
	ov9640_read(REV,&res);
	/* The below is of course bogus */
	DPRINTK("I2C: Probe ov9640 chip..addr=0x%x, REV=%d, res=0x%x\n", address, REV, res);

	if (kind <= 0) {
                 char res = -1;
		mdelay(2000);
		 ov9640_read(REV,&res);
		/* The below is of course bogus */
		DPRINTK("I2C: Probe ov9640 chip..addr=0x%x, REV=%d, res=0x%x\n", address, REV, res);
                /*ov9640 chip id is 0x9648
                 if(res != OV9640_CHIP_ID) {
			DPRINTK(KERN_WARNING "Failed.product id =%d \n",res);
			goto ERROR1;
		 }		 
		else {
                       DPRINTK("OV9640 chip id is 0X%04X\n", OV9640_CHIP_ID);
			if ( ov9640_id == 0 )
				DPRINTK(" detected.\n");
		}*/
	}

	strcpy(new_client->name, "i2c-ov9640");

    /* Only if you use this field */
	data->valid = 0; 


    /* Tell the i2c layer a new client has arrived */
    if ((err = i2c_attach_client(new_client)))
      goto ERROR3;

    /* This function can write default values to the client registers, if
       needed. */
	/*	ov9640_init_client(new_client);	*/
    return 0;

    /* OK, this is not exactly good programming practice, usually. But it is
       very code-efficient in this case. */

ERROR3:
      kfree(new_client);
      g_client = NULL;
ERROR0:
      return err;
}

int i2c_ov9640_detach_client(struct i2c_client *client)
{
	int err;

    /* Try to detach the client from i2c space */
    if ((err = i2c_detach_client(client))) {
      DPRINTK("i2c-ov9640.o: Client deregistration failed, client not detached.\n");
      return err;
    }

    kfree(client); /* Frees client data too, if allocated at the same time */
    g_client = NULL;
    return 0;
}

/* 	Keep track of how far we got in the initialization process. If several
	things have to initialized, and we fail halfway, only those things
	have to be cleaned up! */
static int ov9640_initialized = 0;



/***********************************************************************
*  Power & Reset
***********************************************************************/



#if 0
static struct i2c_driver driver = {
	.name		= "a780 camera driver",
	.id		= I2C_A780_CAMERA,
	//flags:           I2C_DF_DUMMY,
	.attach_adapter	= a780_camera_adapter_attach,        
	.detach_client	= a780_camera_detach,
	.owner		= THIS_MODULE,
};

static struct i2c_adapter a780_camera_adapter = {
        name:                   "a780 camera adapter",
        id:                     I2C_A780_CAMERA,
        client_register:        a780_camera_client_register,
        client_unregister:      a780_camera_client_unregister,
};
static struct i2c_client client_template =
{
    name:   "(unset)",        
    adapter:&a780_camera_adapter,
};
struct i2c_client *a780_camera_client;

int ov9640_write(u8 addr, u8 value)
{
    char    tmp[2]={addr, value};
    int     ret;
    unsigned int flags;
		
    ret = a780_camera_write(tmp, 2);
    local_irq_save(flags)
    enable_irq(IRQ_I2C);
	ret = i2c_master_send(a780_camera_client, buf, count);
    local_irq_restore(flags);
    if(ret < 0)
    {
        err_print("i2c write error code =%d", ret);
        return -EIO;
    }

    ddbg_print("addr = 0x%02x, value = 0x%02x", addr,value);
    return 0;
}
#endif


void ov9640_soft_reset(void)
{
	u8 regValue;
	regValue = 0x80;
	ov9640_write(OV9640_COM7, regValue);
	mdelay(10);
	return;
}

void ov9640_power_down(int powerDown)
{
	// OV9640 PWRDWN, 0 = NORMAL, 1=POWER DOWN
	//GPDR1 |= GPIO_bit(50);
	//OV9640 reset CIF_RST, 0 = NORMAL, 1=RESET
	//GPDR0 |= GPIO_bit(19);
	if (powerDown == 1) {
		mdelay(200);
		ov9640_soft_reset();
		ov9640_write(0x39, 0xf4);
		ov9640_write(0x1e, 0x80);
		ov9640_write(0x6b, 0x3f);
		ov9640_write(0x36, 0x49);
		ov9640_write(0x12, 0x05);
		mdelay(800);
		ov9640_set_powerdown_gpio();
	}
	else {
		ov9640_write(0x39, 0xf0);
		ov9640_write(0x1e, 0x00);
		ov9640_write(0x6b, 0x3f);
		ov9640_write(0x36, 0x49);
		ov9640_write(0x12, 0x10);
		ov9640_clear_powerdown_gpio();
		//GPSR0 = GPIO_bit(19);
		mdelay(20);
		//GPCR0 = GPIO_bit(19);
	}
	mdelay(100);
}

int i2c_ov9640_init(void)
{
	int res;

	if (ov9640_initialized) 
		return 0;
//	SET_GPIO(112, 1);
	DPRINTK("I2C: driver for device ov9640.\n");

	ov9640_gpio_init();

	if ( (res = i2c_add_driver(&ov9640_driver)) ) {
		DPRINTK("i2c-ov9640: Driver registration failed, module not inserted.\n");
		i2c_ov9640_cleanup();
		return res;
	}
	ov9640_initialized ++;
        if(g_client != NULL) {
   	   DPRINTK("I2C: driver for device %s registed!.\n", g_client->name);
	} else {
           DPRINTK("I2C: driver for device unregisted!.\n");
	}
	return 0;
}

void i2c_ov9640_cleanup(void)
{
//	SET_GPIO(112, 0);
	if (ov9640_initialized == 1) {
		if (i2c_del_driver(&ov9640_driver)) {
			DPRINTK("i2c-ov9640: Driver registration failed, module not removed.\n");
			return;
		}
		ov9640_initialized --;
	}
}


//EXPORT_SYMBOL(i2c_ov9640_init);
EXPORT_SYMBOL(ov9640_write);
EXPORT_SYMBOL(ov9640_read);
//EXPORT_SYMBOL(i2c_ov9640_cleanup);
module_init(i2c_ov9640_init);
module_exit(i2c_ov9640_cleanup);
MODULE_LICENSE("GPL");

