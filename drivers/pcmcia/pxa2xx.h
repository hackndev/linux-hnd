/*======================================================================

    Device driver for the PCMCIA control functionality of StrongARM
    SA-1100 microprocessors.

    The contents of this file are subject to the Mozilla Public
    License Version 1.1 (the "License"); you may not use this file
    except in compliance with the License. You may obtain a copy of
    the License at http://www.mozilla.org/MPL/

    Software distributed under the License is distributed on an "AS
    IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
    implied. See the License for the specific language governing
    rights and limitations under the License.

    The initial developer of the original code is John G. Dorsey
    <john+@cs.cmu.edu>.  Portions created by John G. Dorsey are
    Copyright (C) 1999 John G. Dorsey.  All Rights Reserved.
    Copyright (C) 2003 Stefan Eletzhofer <stefan.eletzhofer@inquant.de>

    Alternatively, the contents of this file may be used under the
    terms of the GNU Public License version 2 (the "GPL"), in which
    case the provisions of the GPL are applicable instead of the
    above.  If you wish to allow the use of your version of this file
    only under the terms of the GPL and not to allow others to use
    your version of this file under the MPL, indicate your decision
    by deleting the provisions above and replace them with the notice
    and other provisions required by the GPL.  If you do not delete
    the provisions above, a recipient may use your version of this
    file under either the MPL or the GPL.
    
======================================================================*/

#if !defined(_PCMCIA_PXA_H)
#define _PCMCIA_PXA_H

#include <pcmcia/cs_types.h>
#include <pcmcia/ss.h>
#include <pcmcia/bulkmem.h>
#include <pcmcia/cistpl.h>
#include "cs_internal.h"

#define MCXX_SETUP_MASK     (0x7f)
#define MCXX_ASST_MASK      (0x1f)
#define MCXX_HOLD_MASK      (0x3f)
#define MCXX_SETUP_SHIFT    (0)
#define MCXX_ASST_SHIFT     (7)
#define MCXX_HOLD_SHIFT     (14)

static inline u_int pxa2xx_mcxx_hold(u_int pcmcia_cycle_ns,
					    u_int mem_clk_10khz){
  u_int code = pcmcia_cycle_ns * mem_clk_10khz;
  return (code / 300000) + ((code % 300000) ? 1 : 0) - 1;
}

static inline u_int pxa2xx_mcxx_asst(u_int pcmcia_cycle_ns,
					    u_int mem_clk_10khz){
  u_int code = pcmcia_cycle_ns * mem_clk_10khz;
  return (code / 300000) + ((code % 300000) ? 1 : 0) - 1;
}

static inline u_int pxa2xx_mcxx_setup(u_int pcmcia_cycle_ns,
					    u_int mem_clk_10khz){
  u_int code = pcmcia_cycle_ns * mem_clk_10khz;
  return (code / 100000) + ((code % 100000) ? 1 : 0) - 1;
}

/* This function returns the (approxmiate) command assertion period, in
 * nanoseconds, for a given CPU clock frequency and MCXX_ASST value:
 */

static inline u_int pxa2xx_pcmcia_cmd_time(u_int mem_clk_10khz,
					   u_int pcmcia_mcxx_asst){
  return (300000 * (pcmcia_mcxx_asst + 1) / mem_clk_10khz);
}


/* PCMCIA Memory and I/O timing
 * ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
 *
 * The PC Card Standard, Release 7, section 4.13.4, says that twIORD
 * has a minimum value of 165ns. Section 4.13.5 says that twIOWR has
 * a minimum value of 165ns, as well. Section 4.7.2 (describing
 * common and attribute memory write timing) says that twWE has a
 * minimum value of 150ns for a 250ns cycle time (for 5V operation;
 * see section 4.7.4), or 300ns for a 600ns cycle time (for 3.3V
 * operation, also section 4.7.4). Section 4.7.3 says that taOE
 * has a maximum value of 150ns for a 300ns cycle time (for 5V
 * operation), or 300ns for a 600ns cycle time (for 3.3V operation).
 *
 * When configuring memory maps, Card Services appears to adopt the policy
 * that a memory access time of "0" means "use the default." The default
 * PCMCIA I/O command width time is 165ns. The default PCMCIA 5V attribute
 * and memory command width time is 150ns; the PCMCIA 3.3V attribute and
 * memory command width time is 300ns.
 */
 
/* The PXA 250 and PXA 210 Application Processors Developer's Manual
 * was used to determine correct PXA_PCMCIA_IO_ACCES time
 */
  
#define PXA_PCMCIA_IO_ACCESS      (165) 

/* Default PC Card Common Memory timings*/
                                         
#define PXA_PCMCIA_5V_MEM_ACCESS  (250)
#define PXA_PCMCIA_3V_MEM_ACCESS  (250)

/* Attribute Memory timing - must be constant via PC Card standard*/
 
#define PXA_PCMCIA_ATTR_MEM_ACCESS (300)

/* The socket driver actually works nicely in interrupt-driven form,
 * so the (relatively infrequent) polling is "just to be sure."
 */
#define PXA_PCMCIA_POLL_PERIOD    (2*HZ)

struct pcmcia_low_level;

/* I/O pins replacing memory pins
 * (PCMCIA System Architecture, 2nd ed., by Don Anderson, p.75)
 *
 * These signals change meaning when going from memory-only to 
 * memory-or-I/O interface:
 */
#define iostschg bvd1
#define iospkr   bvd2

int pxa2xx_core_request_irqs(struct pxa2xx_pcmcia_socket *skt, struct pcmcia_irqs *irqs, int nr);
void pxa2xx_core_free_irqs(struct pxa2xx_pcmcia_socket *skt, struct pcmcia_irqs *irqs, int nr);
void pxa2xx_core_disable_irqs(struct pxa2xx_pcmcia_socket *skt, struct pcmcia_irqs *irqs, int nr);
void pxa2xx_core_enable_irqs(struct pxa2xx_pcmcia_socket *skt, struct pcmcia_irqs *irqs, int nr);

extern int pxa2xx_core_drv_pcmcia_probe(struct device *dev);
extern int pxa2xx_core_drv_pcmcia_remove(struct device *dev);

#endif  /* !defined(_PCMCIA_PXA_H) */
