#if defined(CONFIG_ACX_MEM) && !defined(ACX_MEM)
#define ACX_MEM
#endif

#if defined(CONFIG_ACX_CS) && !defined(ACX_MEM)
#define ACX_MEM
#endif

#include "acx_config.h"
#include "wlan_compat.h"
#include "wlan_hdr.h"
#include "wlan_mgmt.h"
#include "acx_struct.h"
#include "acx_func.h"
