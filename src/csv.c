#include <stdio.h>

#include "csv.h"
#include "log.h"
#include "error.h"

#include <windows.h>
#include <csv.h>


/** @brief Get the size of @p hfile in bytes
 *  @param hfile
 *      Opened file HANDLE
 *  @param len
 *      Length of the file is written here
 *  @returns Nonzero on error
 */
static int tomo_csv_file_size(HANDLE hfile, size_t *len)
{
    static const wchar_t *failmsg = L"Cannot get file size";
    LARGE_INTEGER size;

    if (!GetFileSizeEx(hfile, &size)) {
        tomo_error_raise(TOMO_ERROR_WIN32, NULL, failmsg);
        return 1;
    }
    *len = size.QuadPart;
    return 0;
}


/** @brief Opens a read-only HANDLE to the file at @p path
 *  @param path
 *      Path to file
 *  @returns The opened HANDLE or INVALID_HANDLE_VALUE on failure
 */
static HANDLE tomo_csv_open(const wchar_t *path, size_t *len)
{
    static const wchar_t *failfmt = L"Failed to open %s";
    HANDLE res;

    res = CreateFile(path,
                     GENERIC_READ,
                     FILE_SHARE_READ,
                     NULL,
                     OPEN_EXISTING,
                     FILE_FLAG_NO_BUFFERING,
                     NULL);
    if (res == INVALID_HANDLE_VALUE) {
        tomo_error_raise(TOMO_ERROR_WIN32, NULL, failfmt, path);
    } else if (tomo_csv_file_size(res, len)) {
        CloseHandle(res);
        res = INVALID_HANDLE_VALUE;
    }
    return res;
}


/** @brief Allocates memory for the file
 *  @param len
 *      Size of the required mapping
 *  @returns A pointer to the allocated block, or NULL on failure
 *  @note There is an assumption here that the system page size is larger than
 *      the relevant disk's sector size... Probably not going to be an issue,
 *      and I don't feel like going through the mess of finding the sector size
 */
static void *tomo_csv_alloc(size_t len)
{
    static const wchar_t *failmsg = L"Failed allocating memory for CSV buffer";
    const DWORD prot = PAGE_READWRITE;
    DWORD alloctype = MEM_RESERVE | MEM_COMMIT;
    void *res;

    res = VirtualAlloc(NULL, len, alloctype, prot);
    if (!res) {
        tomo_error_raise(TOMO_ERROR_WIN32, NULL, failmsg);
    }
    return res;
}


/** @brief Buffer the file
 *  @param hfile
 *      File HANDLE
 *  @param dst
 *      Destination buffer
 *  @param len
 *      Size of the file
 *  @returns Nonzero on error
 */
static int tomo_csv_buffer(HANDLE hfile, void *dst, size_t len)
{
    static const wchar_t *failmsg = L"Failed reading CSV file";
    DWORD count, nread;

    do {
        count = (DWORD)len; /* Truncate */
        if (!ReadFile(hfile, dst, count, &nread, NULL)) {
            tomo_error_raise(TOMO_ERROR_WIN32, NULL, failmsg);
            return 1;
        } else if (nread != count) {
            tomo_error_raise(TOMO_ERROR_USER, L"File size changed while reading", failmsg);
            return 1;
        }
        len -= count;
        dst = (char *)dst + count;
    } while (len);
    return 0;
}


/** @brief Buffer the file at @p path onto the heap
 *  @param path
 *      Path to file
 *  @param len
 *      The length of the file will be written here
 */
static void *tomo_csv_read(const wchar_t *path, size_t *len)
{
    HANDLE hfile;
    void *res;

    hfile = tomo_csv_open(path, len);
    if (hfile == INVALID_HANDLE_VALUE) {
        return NULL;
    }
    res = tomo_csv_alloc(*len);
    if (res) {
        if (tomo_csv_buffer(hfile, res, *len)) {
            VirtualFree(res, 0, MEM_RELEASE);
            res = NULL;
        }
    }
    CloseHandle(hfile);
    return res;
}


struct parse_ctx {
    TOMO_MRNTABLE *tbl;
    char name[325]; /* where doin it man */
    unsigned col;
    unsigned name_miss;
    unsigned mrn_miss;
};

#define COL_NAME 23
#define COL_MRN  24


/** @brief Move the tail of @p s up one, overwriting the first character */
static void strpop(char *s)
{
    do {
        s[0] = s[1];
        s++;
    } while (*s);
}


static void tomo_server_replace_commas(char *name)
{
    char *comma;

    comma = strstr(name, ", ");
    while (comma) {
        *comma = '^';
        strpop(comma + 1);
        comma = strstr(comma, ", ");
    }
}


static void tomo_csv_fieldcb(void *field, size_t len, void *data)
{
    struct parse_ctx *ctx = data;
    const char *key = field;

    (void)len;

    switch (ctx->col) {
    case COL_NAME:
        snprintf(ctx->name, BUFLEN(ctx->name), "%s", key);
        tomo_server_replace_commas(ctx->name);
        break;
    case COL_MRN:
        if (!ctx->name[0]) {
            ctx->name_miss++;
        } else if (!key[0]) {
            ctx->mrn_miss++;
        } else {
            if (!tomo_mrntable_insert(ctx->tbl, ctx->name, key)) {
                tomo_logf(TOMO_LOG_DEBUG, L"Inserted %S\\%S", ctx->name, key);
            }
        }
        break;
    }
    ctx->col++;
}


static void tomo_csv_rowcb(int row, void *data)
{
    struct parse_ctx *ctx = data;

    (void)row;

    ctx->col = 0;
    memset(ctx->name, 0, sizeof ctx->name);
}


/** @brief Parse the CSV file buffered in @p data
 *  @param tbl
 *      MRN table
 *  @param data
 *      CSV data
 *  @param len
 *      Length of @p data
 *  @returns Nonzero on error
 */
static int tomo_csv_parse(TOMO_MRNTABLE *tbl, void *data, size_t len)
{
    struct csv_parser csvp = { 0 };
    struct parse_ctx ctx = {
        .tbl = tbl,
        .col = 0
    };
    size_t read;
    int res;

    res = csv_init(&csvp, CSV_APPEND_NULL);
    if (res) {
        res = csv_error(&csvp);
        tomo_error_raise(TOMO_ERROR_CSV, &res, L"Cannot initialize CSV parser");
        return 1;
    }
    read = csv_parse(&csvp, data, len, tomo_csv_fieldcb, tomo_csv_rowcb, &ctx);
    res = read < len
       || csv_fini(&csvp, tomo_csv_fieldcb, tomo_csv_rowcb, &ctx);
    csv_free(&csvp);
    if (res) {
        res = csv_error(&csvp);
        tomo_error_raise(TOMO_ERROR_CSV, &res, L"Failed parsing CSV");
    } else if (ctx.mrn_miss) {
        tomo_logf(TOMO_LOG_WARN, L"CSV: %u patients missing IDs", ctx.mrn_miss);
    }
    return tomo_error_state();
}


int tomo_csv_load(TOMO_MRNTABLE *tbl, const wchar_t *path)
{
    size_t len;
    void *data;
    int res = 1;

    if (tomo_mrntable_init(tbl, 256)) {
        return 1;
    }
    data = tomo_csv_read(path, &len);
    if (data) {
        res = tomo_csv_parse(tbl, data, len);
        VirtualFree(data, 0, MEM_RELEASE);
    }
    return res;
}
