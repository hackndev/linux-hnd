/*
 * Linux As Bootloader main entry point
 *
 * Authors: Andrew Zabolotny <zap@homelink.ru>
 *          Joshua Wise
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/version.h>
#include <linux/utsname.h>
#include <linux/fs.h>
#include <linux/syscalls.h>
#include <linux/lab/lab.h>
#include <linux/lab/commands.h>
#include <linux/lab/copy.h>

void lab_runfile(char *source, char *sourcefile);

int globfail;
EXPORT_SYMBOL (globfail);

static int cmdline = 0;

static int __init forcecli_setup(char *__unused)
{
	printk ("lab: Skipping autoboot due to forcecli parameter.\n");
	cmdline = 1;
	return 1;
}
__setup("forcecli", forcecli_setup);

static void print_version (void)
{
	lab_puts ("LAB - Linux As Bootloader, ");
	lab_puts (init_utsname()->release);
	lab_puts ("\r\n");
}

static void print_banner (void)
{
	print_version ();
	lab_puts ("Contact: bootldr@handhelds.org\r\n\r\n");
	
	lab_puts ("Copyright (c) 2003-2004 Joshua Wise, Andrew Zabolotny and others.\r\n");
	lab_puts ("Based on Linux kernel, which is copyright (c) Linus Torvalds and others.\r\n");
	lab_puts ("Partially based on OHH bootldr, which is copyright (c) COMPAQ Computer Corporation.\r\n");
	lab_puts ("This program is provided with NO WARRANTY under the terms of the\r\nGNU General Public License.\r\n");
}

enum ParseState {
	PS_WHITESPACE,
	PS_TOKEN,
	PS_STRING,
	PS_ESCAPE
};

static void parseargs (char *argstr, int *argc_p, char **argv, char **resid)
{
	int argc = 0;
	char c;
	enum ParseState lastState = PS_WHITESPACE;
	enum ParseState stackedState = PS_WHITESPACE;

	/* tokenize the argstr */
	while ((c = *argstr) != 0) {
		enum ParseState newState;

		if ((c == ';' || c == '\n') && lastState != PS_STRING && lastState != PS_ESCAPE)
			break;

		if (lastState == PS_ESCAPE) {
			newState = stackedState;
		} else if (lastState == PS_STRING) {
			if (c == '"') {
				newState = PS_WHITESPACE;
				*argstr = 0;
			} else {
				newState = PS_STRING;
			}
		} else if ((c == ' ') || (c == '\t')) {
			/* whitespace character */
			*argstr = 0;
			newState = PS_WHITESPACE;
		} else if (c == '"') {
			newState = PS_STRING;
			*argstr = 0;
			argv[argc++] = argstr + 1;
		} else if (c == '\\') {
			stackedState = lastState;
			newState = PS_ESCAPE;
		} else {
			/* token */
			if (lastState == PS_WHITESPACE) {
				argv[argc++] = argstr;
			}
			newState = PS_TOKEN;
		}

		lastState = newState;
		argstr++;
	}

	argv [argc] = NULL;
	if (argc_p != NULL)
		*argc_p = argc;

	if (*argstr == ';' || *argstr == '\n')
		*argstr++ = 0;

	*resid = argstr;
}

#if 0
// Commented out until we find a use for it -- zap

// this is getting more compliacated,  this function will averride any of the
// args in argstr with tthe args from argv.  this will allow you to override the
// param linuxargs from the commandline. e.g. init=/myRC will override
// init=linuxRC from the params.
static void unparseargs(char *argstr, int argc, const char **argv)
{
	int i;
	char *cutStart;
	char *cutEnd;
	char *paramEnd;
	int delta;
	int j;

	for (i = 0; i < argc; i++) {
                if (argv[i] != NULL) {
                        // we have an a=b arg
			if ((paramEnd = strchr(argv[i],'='))) {
				int argstrlen = strlen(argstr);
				int needlelen = 0;
				char needle[256];
				paramEnd++;
				lab_puts("haystack = <");
				lab_puts(argstr);
				lab_puts(">\r\n");

				needlelen = (int)(paramEnd - argv[i]);
				strncpy(needle, argv[i], needlelen);
				needle[needlelen] = 0;
				lab_puts("needle = <");
				putnstr(needle,needlelen);
				lab_puts(">\r\n");

				if ((cutStart = (char *)strstr(argstr, needle)) != NULL){
					// found a match
					if (!(cutEnd = strchr(cutStart,' '))) {
						cutEnd = argstr + argstrlen;
					} else {
						cutEnd++; // skip the space
					}
					delta = (int)(cutEnd - cutStart);
					for (j=(int) (cutEnd - argstr); j < argstrlen; j++)
						argstr[j-delta] = argstr[j];
					// new end of argstr
					argstr[argstrlen - delta] = '\0';

				}
			}
			strcat(argstr, " ");
			strcat(argstr, argv[i]);
		}
	}
}
#endif

void lab_exec_string (char *buf)
{
	int argc;
	char *argv[256];
	char *resid;

	while (*buf) {
		memset (argv, 0, sizeof (argv));
		parseargs (buf, &argc, argv, &resid);
		if (argc > 0)
			lab_execcommand (argc, (char**) argv);
		else
			lab_exec_string ("help");
		buf = resid;
	}
}
EXPORT_SYMBOL (lab_exec_string);

static char *blockdevs[] = {
	"/dev/mmcblk0p1", "ext2",  "root=/dev/mmcblk0p1 rootdelay=5",
	"/dev/mmcblk0p2", "ext2",  "root=/dev/mmcblk0p2 rootdelay=5",
	"/dev/mmcblk0p3", "ext2",  "root=/dev/mmcblk0p3 rootdelay=5",
	"/dev/mmcblk0p4", "ext2",  "root=/dev/mmcblk0p4 rootdelay=5",
	"/dev/hda1",      "ext2",  "root=/dev/hda1 rootdelay=5",
	"/dev/hda2",      "ext2",  "root=/dev/hda2 rootdelay=5",
	"/dev/hda3",      "ext2",  "root=/dev/hda3 rootdelay=5",
	"/dev/hda4",      "ext2",  "root=/dev/hda4 rootdelay=5",
	"/dev/hda5",      "ext2",  "root=/dev/hda5 rootdelay=5",
	"/dev/hda6",      "ext2",  "root=/dev/hda6 rootdelay=5",
	"/dev/hda7",      "ext2",  "root=/dev/hda7 rootdelay=5",
	"/dev/mtdblock2", "jffs2", "root=/dev/mtdblock2 rootfstype=jffs2",
	"/dev/mtdblock3", "jffs2", "root=/dev/mtdblock3 rootfstype=jffs2",
	NULL
};
	

void lab_main (int cmdline)
{
	char cmdbuff [512];

	print_banner ();
	if (!cmdline) {
		char **blockdev;
		unsigned char *contents;
		int tleft;
		
		for (tleft = 3; tleft > 0; tleft--)
		{
			lab_printf("\r>> LAB autoboot starting in %d seconds - press a key to abort", tleft);

			if (lab_getc_seconds(NULL, 1))
			{
				lab_printf("\r\n>> Autoboot aborted\r\n");
				goto domenu;
			}

			if (lab_initconsole())
			{
				lab_printf("\r>> LAB autoboot delayed due to new console\n");
				tleft = 5+1;	// give the user time to open a console and hit a key
			}
		}
		lab_puts ("\r\n"
		          ">> Booting now.\r\n");
		sys_mkdir("/mnt", 0000);
		sys_mount("/dev", "/dev", "devfs", 0, "");
		lab_puts (">> Looking for filesystems...\r\n");
		blockdev = blockdevs;
		while (*blockdev) {

			struct stat sstat;

			lab_printf("  >> Trying \"%s\"... ", blockdev[0]);
			if (sys_mount(blockdev[0], "/mnt", blockdev[1],
			    MS_RDONLY, "") < 0) {

			    lab_printf("failed\r\n");
			    blockdev += 3;
			    continue;
			}

			lab_printf("ok");

			if (sys_newstat("/mnt/boot/labrun", &sstat) >= 0) {
				lab_printf(".\r\n");
				lab_printf(">> Executing labrun... ");
				lab_runfile("fs", "/mnt/boot/labrun");
				lab_printf("done\r\n");
				break;
			}
			lab_printf("; no labrun");

			if (sys_newstat("/mnt/boot/zImage", &sstat) >= 0) {
				lab_printf("; found zImage\r\n");
				lab_printf(">> Copying zImage... ");
				lab_copy("fs", "/mnt/boot/zImage", "fs", "/zImage");
				lab_printf("done\r\n>> Unmounting filesystem... ");
				sys_oldumount("/mnt");
				lab_printf("done\r\n>> Booting kernel.\r\n");
				lab_armboot("fs", "/zImage", blockdev[2]);
				break;
			}
			lab_printf(", no zImage.\r\n");
			sys_oldumount("/mnt");
			blockdev += 3;
			continue;
		}
		if (!*blockdev)
			lab_printf(">> No bootable filesystems found!\r\n");
	}

domenu:
	while (1) {
		lab_puts("boot> ");
		lab_readline (cmdbuff, sizeof cmdbuff);

		if (cmdbuff [0])
			lab_exec_string (cmdbuff);
	}
}

extern void lab_cmd_showlogo(int argc,const char** argv);

void lab_run (void)
{
	int err;

#ifdef CONFIG_LAB_BOOTLOGO
	lab_cmd_showlogo(0, NULL);
#endif	

	printk (KERN_INFO "lab: Starting LAB [Linux As Bootloader]\n");
	err = sys_mount ("none", "/root", "ramfs", 0, "");
	if (err) {
		printk (KERN_ERR "lab: Failed to mount ramfs! err=%d, halting\n", err);
		while (1) ;
	}

	err = sys_chdir ("/root");
	if (err) {
		printk (KERN_ERR "lab: chdir /root failed for some reason, halting\n");
		while (1) ;
	}

	lab_initconsole ();

	printk (KERN_INFO "lab: running.\n");
	lab_main (cmdline);
	while (1) ;
}
