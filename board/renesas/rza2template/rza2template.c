/*
 * Copyright (C) 2019 Renesas Electronics Corporation
 *
 * This file is released under the terms of GPL v2 and any later version.
 * See the file COPYING in the root directory of the source tree for details.
 */

#include <common.h>
#include <asm/arch/r7s9210.h>
#include <asm/arch/sys_proto.h>
#include <dm/platform_data/serial_sh.h>
#include <i2c.h>
#include <spi_flash.h>
#include <netdev.h>
#include <asm/arch/mmc.h>
#include <asm/arch/sh_sdhi.h>

//#define DEBUG

DECLARE_GLOBAL_DATA_PTR;

/* Serial Console */
static const struct sh_serial_platdata serial_platdata = {
	.base = SCIF_CONSOLE_BASE,	/* SCIFx_BASE */
	.type = PORT_SCIF,		/* SCIF (not a SCIFA) */
	.clk = CONFIG_SYS_CLK_FREQ,	/* P1 Clock */
};
U_BOOT_DEVICE(rzatemplate_serial) = {
	.name = "serial_sh",
	.platdata = &serial_platdata,
};



#define PFC_BASE	0xFCFFE000
#define PDR_BASE	(PFC_BASE + 0x0000)	/* 16-bit, 2 bytes apart */
#define PODR_BASE	(PFC_BASE + 0x0040)	/* 8-bit, 1 byte apart */
#define PIDR_BASE	(PFC_BASE + 0x0060)	/* 8-bit, 1 byte apart */
#define PMR_BASE	(PFC_BASE + 0x0080)	/* 8-bit, 1 byte apart */
#define DSCR_BASE	(PFC_BASE + 0x0140)	/* 16-bit, 2 bytes apart */
#define PFS_BASE	(PFC_BASE + 0x0200)	/* 8-bit, 8 bytes apart */
#define PWPR		(PFC_BASE + 0x02FF)	/* 8-bit */
#define PFENET		(PFC_BASE + 0x0820)	/* 8-bit */
#define PPOC		(PFC_BASE + 0x0900)	/* 32-bit */
#define PHMOMO		(PFC_BASE + 0x0980)	/* 32-bit */
#define PMODEPFS	(PFC_BASE + 0x09C0)	/* 32-bit */
#define PCKIO		(PFC_BASE + 0x09D0)	/* 8-bit */


void pfc_set_pin_function(u8 port, u8 pin, u8 func)
{
	u16 reg16;
	u16 mask16;

	/* Set pin to 'Non-use (Hi-z input protection)'  */
	reg16 = *(volatile u16 *)(PDR_BASE + (port * 2));
	mask16 = 0x03 << (pin * 2);
	reg16 &= ~mask16;
	*(volatile u16 *)(PDR_BASE + port * 2) = reg16;

	/* Temporary switch to GPIO */
	*(volatile u8 *)(PMR_BASE + port) &= ~(1 << pin);

	/* PFS Register Write Protect : OFF */
	*(volatile u8 *)PWPR = 0x00; /* B0WI=0, PFSWE=0 */
	*(volatile u8 *)PWPR = 0x40; /* B0WI=0, PFSWE=1 */

	/* Set Pin function (interrupt disabled, ISEL=0) */
	*(volatile u8 *)(PFS_BASE + (port * 8) + pin) = func;

	/* PFS Register Write Protect : ON */
	*(volatile u8 *)PWPR = 0x00; /* B0WI=0, PFSWE=0 */
	*(volatile u8 *)PWPR = 0x80; /* B0WI=1, PFSWE=1 */

	/* Port Mode  : Peripheral module pin functions */
	*(volatile u8 *)(PMR_BASE + port) |= (1 << pin);
}

#define GPIO_IN 0
#define GPIO_OUT 1
void pfc_set_gpio(u8 port, u8 pin, u8 dir)
{
	u16 reg16;
	u16 mask16;

	reg16 = *(volatile u16 *)(PDR_BASE + (port * 2));
	mask16 = 0x03 << (pin * 2);
	reg16 &= ~mask16;

	if (dir == GPIO_IN)
		reg16 |= 2 << (pin * 2);	// pin as input
	else
		reg16 |= 3 << (pin * 2);	// pin as output

	*(volatile u16 *)(PDR_BASE + port * 2) = reg16;
}

void gpio_set(u8 port, u8 pin, u8 value)
{
	if (value)
		*(volatile u8 *)(PODR_BASE + port) |= (1 << pin);
	else
		*(volatile u8 *)(PODR_BASE + port) &= ~(1 << pin);
}

u8 gpio_read(u8 port, u8 pin)
{
	return (*(volatile u8 *)(PIDR_BASE + port) >> pin) & 1;
}

/* SECT_HYPERRAM */
void HyperRAM_Init(void)
{
	u32 val;

	/* SCLKSEL Setting */
	*(volatile u16 *)SCLKSEL |= (3 << 4); /* Hyper Clock = G/2phy */

	/* MCR0 Setting */
	val = 	(0 << 31) |	// MAXEN = 0
		(0 << 18) |	// MAXLEN = 0
		(0 << 5) |	// CRT = 0      /* memory space */
		(0 << 4) |	// DEVTYPE = 0  /* HyerRAM */
		(3 << 0);	// WRAPSIZE = 3 /* 32Byte  */
	*(volatile u32 *)HYPER_MCR0 = val;

	/* MCR1 Setting */
	val = 	(0 << 31) |	// MAXEN = 0
		(0 << 18) |	// MAXLEN = 0
		(0 << 5) |	// CRT = 0      /* memory space */
		(1 << 4) |	// DEVTYPE = 1  /* HyerRAM */
		(3 << 0);	// WRAPSIZE = 3 /* 32Byte  */
	*(volatile u32 *)HYPER_MCR1 = val;

	val = 	(0 << 28) |	// RCSHI = 0; /* 1.5 clock */
		(0 << 24) |	// WCSHI = 0; /* 1.5 clock */
		(0 << 20) |	// RCSS = 0;  /* 1 clock */
		(0 << 16) |	// WCSS = 0;  /* 1 clock */
		(0 << 12) |	// RCSH = 0;  /* 1 clock */
		(0 <<  8) |	// WCSH = 0;  /* 1 clock */
		(1 <<  0);	// LTCY = 1;  /* 6 clock Latency */
	*(volatile u32 *)HYPER_MTR0 = val;

	val = 	(0 << 28) |	// RCSHI = 0; /* 1.5 clock */
		(0 << 24) |	// WCSHI = 0; /* 1.5 clock */
		(0 << 20) |	// RCSS = 0;  /* 1 clock */
		(0 << 16) |	// WCSS = 0;  /* 1 clock */
		(0 << 12) |	// RCSH = 0;  /* 1 clock */
		(0 <<  8) |	// WCSH = 0;  /* 1 clock */
		(1 <<  0);	// LTCY = 1;  /* 6 clock Latency */
	*(volatile u32 *)HYPER_MTR1 = val;

	/* Dedicated Pin POC Control Register (PPOC) Setting */
	*(volatile u32 *)PPOC =
			1 * (1 << 9) |	/* POCSEL1 (1=POC1 is valid) */
			1 * (1 << 8) |	/* POCSEL0 (1=POC0 is valid) */
			1 * (1 << 3) |	/* POC3 SDMMC1 (0=1.8v, 1=3.3v) */
			1 * (1 << 2) |	/* SDMMC0 (0=1.8v, 1=3.3v) */
			0 * (1 << 1) |	/* POC1 Hyper (0=1.8v, 1=3.3v) */
			1 * (1 << 0);	/* POC0 QSPI (0=1.8v, 1=3.3v) */

	/* PHMOMO Setting */
	*(volatile u32 *)PHMOMO = 0xFFFFFFFE; /* Select HyperBus */
}
/* SECT_HYPERRAM_END */

int board_early_init_f(void)
{
/* SECT_SDRAM */
	int i;
/* SECT_SDRAM_END */

	/* This function runs early in the boot process, before u-boot is relocated
	   to RAM (hence the '_f' in the function name stands for 'still running from
	   flash'). A temporary stack has been set up for us which is why we can
	   have this as C code. */


	/* For SCIF, the default value of TEND after reset is 0, but
	   the serial_sh driver expects it to be 1, so we will get stuck waiting
	   and never send out the first character. Therefore, we'll send out a
	  'harmless' NULL (0x00) character before the pins are set up so it actually
	   never goes out the pins. */
	*(volatile u16 *)(SCIF_CONSOLE_BASE + 4) = 0x20;	// Set Transmit Enable bit
	*(volatile u8 *)(SCIF_CONSOLE_BASE + 6) = 0x00;		// Transmit a NULL
	while (! (*(volatile u16 *)(SCIF_CONSOLE_BASE + 8) & 0x0040)) {}	// Wait for TEND to go high

#define L2CACHE_8WAY    (0x000000FFuL)  /* All entries(8way) in the L2 cache */

	/* ==== Disable L2 cache ==== */
	PL310_REG(REG1_CONTROL) = 0x00000000uL;	/* Disable L2 cache */

	/* ==== Cache Sync ==== */
	/* Ensures completion of the L2 cache disable operation */
	PL310_REG(REG7_CACHE_SYNC) = 0x00000000uL;

	/* ==== Invalidate all L2 cache by Way ==== */
	/* Set "1" to Way bits[7:0] of the reg7_inv_way register */
	PL310_REG(REG7_INV_WAY) = L2CACHE_8WAY;

	/* Wait until Way bits[7:0] is cleared */
	while ( (PL310_REG(REG7_INV_WAY) & L2CACHE_8WAY) != 0x00000000uL) { /* Wait completion */ }

	/* ==== Cache Sync ==== */
	/* Ensures completion of the invalidate operation */
	PL310_REG(REG7_CACHE_SYNC) = 0x00000000uL;

	/* =========== Pin Setup =========== */
	/* Adjust for your board as needed. */

	/* Serial Console */
#if 0 /*** PLEASE configure pins according to Tables in section 51.3 in the Hardware Manual ***/
	pfc_set_pin_function(P9, 0, 4);	/* P9_0 = TxD */
	pfc_set_pin_function(P9, 1, 4);	/* P9_1 = RxD */
#endif

/* SECT_I2C */
	/* I2C */
#if 0 /*** PLEASE configure pins according to Tables in section 51.3 in the Hardware Manual ***/
	pfc_set_pin_function(PD, 6, 1);	/* P?_? = SCL? */
	pfc_set_pin_function(PD, 7, 1);	/* P?_? = SDA? */
#endif
/* SECT_I2C_END */

/* SECT_ETHERNET */
	/* Ethernet */
	/** RMII mode **/
	/* Ethernet */
	/* Channel 0 */
#if 0 /*** PLEASE configure pins according to Tables in section 51.3 in the Hardware Manual ***/
	pfc_set_pin_function(PE, 0, 7); /* REF50CK0 */
	pfc_set_pin_function(P6, 1, 7); /* RMMI0_TXDEN */
	pfc_set_pin_function(P6, 2, 7); /* RMII0_TXD0 */
	pfc_set_pin_function(P6, 3, 7); /* RMII0_TXD1 */
	pfc_set_pin_function(PE, 4, 7); /* RMII0_CRSDV */
	pfc_set_pin_function(PE, 1, 7); /* RMII0_RXD0 */
	pfc_set_pin_function(PE, 2, 7); /* RMII0_RXD1 */
	pfc_set_pin_function(PE, 3, 7); /* RMII0_RXER */
	pfc_set_pin_function(PE, 5, 1); /* ET0_MDC */
	pfc_set_pin_function(PE, 6, 1); /* ET0_MDIO */
#endif

	/* Ethernet */
	/* Channel 1 */
#if 0 /*** PLEASE configure pins according to Tables in section 51.3 in the Hardware Manual ***/
	pfc_set_pin_function(PK, 3, 7); /* REF50CK1 */
	pfc_set_pin_function(PK, 0, 7); /* RMMI1_TXDEN */
	pfc_set_pin_function(PK, 1, 7); /* RMII1_TXD0 */
	pfc_set_pin_function(PK, 2, 7); /* RMII1_TXD1 */
	pfc_set_pin_function(P3, 2, 7); /* RMII1_CRSDV */
	pfc_set_pin_function(PK, 4, 7); /* RMII1_RXD0 */
	pfc_set_pin_function(P3, 5, 7); /* RMII1_RXD1 */
	pfc_set_pin_function(P3, 1, 7); /* RMII1_RXER */
	pfc_set_pin_function(P3, 3, 1); /* ET1_MDC */
	pfc_set_pin_function(P3, 4, 1); /* ET1_MDIO */
#endif
/* SECT_ETHERNET_END */

/* SECT_SDMMC_CH0 */
	/* SD/MMC Channel 0 */
#if 0 /*** PLEASE configure pins according to Tables in section 51.3 in the Hardware Manual ***/
	pfc_set_pin_function(P5, 0, 3); /* SD0_CD */
	pfc_set_pin_function(P5, 1, 3); /* SD0_WP */
#endif
/* SECT_SDMMC_CH0_END */

/* SECT_SDMMC_CH1 */
	/* SD/MMC Channel 1 */
#if 0 /*** PLEASE configure pins according to Tables in section 51.3 in the Hardware Manual ***/
	pfc_set_pin_function(P5, 4, 3); /* SD1_CD */
	pfc_set_pin_function(P5, 5, 3); /* SD1_WP */
#endif
/* SECT_SDMMC_CH1_END */

/* SECT_SDRAM */
	/* SDRAM */
#if 0 /*** PLEASE configure pins according to Tables in section 51.3 in the Hardware Manual ***/
	/* D0 - D15 */
	for(i = 0; i <= 6; i++)
		pfc_set_pin_function(P0, i, 1);	/* P0_0~6 = D0-D6 */
	for(i = 0; i <= 4; i++)
		pfc_set_pin_function(P1, i, 1);	/* P1_0~4 = D7-D11 */
	for(i = 0; i <= 3; i++)
		pfc_set_pin_function(P2, i, 1);	/* P2_0~3 = D12-D15 */

	/* A1 - A15 */
	for(i=1;i<=7;i++)
		pfc_set_pin_function(P8, i, 1);	/* P8_1~7: A1-A7 */
	for(i=0;i<=7;i++)
		pfc_set_pin_function(P9, i, 1);	/* P9_0~7 = A8-A15 */

	pfc_set_pin_function(P6, 5, 1);	/* P6_5 = CS3 */
	pfc_set_pin_function(P7, 3, 1);	/* P7_3 = RAS */
	pfc_set_pin_function(P7, 4, 1);	/* P7_4 = CAS */
	pfc_set_pin_function(P7, 5, 1);	/* P7_5 = CKE */
	pfc_set_pin_function(P7, 1, 1);	/* P7_1 = RD/WR */
	pfc_set_pin_function(P6, 7, 1);	/* P6_7 = WE0/DQMLL */
	pfc_set_pin_function(P7, 0, 1);	/* P7_0 = WE1/DQMLU */
#endif
/* SECT_SDRAM_END */

	/* LED (example) */
	//pfc_set_gpio(P6, 0, GPIO_OUT); /* P6_0 = GPIO_OUT */
	//pfc_set_gpio(PC, 1, GPIO_OUT); /* PC_1 = GPIO_OUT */

/* SECT_HYPERRAM */
	/* Hyper RAM */
	HyperRAM_Init();
/* SECT_HYPERRAM_END */

/* SECT_SDRAM */
	/**********************************************/
	/* Configure SDRAM (CS3)                      */
	/**********************************************/

	//#define CS2BCR_D	0x00004C00	/* (CS2) Type=SDRAM, 16-bit memory */
	//#define CS2WCR_D	0x00000500	/* (CS2) CAS Latency = 2 */
	#define CS3BCR_D	0x00004C00	/* (CS3) Type=SDRAM, 16-bit memory */
	//#define CS3WCR_D	0x00002D13	/*  */
	#define CS3WCR_D	1 << 13	|	/* (CS2,CS3) WTRP (1 cycles) */\
				3 << 10 |	/* (CS2,CS3) WTRCD (3 cycles) */\
				2 <<  7 |	/*     (CS3) A3CL (CAS Latency = 3) */\
				2 <<  3 |	/* (CS2,CS3) TRWL (2 cycles) */\
				3 <<  0		/* (CS2,CS3) WTRC (5 cycles) */
	#define SDCR_D		0x00120812	/* 13-bit row, 10-bit col, auto-refresh */

	/*
	 * You must refresh all rows within the amount of time specified in the memory spec.
	 * Total Refresh time =  [Number_of_rows] / [Clock_Source / Refresh Counter]
	 * 63.55ms =  [2^13] /  [(132MHz / 16) / 64]
	 */
	/* SDRAM : 8K refresh cycles every 64ms */
	#define RTCOR_D		0xA55A0040	/* Refresh Counter = 64 */
	#define RTCSR_D		0xA55A0010	/* Clock Source=CKIO/16 (CKIO=132MHz) */

	//*(u32 *)CS2BCR = CS2BCR_D;
	//*(u32 *)CS2WCR = CS2WCR_D;
	*(u32 *)CS3BCR = CS3BCR_D;
	*(u32 *)CS3WCR = CS3WCR_D;
	*(u32 *)SDCR = SDCR_D;
	*(u32 *)RTCOR = RTCOR_D;
	*(u32 *)RTCSR = RTCSR_D;

	/* wait */
	#define REPEAT_D 0x000033F1
	for (i=0;i<REPEAT_D;i++) {
		asm("nop");
	}

	/* AC Characteristics Adjustment Register (ACADJ) */
	#define ACADJ 0x1F000090
	*(u32 *)ACADJ = 0x0002000F;	/* fixed value */

	/* The final step is to set the SDRAM Mode Register by written to a
	   specific address (the data value is ignored) */
	/* Check the hardware manual (table 8.11) if your settings differ */
	/*   Burst Length = 1 (fixed)
	 *   Burst Type = Sequential (fixed)
	 *   CAS Latency = 2 or 3 (see table 8.11)
	 *   Write Burst Mode = [burst read/single write] or [burst read/burst write] (see table 8.15)
	 */

	//#define SDRAM_MODE_CS2 0x1F001440	/* CS2: CAS=2, single write, 16bit bus */
	//#define SDRAM_MODE_CS3 0x1F002440	/* CS3: CAS=2, single write, 16bit bus */

	//#define SDRAM_MODE_CS2 0x1F001460	/* CS2: CAS=3, single write, 16bit bus */
	//#define SDRAM_MODE_CS3 0x1F002460	/* CS3: CAS=3, single write, 16bit bus */

	//#define SDRAM_MODE_CS2 0x1F001040	/* CS2: CAS=2, burst write, 16bit bus */
	//#define SDRAM_MODE_CS3 0x1F002040	/* CS3: CAS=2, burst write, 16bit bus */

	#define SDRAM_MODE_CS2 0x1F001060	/* CS2: CAS=3, burst write, 16bit bus */
	#define SDRAM_MODE_CS3 0x1F002060	/* CS3: CAS=3, burst write, 16bit bus */

	*(u16 *)SDRAM_MODE_CS2 = 0;
	*(u16 *)SDRAM_MODE_CS3 = 0;
/* SECT_SDRAM_END */

	return 0;
}

int board_init(void)
{
	gd->bd->bi_boot_params = (CONFIG_SYS_SDRAM_BASE + 0x100);
	return 0;
}

/* SECT_ETHERNET */
int board_eth_init(bd_t *bis)
{
	int ret = -ENODEV;
	ret = sh_eth_initialize(bis);
	return ret;
}
/* SECT_ETHERNET_END */

/* SECT_SDMMC */
int board_mmc_init(bd_t *bis)
{
	int ret = 0;
/* SECT_SDMMC_CH0 */
	ret |= sh_sdhi_init(CONFIG_SYS_SH_SDHI0_BASE,
			    0,
			    SH_SDHI_QUIRK_64BIT_BUF);
/* SECT_SDMMC_CH0_END */
/* SECT_SDMMC_CH1 */
	ret |= sh_sdhi_init(CONFIG_SYS_SH_SDHI1_BASE,
			    1,
			    SH_SDHI_QUIRK_64BIT_BUF);
/* SECT_SDMMC_CH1_END */

	return ret;
}
/* SECT_SDMMC_END */

int board_late_init(void)
{
/* SECT_ETHERNET */
	u8 mac[6]={0x74, 0x90, 0x50, 0xC0, 0xFE, 0x06};
/* SECT_ETHERNET_END */

	/* Increase the clock speed of the QSPI
	 * P0(33MHz) -> B(132MHz)
	 * SPI clock pin will be 1/2 the IP peripheral speed (66MHz) */
	*(volatile u16 *)SCLKSEL |= 2; /* QSPI Clock = B */

/* SECT_ETHERNET */
	if (is_valid_ethaddr(mac))
		eth_setenv_enetaddr("ethaddr", mac);
/* SECT_ETHERNET_END */

	printf(	"\t\t      SPI Flash Memory Map\n"
		"\t\t------------------------------------\n"
		"\t\t         Start      Size\n");
	printf(	"\t\tu-boot:  0x%08X 0x%06X\n", 0,CONFIG_ENV_OFFSET);
	printf(	"\t\t   env:  0x%08X 0x%06X\n", CONFIG_ENV_OFFSET, CONFIG_ENV_SIZE);
	printf(	"\t\t    DT:  0x%08X 0x%06X\n", 0xC0000,CONFIG_ENV_SECT_SIZE);
	printf(	"\t\tKernel:  0x%08X 0x%06X\n",0x200000, 0x50000);
	printf(	"\t\trootfs:  0x%08X 0x%06X\n",0x800000, 0x400000 - 0x800000);

	/* Default addresses */
	#define DTB_ADDR_FLASH		"0x200C0000"	/* Location of Device Tree in QSPI Flash */
	#define DTB_ADDR_RAM		"0x80300000"	/* Internal RAM location to copy Device Tree */
	#define DTB_ADDR_SDRAM		"0x0D800000"	/* External SDRAM location to copy Device Tree */
	#define DTB_ADDR_HYPRAM		"0x40300000"	/* HyperRAM location to copy Device Tree */

	#define KERNEL_ADDR_FLASH	"0x20200000"	/* Flash location of xipImage or uImage binary */
	#define UIMAGE_ADDR_SDRAM	"0x0D000000"	/* Address to copy uImage to in external SDRAM */
	#define UIMAGE_ADDR_SIZE	"0x400000"	/* Size of the uImage binary in Flash (4MB) */


	/* Kernel booting operations */

	/* => run xa_boot */
	/* Boot XIP using internal RAM only, file system is AXFS
	 * 1. Enable full XIP QSPI operation
	 * 2. Copy Device Tree from QSPI to Internal RAM
	 * 3. Boot XIP kernel */
	setenv("xa_boot",	"qspi single ; "
				"cp.b " DTB_ADDR_FLASH " " DTB_ADDR_RAM " 10000 ; "
				"bootx " KERNEL_ADDR_FLASH " " DTB_ADDR_RAM);

	/* => run xsa_boot */
	/* Boot XIP using external 64MB SDRAM, file system is AXFS, LCD FB fixed to internal RAM
	 * 1. Enable full XIP QSPI operation
	 * 2. Copy Device Tree from QSPI to SDRAM
	 * 3. Boot XIP kernel */
	setenv("xsa_boot",	"qspi single ; "
				"cp.b " DTB_ADDR_FLASH " " DTB_ADDR_SDRAM " 10000 ; "
				"bootx " KERNEL_ADDR_FLASH " " DTB_ADDR_SDRAM);

	/* => run xha_boot */
	/* Boot XIP using external 8MB HyperRAM, file system is AXFS, LCD FB fixed to internal RAM
	 * 1. Enable full XIP QSPI operation
	 * 2. Copy Device Tree from QSPI to HyperRAM
	 * 3. Boot XIP kernel */
	setenv("xha_boot",	"qspi single ; "
				"cp.b " DTB_ADDR_FLASH " " DTB_ADDR_HYPRAM " 10000 ; "
				"bootx " KERNEL_ADDR_FLASH " " DTB_ADDR_HYPRAM);

	/* => run u_boot */
	/* Boot SDRAM uImage using external 64MB SDRAM, file system is squashfs, LCD FB fixed to internal RAM
	 * 1. Enable full XIP QSPI operation
	 * 2. Copy Device Tree from QSPI to SDRAM
	 * 3. Copy uImage kernel from QSPI to SDRAM
	 * 4. Decompress uImage and boot kernel */
	setenv("u_boot",	"qspi single ; "
				"cp.b " DTB_ADDR_FLASH " " DTB_ADDR_SDRAM " 10000 ; "
				"cp.b " KERNEL_ADDR_FLASH " " UIMAGE_ADDR_SDRAM " " UIMAGE_ADDR_SIZE " ; "
				"bootm start " UIMAGE_ADDR_SDRAM " - " DTB_ADDR_SDRAM "; bootm loados ; bootm go");

	return 0;
}


int dram_init(void)
{
	#if (1 !=  CONFIG_NR_DRAM_BANKS)
	# error CONFIG_NR_DRAM_BANKS must set 1 in this board.
	#endif
	/* SDRAM setup is already done in board_early_init_f */
	gd->bd->bi_dram[0].start = CONFIG_SYS_SDRAM_BASE;
	gd->bd->bi_dram[0].size = CONFIG_SYS_SDRAM_SIZE;
	gd->ram_size = CONFIG_SYS_SDRAM_SIZE * CONFIG_NR_DRAM_BANKS;

	return 0;
}

const struct rmobile_sysinfo sysinfo = {
	CONFIG_ARCH_RMOBILE_BOARD_STRING
};

void reset_cpu(ulong addr)
{
	/* If you have board specific stuff to do, you can do it
	here before you reboot */

	/* Dummy read (must read WRCSR:WOVF at least once before clearing) */
	*(volatile u8 *)(WRCSR) = *(u8 *)(WRCSR);

	*(volatile u16 *)(WRCSR) = 0xA500;     /* Clear WOVF */
	*(volatile u16 *)(WRCSR) = 0x5A5F;     /* Reset Enable */
	*(volatile u16 *)(WTCNT) = 0x5A00;     /* Counter to 00 */
	*(volatile u16 *)(WTCSR) = 0xA578;     /* Start timer */

	while(1); /* Wait for WDT overflow */
}


