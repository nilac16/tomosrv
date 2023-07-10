#pragma once

#ifndef TOMOSRV_ERROR_H
#define TOMOSRV_ERROR_H

#include "defines.h"


enum {
    TOMO_ERROR_NONE, /* Raising this clears the error state. No args are touched */
    TOMO_ERROR_SYS,  /* errno will be fetched if the data is NULL */
    TOMO_ERROR_WIN32,/* GetLastError() will be called if data is NULL */
    TOMO_ERROR_SOCK, /* WSAGetLastError will be called if data is NULL */
    TOMO_ERROR_CSV,  /* You MUST pass a pointer to the libcsv error code */
    TOMO_ERROR_USER  /* Pass a pointer to the message string */
};


/** @brief Raise an error state
 *  @param type
 *      The type of error
 *  @param data
 *      Data to go with @p type
 *  @param ctx
 *      Context format string
 */
void tomo_error_raise(int type, void *data, const wchar_t *ctx, ...);


/** @brief Convenience function */
void tomo_error_reset(void);


/** @brief Get the error messages
 *  @param msg
 *      If not NULL, a pointer to the message string will be placed here
 *  @param ctx
 *      If not NULL, a pointer to the context string will be placed here
 *  @note There is always a context string, but the message string may be empty.
 *      Neither of these results will ever be NULL
 */
void tomo_error_strings(const wchar_t **msg, const wchar_t **ctx);


/** @brief Overwrites the context string. This simulates the behavior of
 *      catching, modifying and rethrowing
 *  @param ctx
 *      Context format string
 */
void tomo_error_set_ctx(const wchar_t *ctx, ...);


/** @brief Gets the current error state
 *  @returns true if an error state is currently raised
 */
bool tomo_error_state(void);


#endif /* TOMOSRV_ERROR_H */
