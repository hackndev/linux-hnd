obj-$(CONFIG_UNION_FS) += unionfs.o

unionfs-y := subr.o dentry.o file.o inode.o main.o super.o \
	branchman.o rdstate.o copyup.o dirhelper.o rename.o \
	unlink.o lookup.o commonfops.o dirfops.o sioq.o

unionfs-$(CONFIG_UNION_FS_XATTR) += xattr.o
