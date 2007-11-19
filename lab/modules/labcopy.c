/*
 *  lab/modules/labcopy.c
 *
 *  Copyright (C) 2003 Joshua Wise
 *  Bootloader port to Linux Kernel, May 09, 2003
 *
 * "copy" command for LAB CLI.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h> 
#include <asm/dma.h>
#include <asm/io.h>
#include <linux/slab.h>
#include <linux/unistd.h>
#include <linux/ctype.h>
#include <linux/vmalloc.h>
#include <linux/lab/copy.h>
#include <linux/lab/lab.h>
#include <linux/lab/commands.h>

struct lab_srclist* srclist;
struct lab_destlist* destlist;
struct lab_unlinklist* unlinklist;

void lab_cmd_copy(int argc,const char** argv);
void lab_cmd_unlink(int argc,const char** argv);

int labcopy_init(void)
{
	/* We don't need to do much here. */
	lab_addcommand("copy",   lab_cmd_copy,   "Copies data from a source to a destination.");
	lab_addcommand("unlink", lab_cmd_unlink, "Unlinks a file.");
	lab_addcommand("rm",     lab_cmd_unlink, "Unlinks a file.");
	return 0;
}

void labcopy_cleanup(void)
{
	lab_delcommand("copy");
	lab_delcommand("unlink");
	lab_delcommand("rm");
}

void lab_copy_addsrc(char* name, int(*check)(char* fn), unsigned char*(*get)(int* count,char* fn))
{
	struct lab_srclist *mylist;
	
	mylist = srclist;
	if (!mylist)
	{
		srclist=mylist=kmalloc(sizeof(struct lab_srclist), GFP_KERNEL);
		goto buildentry;
	}
	
	while (mylist->next)
		mylist = mylist->next;
	
	mylist->next=kmalloc(sizeof(struct lab_srclist), GFP_KERNEL);
	mylist = mylist->next;
	
buildentry:
	mylist->next = NULL;
	mylist->src = kmalloc(sizeof(struct lab_src), GFP_KERNEL);
	mylist->src->name = name;
	mylist->src->check = check;
	mylist->src->get = get;
	
	printk(KERN_NOTICE "lab: loaded copy source [%s]\n", name);
}
EXPORT_SYMBOL(lab_copy_addsrc);

void lab_copy_adddest(char* name, int(*check)(char* fn), int(*put)(int count,unsigned char* data,char* fn))
{
	struct lab_destlist *mylist;
	
	mylist = destlist;
	if (!mylist)
	{
		destlist=mylist=kmalloc(sizeof(struct lab_destlist), GFP_KERNEL);
		goto buildentry;
	}
	
	while (mylist->next)
		mylist = mylist->next;
	
	mylist->next=kmalloc(sizeof(struct lab_destlist), GFP_KERNEL);
	mylist = mylist->next;
	
buildentry:
	mylist->next = NULL;
	mylist->dest = kmalloc(sizeof(struct lab_dest), GFP_KERNEL);
	mylist->dest->name = name;
	mylist->dest->check = check;
	mylist->dest->put = put;
	
	printk(KERN_NOTICE "lab: loaded copy destination [%s]\n", name);
}
EXPORT_SYMBOL(lab_copy_adddest);

void lab_copy_addunlink(char* name, int(*check)(char* fn), int(*unlink)(char* fn))
{
	struct lab_unlinklist *mylist;
	
	mylist = unlinklist;
	if (!mylist)
	{
		unlinklist=mylist=kmalloc(sizeof(struct lab_unlinklist), GFP_KERNEL);
		goto buildentry;
	}
	
	while (mylist->next)
		mylist = mylist->next;
	
	mylist->next=kmalloc(sizeof(struct lab_unlinklist), GFP_KERNEL);
	mylist = mylist->next;
	
buildentry:
	mylist->next = NULL;
	mylist->unlink = kmalloc(sizeof(struct lab_unlink), GFP_KERNEL);
	mylist->unlink->name = name;
	mylist->unlink->check = check;
	mylist->unlink->unlink = unlink;
	
	printk(KERN_NOTICE "lab: loaded unlink device [%s]\n", name);
}
EXPORT_SYMBOL(lab_copy_addunlink);

void lab_copy(char* source, char* sourcefile, char* dest, char* destfile)
{
	struct lab_srclist *mysrclist;
	struct lab_destlist *mydestlist;
	int count;
	unsigned char* data;
	
	mysrclist = srclist;
	
	while(mysrclist) {
		if (!strcmp(source,mysrclist->src->name))
			break;
		mysrclist = mysrclist->next;
	}
	
	if (!mysrclist) {
		lab_printf("Source [%s] does not exist.\r\n", source);
		globfail++;
		return;
	}
	
	mydestlist = destlist;
	
	while(mydestlist) {
		if (!strcmp(dest,mydestlist->dest->name))
			break;
		mydestlist = mydestlist->next;
	}
	
	if (!mydestlist) {
		lab_printf("Destination [%s] does not exist.\r\n", dest);
		globfail++;
		return;
	}
	
	if (!mysrclist->src->check(sourcefile)) {
		lab_printf("Source [%s] rejected filename \"%s\".\r\n",
			   source, sourcefile);
		globfail++;
		return;
	}
	
	if (!mydestlist->dest->check(destfile)) {
		lab_printf("Destination [%s] rejected filename \"%s\".\r\n",
			   dest, destfile);
		globfail++;
		return;
	}
	
	data = mysrclist->src->get(&count,sourcefile);
	if (!data) {
		lab_puts("Error occured while getting file.\r\n");
		globfail++;
		return;
	}
	
	if (mydestlist->dest->put(count,data,destfile) == 0) {
		lab_puts("Error occured while putting file.\r\n");
		globfail++;
		vfree(data);
		return;
	}
	
	vfree(data);
}
EXPORT_SYMBOL(lab_copy);

void lab_unlink(char* ul, char* ulfile)
{
	struct lab_unlinklist *myunlinklist;
	
	myunlinklist = unlinklist;
	
	while(myunlinklist)
	{
		if (!strcmp(ul,myunlinklist->unlink->name))
			break;
		myunlinklist = myunlinklist->next;
	}
	
	if (!myunlinklist)
	{
		lab_printf("Unlink device [%s] does not exist.\r\n", ul);
		globfail++;
		return;
	}
	
	if (!myunlinklist->unlink->check(ulfile))
	{
		lab_printf("Unlink device [%s] rejected filename \"%s\".\r\n",
			   ul, ulfile);
		globfail++;
		return;
	}
	
	myunlinklist->unlink->unlink(ulfile);
}
EXPORT_SYMBOL(lab_unlink);

void lab_cmd_copy(int argc,const char** argv)
{
	char *temp;
	char *source;
	char *sourcefile;
	char *dest;
	char *destfile;

	if (argc != 3) {
		lab_puts("Usage: boot> copy filespec filespec\r\n"
		         "filespec is device:[file]\r\n"
		         "example: boot> copy ymodem: flash:lab\r\n"
		         "would receive data over ymodem, and flash it to the lab partition.\r\n"
		         "example 2: boot> copy fs:/flash/boot/params ymodem:\r\n");
		return;
	}
	
	temp = source = (char*)argv[1];
	while (*temp != ':' && *temp != '\0')
		temp++;
	if (*temp == '\0') {
		lab_puts("Illegal filespec\r\n");
		globfail++;
		return;
	}
	*temp = '\0';
	temp++;
	sourcefile = temp;
	
	temp = dest = (char*)argv[2];
	while (*temp != ':' && *temp != '\0')
		temp++;
	if (*temp == '\0') {
		lab_puts("Illegal filespec\r\n");
		globfail++;
		return;
	}
	*temp = '\0';
	temp++;
	destfile = temp;
	
	lab_printf("Copying [%s:%s] to [%s:%s] ...\r\n",
		   source, sourcefile, dest, destfile);
	lab_copy(source, sourcefile, dest, destfile);
	
}

void lab_cmd_unlink(int argc,const char** argv)
{
	char *temp;
	char *ul;
	char *ulfile;

	if (argc != 2)
	{
		lab_puts("Usage: boot> unlink filespec\r\n"
		         "filespec is device:[file]\r\n"
		         "example: boot> unlink fs:/flash/boot/params\r\n"
		         "would delete /boot/params on your jffs2 partition.\r\n");
		return;
	}
	
	temp = ul = (char*)argv[1];
	while (*temp != ':' && *temp != '\0')
		temp++;
	if (*temp == '\0')
	{
		lab_puts("Illegal filespec\r\n");
		globfail++;
		return;
	}
	*temp = '\0';
	temp++;
	ulfile = temp;
	
	lab_printf("Unlinking [%s] ...\r\n", ul, ulfile);
	lab_unlink(ul, ulfile);
}

MODULE_AUTHOR("Joshua Wise");
MODULE_DESCRIPTION("LAB copy command module");
MODULE_LICENSE("GPL");
module_init(labcopy_init);
module_exit(labcopy_cleanup);
