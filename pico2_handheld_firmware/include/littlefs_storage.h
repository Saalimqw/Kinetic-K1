/**
 * @file littlefs_storage.h
 * @brief LittleFS File System Storage Shim for RP2350 QSPI Flash
 * 
 * This header defines the memory layout, configuration structures,
 * and initialization routines required to mount LittleFS over a
 * custom secondary partition on the external QSPI Flash storage.
 * 
 * Key Features:
 * - Wear leveling through block-level abstraction
 * - Power-loss resilience via atomic operations
 * - Static memory allocation (no heap usage)
 * - Integration with Pico SDK hardware_flash layer
 * 
 * Flash Memory Layout (4MB Total):
 * +------------------+ 0x00000000
 * | Bootloader       | 16KB
 * +------------------+ 0x00004000
 * | Firmware (App)   | ~300KB (target limit)
 * +------------------+ 0x00050000
 * | Reserved         | 48KB
 * +------------------+ 0x0005C000
 * | LittleFS Data    | 2MB (configurable)
 * +------------------+ 0x0025C000
 * | Configuration    | 64KB
 * +------------------+ 0x0026C000
 * | Unused/Reserved  | Remainder
 * +------------------+ 0x00400000 (4MB)
 */

#ifndef LITTLEFS_STORAGE_H
#define LITTLEFS_STORAGE_H

#include <cstdint>
#include <cstddef>

// Forward declaration from littlefs.h (when library is included)
struct lfs;
struct lfs_config;

// ============================================================================
// FLASH MEMORY PARTITION DEFINITIONS
// ============================================================================

/**
 * @brief Base address of QSPI flash in memory-mapped region
 * 
 * On RP2350, external flash is mapped starting at this address
 * when accessed through XIP (Execute-In-Place) interface.
 */
static constexpr uint32_t XIP_BASE = 0x10000000;

/**
 * @brief Flash sector size (erase block size)
 * 
 * Most QSPI flash chips use 4KB sectors. This is the minimum
 * erasable unit - all writes must be aligned to this boundary.
 */
static constexpr uint32_t FLASH_SECTOR_SIZE = 4096;

/**
 * @brief Flash page size (program unit size)
 * 
 * Minimum writable unit within a sector. Multiple page writes
 * can be performed before erasing the sector.
 */
static constexpr uint32_t FLASH_PAGE_SIZE = 256;

// ============================================================================
// LITTLEFS PARTITION CONFIGURATION
// ============================================================================

/**
 * @brief LittleFS partition offset from flash base
 * 
 * Starting address of the LittleFS filesystem within the flash chip.
 * Positioned after bootloader, firmware, and reserved regions.
 */
static constexpr uint32_t LITTLEFS_PARTITION_OFFSET = 0x0005C000;  // 368KB offset

/**
 * @brief LittleFS partition size
 * 
 * Total space allocated for the filesystem. Larger sizes improve
 * wear leveling but reduce space for other partitions.
 */
static constexpr uint32_t LITTLEFS_PARTITION_SIZE = 0x00200000;  // 2MB

/**
 * @brief Number of blocks in LittleFS partition
 * 
 * Calculated as: partition_size / sector_size
 * Must match LFS_BLOCK_COUNT in CMakeLists.txt
 */
static constexpr uint32_t LITTLEFS_BLOCK_COUNT = LITTLEFS_PARTITION_SIZE / FLASH_SECTOR_SIZE;  // 512 blocks

/**
 * @brief Configuration block for LittleFS partition metadata
 * 
 * Stored at the end of flash, contains:
 * - Filesystem parameters
 * - Wear leveling statistics
 * - Bad block table
 * - Version information
 */
static constexpr uint32_t CONFIG_PARTITION_OFFSET = 0x0025C000;
static constexpr uint32_t CONFIG_PARTITION_SIZE = 0x00010000;  // 64KB

// ============================================================================
// LITTLEFS CONFIGURATION STRUCTURE
// ============================================================================

/**
 * @brief LittleFS configuration parameters
 * 
 * These values are tuned for QSPI flash characteristics:
 * - Block size matches flash sector size (4KB)
 * - Read/program sizes match flash capabilities
 * - Cache sizes balance performance vs RAM usage
 * 
 * All values are compile-time constants to enable static allocation.
 */
struct LittleFSConfig {
    // Block device parameters (must match flash characteristics)
    static constexpr uint32_t BLOCK_SIZE = FLASH_SECTOR_SIZE;      // 4096 bytes
    static constexpr uint32_t BLOCK_COUNT = LITTLEFS_BLOCK_COUNT;  // 512 blocks
    
    // Performance tuning parameters
    static constexpr uint32_t READ_SIZE = 256;      // Minimum read granularity
    static constexpr uint32_t PROG_SIZE = 256;      // Minimum program granularity
    static constexpr uint32_t CACHE_SIZE = 1024;    // Read/write cache (RAM)
    static constexpr uint32_t LOOKAHEAD_SIZE = 64;  // Free block lookahead
    
    // Filesystem limits
    static constexpr uint32_t NAME_MAX = 255;       // Maximum filename length
    static constexpr uint32_t FILE_MAX = 0x7FFFFFFF;// Maximum file size (2GB)
    static constexpr uint32_t ATTR_MAX = 1024;      // Maximum attribute size
    
    // Wear leveling parameters
    static constexpr uint32_t BLOCK_CYCLES = 10000; // Expected erase cycles before wear
    static constexpr uint32_t ERASE_VALUE = 0xFF;   // Erased flash state
    
    // Power-loss resilience
    static constexpr bool COMPACT_THRESH_ENABLED = true;
    static constexpr uint32_t COMPACT_THRESH = 0x40;  // 64% utilization trigger
};

// ============================================================================
// STATIC MEMORY BUFFERS FOR LITTLEFS
// ============================================================================

/**
 * @brief Statically allocated LittleFS filesystem structure
 * 
 * Placed in .bss section to avoid stack overflow.
 * Size determined by LittleFS internal requirements.
 */
extern "C" {
    // Forward declare lfs_t - actual definition requires littlefs.h
    struct lfs;
}

/**
 * @brief Global LittleFS instance
 * 
 * Single filesystem instance for the entire device.
 * Allocated statically to guarantee memory availability.
 */
extern struct lfs g_littlefs_fs;

/**
 * @brief Statically allocated read cache buffer
 * 
 * Used by LittleFS for buffering flash reads.
 * Size: LFS_CACHE_SIZE bytes (defined in CMakeLists.txt)
 */
alignas(4) extern uint8_t g_littlefs_read_cache[LFS_CACHE_SIZE];

/**
 * @brief Statically allocated program cache buffer
 * 
 * Used by LittleFS for buffering flash writes before commit.
 * Size: LFS_PROG_SIZE bytes (matches flash page size)
 */
alignas(4) extern uint8_t g_littlefs_prog_cache[LFS_PROG_SIZE];

/**
 * @brief Statically allocated lookahead buffer
 * 
 * Bitfield tracking free blocks for wear leveling.
 * Size: LFS_LOOKAHEAD_SIZE / 8 bytes (one bit per block)
 */
alignas(4) extern uint8_t g_littlefs_lookahead[LFS_LOOKAHEAD_SIZE / 8];

// ============================================================================
// FLASH HARDWARE ABSTRACTION LAYER
// ============================================================================

/**
 * @brief Flash read callback for LittleFS
 * 
 * Reads data from QSPI flash at the specified offset.
 * Called by LittleFS through the lfs_config structure.
 * 
 * @param config Pointer to LittleFS config structure
 * @param block Block number to read from
 * @param off Offset within the block
 * @param buffer Destination buffer for read data
 * @param size Number of bytes to read
 * @return int 0 on success, negative error code on failure
 */
int littlefs_flash_read(const struct lfs_config* config,
                        lfs_block_t block,
                        lfs_off_t off,
                        void* buffer,
                        lfs_size_t size);

/**
 * @brief Flash program callback for LittleFS
 * 
 * Writes data to QSPI flash at the specified offset.
 * Must be called after sector erase for the target block.
 * 
 * @param config Pointer to LittleFS config structure
 * @param block Block number to program
 * @param off Offset within the block
 * @param buffer Source buffer containing data to write
 * @param size Number of bytes to program
 * @return int 0 on success, negative error code on failure
 */
int littlefs_flash_prog(const struct lfs_config* config,
                        lfs_block_t block,
                        lfs_off_t off,
                        const void* buffer,
                        lfs_size_t size);

/**
 * @brief Flash erase callback for LittleFS
 * 
 * Erases an entire block (sector) in QSPI flash.
 * This is a slow operation (~50ms per sector) and should
 * be minimized through wear leveling.
 * 
 * @param config Pointer to LittleFS config structure
 * @param block Block number to erase
 * @return int 0 on success, negative error code on failure
 */
int littlefs_flash_erase(const struct lfs_config* config,
                         lfs_block_t block);

/**
 * @brief Flash sync callback for LittleFS
 * 
 * Ensures all pending writes are committed to flash.
 * On QSPI flash, this typically involves waiting for
 * the write-in-progress bit to clear.
 * 
 * @param config Pointer to LittleFS config structure
 * @return int 0 on success, negative error code on failure
 */
int littlefs_flash_sync(const struct lfs_config* config);

// ============================================================================
// INITIALIZATION AND MOUNTING
// ============================================================================

/**
 * @brief Initialize LittleFS filesystem
 * 
 * Sets up the lfs_config structure with flash callbacks
 * and mounts the filesystem. Must be called before any
 * file operations.
 * 
 * @return int 0 on success, negative error code on failure
 * 
 * Error Codes:
 * - LFS_ERR_IO: Flash hardware error
 * - LFS_ERR_CORRUPT: Filesystem corruption detected
 * - LFS_ERR_NOENT: Partition not found
 */
int littlefs_mount();

/**
 * @brief Unmount LittleFS filesystem
 * 
 * Safely closes the filesystem and flushes any pending writes.
 * Should be called before system sleep or reset.
 * 
 * @return int 0 on success, negative error code on failure
 */
int littlefs_unmount();

/**
 * @brief Format LittleFS partition
 * 
 * Creates a new filesystem on the partition. This erases
 * ALL data in the LittleFS region - use with caution.
 * 
 * Typical use cases:
 * - First-time initialization
 * - Recovery from corruption
 * - Factory reset
 * 
 * @return int 0 on success, negative error code on failure
 */
int littlefs_format();

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

/**
 * @brief Get filesystem statistics
 * 
 * Retrieves information about filesystem usage including:
 * - Total blocks
 * - Used blocks
 * - Free blocks
 * - Bad blocks
 * 
 * @param total_blocks Output: total number of blocks
 * @param used_blocks Output: number of blocks in use
 * @param free_blocks Output: number of free blocks
 * @return int 0 on success, negative error code on failure
 */
int littlefs_get_stats(uint32_t* total_blocks,
                       uint32_t* used_blocks,
                       uint32_t* free_blocks);

/**
 * @brief Check if filesystem is mounted
 * 
 * @return bool true if mounted, false otherwise
 */
bool littlefs_is_mounted();

#endif // LITTLEFS_STORAGE_H
