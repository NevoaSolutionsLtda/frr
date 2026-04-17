/*
 * BGP -lite northbound operational-state callbacks (empty).
 *
 * Author: MGC Connect / Reinaldo Saraiva <reinaldo.saraiva@magalu.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * No state callbacks -- all 15 writable leaves in frr-bgpd-lite are
 * config-true. Operational data (peer session state, prefix counters,
 * last-error, uptime) is served by the separate frr-bgp-peer module
 * contributed in FRR PR #21297, which is already wired in bgpd and
 * consumed by the MGC Connect PoC via gNMI Subscribe.
 *
 * This file exists only to keep the 4-file layout symmetric with the
 * staticd/ospfd-lite precedent (see poc/frr-grpc-poc/docs/
 * frr_nb_template.md section 5). If a future -lite revision adds a
 * config=false leaf (e.g. computed effective router-id), implement
 * the get_elem / get_next / lookup_entry / get_keys callbacks here.
 */

#include <zebra.h>

#include "northbound.h"

#include "bgpd/bgpd_nb_lite.h"

/* Intentionally empty -- no state callbacks in -lite v0.1. */
