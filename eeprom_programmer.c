#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>

#include "gpio.h"

#ifndef EXIT_SUCCESS
# define EXIT_SUCCESS 0
#endif

#ifndef EXIT_FAILURE
# define EXIT_FAILURE 1
#endif

#define ADDR_0              0
#define ADDR_14             14
#define OUTPUT_ENABLE       15
#define DATA_0              16
#define DATA_7              23
#define WRITE_ENABLE        24

#define SDP_DISABLE_LEN     6
#define SDP_ENABLE_LEN      3

#define EEPROM_SIZE         0x8000
#define EEPROM_PAGE_SIZE    0x40
#define EEPROM_PAGES        0x200


static volatile uint32_t *gpio_port;
static volatile uint32_t *set_reg;
static volatile uint32_t *clr_reg;
static volatile uint32_t *level_reg;


void disable_interrupts() {
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGINT);
    sigprocmask(SIG_BLOCK, &set, NULL);
    return;
}


void enable_interrupts() {
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGINT);
    sigprocmask(SIG_UNBLOCK, &set, NULL);
    return;
}


/* avg 400 ns, range about 350 to 450 */
static void delay_400_ns(){
    volatile uint32_t count = 200;
    __asm volatile (
        "1: subs %[count], %[count], #1\n"
        "    bne 1b\n"
        : [count] "+r" (count)
    );
    return;
}


static void pulse_write_enable(){
    volatile uint32_t count = 200;
    *clr_reg = (1 << WRITE_ENABLE);
    // copy pasted code from delay_400_ns to avoid function call overhead
    __asm volatile (
        "1: subs %[count], %[count], #1\n"
        "    bne 1b\n"
        : [count] "+r" (count)
    );
    *set_reg = (1 << WRITE_ENABLE);
    return;
}


static void delay_write_complete() {
    usleep(12000);
    return;
}


static void set_data_input() {
    for (int pin = DATA_0; pin <= DATA_7; ++pin) {
        initialize_gpio_for_input(gpio_port, pin);
    }
    return;
}


static void set_data_output() {
    for (int pin = DATA_0; pin <= DATA_7; ++pin) {
        initialize_gpio_for_output(gpio_port, pin);
    }
    return;
}


static void initialize_gpio() {
    gpio_port = mmap_bcm_register(GPIO_REGISTER_BASE);
    set_reg = gpio_port + (GPIO_SET_OFFSET / sizeof(uint32_t));
    clr_reg = gpio_port + (GPIO_CLR_OFFSET / sizeof(uint32_t));
    level_reg = gpio_port + (GPIO_LEVEL_OFFSET / sizeof(uint32_t));

    *set_reg = 1 << WRITE_ENABLE;
    *clr_reg = ~(1 << WRITE_ENABLE);

    initialize_gpio_for_output(gpio_port, WRITE_ENABLE);
    initialize_gpio_for_output(gpio_port, OUTPUT_ENABLE);
    for (int pin = ADDR_0; pin <= ADDR_14; ++pin) {
        initialize_gpio_for_output(gpio_port, pin);
    }
    set_data_input();

    return;
}


static void populate_gpio_data(struct GPIOData *gpio_data,
                               const uint8_t *page,
                               const uint16_t page_start,
                               const int data_size) {
    uint32_t prev;
    uint32_t curr = 0x00;
    uint32_t next = (page[0] << DATA_0) | (page_start << ADDR_0);

    for (int offset = 0; offset < data_size; ++offset) {
        const uint16_t addr = page_start + offset;
        prev = curr;
        curr = next;

        if (offset < data_size - 1) {
            next = (page[offset + 1] << DATA_0)
                   | ((addr + 1) << ADDR_0);
        } else {
            next = 0x00;
        }

        gpio_data[offset].set = ~prev & curr;
        gpio_data[offset].clr = curr & ~next;
    }
    return;
}


static void populate_gpio_addresses(struct GPIOData *addresses,
                                    const uint16_t page_start,
                                    const int data_size) {
    uint32_t prev;
    uint32_t curr = 0x00;
    uint32_t next = (page_start << ADDR_0);

    for (int offset = 0; offset < data_size; ++offset) {
        const uint16_t addr = page_start | offset;
        prev = curr;
        curr = next;

        if (offset < data_size - 1) {
            next = (addr + 1) << ADDR_0;
        } else {
            next = 0x00;
        }

        addresses[offset].set = ~prev & curr;
        addresses[offset].clr = curr & ~next;
    }

    return;
}


static void populate_gpio_sequence(struct GPIOData *sequence,
                                   const uint16_t* addr,
                                   const uint8_t* data,
                                   const int sequence_size) {
    uint32_t prev;
    uint32_t curr = 0x00;
    uint32_t next = (data[0] << DATA_0) | (addr[0] << ADDR_0);

    for (int sequence_elem = 0; sequence_elem < sequence_size;
         ++sequence_elem) {
        prev = curr;
        curr = next;

        if (sequence_elem < sequence_size - 1) {
            next = (addr[sequence_elem + 1] << ADDR_0)
                 | (data[sequence_elem + 1] << DATA_0);
        } else {
            next = 0x00;
        }

        sequence[sequence_elem].set = ~prev & curr;
        sequence[sequence_elem].clr = curr & ~next;
    }

    return;
}


/* read data from GPIO into buffer */
static void eeprom_page_read(const struct GPIOData *gpio_data,
                             uint8_t *eeprom_data,
                             const int data_size) {

    *set_reg = (1 << WRITE_ENABLE) & ~(1 << OUTPUT_ENABLE);
    *clr_reg = ~(1 << WRITE_ENABLE) & (1 << OUTPUT_ENABLE);

    set_data_input();

    const struct GPIOData *start = gpio_data;
    const struct GPIOData *end = start + data_size;

    disable_interrupts();

    for (const struct GPIOData *it = start; it < end; ++it) {
        *set_reg = (it->set & ~(1 << OUTPUT_ENABLE)) | (1 << WRITE_ENABLE);

        usleep(1);
        eeprom_data[it - start] = (*level_reg >> DATA_0) & 0xff;
        delay_400_ns();

        *clr_reg = (it->clr & ~(1 << WRITE_ENABLE)) & ~(1 << OUTPUT_ENABLE);
    }

    *set_reg =   (1 <<OUTPUT_ENABLE) | (1 << WRITE_ENABLE);
    *clr_reg = ~((1 <<OUTPUT_ENABLE) | (1 << WRITE_ENABLE));

    enable_interrupts();

    return;
}


/* Write prepared data to GPIO */
static void eeprom_page_write(const struct GPIOData *gpio_data,
                              const int data_size) {

    *set_reg =   (1 << WRITE_ENABLE) | (1 << OUTPUT_ENABLE);
    *clr_reg = ~((1 << WRITE_ENABLE) | (1 << OUTPUT_ENABLE));

    set_data_output();

    const struct GPIOData *start = gpio_data;
    const struct GPIOData *end = start + data_size;

    disable_interrupts();

    for (const struct GPIOData *it = start; it < end; ++it) {
        *set_reg = it->set | (1 << WRITE_ENABLE);
        delay_400_ns();
        delay_400_ns();
        pulse_write_enable();
        delay_400_ns();
        delay_400_ns();
        *clr_reg = it->clr & ~(1 << WRITE_ENABLE);
    }

    enable_interrupts();
    return;
}


/* Enable AT28C256 software data protection */
static void enable_sdp() {

    static uint16_t addresses[] = { 0x5555, 0x2aaa, 0x5555 };
    static uint8_t data[] = { 0xaa, 0x55, 0xa0 };
    static struct GPIOData gpio_sdp_disable[SDP_ENABLE_LEN];
    populate_gpio_sequence(gpio_sdp_disable, addresses, data,
                   SDP_ENABLE_LEN);

    eeprom_page_write(gpio_sdp_disable, SDP_ENABLE_LEN);
}


/* Disable AT28C256 software data protection */
static void disable_sdp() {

    static uint16_t addresses[] = {
        0x5555, 0x2aaa, 0x5555, 0x5555, 0x2aaa, 0x5555
    };
    static uint8_t data[] = { 0xaa, 0x55, 0x80, 0xaa, 0x55, 0x20 };
    static struct GPIOData gpio_sdp_disable[SDP_DISABLE_LEN];
    populate_gpio_sequence(gpio_sdp_disable, addresses, data,
                   SDP_DISABLE_LEN);

    eeprom_page_write(gpio_sdp_disable, SDP_DISABLE_LEN);
}


int write_file(const char* input_file_name) {
    FILE *input = fopen(input_file_name, "rb");
    if (!input) {
        fprintf(stderr, "error opening input file %s\n", input_file_name);
        return EXIT_FAILURE;
    }

    struct GPIOData gpio_page_data[EEPROM_PAGE_SIZE];
    uint8_t eeprom_page_data[EEPROM_PAGE_SIZE];

    disable_sdp();

    for (uint16_t page_start = 0; page_start < EEPROM_SIZE;
         page_start += EEPROM_PAGE_SIZE) {
        if (feof(input)) {
            printf("input eof: page_start 0x%x\n", page_start);
        }

        if (ferror(input)) {
            printf("input error: page_start 0x%x\n", page_start);
        }

        int data_size = fread(eeprom_page_data, sizeof(uint8_t),
                              EEPROM_PAGE_SIZE, input);
        populate_gpio_data(gpio_page_data, eeprom_page_data,
                           page_start, data_size);
        eeprom_page_write(gpio_page_data, data_size);
        delay_write_complete();
    }

    enable_sdp();
    fclose(input);
    return EXIT_SUCCESS;
}


int dump_rom(const char* output_file_name) {
    FILE *output = fopen(output_file_name, "wb");
    if (!output) {
        fprintf(stderr, "error opening output file %s\n", output_file_name);
        return EXIT_FAILURE;
    }
    struct GPIOData addresses[64];
    uint8_t page_data[EEPROM_PAGE_SIZE];

    for (uint16_t page_start = 0; page_start < EEPROM_SIZE;
         page_start += EEPROM_PAGE_SIZE) {
        populate_gpio_addresses(addresses, page_start, EEPROM_PAGE_SIZE);
        eeprom_page_read(addresses, page_data, EEPROM_PAGE_SIZE);
        fwrite(page_data, sizeof(uint8_t), EEPROM_PAGE_SIZE, output);
    }

    fclose(output);
    return EXIT_SUCCESS;
}


void print_usage() {
    printf("Options:\n"
           "-d output_file_name\t dump rom contents\n"
           "-w input_file_name\t write file contents to rom\n"
           "-h\t\t\t print help\n"
    );
}

 
int main(int argc, char** argv) {
    char* input_file_name = NULL;
    char* output_file_name = NULL;

    int arg = 1;
    while (arg < argc) {
        char* option = argv[arg];
        if (option[0] != '-' || strlen(option) != 2) {
            print_usage();
            return EXIT_FAILURE;
        }

        switch (option[1]) {
            case 'd':
                if (arg == argc) {
                    fprintf(stderr, "Error: -d flag needs the name"
                            "of file to dump to\n");
                    return EXIT_FAILURE;
                }
                output_file_name = argv[++arg];
                break;
            case 'h':
                print_usage();
                return EXIT_SUCCESS;
            case 'w':
                if (arg == argc) {
                    fprintf(stderr, "Error: -w flag needs the name"
                            "of file to write\n");
                    return EXIT_FAILURE;
                }
                input_file_name = argv[++arg];
                break;
            default:
                fprintf(stderr, "Error: invalid option -%c", option[1]);
                print_usage();
                return EXIT_FAILURE;
        }
        ++arg;
    }

    if (!input_file_name && !output_file_name) {
        print_usage();
        return EXIT_FAILURE;
    }

    initialize_gpio();

    if (input_file_name) {
        write_file(input_file_name);
    }

    if (output_file_name) {
        dump_rom(output_file_name);
    }

    return EXIT_SUCCESS;
}


