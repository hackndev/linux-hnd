/* H5400 Fingerprint Sensor Interface driver 
 * Copyright (c) 2004 and 2005 J°rgen Andreas Michaelsen <jorgenam@ifi.uio.no> 
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */
#ifndef _FSI_H_
#define _FSI_H_

#define FSI_FRAME_SIZE                    281 /* words */

#define ASIC_FSI_Data32                   ASIC_REGISTER( u32, FSI, Data )
#undef  ASIC_FSI_Data

#define FSI_CMD_MODE_STOP                 0x00
#define FSI_CMD_MODE_START                0x01
#define FSI_CMD_MODE_NAP                  0x02
#define FSI_CMD_STOP_ACQ_INT              0x10
#define FSI_CMD_START_ACQ_INT             0x11
#define FSI_CMD_STOP_INFO_INT             0x14
#define FSI_CMD_START_INFO_INT            0x15
#define FSI_CMD_RESET                     0x20
#define FSI_CMD_START_TEMP                0x30
#define FSI_CMD_STOP_TEMP                 0x31

#define FSI_DB1_EVEN(x)                   (x&0xF)
#define FSI_DB1_ODD(x)                    ((x>>4)&0xF)
#define FSI_DB2_EVEN(x)                   ((x>>8)&0xF)
#define FSI_DB2_ODD(x)                    ((x>>12)&0xF)
#define FSI_DB3_EVEN(x)                   ((x>>16)&0xF)
#define FSI_DB3_ODD(x)                    ((x>>20)&0xF)
#define FSI_DB4_EVEN(x)                   ((x>>24)&0xF)
#define FSI_DB4_ODD(x)                    ((x>>28)&0xF) 

#define FSI_CONTROL_TEMP                  (1<<2)

#define FSI_IOC_MAGIC                     0x81
#define FSI_IOCSTOPTEMP                   _IO(FSI_IOC_MAGIC, 1)
#define FSI_IOCSTARTTEMP                  _IO(FSI_IOC_MAGIC, 2)
#define FSI_IOCINCTEMP                    _IO(FSI_IOC_MAGIC, 3)
#define FSI_IOCSETPRESCALE                _IO(FSI_IOC_MAGIC, 4)
#define FSI_IOCSETDMI                     _IO(FSI_IOC_MAGIC, 5)
#define FSI_IOCSETTRESHOLDON              _IO(FSI_IOC_MAGIC, 6)
#define FSI_IOCSETTRESHOLDOFF             _IO(FSI_IOC_MAGIC, 7)

struct fsi_read_state
{
	unsigned long *buffer;                  /* destination for data read from FIFO (circular buffer) */
	unsigned int buffer_size;               /* circular buffer size */
	unsigned int write_pos;                 /* used by circular buffer producer */
	unsigned int read_pos;                  /* used by circular buffer consumer */
	unsigned int word_count;                /* how many words read so far (written to user) */
	unsigned int word_dest;                 /* how many words we should read */

	unsigned int frame_number;
	unsigned int frame_offset;

	unsigned char do_read;                  /* true if valid data (copy to user) */
	unsigned char finished;                 

	unsigned int treshold_on;               /* level for deciding when finger is present */ 
	unsigned int treshold_off;              /* level for deciding when finger is removed */
	unsigned char detect_count;             /* another (second) treshold */
	unsigned char finger_present;           /* true if finger is present on scanner */
	unsigned int detect_value;
};

#endif /* _FSI_H_ */
