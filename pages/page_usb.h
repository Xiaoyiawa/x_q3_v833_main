#ifndef PROJ_PAGE_USB_H
#define PROJ_PAGE_USB_H

#ifdef __cplusplus
extern "C" {
#endif

#include "../lvgl/lvgl.h"
#include "page_manager.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <stdio.h>
#include "main.h"

BasePage * page_usb_create(void);

#ifdef __cplusplus
}
#endif

#endif