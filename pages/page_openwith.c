#include "page_openwith.h"

#include "page_image.h"
#include "page_txt.h"
#include "main.h"
#include <stdlib.h>
#include <string.h>
#include <limits.h>

static char filename[PATH_MAX];

static void back_click_cb(lv_event_t *e);
static void btn_txt_click_cb(lv_event_t *e);

lv_obj_t * page_openwith_create(const char *path)
{
    strcpy(filename, path);

    lv_obj_t *screen = lv_obj_create(lv_scr_act());
    lv_obj_set_size(screen, lv_pct(100), lv_pct(100));
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(screen, 0, 0);
    lv_obj_set_style_border_width(screen, 0, 0);
    lv_obj_set_style_bg_color(screen, lv_color_hex(0xF5F5F5), 0);

    lv_obj_t *btn_back = lv_btn_create(screen);
    lv_obj_set_size(btn_back, lv_pct(25), lv_pct(12));
    lv_obj_align(btn_back, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_t *lbl_back = lv_label_create(btn_back);
    lv_label_set_text(lbl_back, CUSTOM_SYMBOL_BACK "");
    lv_obj_center(lbl_back);
    lv_obj_add_event_cb(btn_back, back_click_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *container = lv_obj_create(screen);
    lv_obj_set_size(container, lv_pct(80), lv_pct(50));
    lv_obj_align(container, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_flex_flow(container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(container, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(container, 10, 0);
    lv_obj_set_style_bg_opa(container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(container, 0, 0);
    lv_obj_clear_flag(container, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *btn_txt = lv_btn_create(container);
    lv_obj_set_size(btn_txt, lv_pct(100), 40);
    lv_obj_set_style_radius(btn_txt, 8, 0);
    lv_obj_set_style_bg_color(btn_txt, lv_color_hex(0x2196F3), 0);
    lv_obj_t *lbl_txt = lv_label_create(btn_txt);
    lv_label_set_text(lbl_txt, "Text");
    lv_obj_center(lbl_txt);
    lv_obj_set_style_text_color(lbl_txt, lv_color_white(), 0);
    lv_obj_add_event_cb(btn_txt, btn_txt_click_cb, LV_EVENT_CLICKED, screen);

    return screen;
}

static void back_click_cb(lv_event_t *e)
{
    (void)e;
    page_back();
}

static void btn_txt_click_cb(lv_event_t *e)
{
    page_open(page_txt_create(filename));
}
