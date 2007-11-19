#ifndef _INCLUDE_H3600_PCMCIA_H_
#define _INCLUDE_H3600_PCMCIA_H_

#include "soc_common.h"

struct pcmcia_init;
struct pcmcia_state_array;
struct pcmcia_irq_info;
struct pcmcia_low_level;

extern int  h3600_common_pcmcia_init(struct soc_pcmcia_socket *skt);
extern void h3600_common_pcmcia_shutdown(struct soc_pcmcia_socket *skt);
extern int  h3600_common_pcmcia_socket_state(struct soc_pcmcia_socket *skt,
					struct pcmcia_state *state);
extern void h3600_common_pcmcia_socket_init(struct soc_pcmcia_socket *skt);
extern void h3600_common_pcmcia_socket_suspend(struct soc_pcmcia_socket *skt);
extern void h3600_common_pcmcia_set_timing(struct soc_pcmcia_socket *skt);


extern void h3600_pcmcia_change_sleeves (struct device *dev, struct pcmcia_low_level *ops);
extern void h3600_pcmcia_remove_sleeve (void);
extern void h3600_pcmcia_suspend_sockets (void);
extern void h3600_pcmcia_resume_sockets (void);

#endif
