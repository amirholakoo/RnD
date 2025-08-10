/* quirc -- QR‑code recognition library, PSRAM‑enabled version
 * Based on the original implementation by Daniel Beer <dlbeer@gmail.com>
 * Modified for ESP‑IDF so that large runtime buffers (image, pixels,
 * flood‑fill workspace) are placed in external PSRAM when available.
 */

#include <stdlib.h>
#include <string.h>
#include "quirc_internal.h"
#include "esp_heap_caps.h"   /* PSRAM‑aware allocator */

/* -------------------------------------------------------------------------- */
/* Utility wrappers: prefer PSRAM but fall back to normal heap on failure.    */
/* -------------------------------------------------------------------------- */

static void *psram_malloc(size_t size)
{
    void *p = heap_caps_malloc(size, MALLOC_CAP_SPIRAM);
    if (!p) /* PSRAM exhausted or unavailable */
        p = malloc(size);
    return p;
}

static void *psram_calloc(size_t nmemb, size_t size)
{
    void *p = heap_caps_calloc(nmemb, size, MALLOC_CAP_SPIRAM);
    if (!p)
        p = calloc(nmemb, size);
    return p;
}

/* -------------------------------------------------------------------------- */
/* Public API                                                                  */
/* -------------------------------------------------------------------------- */

const char *quirc_version(void)
{
    return "1.0-psram";
}

struct quirc *quirc_new(void)
{
    struct quirc *q = malloc(sizeof(*q)); /* struct is small; DRAM is fine */

    if (!q)
        return NULL;

    memset(q, 0, sizeof(*q));
    return q;
}

void quirc_destroy(struct quirc *q)
{
    free(q->image);
    if (!QUIRC_PIXEL_ALIAS_IMAGE)
        free(q->pixels);
    free(q->flood_fill_vars);
    free(q);
}

/* -------------------------------------------------------------------------- */
/* Resize/allocate image buffers                                               */
/* -------------------------------------------------------------------------- */

int quirc_resize(struct quirc *q, int w, int h)
{
    uint8_t *image  = NULL;
    quirc_pixel_t *pixels = NULL;
    struct quirc_flood_fill_vars *vars = NULL;
    size_t num_vars;
    size_t vars_byte_size;

    if (w < 0 || h < 0)
        goto fail;

    /* Allocate new image buffer (w * h bytes, zero‑initialised) in PSRAM */
    image = psram_calloc((size_t)w * h, 1);
    if (!image)
        goto fail;

    /* Copy old image (if any) into new buffer (trim or pad as needed) */
    size_t olddim = (size_t)q->w * q->h;
    size_t newdim = (size_t)w * h;
    size_t copy_bytes = olddim < newdim ? olddim : newdim;
    memcpy(image, q->image, copy_bytes);

    /* Allocate pixel buffer when not aliased to image */
    if (!QUIRC_PIXEL_ALIAS_IMAGE) {
        pixels = psram_calloc(newdim, sizeof(quirc_pixel_t));
        if (!pixels)
            goto fail;
    }

    /* Flood‑fill workspace: about 2/3 of the image height */
    num_vars = (size_t)h * 2 / 3;
    if (num_vars == 0)
        num_vars = 1;
    vars_byte_size = sizeof(*vars) * num_vars;

    vars = psram_malloc(vars_byte_size);
    if (!vars)
        goto fail;

    /* ---- Commit new buffers ---- */
    q->w = w;
    q->h = h;

    free(q->image);
    q->image = image;

    if (!QUIRC_PIXEL_ALIAS_IMAGE) {
        free(q->pixels);
        q->pixels = pixels;
    }

    free(q->flood_fill_vars);
    q->flood_fill_vars = vars;
    q->num_flood_fill_vars = num_vars;

    return 0;

fail:
    free(image);
    free(pixels);
    free(vars);
    return -1;
}

/* -------------------------------------------------------------------------- */
/* Existing API functions stay unchanged                                       */
/* -------------------------------------------------------------------------- */

int quirc_count(const struct quirc *q)
{
    return q->num_grids;
}

static const char *const error_table[] = {
    [QUIRC_SUCCESS] = "Success",
    [QUIRC_ERROR_INVALID_GRID_SIZE] = "Invalid grid size",
    [QUIRC_ERROR_INVALID_VERSION] = "Invalid version",
    [QUIRC_ERROR_FORMAT_ECC] = "Format data ECC failure",
    [QUIRC_ERROR_DATA_ECC] = "ECC failure",
    [QUIRC_ERROR_UNKNOWN_DATA_TYPE] = "Unknown data type",
    [QUIRC_ERROR_DATA_OVERFLOW] = "Data overflow",
    [QUIRC_ERROR_DATA_UNDERFLOW] = "Data underflow"
};

const char *quirc_strerror(quirc_decode_error_t err)
{
    if (err >= 0 && err < (int)(sizeof(error_table) / sizeof(error_table[0])))
        return error_table[err];
    return "Unknown error";
}
