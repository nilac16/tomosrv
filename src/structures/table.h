#pragma once

#ifndef TOMOSRV_TABLE_H
#define TOMOSRV_TABLE_H

#include "../defines.h"


typedef struct tomo_mrnlist {
    struct tomo_mrnlist *next;

    char mrn[16];   /* Truncation of this field will result in an app ERROR */
} TOMO_MRNLIST;


/** @brief Print @p ls to @p buf
 *  @param buf
 *      Buffer
 *  @param len
 *      Buffer count
 *  @param ls
 *      MRN list
 *  @returns Nonzero on error (truncation)
 */
int tomo_mrnlist_sprint(char *buf, size_t len, const TOMO_MRNLIST *ls);


typedef struct tomo_mrnpair {
    char         *key;
    TOMO_MRNLIST *val;
} TOMO_MRNPAIR;


/** Insert-only hash table for strings keyed to strings. Strings are stored
 *  in heap buffers
 */
typedef struct tomo_mrntable {
    unsigned len, load;
    TOMO_MRNPAIR *table;
} TOMO_MRNTABLE;


/** @brief Initialize @p tbl to at least @p minsize capacity. This function is
 *      safe to call on an initialized table: It will free the table and
 *      reinitialize it
 *  @param tbl
 *      Hash table
 *  @param minsize
 *      Minimum initial size. This is rounded up to the first power of two not
 *      less than its value. If it is a power of two, it is unchanged
 *  @returns Nonzero on error
 */
int tomo_mrntable_init(TOMO_MRNTABLE *tbl, unsigned minsize);


/** @brief Insert @p key, @p val into @p tbl
 *  @param tbl
 *      Hash table
 *  @param key
 *      Key string. This is copied into a heap buffer
 *  @param val
 *      Value string. This is also copied into a heap buffer
 *  @returns Negative on failure, 0 on success, and positive if @p key is
 *      already in the table. If @p key already exists, it is *not* overwritten
 */
int tomo_mrntable_insert(TOMO_MRNTABLE *tbl, const char *key, const char *val);


/** @brief Look up @p key in @p tbl
 *  @param tbl
 *      MRN table
 *  @param key
 *      Key to look up
 *  @returns A pointer to the value list associated with @p key, or NULL if
 *      not found
 */
const TOMO_MRNPAIR *tomo_mrntable_lookup(TOMO_MRNTABLE *tbl, const char *key);


/** @brief Frees memory held by @p tbl
 *  @param tbl
 *      MRN table. The memory for this object is externally managed, but its
 *      contents will be cleared by this call
 */
void tomo_mrntable_free(TOMO_MRNTABLE *tbl);


#endif /* TOMOSRV_TABLE_H */
