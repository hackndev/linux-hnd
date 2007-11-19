#
# Makefile for the linux squashfs routines.
#

obj-$(CONFIG_SQUASHFS_LZMA) += unlzma.o sqlzma.o
unlzma-y := module.o
sqlzma-y := uncomp.o

obj-$(CONFIG_SQUASHFS_LZMA) += squashfs.o
squashfs-y += inode.o
squashfs-y += squashfs2_0.o


