/* SPDX-License-Identifier:    GPL-2.0
 *
 * Copyright (C) 2020 Marvell International Ltd.
 *
 * https://spdx.org/licenses
 */

#ifndef __BOARD_H__
#define __BOARD_H__

#include <asm/arch/soc.h>

/** Reg offsets */

/** Function definitions */
void mem_map_fill(void);
int fdt_get_board_mac_cnt(void);
u64 fdt_get_board_mac_addr(void);
const char *fdt_get_board_model(void);
const char *fdt_get_board_serial(void);
const char *fdt_get_board_revision(void);
void octeontx2_board_get_mac_addr(u8 index, u8 *mac_addr);
#endif /* __BOARD_H__ */