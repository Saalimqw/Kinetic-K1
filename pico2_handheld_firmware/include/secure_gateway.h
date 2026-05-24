/**
 * @file secure_gateway.h
 * @brief TrustZone-M Secure Gateway Interface for RP2350
 * 
 * This header defines the Non-Secure Callable (NSC) entry points
 * that allow the Non-Secure world (main application) to safely
 * invoke secure functions for sensitive operations like API token access.
 * 
 * Memory Safety: All functions use cmse_nonsecure_entry attribute
 * to create a secure gateway that validates parameters and prevents
 * secure memory leakage.
 */

#ifndef SECURE_GATEWAY_H
#define SECURE_GATEWAY_H

#include <cstdint>
#include <cstddef>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Securely retrieve the OpenRouter API Bearer Token
 * 
 * This function runs in the Secure world and provides controlled
 * access to the hardcoded master API token. It performs:
 * 1. Address range validation on the target buffer
 * 2. Safe memory copy from secure to non-secure space
 * 3. Boundary checks to prevent buffer overflows
 * 
 * @param target_buffer Pointer to non-secure buffer for token storage
 * @param max_len Maximum length of the target buffer
 * @return int 0 on success, negative error code on failure
 * 
 * @note This is a Non-Secure Callable (NSC) entry point
 */
__attribute__((cmse_nonsecure_entry))
int sec_get_auth_header(char* target_buffer, size_t max_len);

/**
 * @brief Initialize the secure gateway subsystem
 * 
 * Sets up secure memory regions and initializes the token storage
 * in protected memory. Must be called before any NSC functions.
 * 
 * @return int 0 on success, negative error code on failure
 */
__attribute__((cmse_nonsecure_entry))
int sec_gateway_init(void);

#ifdef __cplusplus
}
#endif

#endif // SECURE_GATEWAY_H
