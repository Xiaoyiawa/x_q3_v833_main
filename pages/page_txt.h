// page_txt.h
#ifndef PROJ_PAGE_TXT_H
#define PROJ_PAGE_TXT_H

#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *      INCLUDES
 *********************/
#include "../lvgl/lvgl.h"
#include "../lv_lib_100ask/lv_lib_100ask.h"
#include "page_manager.h"

// 添加字体声明
extern lv_font_t HarmonyOS_16;

/*********************
 *      DEFINES
 *********************/

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 * GLOBAL PROTOTYPES
 **********************/
lv_obj_t * page_txt(char * filename);

/**********************
 *      MACROS
 **********************/

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif