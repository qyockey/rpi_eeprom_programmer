#include "gpio.h"


// Return a pointer to a periphery subsystem register.
void *mmap_bcm_register(off_t register_offset) {
	const off_t base = PERI_BASE;

	int mem_fd;
	if ((mem_fd = open("/dev/mem", O_RDWR|O_SYNC) ) < 0) {
		perror("can't open /dev/mem: ");
		fprintf(stderr, "You need to run this as root!\n");
		return NULL;
	}

	uint32_t *result =
		(uint32_t*) mmap(NULL,			// Any adddress in our space will do
				 PAGE_SIZE,
				 PROT_READ|PROT_WRITE,	// Enable r/w on GPIO registers.
				 MAP_SHARED,
				 mem_fd,		// File to map
				 base + register_offset // Offset to bcm register
				 );
	close(mem_fd);

	if (result == MAP_FAILED) {
		fprintf(stderr, "mmap error %p\n", result);
		return NULL;
	}
	return result;
}

void initialize_gpio_for_input(volatile uint32_t *gpio_registerset, int bit) {
	*(gpio_registerset+(bit/10)) &= ~(7<<((bit%10)*3));	// set as input
}


void initialize_gpio_for_output(volatile uint32_t *gpio_registerset, int bit) {
	*(gpio_registerset+(bit/10)) &= ~(7<<((bit%10)*3));	// prepare: set as input
	*(gpio_registerset+(bit/10)) |=	(1<<((bit%10)*3));	// set as output.
}


