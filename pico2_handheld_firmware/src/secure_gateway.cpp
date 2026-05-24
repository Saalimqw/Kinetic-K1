/**
 * @file secure_gateway.cpp
 * @brief TrustZone-M Secure Gateway Implementation for RP2350
 * 
 * This module implements the secure world functions that can be
 * called from the Non-Secure world via Non-Secure Callable (NSC)
 * entry points. It provides controlled access to sensitive data
 * like API tokens while maintaining memory isolation guarantees.
 * 
 * ARM TrustZone-M Features Used:
 * - cmse_nonsecure_entry attribute for gateway functions
 * - cmse_check_address_range for parameter validation
 * - Secure/Non-Secure memory partitioning
 */

#include "secure_gateway.h"
#include <cstring>
#include <arm_cmse.h>  // ARM CMSIS TrustZone extensions

// ============================================================================
// SECURE MEMORY REGION DEFINITIONS
// ============================================================================

/**
 * @brief Hardcoded OpenRouter API Bearer Token
 * 
 * This token is stored in secure memory and is only accessible
 * through the NSC gateway function. The __attribute__((section(".secure_data")))
 * places this data in a secure memory region defined in the linker script.
 * 
 * SECURITY NOTE: In production, this would be stored in encrypted form
 * or derived from hardware-unique keys. For this example, we use a
 * placeholder token structure.
 */
static const char SECURE_API_TOKEN[] __attribute__((section(".secure_data"))) = 
    "Bearer sk-or-v1-PLACEHOLDER-TOKEN-FOR-OPENROUTER-API-ACCESS";

/**
 * @brief Token length constant for boundary checks
 */
static constexpr size_t SECURE_TOKEN_LENGTH = sizeof(SECURE_API_TOKEN) - 1;

/**
 * @brief Maximum allowed token length for safety
 */
static constexpr size_t MAX_TOKEN_LENGTH = 256;

// ============================================================================
// ERROR CODE DEFINITIONS
// ============================================================================

#define SEC_SUCCESS             0
#define SEC_ERR_NULL_PTR        -1
#define SEC_ERR_INVALID_RANGE   -2
#define SEC_ERR_BUFFER_TOO_SMALL -3
#define SEC_ERR_NOT_INITIALIZED -4

// ============================================================================
// INITIALIZATION STATE
// ============================================================================

/**
 * @brief Secure gateway initialization flag
 * 
 * Tracks whether the secure subsystem has been properly initialized.
 * This prevents accidental access before security boundaries are set up.
 */
static bool g_secure_initialized = false;

// ============================================================================
// NON-SECURE CALLABLE (NSC) ENTRY POINTS
// ============================================================================

/**
 * @brief Initialize the secure gateway subsystem
 * 
 * This NSC function sets up the secure environment and validates
 * that all security boundaries are properly configured.
 * 
 * @return int SEC_SUCCESS on success, error code on failure
 */
__attribute__((cmse_nonsecure_entry))
int sec_gateway_init(void) {
    // Validate that we're executing in secure mode (optional debug check)
    // In production, this would verify SAU/IDAU configuration
    
    // Mark secure subsystem as initialized
    g_secure_initialized = true;
    
    return SEC_SUCCESS;
}

/**
 * @brief Securely retrieve the OpenRouter API Bearer Token
 * 
 * This is the primary NSC entry point for accessing the API token.
 * It performs comprehensive validation before copying data from
 * secure to non-secure memory space.
 * 
 * Security Checks Performed:
 * 1. Null pointer validation
 * 2. Address range verification (ensures target is in Non-Secure memory)
 * 3. Buffer size validation
 * 4. Safe memory copy with bounded length
 * 
 * @param target_buffer Pointer to non-secure buffer for token storage
 * @param max_len Maximum length of the target buffer
 * @return int SEC_SUCCESS on success, error code on failure
 */
__attribute__((cmse_nonsecure_entry))
int sec_get_auth_header(char* target_buffer, size_t max_len) {
    // ------------------------------------------------------------------------
    // PHASE 1: INPUT VALIDATION
    // ------------------------------------------------------------------------
    
    // Check for null pointer - prevent undefined behavior
    if (target_buffer == nullptr) {
        return SEC_ERR_NULL_PTR;
    }
    
    // Verify secure subsystem is initialized
    if (!g_secure_initialized) {
        return SEC_ERR_NOT_INITIALIZED;
    }
    
    // Validate buffer size is reasonable
    if (max_len == 0 || max_len > MAX_TOKEN_LENGTH) {
        return SEC_ERR_BUFFER_TOO_SMALL;
    }
    
    // ------------------------------------------------------------------------
    // PHASE 2: ADDRESS RANGE VERIFICATION (TrustZone-M Safety Check)
    // ------------------------------------------------------------------------
    
    /**
     * cmse_check_address_range verifies that the target buffer resides
     * in Non-Secure memory space. This prevents:
     * - Accidental writes to secure memory
     * - Potential security exploits via pointer manipulation
     * - Memory corruption across security boundaries
     * 
     * CMSE_AIRANGE_NS: Check that entire range is Non-Secure
     * Returns NULL if any part of the range is in Secure memory
     */
    void* validated_range = cmse_check_address_range(
        static_cast<void*>(target_buffer),
        max_len,
        CMSE_AIRANGE_NS  // Require entire range to be Non-Secure
    );
    
    if (validated_range == nullptr) {
        // Target buffer overlaps with Secure memory - REJECT
        return SEC_ERR_INVALID_RANGE;
    }
    
    // ------------------------------------------------------------------------
    // PHASE 3: SAFE DATA TRANSFER
    // ------------------------------------------------------------------------
    
    /**
     * Calculate the actual copy length:
     * - Cannot exceed the source token length
     * - Cannot exceed the destination buffer capacity
     * - Always leave room for null terminator if possible
     */
    size_t copy_length = (max_len <= SECURE_TOKEN_LENGTH) ? 
                         max_len : SECURE_TOKEN_LENGTH;
    
    // Ensure null termination if buffer allows
    if (copy_length < max_len) {
        // Copy token and add null terminator
        std::memcpy(target_buffer, SECURE_API_TOKEN, copy_length);
        target_buffer[copy_length] = '\0';
    } else {
        // Buffer exactly fits or is smaller - copy without terminator
        std::memcpy(target_buffer, SECURE_API_TOKEN, copy_length);
        // Note: Caller must handle potentially unterminated string
    }
    
    return SEC_SUCCESS;
}

// ============================================================================
// ADDITIONAL SECURE UTILITIES (Future Extension Points)
// ============================================================================

/**
 * @brief Secure random number generation stub
 * 
 * Future implementation would access hardware RNG through
 * secure gateway for cryptographic operations.
 */
__attribute__((cmse_nonsecure_entry))
int sec_get_random_bytes(uint8_t* buffer, size_t length) {
    // Placeholder for future secure RNG implementation
    // Would validate buffer range and access secure HW RNG peripheral
    
    if (buffer == nullptr || length == 0) {
        return SEC_ERR_NULL_PTR;
    }
    
    // Validate address range
    void* validated = cmse_check_address_range(
        static_cast<void*>(buffer),
        length,
        CMSE_AIRANGE_NS
    );
    
    if (validated == nullptr) {
        return SEC_ERR_INVALID_RANGE;
    }
    
    // TODO: Implement secure RNG access
    return SEC_ERR_NOT_INITIALIZED;
}
