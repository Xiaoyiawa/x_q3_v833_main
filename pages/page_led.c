#include "page_led.h"
#include "platform/led_ctrl.h"
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>

static void switch_event_cb(lv_event_t * e) {
    lv_obj_t * sw = lv_event_get_target(e);
    bool state = lv_obj_has_state(sw, LV_STATE_CHECKED);
    led_ctrl(state ? 1 : 0);
}

BasePage * page_led_create(void) {
    lv_obj_t * scr = lv_obj_create(lv_scr_act());
    lv_obj_remove_style_all(scr);
    lv_obj_set_size(scr, lv_pct(100), lv_pct(100));
    lv_obj_set_style_bg_color(scr, lv_color_white(), 0);

    /* 创建 Switch 控件，宽80 高40（即高40x宽80） */
    lv_obj_t * sw = lv_switch_create(scr);
    lv_obj_set_size(sw, 80, 40);
    lv_obj_align(sw, LV_ALIGN_CENTER, 0, 0);

    /* 读取当前 LED 状态 */
    int fd = open("/dev/led_ctrl", O_RDONLY);
    if (fd >= 0) {
        char buf[2] = {0};
        if (read(fd, buf, 1) > 0) {
            if (buf[0] == '0')
                lv_obj_clear_state(sw, LV_STATE_CHECKED);
            else
                lv_obj_add_state(sw, LV_STATE_CHECKED);
        } else {
            lv_obj_clear_state(sw, LV_STATE_CHECKED);
        }
        close(fd);
    } else {
        /* 设备不存在则默认关闭 */
        lv_obj_clear_state(sw, LV_STATE_CHECKED);
    }

    /* 控件状态变化时写入设备 */
    lv_obj_add_event_cb(sw, switch_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    return base_page_create(scr);
}