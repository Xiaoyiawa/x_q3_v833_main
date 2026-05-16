#ifndef PROJ_PAGE_2048_H
#define PROJ_PAGE_2048_H

#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *      INCLUDES
 *********************/

#include "main.h"
#include "../lv_lib_100ask/lv_lib_100ask.h"
#include "page_manager.h"
#include "../cJSON/cJSON.h"
#include <sys/stat.h>
#include <errno.h>

/*********************
 *      DEFINES
 *********************/

#define MATRIX_SIZE 4
#define SAVE_PATH "./setting/2048.json"


/*********************
 *      TYPEDEFS
 *********************/

typedef struct {
    BasePage base;
    lv_obj_t * game;
    lv_obj_t * score_label;
} Page2048;

BasePage * page_2048_create(void);

#ifdef __cplusplus
}
#endif

#endif