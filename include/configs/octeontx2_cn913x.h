/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright (C) 2016 Stefan Roese <sr@denx.de>
 * Copyright (C) 2019 Marvell International Ltd.
 */

#ifndef _CONFIG_OCTEONTX2_CN913X_H
#define _CONFIG_OCTEONTX2_CN913X_H

#define DEFAULT_CONSOLE		"console=ttyS0,115200 "	\
	"earlycon=uart8250,mmio32,0xf0512000"

#include <configs/mvebu_armada-common.h>

/*
 * High Level Configuration Options (easy to change)
 */
#define CONFIG_SYS_TCLK		250000000	/* 250MHz */

#define CONFIG_SYS_MAX_NAND_DEVICE	1
#define CONFIG_SYS_NAND_MAX_CHIPS	1
#define CONFIG_SYS_NAND_ONFI_DETECTION

#define CONFIG_USB_MAX_CONTROLLER_COUNT (3 + 3)

#define BOOT_TARGET_DEVICES(func) \
	func(MMC, mmc, 1) \
	func(MMC, mmc, 0) \
	func(USB, usb, 0) \
	func(SCSI, scsi, 0) \
	func(PXE, pxe, na) \
	func(DHCP, dhcp, na)

#include <config_distro_bootcmd.h>

#undef CONFIG_EXTRA_ENV_SETTINGS
#define CONFIG_EXTRA_ENV_SETTINGS \
	"kernel_addr_r=0x7000000\0" \
	"fdt_addr_r=0x6f00000\0" \
	"ramdisk_addr_r=0x9000000\0" \
	"scriptaddr=0x6e00000\0" \
	"pxefile_addr_r=0x6000000\0" \
	BOOTENV

/* RTC configuration */
#ifdef CONFIG_MARVELL_RTC
#define ERRATA_FE_3124064
#endif

#endif /* _CONFIG_OCTEONTX2_CN913X_H */
