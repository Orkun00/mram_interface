/**
 * @file mram.h
 * @brief MRAM (Magnetoresistive Random Access Memory) Interface
 *
 * This interface provides functions to interact with an MRAM device via SPI.
 * The implementation supports basic operations such as reading, writing,
 * sleep mode control, and status register access.
 *
 * @note The MRAM device uses a 19-bit addressing scheme for 512KB (4Mbit) of memory
 * 
 * @version 1.0.0
 * @author Orkun Acar
 * @date 16.06.2025
 */

#ifndef MRAM_INTERFACE_MRAM_H
#define MRAM_INTERFACE_MRAM_H

/*******************************************************************************
 * Includes
 ******************************************************************************/
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>


/*******************************************************************************
 * Version Information
 ******************************************************************************/
/** @brief Library major version */
#define MRAM_VERSION_MAJOR 1
/** @brief Library minor version */
#define MRAM_VERSION_MINOR 0
/** @brief Library patch version */
#define MRAM_VERSION_PATCH 0

/*******************************************************************************
 * Constants
 ******************************************************************************/
/** @brief GPIO output level - Low */
#define MRAM_GPIO_LOW 0
/** @brief GPIO output level - High */
#define MRAM_GPIO_HIGH 1

/*******************************************************************************
 * Memory Configuration
 ******************************************************************************/
/** @brief Total size of MRAM in bytes (512KB) */
#define MRAM_SIZE_BYTES (512 * 1024)
/** @brief Maximum valid address (0 to MRAM_SIZE_BYTES - 1) */
#define MRAM_MAX_ADDRESS (MRAM_SIZE_BYTES - 1)
/** @brief Address mask for 19-bit addressing */
#define MRAM_ADDRESS_MASK 0x7FFFF

/*******************************************************************************
 * Timing Parameters
 ******************************************************************************/
/** @brief Recovery time from Deep Power-down (microseconds) */
#define MRAM_TRDP_US 400  // Conservative value to ensure recovery
/** @brief Time to enter Deep Power-down (microseconds) */
#define MRAM_TDP_US 100   // Time to achieve sleep mode current
/** @brief Write cycle time (nanoseconds) */
#define MRAM_TWC_NS 250

/*******************************************************************************
 * Status Register Bit Definitions
 ******************************************************************************/
/** @brief Write Enable Latch bit mask */
#define MRAM_STATUS_WEL     (1 << 1)
/** @brief Block Protection 0 bit mask */
#define MRAM_STATUS_BP0     (1 << 2)
/** @brief Block Protection 1 bit mask */
#define MRAM_STATUS_BP1     (1 << 3)
/** @brief Write Protect Enable bit mask */
#define MRAM_STATUS_WPEN    (1 << 7)

/*******************************************************************************
 * Command Codes
 ******************************************************************************/
/** @brief MRAM Command: Write Enable */
static const uint8_t MRAM_CMD_WREN = 0x06;
/** @brief MRAM Command: Write Disable */
static const uint8_t MRAM_CMD_WRDI = 0x04;
/** @brief MRAM Command: Read Status Register */
static const uint8_t MRAM_CMD_RDSR = 0x05;
/** @brief MRAM Command: Write Status Register */
static const uint8_t MRAM_CMD_WRSR = 0x01;
/** @brief MRAM Command: Read Data */
static const uint8_t MRAM_CMD_READ = 0x03;
/** @brief MRAM Command: Write Data */
static const uint8_t MRAM_CMD_WRITE = 0x02;
/** @brief MRAM Command: Enter Sleep Mode */
static const uint8_t MRAM_CMD_SLEEP = 0xB9;
/** @brief MRAM Command: Exit Sleep Mode */
static const uint8_t MRAM_CMD_WAKE = 0xAB;

/*******************************************************************************
 * Command Reference Table
 ******************************************************************************/
/* Command Set Reference:
 * Command    | Description           | Hex Code | Address Bytes | Data Bytes
 * -----------|---------------------|----------|---------------|------------
 * WREN       | Write Enable         | 06h      | 0            | 0
 * WRDI       | Write Disable        | 04h      | 0            | 0
 * RDSR       | Read Status Register | 05h      | 0            | 1
 * WRSR       | Write Status Register| 01h      | 0            | 1
 * READ       | Read Data Bytes      | 03h      | 3            | 1 to ∞
 * WRITE      | Write Data Bytes     | 02h      | 3            | 1 to ∞
 * SLEEP      | Enter Sleep Mode     | B9h      | 0            | 0
 * WAKE       | Exit Sleep Mode      | ABh      | 0            | 0
 */

/*******************************************************************************
 * Data Structures
 ******************************************************************************/
/**
 * @brief MRAM device interface structure
 *
 * This structure contains function pointers for GPIO and SPI operations
 * that must be implemented by the user for their specific hardware.
 */
struct mram {
    /** @brief GPIO write function pointer 
     * @return true on success, false on failure
     */
    bool (*gpio_write)(uint8_t pin, uint8_t value);
    /** @brief SPI transfer function pointer 
     * @return true on success, false on failure
     */
    bool (*spi_transfer)(const uint8_t* tx_buf, uint8_t* rx_buf, size_t len);
    /** @brief Chip select pin number */
    uint8_t cs_pin;
};

/*******************************************************************************
 * Function Prototypes
 ******************************************************************************/

/**
 * @brief Initialize MRAM device interface
 *
 * @param mram Pointer to MRAM interface structure to initialize
 * @param gpio_write Function pointer to GPIO write implementation
 * @param spi_transfer Function pointer to SPI transfer implementation
 * @param cs_pin Chip select pin number
 * @return true if initialization successful, false if any parameter is NULL
 */
bool mram_init(struct mram* mram, bool (*gpio_write)(uint8_t, uint8_t),
               bool (*spi_transfer)(const uint8_t*, uint8_t*, size_t),
               uint8_t cs_pin);

/**
 * @brief Enable write operations on the MRAM device
 *
 * Sets the Write Enable Latch (WEL) bit in the status register.
 * This must be called before any write operation.
 *
 * @param mram Pointer to the MRAM interface structure
 * @return true if successful, false if mram is NULL or communication fails
 */
bool mram_write_enable(struct mram* mram);

/**
 * @brief Disable write operations on the MRAM device
 *
 * Clears the Write Enable Latch (WEL) bit in the status register.
 * This is automatically called after write operations complete.
 *
 * @param mram Pointer to the MRAM interface structure
 * @return true if successful, false if mram is NULL or communication fails
 */
bool mram_write_disable(struct mram* mram);

/**
 * @brief Read data from the MRAM device
 *
 * @param mram Pointer to the MRAM interface structure
 * @param addr Starting address (19-bit maximum)
 * @param buffer Pointer to buffer where read data will be stored
 * @param len Number of bytes to read
 * @return true if successful, false if:
 *         - mram is NULL
 *         - buffer is NULL
 *         - len is 0
 *         - addr + len exceeds MRAM_MAX_ADDRESS
 *         - communication fails
 *
 * @note The address is masked to 19 bits (512KB address space)
 */
bool mram_read(struct mram* mram, uint32_t addr, uint8_t* buffer, size_t len);

/**
 * @brief Write data to the MRAM device
 *
 * @param mram Pointer to the MRAM interface structure
 * @param addr Starting address (19-bit maximum)
 * @param data Pointer to data to be written
 * @param len Number of bytes to write
 * @return true if successful, false if:
 *         - mram is NULL
 *         - data is NULL
 *         - len is 0
 *         - addr + len exceeds MRAM_MAX_ADDRESS
 *         - write enable fails
 *         - communication fails
 *
 * @note The address is masked to 19 bits (512KB address space)
 * @note This function automatically handles write enable/disable
 */
bool mram_write(struct mram* mram, uint32_t addr, const uint8_t* data, size_t len);

/**
 * @brief Put the MRAM device into sleep mode
 *
 * Sleep mode reduces power consumption. The device must be woken up
 * before any other operations can be performed.
 *
 * @param mram Pointer to the MRAM interface structure
 * @return true if successful, false if mram is NULL or communication fails
 */
bool mram_sleep(struct mram* mram);

/**
 * @brief Wake up the MRAM device from sleep mode
 *
 * @param mram Pointer to the MRAM interface structure
 * @return true if successful, false if mram is NULL or communication fails
 *
 * @note The CS pin must remain high for at least MRAM_TRDP_US microseconds
 * @note This command must be executed after sleep mode entry and prior to any other command
 */
bool mram_wake(struct mram* mram);

/**
 * @brief Read the MRAM status register
 *
 * @param mram Pointer to the MRAM interface structure
 * @param status Pointer to store the status register value
 * @return true if successful, false if mram is NULL, status is NULL, or communication fails
 *
 * Status Register Bits:
 * - Bit 7 (WPEN): Write Protect Enable
 * - Bit 3 (BP1): Block Protect 1
 * - Bit 2 (BP0): Block Protect 0
 * - Bit 1 (WEL): Write Enable Latch
 * - Other bits: Reserved, read as 0
 *
 * @note This command cannot immediately follow a READ command
 * @note If needed after a READ command, either:
 *       1. Execute another command before RDSR
 *       2. Execute two successive RDSR commands (second will be correct)
 */
bool mram_read_status_register(struct mram* mram, uint8_t* status);

/**
 * @brief Write to the MRAM status register
 *
 * @param mram Pointer to the MRAM interface structure
 * @param status Value to write to the status register
 * @return true if successful, false if mram is NULL or communication fails
 *
 * Status Register Bits:
 * - Bit 7 (WPEN): Write Protect Enable
 * - Bit 3 (BP1): Block Protect 1
 * - Bit 2 (BP0): Block Protect 0
 * - Bit 1 (WEL): Write Enable Latch (read-only)
 * - Other bits: Reserved, write as 0
 *
 * @note This command cannot immediately follow a READ command
 * @note If needed after a READ command, either:
 *       1. Execute another command before WRSR
 *       2. Execute two successive WRSR commands (second will work correctly)
 * @note This function automatically handles write enable/disable
 */
bool mram_write_status_register(struct mram* mram, uint8_t status);

/**
 * @brief Check if write operations are currently enabled
 *
 * @param mram Pointer to the MRAM interface structure
 * @return true if write is enabled, false if disabled or on error
 */
bool mram_is_write_enabled(struct mram* mram);

/**
 * @brief Check if write protection is enabled
 *
 * @param mram Pointer to the MRAM interface structure
 * @return true if write protection is enabled, false if disabled or on error
 */
bool mram_is_write_protected(struct mram* mram);

/**
 * @brief Check if a specific block is write protected
 *
 * @param mram Pointer to the MRAM interface structure
 * @param block_number Block number to check (0 or 1)
 * @return true if the specified block is protected, false if not protected or on error
 */
bool mram_is_block_protected(struct mram* mram, uint8_t block_number);

#endif //MRAM_INTERFACE_MRAM_H
