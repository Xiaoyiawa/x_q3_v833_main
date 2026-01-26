#include "page_demo.h"
#include <stdlib.h>
#include <stdio.h>

static void slider1_changed(lv_event_t * e);
static void btn_click(lv_event_t * e);
static void switch_changed(lv_event_t * e);

static bool led_switch_state = false;

lv_obj_t * page_demo() {
    lv_obj_t * screen = lv_obj_create(lv_scr_act());
    lv_obj_set_size(screen, lv_pct(100), lv_pct(100));

    lv_obj_set_flex_flow(screen, LV_FLEX_FLOW_COLUMN);
	lv_obj_set_scroll_dir(screen, LV_DIR_VER);

	lv_obj_t * battery_label = lv_label_create(screen);
	
	// 读取电量
	FILE *fp = fopen("/sys/class/power_supply/battery/capacity", "r");
	if (fp) {
		int capacity;
		if (fscanf(fp, "%d", &capacity) == 1) {
			char text[32];
			snprintf(text, sizeof(text), "Battery: %d%%", capacity);
			lv_label_set_text(battery_label, text);
		} else {
			lv_label_set_text(battery_label, "Battery: --%");
		}
		fclose(fp);
	} else {
		lv_label_set_text(battery_label, "Battery: --%");
	}
	
	lv_obj_align(battery_label, LV_ALIGN_TOP_RIGHT, -10, 10);
	
	lv_obj_t * label1 = lv_label_create(screen);
	lv_label_set_text(label1, "Hello World!");
	lv_obj_align(label1, LV_FLEX_ALIGN_CENTER, 0, 0);

	lv_obj_t * slider1 = lv_slider_create(screen);
	lv_obj_set_size(slider1, lv_pct(80), lv_pct(10));
	lv_obj_align(slider1, LV_FLEX_ALIGN_CENTER, 0, 0);
	lv_slider_set_range(slider1, 1, 255);
	lv_obj_add_event_cb(slider1, slider1_changed, LV_EVENT_VALUE_CHANGED, NULL);
	
	lv_obj_t * img1 = lv_img_create(screen);
	lv_obj_set_size(img1, 128, 128);
	lv_img_set_src(img1, "/mnt/UDISK/lvgl/res/avatar.png");
	
	lv_obj_t * switch_label = lv_label_create(screen);
	lv_label_set_text(switch_label, "LED");
	lv_obj_align(switch_label, LV_FLEX_ALIGN_CENTER, 0, 0);
	
	lv_obj_t * sw = lv_switch_create(screen);
	lv_obj_set_size(sw, 50, 25);
	lv_obj_align(sw, LV_FLEX_ALIGN_CENTER, 0, 0);
	lv_obj_add_event_cb(sw, switch_changed, LV_EVENT_VALUE_CHANGED, NULL);
	
	lv_obj_t * btn = lv_btn_create(screen);
	lv_obj_set_size(btn, 100, 50);
	lv_obj_align(btn, LV_FLEX_ALIGN_CENTER, 0, 0);
	lv_obj_t * btn_label = lv_label_create(btn);
	lv_label_set_text(btn_label, "back");
	lv_obj_center(btn_label);
	lv_obj_add_event_cb(btn, btn_click, LV_EVENT_CLICKED, NULL);

    led_switch_state = false;

	return screen;
}

static void slider1_changed(lv_event_t * e) {
    lv_obj_t * slider = lv_event_get_target(e);
    int value = lv_slider_get_value(slider);
    lcdBrightness(value);
}

static void btn_click(lv_event_t * e) {
    if (led_switch_state) {
        system("echo 0 > /dev/led_ctrl");
    }
    page_back();
}

static void switch_changed(lv_event_t * e) {
    lv_obj_t * sw = lv_event_get_target(e);
    if(lv_obj_has_state(sw, LV_STATE_CHECKED)) {
        system("echo 1 > /dev/led_ctrl");
        led_switch_state = true;
    } else {
        system("echo 0 > /dev/led_ctrl");
        led_switch_state = false;
    }
}