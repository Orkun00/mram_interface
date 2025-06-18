#include "mram.h"
#include <unistd.h>  // for usleep

// Helper function to transfer a single byte
static bool mram_transfer_byte(struct mram* mram, uint8_t tx_byte, uint8_t* rx_byte) {
    if (mram == NULL) return false;
    
    uint8_t tx_buf[1] = { tx_byte };
    uint8_t rx_buf[1];

    if (!mram->spi_transfer(tx_buf, rx_byte ? rx_buf : NULL, 1))
        return false;

    if (rx_byte)
        *rx_byte = rx_buf[0];

    return true;
}

bool mram_init(struct mram* mram, bool (*gpio_write)(uint8_t, uint8_t),
               bool (*spi_transfer)(const uint8_t*, uint8_t*, size_t),
               uint8_t cs_pin) {
    // Validate parameters
    if (mram == NULL || gpio_write == NULL || spi_transfer == NULL)
        return false;
    
    // Initialize the structure
    mram->gpio_write = gpio_write;
    mram->spi_transfer = spi_transfer;
    mram->cs_pin = cs_pin;
    
    // Ensure CS is high (device deselected)
    if (!mram->gpio_write(cs_pin, MRAM_GPIO_HIGH))
        return false;
    
    // Disable writing by default
    return mram_write_disable(mram);
}

bool mram_write_enable(struct mram* mram) {
    if (mram == NULL) return false;
    
    if (!mram->gpio_write(mram->cs_pin, MRAM_GPIO_LOW)) return false;
    if (!mram_transfer_byte(mram, MRAM_CMD_WREN, NULL)) return false;
    if (!mram->gpio_write(mram->cs_pin, MRAM_GPIO_HIGH)) return false;
    return true;
}

//The Write Enable Latch (WEL) is reset to 0 on power-up or when the WRDI command is completed.
bool mram_write_disable(struct mram* mram) {
    if (mram == NULL) return false;
    
    if (!mram->gpio_write(mram->cs_pin, MRAM_GPIO_LOW)) return false;
    if (!mram_transfer_byte(mram, MRAM_CMD_WRDI, NULL)) return false;
    if (!mram->gpio_write(mram->cs_pin, MRAM_GPIO_HIGH)) return false;
    return true;
}

//You need to give me 19 bits address otherwise it will return false. Also you need to give me a buffer to store the data and the length of the data.
bool mram_read(struct mram* mram, uint32_t addr, uint8_t* buffer, size_t len) {
    if (mram == NULL || buffer == NULL || len == 0) {
        return false;
    }
    
    // Validate address range
    if ((addr > MRAM_MAX_ADDRESS) || ((addr + len - 1) > MRAM_MAX_ADDRESS)) {
        return false;
    }

    // Mask address to 19 bits
    addr &= MRAM_ADDRESS_MASK;
    
    // Command structure: CMD(1) + ADDR(3) + DATA(n)
    uint8_t tx_buf[4] = {
        MRAM_CMD_READ,
        (addr >> 16) & 0xFF,
        (addr >> 8) & 0xFF,
        addr & 0xFF
    };
    
    // Buffer for receiving command echo and data
    uint8_t rx_buf[len + 4];
    
    if (!mram->gpio_write(mram->cs_pin, MRAM_GPIO_LOW)) return false;
    
    // Single transfer for command, address and data
    // First 4 bytes received are garbage (command + address echo)
    if (!mram->spi_transfer(tx_buf, rx_buf, len + 4)) {
        mram->gpio_write(mram->cs_pin, MRAM_GPIO_HIGH);
        return false;
    }
    
    // Copy actual data, skipping command and address echo
    for (size_t i = 0; i < len; i++) {
        buffer[i] = rx_buf[i + 4];
    }
    
    if (!mram->gpio_write(mram->cs_pin, MRAM_GPIO_HIGH)) return false;
    return true;
}

//I need address and data buffer and length of the data. Address is 24 bits but only 19 bits are used. So dont give me more than 19 bits otherwise i will get rid of rest of the bits.
bool mram_write(struct mram* mram, uint32_t addr, const uint8_t* data, size_t len) {
    if (mram == NULL || data == NULL || len == 0) {
        return false;
    }
    
    // Validate address range
    if ((addr > MRAM_MAX_ADDRESS) || ((addr + len - 1) > MRAM_MAX_ADDRESS)) {
        return false;
    }

    // Mask address to 19 bits
    addr &= MRAM_ADDRESS_MASK;
    
    // Enable writing before write operation
    if (!mram_write_enable(mram)) return false;
    
    // Command structure: CMD(1) + ADDR(3) + DATA(n)
    uint8_t tx_buf[len + 4];
    tx_buf[0] = MRAM_CMD_WRITE;
    tx_buf[1] = (addr >> 16) & 0xFF;
    tx_buf[2] = (addr >> 8) & 0xFF;
    tx_buf[3] = addr & 0xFF;
    
    // Copy data after command and address
    for (size_t i = 0; i < len; i++) {
        tx_buf[i + 4] = data[i];
    }
    
    if (!mram->gpio_write(mram->cs_pin, MRAM_GPIO_LOW)) return false;
    
    // Single transfer for command, address and data
    if (!mram->spi_transfer(tx_buf, NULL, len + 4)) {
        mram->gpio_write(mram->cs_pin, MRAM_GPIO_HIGH);
        return false;
    }
    
    if (!mram->gpio_write(mram->cs_pin, MRAM_GPIO_HIGH)) return false;
    
    // Disable writing after operation complete
    if (!mram_write_disable(mram)) return false;
    return true;
}

//I work with mram instance given to me.
bool mram_sleep(struct mram* mram) {
    if (mram == NULL) return false;
    
    if (!mram->gpio_write(mram->cs_pin, MRAM_GPIO_LOW)) return false;
    if (!mram_transfer_byte(mram, MRAM_CMD_SLEEP, NULL)) return false;
    if (!mram->gpio_write(mram->cs_pin, MRAM_GPIO_HIGH)) return false;

    // Wait for tDP time to ensure device enters sleep mode
    usleep(MRAM_TDP_US);
    return true;
}

//The CS pin must remain high until the tRDP period is over. WAKE must be executed after sleep mode entry and prior to any other command.
bool mram_wake(struct mram* mram) {
    if (mram == NULL) return false;
    
    if (!mram->gpio_write(mram->cs_pin, MRAM_GPIO_LOW)) return false;
    if (!mram_transfer_byte(mram, MRAM_CMD_WAKE, NULL)) return false;
    if (!mram->gpio_write(mram->cs_pin, MRAM_GPIO_HIGH)) return false;

    // Wait for tRDP time to ensure device is ready
    // CS must remain high during this period
    usleep(MRAM_TRDP_US);
    return true;
}

/*An RDSR command cannot immediately follow a READ command. If an RDSR command immediately follows a READ com-
mand, the output data will not be correct. Any other sequence of commands is allowed. If an RDSR command is required
immediately following a READ command, it is necessary that another command be inserted before the RDSR is executed.
Alternatively, two successive RDSR commands can be issued following the READ command. The second RDSR will output the
proper state of the Status Register.*/
bool mram_read_status_register(struct mram* mram, uint8_t* status) {
    if (mram == NULL || status == NULL) return false;
    uint8_t rx_byte;
    
    if (!mram->gpio_write(mram->cs_pin, MRAM_GPIO_LOW)) return false;
    
    // Two transfers: first sends command, second gets status
    if (!mram_transfer_byte(mram, MRAM_CMD_RDSR, NULL)) return false;
    if (!mram_transfer_byte(mram, 0xFF, &rx_byte)) return false;
    
    *status = rx_byte;
    
    if (!mram->gpio_write(mram->cs_pin, MRAM_GPIO_HIGH)) return false;
    return true;
}

/*The Write Status Register (WRSR) command allows the Status Register to be written. The Status Register can be written to set the write enable latch bit, status register write protect bit, and block write protect bits. The WRSR command is entered by driving CS low, sending the command code, and then driving CS high. The WRSR command cannot immediately follow a READ command. If a WRSR command immediately follows a READ command, the output data will not be correct. Any other sequence of commands is allowed. If a WRSR command is required immediately following a READ command, it is necessary that another command be inserted before the WRSR is executed. Alternatively, two successive WRSR commands can be issued following the READ command. The second WRSR will output the proper state of the Status Register.*/
bool mram_write_status_register(struct mram* mram, uint8_t status) {
    if (mram == NULL) return false;
    
    // Enable writing before modifying status register
    if (!mram_write_enable(mram)) return false;
    
    if (!mram->gpio_write(mram->cs_pin, MRAM_GPIO_LOW)) return false;
    if (!mram_transfer_byte(mram, MRAM_CMD_WRSR, NULL)) return false;
    if (!mram_transfer_byte(mram, status, NULL)) return false;
    if (!mram->gpio_write(mram->cs_pin, MRAM_GPIO_HIGH)) return false;
    
    // Disable writing after operation complete
    if (!mram_write_disable(mram)) return false;
    return true;
}

bool mram_is_write_enabled(struct mram* mram) {
    if (mram == NULL) return false;
    uint8_t status;
    if (!mram_read_status_register(mram, &status)) return false;
    return (status & 0x02);  // Check WEL bit
}

bool mram_is_write_protected(struct mram* mram) {
    if (mram == NULL) return false;
    uint8_t status;
    if (!mram_read_status_register(mram, &status)) return false;
    return (status & 0x80);  // Check WPEN bit
}

bool mram_is_block_protected(struct mram* mram, uint8_t block_number) {
    if (mram == NULL) return false;
    uint8_t status;
    if (!mram_read_status_register(mram, &status)) return false;
    return (status & (0x04 << block_number));  // Check BP0 and BP1 bits
}




