#ifndef PROJ_PAGE_TOTP_H
#define PROJ_PAGE_TOTP_H

#ifdef __cplusplus
extern "C" {
#endif

#include "../lvgl/lvgl.h"
#include "../lv_lib_100ask/lv_lib_100ask.h"
#include "page_manager.h"

lv_obj_t * page_totp(void);

#ifdef __cplusplus
}
#endif

#endif