// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Bridge between the core northbound bmp monitor callbacks
 * (bgp_nb_config.c) and the optional bgpd_bmp module: the module
 * cannot be linked from the core daemon, so it publishes its
 * lookup/apply internals here at load time. The core callbacks fail
 * commits with an explicit error while the module is not loaded.
 */
#ifndef _FRR_BGP_NB_BMP_H
#define _FRR_BGP_NB_BMP_H

#include "bgpd/bgp_bmp.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * FRR never dlclose()s modules at runtime, so the pointers stay
 * valid once published; there is no unload counterpart by design.
 */
/* defined in bgp_nb_config.c; filled by the bgpd_bmp module init */
struct bgp_nb_bmp_ops {
	struct bmp_targets *(*find_target)(struct bgp *bgp,
					   const char *name);
	void (*monitor_apply)(struct bmp_targets *bt, afi_t afi,
			      safi_t safi, uint8_t flag, bool enable);
};

extern struct bgp_nb_bmp_ops bgp_nb_bmp_ops;

#ifdef __cplusplus
}
#endif

#endif /* _FRR_BGP_NB_BMP_H */
