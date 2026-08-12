#include "page_wifi.h"
#include "platform/network.h"
#include "views/custom_msgbox.h"
#include "main.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>

/* ==================== 常量 ==================== */
#define SCREEN_W        240
#define SCREEN_H        240

#define TOP_BAR_H       30
#define SWITCH_AREA_H   (SCREEN_H * 20 / 100)
#define AP_ITEM_H       LV_SIZE_CONTENT
#define PASSWORD_INPUT_H 35

/* ==================== 结构体 ==================== */
typedef struct {
    wifi_ap_t *aps;
    int count;
    int ret;
} scan_result_t;

typedef struct {
    int ret;
    char ssid[64];
    char password[128];
} connect_result_t;

typedef struct {
    BasePage base;
    lv_obj_t *screen;
    lv_obj_t *container;
    lv_obj_t *switch_obj;
    lv_obj_t *status_label;
    wifi_ap_t *ap_list;
    int ap_count;
    bool is_scanning;
    bool is_connecting;
    char *connecting_ssid;
    lv_obj_t *password_screen;
    lv_obj_t *password_input;
    lv_obj_t *password_keyboard;
    char *pending_ssid;
    char pending_password[128];
    wifi_ap_t *item_data[64];
    int item_count;
    bool first_scan;
    lv_obj_t *delete_dialog_bg;
    lv_obj_t *delete_dialog_box;
    char *delete_target_ssid;
    bool delete_dialog_showing;
    lv_timer_t *reset_timer;      /* 用于3秒后恢复状态 */
} WifiPage;

static WifiPage *g_page = NULL;

/* ==================== 函数声明 ==================== */
static void back_click(lv_event_t *e);
static void refresh_click(lv_event_t *e);
static void switch_event_cb(lv_event_t *e);
static void ap_click_cb(lv_event_t *e);
static void network_item_long_pressed_cb(lv_event_t *e);
static void password_back_click(lv_event_t *e);
static void password_join_click(lv_event_t *e);
static void password_input_focus_cb(lv_event_t *e);
static void update_ap_list(void);
static void add_ap_item(wifi_ap_t *ap);
static void clear_container(void);
static void show_password_page(const char *ssid);
static void hide_password_page(void);
static void set_state_text(const char *text);
static void *scan_thread(void *arg);
static void *connect_thread(void *arg);
static void on_scan_done(void *data);
static void on_connect_done(void *data);
static void show_delete_network_dialog(WifiPage *p, const char *ssid);
static void hide_delete_network_dialog(WifiPage *p);
static void delete_network_confirm_cb(lv_event_t *e);
static void delete_network_cancel_cb(lv_event_t *e);
static void reset_state_timer_cb(lv_timer_t *timer);

/* ==================== 信号强度转文字 ==================== */
static const char* signal_to_text(int signal) {
    if (signal >= -50) return "强";
    else if (signal >= -65) return "中";
    else if (signal >= -80) return "弱";
    else return "极弱";
}

/* ==================== 排序函数 ==================== */
static int ap_cmp(const void *a, const void *b) {
    const wifi_ap_t *pa = (const wifi_ap_t *)a;
    const wifi_ap_t *pb = (const wifi_ap_t *)b;
    const char *connected = network_wifi_get_connected_ssid();
    int pa_cur = (connected && strcmp(connected, pa->ssid) == 0);
    int pb_cur = (connected && strcmp(connected, pb->ssid) == 0);
    if (pa_cur != pb_cur) return pa_cur ? -1 : 1;
    if (pa->saved != pb->saved) return pa->saved ? -1 : 1;
    if (pa->signal != pb->signal) return pa->signal > pb->signal ? -1 : 1;
    return strcmp(pa->ssid, pb->ssid);
}

/* ==================== UI 创建 ==================== */

BasePage *page_wifi_create(void) {
    printf("[WiFi Page] Creating WiFi page...\n");
    fflush(stdout);

    WifiPage *page = calloc(1, sizeof(WifiPage));
    if (!page) {
        printf("[WiFi Page] calloc failed\n");
        fflush(stdout);
        return NULL;
    }
    g_page = page;
    page->first_scan = true;
    page->pending_password[0] = '\0';
    page->reset_timer = NULL;
    printf("[WiFi Page] Page struct allocated\n");
    fflush(stdout);

    lv_obj_t *scr = lv_obj_create(lv_scr_act());
    if (!scr) {
        printf("[WiFi Page] lv_obj_create failed\n");
        fflush(stdout);
        free(page);
        return NULL;
    }
    page->screen = scr;
    printf("[WiFi Page] Screen created\n");
    fflush(stdout);

    lv_obj_remove_style_all(scr);
    lv_obj_set_size(scr, SCREEN_W, SCREEN_H);
    lv_obj_set_style_bg_color(scr, lv_color_hex(0xf1f5f9), 0);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    /* ===== 顶部栏 ===== */
    printf("[WiFi Page] Creating top bar...\n");
    fflush(stdout);
    lv_obj_t *bar = lv_obj_create(scr);
    if (!bar) {
        printf("[WiFi Page] bar create failed\n");
        fflush(stdout);
        free(page);
        return NULL;
    }
    lv_obj_set_size(bar, SCREEN_W, TOP_BAR_H);
    lv_obj_set_pos(bar, 0, 0);
    lv_obj_set_style_pad_all(bar, 0, 0);
    lv_obj_set_style_border_width(bar, 0, 0);
    lv_obj_set_style_bg_color(bar, lv_color_hex(0x333333), 0);
    lv_obj_set_flex_flow(bar, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(bar, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t *btn_back = lv_btn_create(bar);
    if (!btn_back) {
        printf("[WiFi Page] btn_back create failed\n");
        fflush(stdout);
        free(page);
        return NULL;
    }
    lv_obj_set_size(btn_back, 40, 26);
    lv_obj_set_style_pad_all(btn_back, 0, 0);
    lv_obj_t *lbl_back = lv_label_create(btn_back);
    if (lbl_back) {
        lv_label_set_text(lbl_back, CUSTOM_SYMBOL_BACK "");
        lv_obj_center(lbl_back);
    }
    lv_obj_add_event_cb(btn_back, back_click, LV_EVENT_CLICKED, NULL);

    lv_obj_t *title = lv_label_create(bar);
    if (title) {
        lv_label_set_text(title, "WiFi");
        lv_obj_set_style_text_color(title, lv_color_white(), 0);
    }

    lv_obj_t *btn_refresh = lv_btn_create(bar);
    if (!btn_refresh) {
        printf("[WiFi Page] btn_refresh create failed\n");
        fflush(stdout);
        free(page);
        return NULL;
    }
    lv_obj_set_size(btn_refresh, 44, 26);
    lv_obj_set_style_pad_all(btn_refresh, 0, 0);
    lv_obj_t *lbl_refresh = lv_label_create(btn_refresh);
    if (lbl_refresh) {
        lv_label_set_text(lbl_refresh, "扫描");
        lv_obj_center(lbl_refresh);
    }
    lv_obj_add_event_cb(btn_refresh, refresh_click, LV_EVENT_CLICKED, NULL);

    /* ===== WiFi 开关区域 ===== */
    printf("[WiFi Page] Creating switch area...\n");
    fflush(stdout);
    lv_obj_t *switch_area = lv_obj_create(scr);
    if (!switch_area) {
        printf("[WiFi Page] switch_area create failed\n");
        fflush(stdout);
        free(page);
        return NULL;
    }
    lv_obj_set_size(switch_area, SCREEN_W, SWITCH_AREA_H);
    lv_obj_set_pos(switch_area, 0, TOP_BAR_H);
    lv_obj_set_style_border_width(switch_area, 1, 0);
    lv_obj_set_style_border_color(switch_area, lv_color_hex(0xCCCCCC), 0);
    lv_obj_set_style_pad_all(switch_area, 4, 0);
    lv_obj_set_flex_flow(switch_area, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(switch_area, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(switch_area, 6, 0);

    /* 状态标签（宽度165px，滚动展示） */
    page->status_label = lv_label_create(switch_area);
    if (page->status_label) {
        lv_label_set_text(page->status_label, "WiFi开关");
        lv_obj_set_width(page->status_label, 165);
        lv_label_set_long_mode(page->status_label, LV_LABEL_LONG_SCROLL_CIRCULAR);
        lv_obj_set_style_text_color(page->status_label, lv_color_hex(0x333333), 0);
    }

    /* WiFi 开关 */
    page->switch_obj = lv_switch_create(switch_area);
    if (!page->switch_obj) {
        printf("[WiFi Page] switch_obj create failed\n");
        fflush(stdout);
        free(page);
        return NULL;
    }
    lv_obj_set_size(page->switch_obj, 40, 26);
    lv_obj_add_event_cb(page->switch_obj, switch_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    /* ===== 网络列表容器 ===== */
    printf("[WiFi Page] Creating network container...\n");
    fflush(stdout);
    page->container = lv_obj_create(scr);
    if (!page->container) {
        printf("[WiFi Page] container create failed\n");
        fflush(stdout);
        free(page);
        return NULL;
    }
    lv_obj_set_pos(page->container, 0, TOP_BAR_H + SWITCH_AREA_H);
    lv_obj_set_size(page->container, SCREEN_W,
                    SCREEN_H - TOP_BAR_H - SWITCH_AREA_H);
    lv_obj_set_style_border_width(page->container, 0, 0);
    lv_obj_set_style_pad_all(page->container, 4, 0);
    lv_obj_set_style_pad_row(page->container, 4, 0);
    lv_obj_set_flex_flow(page->container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_scrollbar_mode(page->container, LV_SCROLLBAR_MODE_AUTO);
    lv_obj_set_user_data(page->container, page);

    /* ===== 检测 WiFi 状态 ===== */
    printf("[WiFi Page] Checking WiFi status...\n");
    fflush(stdout);
    if (network_wifi_is_enabled()) {
        printf("[WiFi Page] WiFi is enabled\n");
        fflush(stdout);
        if (page->switch_obj) {
            lv_obj_add_state(page->switch_obj, LV_STATE_CHECKED);
        }
        set_state_text("WiFi开关");
        refresh_click(NULL);
    } else {
        printf("[WiFi Page] WiFi is disabled\n");
        fflush(stdout);
        if (page->switch_obj) {
            lv_obj_clear_state(page->switch_obj, LV_STATE_CHECKED);
        }
        set_state_text("WiFi开关");
    }

    page->base.obj = scr;
    printf("[WiFi Page] Page creation complete\n");
    fflush(stdout);
    return (BasePage *)page;
}

/* ==================== UI 回调 ==================== */

static void back_click(lv_event_t *e) {
    (void)e;
    printf("[WiFi Page] Back clicked\n");
    fflush(stdout);
    if (g_page) {
        if (g_page->reset_timer) {
            lv_timer_del(g_page->reset_timer);
            g_page->reset_timer = NULL;
        }
        if (g_page->ap_list) {
            free(g_page->ap_list);
            g_page->ap_list = NULL;
        }
        for (int i = 0; i < g_page->item_count; i++) {
            if (g_page->item_data[i]) {
                free(g_page->item_data[i]);
                g_page->item_data[i] = NULL;
            }
        }
        g_page->item_count = 0;
        if (g_page->connecting_ssid) {
            free(g_page->connecting_ssid);
            g_page->connecting_ssid = NULL;
        }
        if (g_page->pending_ssid) {
            free(g_page->pending_ssid);
            g_page->pending_ssid = NULL;
        }
        if (g_page->delete_target_ssid) {
            free(g_page->delete_target_ssid);
            g_page->delete_target_ssid = NULL;
        }
        hide_delete_network_dialog(g_page);
        g_page = NULL;
    }
    page_back();
}

static void refresh_click(lv_event_t *e) {
    (void)e;
    printf("[WiFi Page] Refresh clicked\n");
    fflush(stdout);

    if (!g_page) {
        printf("[WiFi Page] g_page is NULL in refresh_click\n");
        fflush(stdout);
        return;
    }

    if (!network_wifi_is_enabled()) {
        custom_toast_create("请先开启 WiFi");
        return;
    }
    if (g_page->is_scanning) {
        printf("[WiFi Page] Already scanning\n");
        fflush(stdout);
        return;
    }
    g_page->is_scanning = true;
    set_state_text("扫描中...");
    clear_container();

    pthread_t tid;
    if (pthread_create(&tid, NULL, scan_thread, NULL) != 0) {
        printf("[WiFi Page] pthread_create failed\n");
        fflush(stdout);
        g_page->is_scanning = false;
        set_state_text("WiFi开关");
        custom_toast_create("扫描启动失败");
    } else {
        pthread_detach(tid);
    }
}

static void switch_event_cb(lv_event_t *e) {
    if (!g_page || !g_page->switch_obj) {
        printf("[WiFi Page] switch_event_cb: g_page or switch_obj NULL\n");
        fflush(stdout);
        return;
    }

    bool on = lv_obj_has_state(g_page->switch_obj, LV_STATE_CHECKED);
    printf("[WiFi Page] Switch toggled: %d\n", on);
    fflush(stdout);

    if (on) {
        int ret = network_wifi_init();
        if (ret == 0) {
            set_state_text("WiFi开关");
            g_page->first_scan = true;
            refresh_click(NULL);
        } else {
            custom_toast_create("WiFi 开启失败");
            if (g_page->switch_obj) {
                lv_obj_clear_state(g_page->switch_obj, LV_STATE_CHECKED);
            }
            set_state_text("WiFi开关");
        }
    } else {
        network_wifi_deinit();
        if (g_page->ap_list) {
            free(g_page->ap_list);
            g_page->ap_list = NULL;
        }
        g_page->ap_count = 0;
        clear_container();
        /* 关闭时显示“已关闭”，3秒后恢复“WiFi开关” */
        set_state_text("已关闭");
        if (g_page->reset_timer) {
            lv_timer_del(g_page->reset_timer);
            g_page->reset_timer = NULL;
        }
        g_page->reset_timer = lv_timer_create(reset_state_timer_cb, 3000, g_page);
    }
}

static void ap_click_cb(lv_event_t *e) {
    lv_obj_t *row = lv_event_get_target(e);
    if (!row) return;
    
    wifi_ap_t *ap = (wifi_ap_t *)lv_obj_get_user_data(row);
    if (!ap) {
        ap = (wifi_ap_t *)lv_event_get_user_data(e);
        if (!ap) return;
    }

    if (!g_page) {
        printf("[WiFi Page] ap_click_cb: g_page NULL\n");
        fflush(stdout);
        return;
    }

    if (g_page->is_connecting) {
        printf("[WiFi Page] Already connecting\n");
        fflush(stdout);
        return;
    }

    printf("[WiFi Page] AP clicked: %s\n", ap->ssid);
    fflush(stdout);

    const char *connected = network_wifi_get_connected_ssid();

    if (connected && strcmp(connected, ap->ssid) == 0) {
        network_wifi_disconnect();
        custom_toast_create("已断开");
        set_state_text("WiFi开关");
        refresh_click(NULL);
        return;
    }

    const char *saved_pw = network_wifi_get_saved_password(ap->ssid);
    if (saved_pw) {
        printf("[WiFi Page] Saved password found, connecting...\n");
        fflush(stdout);
        custom_toast_create("连接中...");
        g_page->is_connecting = true;
        g_page->connecting_ssid = strdup(ap->ssid);
        set_state_text("扫描中...");  /* 保持扫描中状态 */
        pthread_t tid;
        char *ssid_copy = strdup(ap->ssid);
        if (ssid_copy && pthread_create(&tid, NULL, connect_thread, ssid_copy) == 0) {
            pthread_detach(tid);
        } else {
            free(ssid_copy);
            g_page->is_connecting = false;
            custom_toast_create("连接启动失败");
            set_state_text("WiFi开关");
        }
        return;
    }

    if (ap->encrypted) {
        printf("[WiFi Page] Encrypted AP, showing password page\n");
        fflush(stdout);
        show_password_page(ap->ssid);
    } else {
        printf("[WiFi Page] Open AP, connecting directly\n");
        fflush(stdout);
        custom_toast_create("连接中...");
        g_page->is_connecting = true;
        g_page->connecting_ssid = strdup(ap->ssid);
        set_state_text("扫描中...");
        pthread_t tid;
        char *ssid_copy = strdup(ap->ssid);
        if (ssid_copy && pthread_create(&tid, NULL, connect_thread, ssid_copy) == 0) {
            pthread_detach(tid);
        } else {
            free(ssid_copy);
            g_page->is_connecting = false;
            custom_toast_create("连接启动失败");
            set_state_text("WiFi开关");
        }
    }
}

static void network_item_long_pressed_cb(lv_event_t *e) {
    lv_obj_t *row = lv_event_get_target(e);
    wifi_ap_t *ap = (wifi_ap_t *)lv_obj_get_user_data(row);
    if (!ap || !ap->saved) return;
    printf("[WiFi Page] Long press on saved network: %s\n", ap->ssid);
    fflush(stdout);
    show_delete_network_dialog(g_page, ap->ssid);
}

/* ==================== 密码输入页面 ==================== */

static void show_password_page(const char *ssid) {
    if (!g_page) return;
    if (g_page->password_screen) {
        printf("[WiFi Page] Password page already exists\n");
        fflush(stdout);
        return;
    }
    g_page->pending_ssid = strdup(ssid);
    g_page->pending_password[0] = '\0';
    printf("[WiFi Page] Showing password page for: %s\n", ssid);
    fflush(stdout);

    lv_obj_t *scr = lv_obj_create(lv_scr_act());
    if (!scr) {
        printf("[WiFi Page] password screen create failed\n");
        fflush(stdout);
        return;
    }
    g_page->password_screen = scr;
    lv_obj_remove_style_all(scr);
    lv_obj_set_size(scr, SCREEN_W, SCREEN_H);
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_70, 0);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(scr, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(scr, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START);

    lv_obj_t *bar = lv_obj_create(scr);
    lv_obj_set_size(bar, SCREEN_W, TOP_BAR_H);
    lv_obj_set_style_pad_all(bar, 0, 0);
    lv_obj_set_style_border_width(bar, 0, 0);
    lv_obj_set_style_bg_color(bar, lv_color_hex(0x333333), 0);
    lv_obj_clear_flag(bar, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(bar, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(bar, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t *btn_back = lv_btn_create(bar);
    if (btn_back) {
        lv_obj_set_size(btn_back, 40, 26);
        lv_obj_set_style_pad_all(btn_back, 0, 0);
        lv_obj_t *lbl_back = lv_label_create(btn_back);
        if (lbl_back) {
            lv_label_set_text(lbl_back, CUSTOM_SYMBOL_BACK "");
            lv_obj_center(lbl_back);
        }
        lv_obj_add_event_cb(btn_back, password_back_click, LV_EVENT_CLICKED, NULL);
    }

    lv_obj_t *title = lv_label_create(bar);
    if (title) {
        lv_label_set_text_fmt(title, "请输入密码");
        lv_obj_set_style_text_color(title, lv_color_white(), 0);
    }

    lv_obj_t *btn_join = lv_btn_create(bar);
    if (btn_join) {
        lv_obj_set_size(btn_join, 40, 26);
        lv_obj_set_style_pad_all(btn_join, 0, 0);
        lv_obj_t *lbl_join = lv_label_create(btn_join);
        if (lbl_join) {
            lv_label_set_text(lbl_join, "Join");
            lv_obj_center(lbl_join);
        }
        lv_obj_add_event_cb(btn_join, password_join_click, LV_EVENT_CLICKED, NULL);
    }

    g_page->password_input = lv_textarea_create(scr);
    if (g_page->password_input) {
        lv_obj_set_width(g_page->password_input, lv_pct(100));
        lv_obj_set_height(g_page->password_input, 40);
        lv_obj_set_style_bg_color(g_page->password_input, lv_color_white(), 0);
        lv_obj_set_style_text_color(g_page->password_input, lv_color_black(), 0);
        lv_textarea_set_one_line(g_page->password_input, true);
        lv_textarea_set_password_mode(g_page->password_input, true);
        lv_textarea_set_placeholder_text(g_page->password_input, "请输入密码");
        lv_obj_add_event_cb(g_page->password_input, password_input_focus_cb,
                            LV_EVENT_FOCUSED, NULL);
    }

    g_page->password_keyboard = lv_keyboard_create(scr);
    if (g_page->password_keyboard) {
        lv_obj_set_width(g_page->password_keyboard, lv_pct(100));
        lv_obj_set_flex_grow(g_page->password_keyboard, 1);
        lv_keyboard_set_textarea(g_page->password_keyboard, g_page->password_input);
    }

    if (g_page->password_input) {
        lv_textarea_set_cursor_pos(g_page->password_input, 0);
    }

    printf("[WiFi Page] Password page created\n");
    fflush(stdout);
}

static void hide_password_page(void) {
    if (!g_page) return;
    if (g_page->password_screen) {
        lv_obj_del(g_page->password_screen);
        g_page->password_screen = NULL;
        g_page->password_input = NULL;
        g_page->password_keyboard = NULL;
    }
    if (g_page->pending_ssid) {
        free(g_page->pending_ssid);
        g_page->pending_ssid = NULL;
    }
}

static void password_back_click(lv_event_t *e) {
    (void)e;
    hide_password_page();
}

static void password_join_click(lv_event_t *e) {
    (void)e;
    if (!g_page || !g_page->pending_ssid) {
        printf("[WiFi Page] password_join: no pending ssid\n");
        fflush(stdout);
        return;
    }
    if (!g_page->password_input) {
        printf("[WiFi Page] password_join: password_input NULL\n");
        fflush(stdout);
        return;
    }

    const char *pw = lv_textarea_get_text(g_page->password_input);
    if (!pw || strlen(pw) == 0) {
        custom_toast_create("请输入密码");
        return;
    }

    strncpy(g_page->pending_password, pw, sizeof(g_page->pending_password)-1);
    char *ssid_copy = strdup(g_page->pending_ssid);
    hide_password_page();

    if (!ssid_copy) {
        custom_toast_create("内存不足");
        return;
    }

    custom_toast_create("连接中...");
    g_page->is_connecting = true;
    g_page->connecting_ssid = strdup(ssid_copy);
    set_state_text("扫描中...");
    pthread_t tid;
    if (pthread_create(&tid, NULL, connect_thread, ssid_copy) == 0) {
        pthread_detach(tid);
    } else {
        free(ssid_copy);
        g_page->is_connecting = false;
        custom_toast_create("连接启动失败");
        set_state_text("WiFi开关");
    }
}

static void password_input_focus_cb(lv_event_t *e) {
    (void)e;
}

/* ==================== 网络列表操作 ==================== */

static void clear_container(void) {
    printf("[WiFi Page] clear_container\n");
    fflush(stdout);

    if (!g_page || !g_page->container) {
        printf("[WiFi Page] clear_container: container invalid\n");
        fflush(stdout);
        return;
    }

    lv_obj_clean(g_page->container);

    for (int i = 0; i < g_page->item_count; i++) {
        if (g_page->item_data[i]) {
            free(g_page->item_data[i]);
            g_page->item_data[i] = NULL;
        }
    }
    g_page->item_count = 0;
}

static void add_ap_item(wifi_ap_t *ap) {
    if (!g_page || !g_page->container) return;

    wifi_ap_t *ap_data = malloc(sizeof(wifi_ap_t));
    if (!ap_data) return;
    memcpy(ap_data, ap, sizeof(wifi_ap_t));
    g_page->item_data[g_page->item_count++] = ap_data;

    lv_obj_t *row = lv_btn_create(g_page->container);
    if (!row) {
        free(ap_data);
        return;
    }
    lv_obj_set_width(row, lv_pct(100));
    lv_obj_set_height(row, LV_SIZE_CONTENT);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_set_style_pad_all(row, 8, 0);
    lv_obj_set_style_pad_row(row, 2, 0);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_COLUMN);
    lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(row, ap_click_cb, LV_EVENT_CLICKED, ap_data);
    lv_obj_set_user_data(row, ap_data);

    if (ap->saved) {
        lv_obj_add_event_cb(row, network_item_long_pressed_cb, LV_EVENT_LONG_PRESSED, NULL);
    }

    const char *connected = network_wifi_get_connected_ssid();
    bool is_current = (connected && strcmp(connected, ap->ssid) == 0);

    lv_obj_t *line1 = lv_obj_create(row);
    lv_obj_remove_style_all(line1);
    lv_obj_set_width(line1, lv_pct(100));
    lv_obj_set_height(line1, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(line1, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(line1, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(line1, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t *marker = lv_label_create(line1);
    if (marker) {
        const char *mark = "";
        if (is_current) mark = "◆ ";
        else if (ap->saved) mark = "★ ";
        lv_label_set_text(marker, mark);
        lv_obj_set_width(marker, 20);
        lv_obj_clear_flag(marker, LV_OBJ_FLAG_CLICKABLE);
    }

    lv_obj_t *ssid_label = lv_label_create(line1);
    if (ssid_label) {
        lv_label_set_text(ssid_label, ap->ssid[0] ? ap->ssid : "(隐藏网络)");
        lv_obj_set_width(ssid_label, lv_pct(70));
        lv_label_set_long_mode(ssid_label, LV_LABEL_LONG_WRAP);
        lv_obj_clear_flag(ssid_label, LV_OBJ_FLAG_CLICKABLE);
    }

    lv_obj_t *signal_label = lv_label_create(line1);
    if (signal_label) {
        if (is_current) {
            lv_label_set_text(signal_label, "已连");
            lv_obj_set_style_text_color(signal_label, lv_color_hex(0x00AA00), 0);
        } else if (ap->signal > -100) {
            lv_label_set_text(signal_label, signal_to_text(ap->signal));
        } else {
            lv_label_set_text(signal_label, "");
        }
        lv_obj_align(signal_label, LV_ALIGN_RIGHT_MID, 0, 0);
        lv_obj_set_width(signal_label, 32);
        lv_label_set_long_mode(signal_label, LV_LABEL_LONG_DOT);
        lv_obj_clear_flag(signal_label, LV_OBJ_FLAG_CLICKABLE);
    }

    if (!is_current) {
        lv_obj_t *line2 = lv_obj_create(row);
        lv_obj_remove_style_all(line2);
        lv_obj_set_width(line2, lv_pct(100));
        lv_obj_set_height(line2, LV_SIZE_CONTENT);
        lv_obj_set_style_pad_left(line2, 20, 0);
        lv_obj_clear_flag(line2, LV_OBJ_FLAG_CLICKABLE);

        lv_obj_t *tip = lv_label_create(line2);
        if (tip) {
            const char *t = ap->encrypted ? "加密网络" : "开放网络";
            lv_label_set_text(tip, t);
            lv_obj_set_style_text_color(tip, lv_color_hex(0x808080), 0);
            lv_label_set_long_mode(tip, LV_LABEL_LONG_WRAP);
            lv_obj_clear_flag(tip, LV_OBJ_FLAG_CLICKABLE);
        }
    }
}

static void update_ap_list(void) {
    printf("[WiFi Page] update_ap_list, count=%d\n", g_page ? g_page->ap_count : -1);
    fflush(stdout);

    if (!g_page) {
        printf("[WiFi Page] update_ap_list: g_page is NULL\n");
        fflush(stdout);
        return;
    }

    clear_container();

    int count = g_page->ap_count;
    if (count == 0 || !g_page->ap_list) {
        lv_obj_t *empty = lv_label_create(g_page->container);
        if (empty) {
            if (!network_wifi_is_enabled()) {
                lv_label_set_text(empty, "WiFi 已关闭");
            } else if (g_page->is_scanning) {
                lv_label_set_text(empty, "正在扫描...");
            } else {
                lv_label_set_text(empty, "未发现网络");
            }
            lv_obj_set_width(empty, lv_pct(100));
            lv_label_set_long_mode(empty, LV_LABEL_LONG_WRAP);
            lv_obj_set_style_text_align(empty, LV_TEXT_ALIGN_CENTER, 0);
            lv_obj_clear_flag(empty, LV_OBJ_FLAG_CLICKABLE);
        }
        return;
    }

    wifi_ap_t *sorted = malloc(count * sizeof(wifi_ap_t));
    if (!sorted) return;
    memcpy(sorted, g_page->ap_list, count * sizeof(wifi_ap_t));
    qsort(sorted, count, sizeof(wifi_ap_t), ap_cmp);

    for (int i = 0; i < count; i++) {
        add_ap_item(&sorted[i]);
    }
    free(sorted);
}

static void set_state_text(const char *text) {
    if (!g_page) return;
    if (g_page->status_label) {
        lv_label_set_text(g_page->status_label, text);
        printf("[WiFi Page] Status: %s\n", text);
        fflush(stdout);
    }
    /* 如果设置为“WiFi开关”，取消任何待执行的恢复定时器 */
    if (text && strcmp(text, "WiFi开关") == 0) {
        if (g_page->reset_timer) {
            lv_timer_del(g_page->reset_timer);
            g_page->reset_timer = NULL;
        }
    }
}

/* ==================== 3秒后恢复状态定时器 ==================== */
static void reset_state_timer_cb(lv_timer_t *timer) {
    WifiPage *p = (WifiPage *)timer->user_data;
    if (p) {
        set_state_text("WiFi开关");
        p->reset_timer = NULL;
    }
    lv_timer_del(timer);
}

/* ==================== 删除网络对话框 ==================== */

static void show_delete_network_dialog(WifiPage *p, const char *ssid) {
    if (p->delete_dialog_showing) return;
    p->delete_dialog_showing = true;

    if (p->delete_target_ssid) free(p->delete_target_ssid);
    p->delete_target_ssid = strdup(ssid);

    lv_obj_t *scr = p->screen;

    p->delete_dialog_bg = lv_obj_create(scr);
    lv_obj_set_size(p->delete_dialog_bg, lv_pct(100), lv_pct(100));
    lv_obj_set_style_bg_color(p->delete_dialog_bg, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(p->delete_dialog_bg, LV_OPA_50, 0);
    lv_obj_clear_flag(p->delete_dialog_bg, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_move_foreground(p->delete_dialog_bg);

    p->delete_dialog_box = lv_obj_create(p->delete_dialog_bg);
    lv_obj_set_size(p->delete_dialog_box, 220, 140);
    lv_obj_align(p->delete_dialog_box, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(p->delete_dialog_box, lv_color_white(), 0);
    lv_obj_set_style_radius(p->delete_dialog_box, 10, 0);
    lv_obj_clear_flag(p->delete_dialog_box, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *label = lv_label_create(p->delete_dialog_box);
    lv_label_set_text_fmt(label, "要删除此网络吗？\n\"%s\"", ssid);
    lv_obj_set_width(label, lv_pct(100));
    lv_obj_align(label, LV_ALIGN_TOP_MID, 0, 10);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(label, lv_color_hex(0xFF4444), 0);

    lv_obj_t *btn_confirm = lv_btn_create(p->delete_dialog_box);
    lv_obj_set_size(btn_confirm, 80, 35);
    lv_obj_align(btn_confirm, LV_ALIGN_BOTTOM_LEFT, 10, -10);
    lv_obj_t *lbl_cfm = lv_label_create(btn_confirm);
    lv_label_set_text(lbl_cfm, "确认");
    lv_obj_center(lbl_cfm);
    lv_obj_set_style_bg_color(btn_confirm, lv_color_hex(0xFF4444), LV_PART_MAIN);
    lv_obj_set_style_text_color(lbl_cfm, lv_color_white(), 0);
    lv_obj_add_event_cb(btn_confirm, delete_network_confirm_cb, LV_EVENT_CLICKED, p);

    lv_obj_t *btn_cancel = lv_btn_create(p->delete_dialog_box);
    lv_obj_set_size(btn_cancel, 80, 35);
    lv_obj_align(btn_cancel, LV_ALIGN_BOTTOM_RIGHT, -10, -10);
    lv_obj_t *lbl_ccl = lv_label_create(btn_cancel);
    lv_label_set_text(lbl_ccl, "取消");
    lv_obj_center(lbl_ccl);
    lv_obj_set_style_bg_color(btn_cancel, lv_color_hex(0xCCCCCC), LV_PART_MAIN);
    lv_obj_add_event_cb(btn_cancel, delete_network_cancel_cb, LV_EVENT_CLICKED, p);
}

static void hide_delete_network_dialog(WifiPage *p) {
    if (!p->delete_dialog_showing) return;
    p->delete_dialog_showing = false;
    if (p->delete_dialog_bg) {
        lv_obj_del(p->delete_dialog_bg);
        p->delete_dialog_bg = NULL;
        p->delete_dialog_box = NULL;
    }
    if (p->delete_target_ssid) {
        free(p->delete_target_ssid);
        p->delete_target_ssid = NULL;
    }
}

static void delete_network_confirm_cb(lv_event_t *e) {
    WifiPage *p = (WifiPage *)lv_event_get_user_data(e);
    if (!p || !p->delete_target_ssid) {
        hide_delete_network_dialog(p);
        return;
    }
    char *ssid = strdup(p->delete_target_ssid);
    hide_delete_network_dialog(p);

    int ret = network_wifi_remove_saved(ssid);
    free(ssid);
    if (ret == 0) {
        custom_toast_create("网络已删除");
        refresh_click(NULL);
    } else {
        custom_toast_create("删除失败");
    }
}

static void delete_network_cancel_cb(lv_event_t *e) {
    WifiPage *p = (WifiPage *)lv_event_get_user_data(e);
    if (!p) return;
    hide_delete_network_dialog(p);
}

/* ==================== 后台线程 ==================== */

static void *scan_thread(void *arg) {
    (void)arg;
    printf("[WiFi Page] Scan thread started\n");
    fflush(stdout);

    wifi_ap_t *aps = NULL;
    int count = 0;
    int ret = network_wifi_scan(&aps, &count);
    printf("[WiFi Page] Scan thread ret=%d, count=%d\n", ret, count);
    fflush(stdout);

    scan_result_t *result = malloc(sizeof(scan_result_t));
    if (result) {
        result->aps = aps;
        result->count = count;
        result->ret = ret;
        lv_async_call((lv_async_cb_t)on_scan_done, result);
    } else {
        free(aps);
        lv_async_call((lv_async_cb_t)on_scan_done, NULL);
    }

    return NULL;
}

static void on_scan_done(void *data) {
    scan_result_t *result = (scan_result_t *)data;
    int ret = -1;
    int count = 0;
    wifi_ap_t *aps = NULL;

    if (result) {
        ret = result->ret;
        count = result->count;
        aps = result->aps;
        free(result);
    } else {
        ret = -1;
    }

    printf("[WiFi Page] on_scan_done ret=%d, count=%d\n", ret, count);
    fflush(stdout);

    if (!g_page) {
        printf("[WiFi Page] on_scan_done: g_page is NULL\n");
        fflush(stdout);
        if (aps) free(aps);
        return;
    }

    g_page->is_scanning = false;

    if (ret != 0 || !aps || count == 0) {
        if (g_page->first_scan) {
            g_page->first_scan = false;
            printf("[WiFi Page] First scan empty, auto re-scan...\n");
            fflush(stdout);
            if (aps) free(aps);
            refresh_click(NULL);
            return;
        }
        if (ret != 0) {
            custom_toast_create("扫描失败");
        }
        if (aps) free(aps);
        if (g_page->ap_list) {
            free(g_page->ap_list);
            g_page->ap_list = NULL;
        }
        g_page->ap_count = 0;
        clear_container();
        lv_obj_t *empty = lv_label_create(g_page->container);
        if (empty) {
            lv_label_set_text(empty, "没有扫描到网络");
            lv_obj_set_width(empty, lv_pct(100));
            lv_label_set_long_mode(empty, LV_LABEL_LONG_WRAP);
            lv_obj_set_style_text_align(empty, LV_TEXT_ALIGN_CENTER, 0);
            lv_obj_clear_flag(empty, LV_OBJ_FLAG_CLICKABLE);
        }
        set_state_text("WiFi开关");
        return;
    }

    g_page->first_scan = false;

    if (g_page->ap_list) {
        free(g_page->ap_list);
    }
    g_page->ap_list = aps;
    g_page->ap_count = count;
    update_ap_list();
    /* 扫描完成，恢复状态（如果正在连接中，状态文字已在连接时设为“扫描中...”，这里不覆盖） */
    if (!g_page->is_connecting) {
        set_state_text("WiFi开关");
    }
}

static void *connect_thread(void *arg) {
    char *ssid = (char *)arg;
    printf("[WiFi Page] Connect thread for ssid: %s\n", ssid ? ssid : "NULL");
    fflush(stdout);

    connect_result_t *result = malloc(sizeof(connect_result_t));
    if (!result) {
        free(ssid);
        return NULL;
    }

    if (ssid) {
        strncpy(result->ssid, ssid, sizeof(result->ssid)-1);
    } else {
        result->ssid[0] = '\0';
    }
    strncpy(result->password, g_page->pending_password, sizeof(result->password)-1);

    int ret = -1;
    wifi_ap_t *ap = NULL;
    if (g_page) {
        for (int i = 0; i < g_page->ap_count; i++) {
            if (g_page->ap_list && strcmp(g_page->ap_list[i].ssid, ssid) == 0) {
                ap = &g_page->ap_list[i];
                break;
            }
        }
    }

    const char *saved_pw = network_wifi_get_saved_password(ssid);
    const char *password = g_page->pending_password[0] ? g_page->pending_password : saved_pw;

    if (ap && ap->encrypted && password && strlen(password) > 0) {
        ret = network_wifi_connect_secure(ssid, password);
    } else if (ap && !ap->encrypted) {
        ret = network_wifi_connect_open(ssid);
    } else if (ap && ap->encrypted && (!password || strlen(password) == 0)) {
        printf("[WiFi Page] Encrypted AP but no password\n");
        fflush(stdout);
        ret = -1;
    } else {
        if (password && strlen(password) > 0) {
            ret = network_wifi_connect_secure(ssid, password);
        } else {
            ret = -1;
        }
    }

    result->ret = ret;
    lv_async_call((lv_async_cb_t)on_connect_done, result);
    free(ssid);
    return NULL;
}

static void on_connect_done(void *data) {
    connect_result_t *result = (connect_result_t *)data;
    int ret = result->ret;
    printf("[WiFi Page] on_connect_done ret=%d\n", ret);
    fflush(stdout);

    if (!g_page) {
        printf("[WiFi Page] on_connect_done: g_page is NULL\n");
        fflush(stdout);
        free(result);
        return;
    }

    g_page->is_connecting = false;
    if (g_page->connecting_ssid) {
        free(g_page->connecting_ssid);
        g_page->connecting_ssid = NULL;
    }

    if (ret == 0) {
        if (strlen(result->password) > 0) {
            network_wifi_save(result->ssid, result->password);
        }
        custom_toast_create("连接成功");
        set_state_text("WiFi开关");
        refresh_click(NULL);
    } else {
        custom_toast_create("连接失败");
        set_state_text("WiFi开关");
    }
    free(result);
}