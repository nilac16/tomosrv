#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "table.h"
#include "../error.h"
#include "../log.h"

#include <intrin.h>

/** Just let yourselves support POSIX guys, we know you don't give a fuck about
 *  ISO C anyway */
#define strdup(s) _strdup(s)


int tomo_mrnlist_sprint(char *buf, size_t len, const TOMO_MRNLIST *ls)
{
    static const wchar_t *failmsg = L"Cannot copy MRN string";
    static const char *delim[2] = { "", "\n" };
    const TOMO_MRNLIST *node;
    int count, idx = 0;
    
    for (node = ls; node; node = node->next) {
        count = snprintf(buf, len, "%s%s", delim[idx], ls->mrn);
        if (count < 0) {
            tomo_error_raise(TOMO_ERROR_SYS, NULL, failmsg);
            return 1;
        } else if (count >= len) {
            tomo_error_raise(TOMO_ERROR_USER, L"Truncation occurred", failmsg);
            return 1;
        }
        buf += count;
        len -= count;
        idx |= 1;
    }
    return 0;
}


/** @brief Extremely basic universal string hash function */
static unsigned tomohash(const char *key)
{
    const unsigned prime = 17;
    unsigned pow = 1, res = 0;

    for (; *key; key++) {
        res += pow * *key;
        pow *= prime;
    }
    return res;
}


/** @brief Modulo with the table size, made trivial by the invariant that
 *      tbl->len % 2 == 0
 */
static unsigned tomomod(const TOMO_MRNTABLE *tbl, unsigned hash)
{
    return hash & (tbl->len - 1);
}


/** @brief Rounds @p x up to the next highest power of two, using ancient two's
 *      complement bit-twiddling techniques. I fetched/adapted this code from
 *      one of the bajillion answers of its kind on good old Stack Overflow
 *  @param x
 *      Value to round up
 *  @returns The first power of two not less than @p x (could be equal to x!)
 */
static unsigned next_pow2(unsigned x)
{
    const size_t width = sizeof x;

    x--;
    switch (width) {
    case 8:
        x |= x >> 32;
    case 4:
        x |= x >> 16;
    case 2:
        x |= x >> 8;
    case 1:
        x |= x >> 4;
        x |= x >> 2;
        x |= x >> 1;
    }
    return x + 1;
}


/** @brief Find @p key in @p tbl
 *  @param tbl
 *      Table
 *  @param key
 *      Key string
 *  @returns A pointer to the containing bucket, or the first empty bucket where
 *      it should be placed
 */
static TOMO_MRNPAIR *tomo_mrntable_find(TOMO_MRNTABLE *tbl,
                                        const char    *key)
{
    unsigned hash;

    hash = tomomod(tbl, tomohash(key));
    while (tbl->table[hash].key) {
        if (!strcmp(tbl->table[hash].key, key)) {
            break;
        }
        hash = tomomod(tbl, hash + 1);
    }
    return &tbl->table[hash];
}


static int tomo_strcpy(char *dst, size_t len, const char *str)
{
    static const wchar_t *failmsg = L"Cannot copy string";
    int count;

    count = snprintf(dst, len, "%s", str);
    if (count < 0) {
        tomo_error_raise(TOMO_ERROR_SYS, NULL, failmsg);
        return 1;
    } else if ((size_t)count >= len) {
        tomo_error_raise(TOMO_ERROR_USER, L"Buffer overrun", failmsg);
        return 1;
    }
    return 0;
}


/** @brief Insert @p val into the MRN list at @p head
 *  @param head
 *      Pointer to head pointer
 *  @param val
 *      Value string to insert
 *  @returns Negative on error, zero on success, and positive if @p val was
 *      already in the list
 */
static int tomo_mrnlist_insert(TOMO_MRNLIST **head, const char *val)
{
    static const wchar_t *buffail = L"Failed copying MRN string";

    while (*head) {
        if (!strcmp((*head)->mrn, val)) {
            return 1;
        }
        head = &(*head)->next;
    }
    *head = calloc(1UL, sizeof **head);
    if (!*head) {
        tomo_error_raise(TOMO_ERROR_SYS, NULL, L"Failed allocating MRN list node");
        return -1;
    }
    if (tomo_strcpy((*head)->mrn, BUFLEN((*head)->mrn), val)) {
        tomo_error_set_ctx(buffail);
        return -1;
    }
    return 0;
}


/** @brief Inserts @p key, @p val into the list at @p pair
 *  @param pair
 *      Hash bucket
 *  @param key
 *      Key string
 *  @param val
 *      Value string
 *  @returns Negative on error, zero on success, and one if @p val was already
 *      in the list
 */
static int tomo_mrnpair_insert(TOMO_MRNPAIR *pair,
                               const char   *key,
                               const char   *val)
{
    if (!pair->key) {
        pair->key = strdup(key);
        if (!pair->key) {
            tomo_error_raise(TOMO_ERROR_SYS, NULL, L"Failed allocating MRN list key");
            return -1;
        }
    }
    return tomo_mrnlist_insert(&pair->val, val);
}


/** @brief Checks the load factor ratio to the table capacity
 *  @param tbl
 *      Hash table
 *  @returns true if the table should be reallocated
 */
static bool tomo_mrntable_overload(const TOMO_MRNTABLE *tbl)
{
    /* The load limit is this proportion of table capacity */
    const unsigned num = 2, denom = 3;
    const unsigned lim = (num * tbl->len) / denom;

    return tbl->load >= lim;
}


/** @brief Change the capacity of @p tbl
 *  @param tbl
 *      Hash table
 *  @param newlen
 *      New length of the hash table. This should be larger than the current
 *      length, and a power of two
 *  @returns Nonzero on error
 *  @warning This function is not designed to reduce the capacity, and you will
 *      very likely end up leaking string buffers if you attempt to do so!
 */
static int tomo_mrntable_realloc(TOMO_MRNTABLE *tbl, unsigned newlen)
{
    TOMO_MRNTABLE next = {
        .len = newlen,
        .load = tbl->load
    };
    TOMO_MRNPAIR *pair;
    unsigned i;

    assert(newlen > tbl->len);
    assert(__popcnt(newlen) == 1);
    next.table = calloc(next.len, sizeof *next.table);
    if (!next.table) {
        tomo_error_raise(TOMO_ERROR_SYS, NULL, L"Failed reallocating MRN table");
        return 1;
    }
    /* Move the KV pairs. Insert should not be used here because it copies data */
    for (i = 0; i < tbl->len; i++) {
        if (tbl->table[i].key) {
            pair = tomo_mrntable_find(&next, tbl->table[i].key);
            assert(!pair->key);
            *pair = tbl->table[i];
        }
    }
    free(tbl->table);
    *tbl = next;
    return 0;
}


int tomo_mrntable_init(TOMO_MRNTABLE *tbl, unsigned minsize)
{
    unsigned len;

    len = next_pow2(minsize);
    tomo_mrntable_free(tbl);
    return tomo_mrntable_realloc(tbl, len);
}


int tomo_mrntable_insert(TOMO_MRNTABLE *tbl, const char *key, const char *val)
{
    TOMO_MRNPAIR *pair;
    int res;

    pair = tomo_mrntable_find(tbl, key);
    res = tomo_mrnpair_insert(pair, key, val);
    if (res < 0) {
        return 1;
    } else if (!res) {
        tbl->load++;
        if (tomo_mrntable_overload(tbl)) {
            if (tomo_mrntable_realloc(tbl, tbl->len * 2)) {
                return -1;
            }
        }
        return 0;
    } else {
        return 1;
    }
}


const TOMO_MRNPAIR *tomo_mrntable_lookup(TOMO_MRNTABLE *tbl, const char *key)
{
    TOMO_MRNPAIR *pair;

    pair = tomo_mrntable_find(tbl, key);
    return (pair->key) ? pair : NULL;
}


static void tomo_mrnlist_free(TOMO_MRNLIST *ls)
{
    TOMO_MRNLIST *next;

    for (; ls; ls = next) {
        next = ls->next;
        free(ls);
    }
}


void tomo_mrntable_free(TOMO_MRNTABLE *tbl)
{
    static const TOMO_MRNTABLE zero = { 0 };
    unsigned i;

    for (i = 0; i < tbl->len; i++) {
        if (tbl->table[i].key) {
            free(tbl->table[i].key);
            tomo_mrnlist_free(tbl->table[i].val);
        }
    }
    free(tbl->table);
    *tbl = zero;
}
