#ifndef DATA_WRITE_H
#define DATA_WRITE_H

/* -*- mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 * Copyright (c) 2015, Henner Zeller <h.zeller@acm.org>
 * This is provided as-is to the public domain.
 *
 * This is not meant to be useful code, just an attempt
 * to understand GPIO performance if sent with DMA (which is: too low).
 * It might serve as an educational example though.
 */

#include <assert.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

// Physical Memory Allocation, from raspberrypi/userland demo.
#include "mailbox.h"

// sleep
#include "unistd.h"

// GPIO which we want to toggle in this example.
#define TOGGLE_GPIO 14

// Raspberry Pi 2 or 1 ? Since this is a simple example, we don't
// bother auto-detecting but have it a compile-time option.
#ifndef PI_VERSION
#	define PI_VERSION 2
#endif

#define BCM2708_PI1_PERI_BASE	0x20000000
#define BCM2709_PI2_PERI_BASE	0x3F000000
#define BCM2711_PI4_PERI_BASE	0xFE000000

// --- General, Pi-specific setup.
#if PI_VERSION == 1
#	define PERI_BASE BCM2708_PI1_PERI_BASE
#elif PI_VERSION == 2 || PI_VERSION == 3
#	define PERI_BASE BCM2709_PI2_PERI_BASE
#else
#	define PERI_BASE BCM2711_PI4_PERI_BASE
#endif

#define PAGE_SIZE 4096

// ---- GPIO specific defines
#define GPIO_REGISTER_BASE 0x200000
#define GPIO_SET_OFFSET 0x1C
#define GPIO_CLR_OFFSET 0x28
#define GPIO_LEVEL_OFFSET 0x34
#define PHYSICAL_GPIO_BUS (0x7E000000 + GPIO_REGISTER_BASE)

// ---- Memory mappping defines
#define BUS_TO_PHYS(x) ((x)&~0xC0000000)

// ---- Memory allocating defines
// https://github.com/raspberrypi/firmware/wiki/Mailbox-property-interface
#define MEM_FLAG_DIRECT		 (1 << 2)
#define MEM_FLAG_COHERENT	 (2 << 2)
#define MEM_FLAG_L1_NONALLOCATING (MEM_FLAG_DIRECT | MEM_FLAG_COHERENT)

// ---- DMA specific defines
#define DMA_CHANNEL	 5	 // That usually is free.
#define DMA_BASE	0x007000

// BCM2385 ARM Peripherals 4.2.1.2
#define DMA_CB_TI_NO_WIDE_BURSTS	(1<<26)
#define DMA_CB_TI_SRC_INC		(1<<8)
#define DMA_CB_TI_DEST_INC		(1<<4)
#define DMA_CB_TI_TDMODE		(1<<1)

#define DMA_CS_RESET			(1<<31)
#define DMA_CS_ABORT			(1<<30)
#define DMA_CS_DISDEBUG 		(1<<28)
#define DMA_CS_END			(1<<1)
#define DMA_CS_ACTIVE	 		(1<<0)

#define DMA_CB_TXFR_LEN_YLENGTH(y)	(((y-1)&0x4fff) << 16)
#define DMA_CB_TXFR_LEN_XLENGTH(x)	((x)&0xffff)
#define DMA_CB_STRIDE_D_STRIDE(x)	(((x)&0xffff) << 16)
#define DMA_CB_STRIDE_S_STRIDE(x)	((x)&0xffff)

#define DMA_CS_PRIORITY(x)		((x)&0xf << 16)
#define DMA_CS_PANIC_PRIORITY(x)	((x)&0xf << 20)


// Documentation: BCM2835 ARM Peripherals @4.2.1.2
struct dma_channel_header {
	uint32_t cs;		// control and status.
	uint32_t cblock;	// control block address.
};

// @4.2.1.1
struct dma_cb {			// 32 bytes.
	uint32_t info;		// transfer information.
	uint32_t src;		// physical source address.
	uint32_t dst;		// physical destination address.
	uint32_t length;	// transfer length.
	uint32_t stride;	// stride mode.
	uint32_t next;		// next control block; Physical address. 32 byte aligned.
	uint32_t pad[2];
};

// A memory block that represents memory that is allocated in physical
// memory and locked there so that it is not swapped out.
// It is not backed by any L1 or L2 cache, so writing to it will directly
// modify the physical memory (and it is slower of course to do so).
// This is memory needed with DMA applications so that we can write through
// with the CPU and have the DMA controller 'see' the data.
// The UncachedMemBlock_{alloc,free,to_physical}
// functions are meant to operate on these.
struct UncachedMemBlock {
	void *mem;	// User visible value: the memory to use.
	//-- Internal representation.
	uint32_t bus_addr;
	uint32_t mem_handle;
	size_t size;
};


// Layout of our input data. The values are pre-expanded to set and clr.
struct GPIOData {
	uint32_t set;
	uint32_t clr;
};


// Return a pointer to a periphery subsystem register.
void *mmap_bcm_register(off_t register_offset);


void initialize_gpio_for_input(volatile uint32_t *gpio_registerset, int bit);


void initialize_gpio_for_output(volatile uint32_t *gpio_registerset, int bit);


#endif


