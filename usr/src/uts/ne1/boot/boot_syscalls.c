/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License (the "License").
 * You may not use this file except in compliance with the License.
 *
 * You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
 * or http://www.opensolaris.org/os/licensing.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 * When distributing Covered Code, include this CDDL HEADER in each
 * file and include the License file at usr/src/OPENSOLARIS.LICENSE.
 * If applicable, add the following below this CDDL HEADER, with the
 * fields enclosed by brackets "[]" replaced with your own identifying
 * information: Portions Copyright [yyyy] [name of copyright owner]
 *
 * CDDL HEADER END
 */

/*
 * Copyright (c) 2006-2009 NEC Corporation
 * All rights reserved.
 */

#include <sys/types.h>
#include <sys/bootsvcs.h>
#include <sys/platform.h>
#include <sys/systm.h>
#include <sys/prom_debug.h>
#include <asm/cpufunc.h>

static void putchar(char c);
static uchar_t getchar(void);
static int ischar(void);

static void	boot_console_init(void);

#define	DISABLE_CONSOLE_INTR(intr, bits)				\
	do {								\
		(intr) = readl(NE1_CONSOLE_IER);			\
		if ((intr) & (bits)) {					\
			writel((intr) & ~(bits), NE1_CONSOLE_IER);	\
		}							\
	} while (0)

#define	RESTORE_CONSOLE_INTR(intr)					\
	do {								\
		if (panicstr == NULL) {					\
			if ((intr) & NE1_UART_IER_IE0) {		\
				/*					\
				 * Enable THRE interrupt because output	\
				 * data	may be spooled by ne_uart driver. \
				 */					\
				(intr) |= NE1_UART_IER_IE1;		\
			}						\
			writel((intr), NE1_CONSOLE_IER);		\
		}							\
	} while (0)

static uchar_t
getchar(void)
{
	uint32_t	data, intr;

	/* Disable IE0 interrupt to disable ne_uart driver input. */
	DISABLE_CONSOLE_INTR(intr, NE1_UART_IER_IE0);

	/* Wait for data. */
	while (ischar() == 0);

	/* Read data. */
	data = readl(NE1_CONSOLE_RBR);

	RESTORE_CONSOLE_INTR(intr);

	return (uchar_t)(data & 0xff);
}

static void
putchar(char c)
{
	uint32_t	intr;

	/* Disable IE1 interrupt to disable ne_uart driver output. */
	DISABLE_CONSOLE_INTR(intr, NE1_UART_IER_IE1);

	/* Wait until all data in output FIFO is flushed. */
	while ((readl(NE1_CONSOLE_LSR) & NE1_UART_LSR_LSR5) == 0);

	/* Write data. */
	writel(c, NE1_CONSOLE_THR);

	RESTORE_CONSOLE_INTR(intr);
}

static int
ischar(void)
{
	/* Check status. */
	return (readl(NE1_CONSOLE_LSR) & NE1_UART_LSR_LSR0);
}


/* setup boot syscall fields needed by the kernel */
static struct boot_syscalls sc = {
	getchar,
	putchar,
	ischar
};

/*
 * void
 * boot_sysp_init(void)
 *	Initialize boot_syscalls.
 */
void
boot_sysp_init(void)
{
	/* Initialize boot console. */
	boot_console_init();

	sysp = &sc;
}

/*
 * static void
 * boot_console_init(void)
 *	Initialize boot console.
 *	  Baud Rate : 38400 bps
 *	  Data size : 8 bit
 *	  Stop      : 1 bit
 *	  Parity    : None
 */
static void
boot_console_init(void)
{
	writel(0x00, NE1_CONSOLE_LCR);
	writel(0x00, NE1_CONSOLE_IER);
	writel(0x07, NE1_CONSOLE_FCR);
	writel(0x80, NE1_CONSOLE_LCR);
	writel(UART_DEFAULT_BAUDRATE_DLL, NE1_CONSOLE_DLL);
	writel(UART_DEFAULT_BAUDRATE_DLH, NE1_CONSOLE_DLH);
	writel(0x03, NE1_CONSOLE_LCR);
	writel(0x02, NE1_CONSOLE_MCR);
}
