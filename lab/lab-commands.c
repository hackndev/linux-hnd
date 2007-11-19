/*
 *  linux/lab/lab-commands.c
 *
 *  Copyright (C) 2003 Joshua Wise
 *  Bootloader port to Linux Kernel, May 07, 2003
 *
 *  cli for LAB
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/lab/lab.h>
#include <linux/lab/commands.h>

struct lab_cmdlist* cmdlist = NULL;
void lab_command_help(int,const char**);

void lab_cmdinit ()
{
	if (cmdlist) return;
	cmdlist = kmalloc (sizeof (struct lab_cmdlist), GFP_KERNEL);
	cmdlist->command = kmalloc(sizeof (struct lab_command), GFP_KERNEL);
	cmdlist->command->cmdname = "help";
	cmdlist->command->func = &lab_command_help;
	cmdlist->command->help = "Displays this help";
	cmdlist->command->subcmd = NULL;
	cmdlist->next = NULL;
}

void lab_addcommand (char *cmd, void (*func)(int, const char **), char *help)
{
	struct lab_cmdlist *mylist;
	mylist = cmdlist;

	if (mylist == NULL) {
		lab_cmdinit ();
		mylist = cmdlist;
	}

	while (mylist) {
		if (!strcmp(cmd, mylist->command->cmdname)) {
			if (mylist->command->func) {
				printk (KERN_NOTICE "lab: tried to insert command %s, but it already exists!\n", cmd);
				return;
			} else
				/* exists, but only has subcommands. go ahead and just set it. */
				goto justset;
		}
		mylist = mylist->next;
	}

	mylist = cmdlist;

	while (mylist->next)
		mylist = mylist->next;

	mylist->next = kmalloc(sizeof(struct lab_cmdlist), GFP_KERNEL);
	mylist = mylist->next;
	mylist->next = NULL;
	mylist->command = kmalloc(sizeof(struct lab_command), GFP_KERNEL);
	mylist->command->subcmd = NULL;
	mylist->command->cmdname = cmd;

justset:
	mylist->command->func = func;
	mylist->command->help = help;

	printk(KERN_NOTICE "lab: loaded command %s\n", cmd);
}
EXPORT_SYMBOL(lab_addcommand);

void lab_delcommand (char* cmd)
{
	struct lab_cmdlist *mylist = cmdlist;
	struct lab_cmdlist *lastsc = NULL;
	struct lab_cmdlist *prevcmd = NULL;

	if (mylist == NULL)
		return;

	while (mylist) {
		if (mylist->command && mylist->command->cmdname)
			if (!strcmp (cmd, mylist->command->cmdname))
				break;

		prevcmd = mylist;
		mylist = mylist->next;
	}

	if (!mylist) {
		printk(KERN_NOTICE "lab: tried to delete command %s, but it doesn't exist!\n", cmd);
		return;
	}

	while (mylist->command && mylist->command->subcmd) {
		struct lab_cmdlist* sclist = mylist->command->subcmd;

		if (lastsc)
			kfree(lastsc);

		if (sclist->command)
			kfree(sclist->command);

		lastsc = sclist;
		mylist->command->subcmd = sclist->next;
	}
	if (lastsc)
		kfree(lastsc);

	if (mylist->command)
		kfree(mylist->command);

	if (prevcmd)
		prevcmd->next = mylist->next;
	kfree(mylist);
}
EXPORT_SYMBOL(lab_delcommand);

void lab_addsubcommand(char* cmd, char* cmd2, void(*func)(int,const char**), char* help)
{
	struct lab_cmdlist* mylist;
	mylist = cmdlist;

	if (mylist == NULL) {
		lab_cmdinit ();
		mylist = cmdlist;
	}

	while(mylist) {
		if (!strcmp(cmd, mylist->command->cmdname)) {
			/* found an entry for cmd, let's see if it has subcmds. */
			if (mylist->command->subcmd) {
				/* already has subcommands. let's seek through 'em. */
				mylist = mylist->command->subcmd;
				while (mylist->next)
					mylist = mylist->next;

				/* ok, we're at the end, let's add a new one. */
				mylist->next = kmalloc(sizeof(struct lab_cmdlist), GFP_KERNEL);
				mylist = mylist->next;
				mylist->next = NULL;
				mylist->command = kmalloc(sizeof(struct lab_command), GFP_KERNEL);
				mylist->command->cmdname = cmd2;
				mylist->command->func = func;
				mylist->command->help = help;
				mylist->command->subcmd = NULL;
				printk(KERN_NOTICE "lab: loaded command [%s]%s\n", cmd, cmd2);
				return; /* all done */
			}

			/* ok, it didn't have subcommands already, but that's ok,
			 * we'll start a new chain.
			 */

			mylist->command->subcmd = kmalloc(sizeof(struct lab_cmdlist), GFP_KERNEL);
			mylist = mylist->command->subcmd;
			/* no backing out now. */
			mylist->next = NULL;
			mylist->command = kmalloc(sizeof(struct lab_command), GFP_KERNEL);
			mylist->command->cmdname = cmd2;
			mylist->command->func = func;
			mylist->command->help = help;
			mylist->command->subcmd = NULL;
			printk(KERN_NOTICE "lab: loaded command [%s]%s\n", cmd, cmd2);
			return; /* all done. */
		}
		mylist = mylist->next;
	}

	/* ok, the command itself doesn't already exist. that's ok anyway. */
	mylist = cmdlist;
	while (mylist->next)
		mylist = mylist->next;

	mylist->next = kmalloc(sizeof(struct lab_cmdlist), GFP_KERNEL);
	mylist = mylist->next;
	mylist->next = NULL;
	mylist->command = kmalloc(sizeof(struct lab_command), GFP_KERNEL);
	mylist->command->cmdname = cmd;
	mylist->command->func = NULL;
	mylist->command->help = NULL;
	mylist->command->subcmd = kmalloc(sizeof(struct lab_cmdlist), GFP_KERNEL);
	/* ok, created the command, now to create the subcmd tree */
	mylist = mylist->command->subcmd;
	mylist->next = NULL;
	mylist->command = kmalloc(sizeof(struct lab_command), GFP_KERNEL);
	mylist->command->cmdname = cmd2;
	mylist->command->func = func;
	mylist->command->help = help;
	mylist->command->subcmd = NULL;
	printk(KERN_NOTICE "lab: loaded command [%s]%s\n", cmd, cmd2);
}
EXPORT_SYMBOL(lab_addsubcommand);

void lab_execcommand(int argc, char** argv)
{
	void(*func)(int,const char**);
	struct lab_cmdlist* mylist;

	mylist = cmdlist;

	if (mylist == NULL) {
		lab_cmdinit ();
		mylist = cmdlist;
	}

	func = 0;

	while (mylist) {
		if (!strcmp(argv[0],mylist->command->cmdname)) {
			if (mylist->command->subcmd && (argc > 1)) {
				struct lab_cmdlist* sublist;
				/* it might be a subcommand we're calling... */
				sublist = mylist->command->subcmd;
				while(sublist) {
					if (!strcmp(argv[1],sublist->command->cmdname)) {
						if (sublist->command->func) {
							func = sublist->command->func;
							goto breakout;
						}
					}
					sublist = sublist->next;
				}

				if (!func) {
					func = (void(*)(int,const char**))1;
					goto breakout;
				}
			}

                        /* subcommands might not have functions */
			if (mylist->command->func) {
				func = mylist->command->func;
				goto breakout;
			}
		}
		mylist = mylist->next;
	}

breakout:
	if ((int)func == 0) {
		lab_printf ("Couldn't find command %s.\r\n", argv [0]);
		return;
	}

	if ((int)func == 1) {
		lab_printf ("Couldn't find subcommand %s %s.\r\n",
			    argv [0], argv [1]);
		return;
	}

	func (argc, (const char**)argv);
}

void lab_command_help (int argc, const char **argv)
{
	struct lab_cmdlist *mylist;

	mylist = cmdlist;

	while (mylist) {
		if (argc > 1)
			if (strcmp(argv[1], mylist->command->cmdname)) {
				mylist = mylist->next;
				continue;
			}

		if (mylist->command->func)
			lab_printf ("%s - %s\r\n", mylist->command->cmdname,
				    mylist->command->help ?
				    mylist->command->help : "No help");

		if (mylist->command->subcmd) {
			struct lab_cmdlist* sublist;

			sublist = mylist->command->subcmd;

			while (sublist)	{
				if (argc > 2)
					if (strcmp (argv[2], sublist->command->cmdname)) {
						sublist = sublist->next;
						continue;
					}

				if (sublist->command->func)
					lab_printf ("%s %s - %s\r\n",
						    mylist->command->cmdname,
						    sublist->command->cmdname,
						    sublist->command->help ?
						    sublist->command->help : "No help");
				else
					lab_printf ("BUG: Subcommand [%s]%s exists with no function! Fix me!\r\n",
						    mylist->command->cmdname,
						    sublist->command->cmdname);
				sublist = sublist->next;
			}
		}

		mylist = mylist->next;
	}
}
