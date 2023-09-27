// SPDX-License-Identifier:    GPL-2.0+
/*
 * Copyright (C) 2016 Stefan Roese <sr@denx.de>
 * Copyright (C) 2020 Marvell International Ltd.
 *
 * https://spdx.org/licenses
 */

#include <common.h>
#include <console.h>
#include <dm.h>
#include <env.h>
#include <i2c.h>
#include <asm/io.h>
#include <asm/arch/cpu.h>
#include <asm/arch/soc.h>
#include <power/regulator.h>
#include <tlv_eeprom.h>
#ifdef CONFIG_BOARD_CONFIG_EEPROM
#include <mvebu/cfg_eeprom.h>
#endif
#ifdef CONFIG_ARMV8_SEC_FIRMWARE_SUPPORT
#include <asm/armv8/sec_firmware.h>
#endif

#define CP_USB20_BASE_REG(cp, p)	(MVEBU_REGS_BASE_CP(0, cp) + \
						0x00580000 + 0x1000 * (p))
#define CP_USB20_TX_CTRL_REG(cp, p)	(CP_USB20_BASE_REG(cp, p) + 0xC)
#define CP_USB20_TX_OUT_AMPL_MASK	(0x7 << 20)
#define CP_USB20_TX_OUT_AMPL_VALUE	(0x3 << 20)

DECLARE_GLOBAL_DATA_PTR;

#define BOOTCMD_NAME	"pci-bootcmd"

__weak int soc_early_init_f(void)
{
	return 0;
}

int board_early_init_f(void)
{
	soc_early_init_f();

	return 0;
}

int board_early_init_r(void)
{
	if (CONFIG_IS_ENABLED(DM_REGULATOR)) {
		/* Check if any existing regulator should be turned down */
		regulators_enable_boot_off(false);
	}

	return 0;
}

int board_init(void)
{
	/* address of boot parameters */
	gd->bd->bi_boot_params = CONFIG_SYS_SDRAM_BASE + 0x100;

	writel(0xfbfff7f, 0xf2440144); //define output enable
	writel(0x0000000, 0xf2440140); //define output value 0


#if (CONFIG_IS_ENABLED(BOARD_CONFIG_EEPROM))
		cfg_eeprom_init();
#endif

	return 0;
}

int board_fix_fdt (void *fdt)
{
        return 0;
}


#if (CONFIG_IS_ENABLED(OCTEONTX_SERIAL_BOOTCMD) ||	\
	CONFIG_IS_ENABLED(OCTEONTX_SERIAL_PCIE_CONSOLE)) &&	\
	!CONFIG_IS_ENABLED(CONSOLE_MUX)
# error CONFIG_CONSOLE_MUX must be enabled!
#endif

#if CONFIG_IS_ENABLED(OCTEONTX_SERIAL_BOOTCMD)
static int init_bootcmd_console(void)
{
	int ret = 0;
	char *stdinname = env_get("stdin");
	struct udevice *bootcmd_dev = NULL;
	bool stdin_set;
	char iomux_name[128];

	debug("%s: stdin before: %s\n", __func__,
	      stdinname ? stdinname : "NONE");
	if (!stdinname) {
		env_set("stdin", "serial");
		stdinname = env_get("stdin");
	}
	stdin_set = !!strstr(stdinname, BOOTCMD_NAME);
	ret = uclass_get_device_by_driver(UCLASS_SERIAL,
					  DM_GET_DRIVER(octeontx_bootcmd),
					  &bootcmd_dev);
	if (ret) {
		pr_err("%s: Error getting %s serial class\n", __func__,
		       BOOTCMD_NAME);
	} else if (bootcmd_dev) {
		if (stdin_set)
			strncpy(iomux_name, stdinname, sizeof(iomux_name));
		else
			snprintf(iomux_name, sizeof(iomux_name), "%s,%s",
				 stdinname, bootcmd_dev->name);
		ret = iomux_doenv(stdin, iomux_name);
		if (ret)
			pr_err("%s: Error %d enabling the PCI bootcmd input console \"%s\"\n",
			       __func__, ret, iomux_name);
		if (!stdin_set)
			env_set("stdin", iomux_name);
	}
	debug("%s: Set iomux and stdin to %s (ret: %d)\n",
	      __func__, iomux_name, ret);
	return ret;
}
#endif

/*
 * Read TLV formatted data from eeprom.
 * Only read as much data as indicated by the TLV header.
 */
// TODO: this should be a library function?!
static bool get_tlvinfo_from_eeprom(int index, u8 *buffer, size_t length) {
	struct tlvinfo_header *eeprom_hdr = (struct tlvinfo_header *) buffer;
	struct tlvinfo_tlv *eeprom_tlv = (struct tlvinfo_tlv *) &buffer[sizeof(struct tlvinfo_header)];

	if(length < TLV_INFO_HEADER_SIZE) {
		pr_err("%s: buffer too small for tlv header!\n", __func__);
		return false;
	}
	if(read_tlv_eeprom((void *)eeprom_hdr, 0, TLV_INFO_HEADER_SIZE, index) != 0) {
		pr_err("%s: failed to read from eeprom!\n", __func__);
		return false;
	}
	if(!is_valid_tlvinfo_header(eeprom_hdr)) {
		pr_warn("%s: invalid tlv header!\n", __func__);
		return false;
	}
	if(length - TLV_INFO_HEADER_SIZE < be16_to_cpu(eeprom_hdr->totallen)) {
		pr_err("%s: buffer too small for tlv data!\n", __func__);
		return false;
	}
	if(read_tlv_eeprom((void *)eeprom_tlv, sizeof(struct tlvinfo_header), be16_to_cpu(eeprom_hdr->totallen), index) != 0) {
		pr_err("%s: failed to read from eeprom!\n", __func__);
		return false;
	}

	return true;
}

static void get_fdtfile_from_tlv_eeprom(u8 *buffer, size_t length) {
	char cpu[5] = {0};
	char carrier[12] = {0};
	static u8 eeprom[TLV_INFO_MAX_LEN];
	char sku[257];

	for(int i = 0; i < 2;i++) {
		// read eeprom
		if(!get_tlvinfo_from_eeprom(i, eeprom, sizeof(eeprom))) {
			pr_info("%s: failed to read eeprom %d\n", __func__, i);
			continue;
		}

		// read sku
		if(!tlvinfo_read_tlv(eeprom, TLV_CODE_PART_NUMBER, sku, sizeof(sku))) {
			pr_warn("%s: could not find sku in eeprom\n", __func__);
			continue;
		}
		pr_debug("%s: read sku %s\n", __func__, sku);

		if(memcmp(&sku[0], "VT", 2) == 0) {
			// parse sku - processor or carrier indicated at index 2-6
			if(memcmp(&sku[2], "CFCB", 4) == 0) {
				// AIR-300 Carrier
				strcpy(carrier, "vt-air-300");

				// #CPs on carrier indicated by next 4 chars
				memcpy(cpu, &sku[2+4], 4);
			} else {
				pr_err("%s: did not recognise SKU %s!\n", __func__, sku);
			}
		} else {
			// parse sku - processor or carrier indicated at index 2-6
			if(memcmp(&sku[2], "CFCB", 4) == 0) {
				// Clearfog Base
				strcpy(carrier, "cf-base");

				// Carrier has no extra CPs
				strcpy(cpu, "9130");
			} else if(memcmp(&sku[2], "CFCP", 4) == 0) {
				// Clearfog Pro
				strcpy(carrier, "cf-pro");

				// Carrier has no extra CPs
				strcpy(cpu, "9130");
			} else if(memcmp(&sku[2], "C", 1) == 0) {
				// COM-Express 7 - C9130 / C9131 / C9132 ...
				strcpy(carrier, "cex7");

				// COM can have additional CPs indicated in next 4 chars
				memcpy(cpu, &sku[2+1], 4);
			} else if(memcmp(&sku[2], "S9130", 4) == 0) {
				// SoM - S913x
			} else if(memcmp(&sku[2], "CFSW", 4) == 0) {
				// SolidWan SOM S9131
				strcpy(carrier, "cf-solidwan");

				// Carrier has 1 extra CPs
				strcpy(cpu, "9131");
			} else {
				pr_err("%s: did not recognise SKU %s!\n", __func__, sku);
			}
		}
	}

	if(!cpu[0]) {
		pr_err("%s: could not identify SoC, defaulting to %s!\n", __func__, "CN9130");
		strcpy(cpu, "9130");
	}

	if(!carrier[0]) {
		pr_err("%s: could not identify carrier, defaulting to %s!\n", __func__, "Clearfog Pro");
		strcpy(carrier, "cf-pro");
	}

	// assemble fdtfile
	if(snprintf(buffer, length, "marvell/cn%s-%s.dtb", cpu, carrier) >= length) {
		pr_err("%s: fdtfile buffer too small, result truncated!\n", __func__);
	}
}

u64 fdt_get_board_info(void);
int board_late_init(void)
{
	char fdtfile[32] = {0};

	// identify device
	get_fdtfile_from_tlv_eeprom(fdtfile, sizeof(fdtfile));
	if (!env_get("fdtfile"))
		env_set("fdtfile", fdtfile);

#if CONFIG_IS_ENABLED(OCTEONTX_SERIAL_BOOTCMD)
	if (init_bootcmd_console())
		printf("Failed to init bootcmd input\n");
#endif
	fdt_get_board_info();
	return 0;
}

void ft_cpu_setup(void *blob, struct bd_info *bd)
{
#ifdef CONFIG_ARMV8_SEC_FIRMWARE_SUPPORT
	fdt_fixup_kaslr(blob);
#endif
}

int ft_board_setup(void *blob, struct bd_info *bd)
{
	ft_cpu_setup(blob, bd);
	return 0;
}
