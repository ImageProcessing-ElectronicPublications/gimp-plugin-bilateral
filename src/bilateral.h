#ifndef __BILATERAL_H__
#define __BILATERAL_H__
#include <stdint.h>
#include <libgimp/gimp.h>
#include "main.h"

#ifdef __cplusplus
extern "C" {
#endif
    void bilateral_filter(uint32_t, uint32_t, uint32_t, int, gboolean, GimpDrawable *);

    void bilateral_enhance(uint32_t, uint32_t, float, uint32_t, int, gboolean, GimpDrawable *);
#ifdef __cplusplus
}
#endif
#endif
