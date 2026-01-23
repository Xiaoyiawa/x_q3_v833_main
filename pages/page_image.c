#include "page_image.h"

static void back_click(lv_event_t * e);

lv_obj_t * page_image(char * src)
{
    lv_obj_t * screen = lv_obj_create(lv_scr_act());
    lv_obj_remove_style_all(screen);
    lv_obj_set_size(screen, lv_pct(100), lv_pct(100));

    lv_obj_t * img = lv_img_create(screen);
    lv_obj_set_size(img, lv_pct(100), lv_pct(100));
    lv_obj_center(img);
    lv_img_set_src(img, src);
    //lv_img_set_zoom(img, 128);

    lv_obj_t * btn_back = lv_btn_create(screen);
    lv_obj_set_size(btn_back, lv_pct(25), lv_pct(12));
    lv_obj_align(btn_back, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    lv_obj_t * btn_back_label = lv_label_create(btn_back);
    lv_label_set_text(btn_back_label, "back");
    lv_obj_center(btn_back_label);
    lv_obj_add_event_cb(btn_back, back_click, LV_EVENT_CLICKED, NULL);

    return screen;
}

static void back_click(lv_event_t * e)
{
    page_back();
}