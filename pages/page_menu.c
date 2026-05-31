#include "page_menu.h"

#include "platform/battery_manager.h"
#include "page_file_manager.h"
#include "page_bird.h"
#include "page_calculator.h"
#include "page_demo.h"
#include "page_ftp.h"
#include "page_2048.h"
#include "page_recorder.h"
#include "page_usb.h"
#include "page_upgrade.h"
#include "page_led.h"
#include "main.h"
#include "platform/str_utils.h"

typedef struct
{
    BasePage base;
    lv_obj_t * label_time;
    lv_obj_t * label_battery;
    lv_timer_t * battery_timer;
} MenuPage;


static void btn_demo_click(lv_event_t * e);
static void btn_back_click(lv_event_t * e);
static void btn_file_manager_click(lv_event_t * e);
static void btn_calculator_click(lv_event_t * e);
static void btn_bird_click(lv_event_t * e);
static void btn_ftp_click(lv_event_t * e);
static void btn_recorder_click(lv_event_t * e);
static void btn_usb_click(lv_event_t * e);
static void btn_2048_click(lv_event_t * e);
static void btn_upgrade_click(lv_event_t * e);
static void btn_led_click(lv_event_t * e);

static lv_obj_t * page_menu_obj(MenuPage * page);
static void menu_on_destroy(void * p);
static void battery_timer_cb(lv_timer_t * timer);

BasePage * page_menu_create(void)
{
    MenuPage * page = malloc(sizeof(MenuPage));
    if(!page) return NULL;
    memset(page, 0, sizeof(MenuPage));

    page->base.obj        = page_menu_obj(page);
    page->base.on_destroy = menu_on_destroy;   // 注册销毁回调
    return (BasePage *)page;
}

static void battery_timer_cb(lv_timer_t * timer)
{
    lv_obj_t * label = (lv_obj_t *)timer->user_data;
    uint8_t cap = battery_get_capacity();
    char buf[16];
    snprintf(buf, sizeof(buf), "%d%%", cap);
    lv_label_set_text(label, buf);
}

lv_obj_t * page_menu_obj(MenuPage * page)
{
    lv_obj_t * screen = lv_obj_create(lv_scr_act());
    lv_obj_set_size(screen, lv_pct(100), lv_pct(100));
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(screen, 0, LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(screen, 0, LV_STATE_DEFAULT);

    lv_obj_t * btn_back = lv_btn_create(screen);
    lv_obj_set_size(btn_back, lv_pct(25), lv_pct(12));
    lv_obj_align(btn_back, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_t * btn_back_label = lv_label_create(btn_back);
    lv_label_set_text(btn_back_label, CUSTOM_SYMBOL_BACK "");
    lv_obj_center(btn_back_label);
    lv_obj_add_event_cb(btn_back, btn_back_click, LV_EVENT_CLICKED, NULL);

    page->label_battery = lv_label_create(screen);
    lv_obj_align(page->label_battery, LV_ALIGN_TOP_RIGHT, -10, 5);
    lv_obj_set_style_text_color(page->label_battery, lv_color_hex(0x333333), 0);
    uint8_t cap = battery_get_capacity();
    char buf[16];
    snprintf(buf, sizeof(buf), "%d%%", cap);
    lv_label_set_text(page->label_battery, buf);

    page->battery_timer = lv_timer_create(battery_timer_cb, 1000, page->label_battery);
    
    lv_obj_t * container = lv_obj_create(screen);
    lv_obj_align(container, LV_ALIGN_TOP_MID, 0, lv_pct(12));
    lv_obj_set_size(container, lv_pct(100), lv_pct(88));
    lv_obj_set_flex_flow(container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_scroll_dir(container, LV_DIR_VER);

    /* ===== 菜单项 ===== */
    lv_obj_t * btn_file_manager = lv_btn_create(container);
    lv_obj_set_size(btn_file_manager, lv_pct(64), lv_pct(32));
    lv_obj_align(btn_file_manager, LV_FLEX_ALIGN_CENTER, 0, 0);
    lv_obj_t * btn_label_file_manager = lv_label_create(btn_file_manager);
    lv_label_set_text(btn_label_file_manager, "File Manager");
    lv_obj_center(btn_label_file_manager);
    lv_obj_add_event_cb(btn_file_manager, btn_file_manager_click, LV_EVENT_CLICKED, NULL);

    lv_obj_t * btn_calculator = lv_btn_create(container);
    lv_obj_set_size(btn_calculator, lv_pct(64), lv_pct(32));
    lv_obj_align(btn_calculator, LV_FLEX_ALIGN_CENTER, 0, 0);
    lv_obj_t * btn_label_calculator = lv_label_create(btn_calculator);
    lv_label_set_text(btn_label_calculator, "Calculator");
    lv_obj_center(btn_label_calculator);
    lv_obj_add_event_cb(btn_calculator, btn_calculator_click, LV_EVENT_CLICKED, NULL);

    lv_obj_t * btn_bird = lv_btn_create(container);
    lv_obj_set_size(btn_bird, lv_pct(64), lv_pct(32));
    lv_obj_align(btn_bird, LV_FLEX_ALIGN_CENTER, 0, 0);
    lv_obj_t * btn_label_bird = lv_label_create(btn_bird);
    lv_label_set_text(btn_label_bird, "Flappy Bird");
    lv_obj_center(btn_label_bird);
    lv_obj_add_event_cb(btn_bird, btn_bird_click, LV_EVENT_CLICKED, NULL);

    lv_obj_t * btn_ftp = lv_btn_create(container);
    lv_obj_set_size(btn_ftp, lv_pct(64), lv_pct(32));
    lv_obj_align(btn_ftp, LV_FLEX_ALIGN_CENTER, 0, 0);
    lv_obj_t * btn_label_ftp = lv_label_create(btn_ftp);
    lv_label_set_text(btn_label_ftp, "FTP");
    lv_obj_center(btn_label_ftp);
    lv_obj_add_event_cb(btn_ftp, btn_ftp_click, LV_EVENT_CLICKED, NULL);

    lv_obj_t * btn_2048 = lv_btn_create(container);
    lv_obj_set_size(btn_2048, lv_pct(64), lv_pct(32));
    lv_obj_align(btn_2048, LV_FLEX_ALIGN_CENTER, 0, 0);
    lv_obj_t * btn_label_2048 = lv_label_create(btn_2048);
    lv_label_set_text(btn_label_2048, "2048");
    lv_obj_center(btn_label_2048);
    lv_obj_add_event_cb(btn_2048, btn_2048_click, LV_EVENT_CLICKED, NULL);

    lv_obj_t * btn_recorder = lv_btn_create(container);
    lv_obj_set_size(btn_recorder, lv_pct(64), lv_pct(32));
    lv_obj_align(btn_recorder, LV_FLEX_ALIGN_CENTER, 0, 0);
    lv_obj_t * btn_label_recorder = lv_label_create(btn_recorder);
    lv_label_set_text(btn_label_recorder, "Recorder");
    lv_obj_center(btn_label_recorder);
    lv_obj_add_event_cb(btn_recorder, btn_recorder_click, LV_EVENT_CLICKED, NULL);

    lv_obj_t * btn_upgrade = lv_btn_create(container);
    lv_obj_set_size(btn_upgrade, lv_pct(64), lv_pct(32));
    lv_obj_align(btn_upgrade, LV_FLEX_ALIGN_CENTER, 0, 0);
    lv_obj_t * btn_label_upgrade = lv_label_create(btn_upgrade);
    lv_label_set_text(btn_label_upgrade, "upgrade");
    lv_obj_center(btn_label_upgrade);
    lv_obj_add_event_cb(btn_upgrade, btn_upgrade_click, LV_EVENT_CLICKED, NULL);

    lv_obj_t * btn_led = lv_btn_create(container);
    lv_obj_set_size(btn_led, lv_pct(64), lv_pct(32));
    lv_obj_align(btn_led, LV_FLEX_ALIGN_CENTER, 0, 0);
    lv_obj_t * btn_label_led = lv_label_create(btn_led);
    lv_label_set_text(btn_label_led, "LED");
    lv_obj_center(btn_label_led);
    lv_obj_add_event_cb(btn_led, btn_led_click, LV_EVENT_CLICKED, NULL);

    lv_obj_t * btn_usb = lv_btn_create(container);
    lv_obj_set_size(btn_usb, lv_pct(64), lv_pct(32));
    lv_obj_align(btn_usb, LV_FLEX_ALIGN_CENTER, 0, 0);
    lv_obj_t * btn_label_usb = lv_label_create(btn_usb);
    lv_label_set_text(btn_label_usb, "Debug");
    lv_obj_center(btn_label_usb);
    lv_obj_add_event_cb(btn_usb, btn_usb_click, LV_EVENT_CLICKED, NULL);

    lv_obj_t * btn_demo = lv_btn_create(container);
    lv_obj_set_size(btn_demo, lv_pct(64), lv_pct(32));
    lv_obj_align(btn_demo, LV_FLEX_ALIGN_CENTER, 0, 0);
    lv_obj_t * btn_label_demo = lv_label_create(btn_demo);
    lv_label_set_text(btn_label_demo, "Test Page");
    lv_obj_center(btn_label_demo);
    lv_obj_add_event_cb(btn_demo, btn_demo_click, LV_EVENT_CLICKED, NULL);

    return screen;
}

static void menu_on_destroy(void * p)
{
    MenuPage * page = (MenuPage *)p;
    if (page->battery_timer) {
        lv_timer_del(page->battery_timer);
        page->battery_timer = NULL;
    }
}

/* ===== 事件回调 ===== */
static void btn_demo_click(lv_event_t * e)
{
    page_open(demo_page_create());
}

static void btn_back_click(lv_event_t * e)
{
    page_back();
}

static void btn_file_manager_click(lv_event_t * e)
{
    page_open(page_file_manager_create());
}

static void btn_calculator_click(lv_event_t * e)
{
    page_open_obj(page_calc());
}

static void btn_bird_click(lv_event_t * e)
{
    page_open(page_bird_create());
}

static void btn_ftp_click(lv_event_t * e)
{
    page_open_obj(page_ftp());
}

static void btn_recorder_click(lv_event_t * e)
{
    page_open(recorder_page_create());
}

static void btn_usb_click(lv_event_t * e)
{
    page_open(page_usb_create());
}

static void btn_2048_click(lv_event_t * e)
{
    page_open(page_2048_create());
}

static void btn_upgrade_click(lv_event_t * e)
{
    page_open(page_upgrade_create("https://raw.giteeusercontent.com/testxiaoyi/demo_upgrade/raw/master/version.json"));
}

static void btn_led_click(lv_event_t * e)
{
    page_open(page_led_create());
}