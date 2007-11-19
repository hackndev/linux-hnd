/* YMODEM support for bootldr
 * ^^^^^^^^^^^^^^^^^^^^^^^^^^
 * Copyright (C) 2001  John G Dorsey
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The author may be contacted via electronic mail at <john+@cs.cmu.edu>,
 * or at the following address:
 *
 *   John Dorsey
 *   Carnegie Mellon University
 *   HbH2201 - ICES
 *   5000 Forbes Avenue
 *   Pittsburgh, PA  15213
 */

#if !defined(_YMODEM_H)
#define _YMODEM_H

#define PACKET_SEQNO_INDEX      (1)
#define PACKET_SEQNO_COMP_INDEX (2)

#define PACKET_HEADER		(3)	/* start, block, block-complement */
#define PACKET_TRAILER_CRC	(2)	/* CRC bytes */
#define PACKET_TRAILER		(1)
#define PACKET_OVERHEAD_CRC 	(PACKET_HEADER + PACKET_TRAILER_CRC)
#define PACKET_OVERHEAD 	(PACKET_HEADER + PACKET_TRAILER)
#define PACKET_SIZE     	(256)
#define PACKET_1K_SIZE  	(4097)

#define FILE_NAME_LENGTH (255)
#define FILE_SIZE_LENGTH (16)

/* ASCII control codes: */
#define SOH (0x01)	/* start of 128-byte data packet */
#define STX (0x02)	/* start of 1024-byte data packet */
#define EOT (0x04)	/* end of transmission */
#define ACK (0x06)	/* receive OK */
#define NAK (0x15)	/* receiver error; retry */
#define CAN (0x18)	/* two of these in succession aborts transfer */
#define CRC (0x43)	/* use in place of first NAK for CRC mode */
#define GRC (0x47)	/* use in place of first NAK for -g */

#define INITIAL_TIMEOUT (15)
#define CRC_TIMEOUT     (3)
#define NAK_TIMEOUT     (10)

/* Number of attempts at soliciting CRC mode from sender before falling
 * back to arithmetic checksum:
 */
#define MAX_CRC_TRIES (5)

/* Number of consecutive receive errors we will tolerate before giving 
 * up:
 */
#define MAX_ERRORS    (5)

extern char* lab_ymodem_receive(unsigned int* length);

#endif  /* !define(_YMODEM_H) */
