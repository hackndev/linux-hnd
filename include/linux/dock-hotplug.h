/*
 * Docking/undocking hotplug event support.
 *
 * Copyright 2004 Andrew Zabolotny <zap@homelink.ru>
 */

#ifndef _LINUX_DOCK_HOTPLUG_H
#define _LINUX_DOCK_HOTPLUG_H

typedef enum {
	DOCKFLAV_DOCKSTATION,	/* A real dock station */
	DOCKFLAV_CRADLE,	/* Cradle (typicaly used with PDAs) */
	DOCKFLAV_CABLE		/* Sync cables (PDAs) */
} dock_flavour_t;

/* Most of the fields are equivalent to those found in the PnP BIOS
 * specification. However, some new bitmask definitions have been added
 * in order to support features present in modern PDAs and other smart gadgets.
 */
struct dock_hotplug_caps_t {
#define UNKNOWN_DOCKING_IDENTIFIER	0xFFFFFFFF
	__u32	location;	/* Docking station location identifier */
	__u32	serial;		/* Serial number or 0 */
	__u32	capabilities;	/* Capabilities */
/* 0 - Docking station does not provide support for controlling the
       docking/undocking sequence (Surprise Style).
   1 - Docking station provides support for controlling the docking/undocking
       sequence (VCR Style). */
#define DOCKCAPS_CONTROLLED	1

/* The mask to separate out the 'docking type' bits from caps */
#define DOCKCAPS_TYPE_MASK	6
/* System should be powered off to dock or undock (Cold Docking) */
#define DOCKCAPS_COLD		0
/* System supports Warm Docking/Undocking, system must be in suspend */
#define DOCKCAPS_WARM		2
/* System supports Hot Docking/Undocking, not required to be in suspend  */
#define DOCKCAPS_HOT		4

/*------ Additional bits (not part of the PnP BIOS specification) ------*/

/* Dock station provides an USB Device Connector (this computer may act as
   a USB device), e.g. the dock station is something like a PDA cradle */
#define DOCKCAPS_UDC		0x00010000
/* Dock station provides an USB Host Connector. This means that USB devices
   can be connected to this computer via the dock station */
#define DOCKCAPS_UHC		0x00020000
/* Dock station provides a RS-232 connection with the "host" computer */
#define DOCKCAPS_RS232		0x00040000
	dock_flavour_t	flavour;/* Docking station flavour - dockstation, */
};

extern int dock_hotplug_event(int dock, struct dock_hotplug_caps_t *info);

#endif /* _LINUX_DOCK_HOTPLUG_H */
