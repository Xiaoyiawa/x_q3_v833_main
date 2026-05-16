/* 
* 这个页面主要是为我设计的
* GT版比普通版开adb麻烦点
* 还不能开机自启
* 索性写了个这个
* 学了4个星期的c，但大部分还是ai帮忙的（
*/

#include "page_usb.h"

typedef struct {
    BasePage base;
    lv_obj_t *btn_http;
    lv_obj_t *btn_telnet;
    lv_obj_t *btn_adb;
    lv_obj_t *btn_wifi;
    lv_obj_t *label_ip;
    bool http_on;
    bool wifi_on;
} UsbPage;

static void http_click_cb(lv_event_t *e);
static void telnet_click_cb(lv_event_t *e);
static void adb_click_cb(lv_event_t *e);
static void wifi_click_cb(lv_event_t *e);
static void page_usb_destroy(void *p);
static void update_ip_label(lv_obj_t *label);

// 使用ps检测进程是否运行
static int is_process_running(const char *proc_name)
{
    char cmd[128];
    snprintf(cmd, sizeof(cmd), "ps | grep -E '\\b%s\\b' | grep -v grep > /dev/null 2>&1", proc_name);
    return system(cmd) == 0;
}

static void update_http_button(UsbPage *page)
{
    if (page->http_on) {
        lv_obj_set_style_bg_color(page->btn_http, lv_color_hex(0x4CAF50), LV_PART_MAIN);
        lv_obj_t *lbl = lv_obj_get_child(page->btn_http, 0);
        lv_label_set_text(lbl, "httpd(on)");
    } else {
        lv_obj_set_style_bg_color(page->btn_http, lv_color_hex(0x888888), LV_PART_MAIN);
        lv_obj_t *lbl = lv_obj_get_child(page->btn_http, 0);
        lv_label_set_text(lbl, "httpd");
    }
}

static void update_wifi_button(UsbPage *page)
{
    if (page->wifi_on) {
        lv_obj_set_style_bg_color(page->btn_wifi, lv_color_hex(0x4CAF50), LV_PART_MAIN);
        lv_obj_t *lbl = lv_obj_get_child(page->btn_wifi, 0);
        lv_label_set_text(lbl, "WIFI(on)");
    } else {
        lv_obj_set_style_bg_color(page->btn_wifi, lv_color_hex(0x888888), LV_PART_MAIN);
        lv_obj_t *lbl = lv_obj_get_child(page->btn_wifi, 0);
        lv_label_set_text(lbl, "WIFI");
    }
}

// 刷新IP地址显示
static void update_ip_label(lv_obj_t *label)
{
    FILE *fp;
    char buffer[512];
    char ip[20] = "";
    int found = 0;

    fp = popen("ifconfig", "r");
    if (fp == NULL) {
        lv_label_set_text(label, "No network");
        return;
    }

    while (fgets(buffer, sizeof(buffer), fp) != NULL) {
        if ((strstr(buffer, "inet addr:") != NULL || strstr(buffer, "inet ") != NULL) &&
            strstr(buffer, "127.0.0.1") == NULL) {
            char *ip_start;
            if (strstr(buffer, "inet addr:") != NULL)
                ip_start = strstr(buffer, "inet addr:") + 10;
            else
                ip_start = strstr(buffer, "inet ") + 5;

            sscanf(ip_start, "%s", ip);
            found = 1;
            break;
        }
    }
    pclose(fp);

    if (found && strlen(ip) > 0)
        lv_label_set_text(label, ip);
    else
        lv_label_set_text(label, "No Connection");
}

static lv_obj_t * page_usb_create_ui(UsbPage *page)
{
    lv_obj_t * scr = lv_obj_create(lv_scr_act());
    lv_obj_remove_style_all(scr);
    lv_obj_set_size(scr, lv_pct(100), lv_pct(100));
    lv_obj_set_style_bg_color(scr, lv_color_white(), 0);

    lv_coord_t btn_w = 100;
    lv_coord_t btn_h = 50;

    lv_obj_t *btn_http = lv_btn_create(scr);
    lv_obj_set_size(btn_http, btn_w, btn_h);
    lv_obj_align(btn_http, LV_ALIGN_TOP_LEFT, 8, 4);
    lv_obj_t *lbl_http = lv_label_create(btn_http);
    lv_label_set_text(lbl_http, "httpd");
    lv_obj_center(lbl_http);
    lv_obj_add_event_cb(btn_http, http_click_cb, LV_EVENT_CLICKED, page);
    page->btn_http = btn_http;

    lv_obj_t *btn_telnet = lv_btn_create(scr);
    lv_obj_set_size(btn_telnet, btn_w, btn_h);
    lv_obj_align(btn_telnet, LV_ALIGN_TOP_RIGHT, -8, 4);
    lv_obj_t *lbl_telnet = lv_label_create(btn_telnet);
    lv_label_set_text(lbl_telnet, "telnetd");
    lv_obj_center(lbl_telnet);
    lv_obj_add_event_cb(btn_telnet, telnet_click_cb, LV_EVENT_CLICKED, page);
    page->btn_telnet = btn_telnet;

    lv_obj_t *btn_adb = lv_btn_create(scr);
    lv_obj_set_size(btn_adb, btn_w, btn_h);
    lv_obj_align(btn_adb, LV_ALIGN_TOP_RIGHT, -8, 59);
    lv_obj_t *lbl_adb = lv_label_create(btn_adb);
    lv_label_set_text(lbl_adb, "ADB");
    lv_obj_center(lbl_adb);
    lv_obj_add_event_cb(btn_adb, adb_click_cb, LV_EVENT_CLICKED, page);
    page->btn_adb = btn_adb;

    lv_obj_t *btn_wifi = lv_btn_create(scr);
    lv_obj_set_size(btn_wifi, btn_w, btn_h);
    lv_obj_align(btn_wifi, LV_ALIGN_TOP_LEFT, 8, 59);
    lv_obj_t *lbl_wifi = lv_label_create(btn_wifi);
    lv_label_set_text(lbl_wifi, "WIFI");
    lv_obj_center(lbl_wifi);
    lv_obj_add_event_cb(btn_wifi, wifi_click_cb, LV_EVENT_CLICKED, page);
    page->btn_wifi = btn_wifi;


    lv_obj_t *label_ip = lv_label_create(scr);
    lv_obj_align(label_ip, LV_ALIGN_BOTTOM_MID, 0, -10);
    lv_obj_set_style_text_align(label_ip, LV_TEXT_ALIGN_CENTER, 0);
    page->label_ip = label_ip;
    update_ip_label(label_ip);

    return scr;
}

static void http_click_cb(lv_event_t *e)
{
    UsbPage *page = (UsbPage *)lv_event_get_user_data(e);
    if (!page) return;

    if (!page->http_on) {
        system("cd / && python -m http.server --bind 0.0.0.0 80 &");
        page->http_on = true;
    } else {
        // 感谢豆包给的kill方案
        system("kill -9 $(ps | grep \"python.*http.server.*0.0.0.0.*80\" | grep -v grep | awk '{print $1}')");
        page->http_on = false;
    }
    update_http_button(page);
}

static void telnet_click_cb(lv_event_t *e)
{
    UsbPage *page = (UsbPage *)lv_event_get_user_data(e);
    if (!page) return;
    system("/mnt/UDISK/tools/bin/busybox_1 telnetd -f /mnt/UDISK/tools/config/telnet.txt -l /mnt/UDISK/tools/bin/bash -p 23 &");
    printf("telnetd restarted\n");
}

static void adb_click_cb(lv_event_t *e)
{
    UsbPage *page = (UsbPage *)lv_event_get_user_data(e);
    if (!page) return;
    // 我都设备因为上次不知道这么了被我干坏了，开adb只能usm和adb一起开
    system("/mnt/UDISK/tools/set_usb_mode init");
    usleep(3000000);
    system("/mnt/UDISK/tools/set_usb_mode adb,mass_storage");
    printf("ADB mode set\n");
    if (system("ps | grep '{S99usb}' | grep -v grep > /dev/null 2>&1") != 0) {
        system("/bin/sh /mnt/app/robot/shell/S99usb run &");
    }
}

static void wifi_click_cb(lv_event_t *e)
{
    UsbPage *page = (UsbPage *)lv_event_get_user_data(e);
    if (!page) return;

    if (!page->wifi_on) {

        // 先杀死wpa_supplicant和udhcpc
        system("killall -q wpa_supplicant udhcpc");
        usleep(300000);

        int driver_ok = !system("lsmod | grep -qE 'wlan|xradio'");

        // 检测一下驱动
        if (!driver_ok) {
            printf("wlan device not fonud!\n");
            printf("loading drivers...\n");
            system("insmod /lib/modules/4.9.118/xradio_mac.ko 2>/dev/null");
            system("insmod /lib/modules/4.9.118/xradio_core.ko 2>/dev/null");
            system("insmod /lib/modules/4.9.118/xradio_wlan.ko 2>/dev/null");
        } else {
            printf("wlan devices on\n");
        }

        //启动
        system("wpa_supplicant -Dnl80211 -i wlan0 -c /mnt/UDISK/tools/config/wpa_supplicant.conf -B");
        usleep(7000000);
        system("udhcpc -i wlan0 &");
        page->wifi_on = true;
    } else {
        // 获取wpa_supplicant和udhcpc的pid
        // 用kill -9杀死
        // 有时killall杀不掉
        system("kill -9 $(ps | grep wpa_supplicant | grep wlan0 | grep -v grep | awk '{print $1}')");
        printf("kill wpa_supplicant\n");
        usleep(300000);
        system("kill -9 $(ps | grep udhcpc | grep wlan0 | grep -v grep | awk '{print $1}')");
        printf("kill udhcpc\n");

        system("ifconfig wlan0 down");
        page->wifi_on = false;
    }
    update_wifi_button(page);
    // 刷新IP地址
    if (page->label_ip) update_ip_label(page->label_ip);
}

static void page_usb_destroy(void *p)
{
    UsbPage *page = (UsbPage *)p;
    if (page) {
        // 能睡了
        sys_set_dont_deep_sleep(false);
        sys_set_dont_timeout(false);
    }
}

BasePage * page_usb_create(void)
{
    UsbPage *page = malloc(sizeof(UsbPage));
    if (!page) return NULL;
    memset(page, 0, sizeof(UsbPage));

    // 不许睡！
    sys_set_dont_deep_sleep(true);
    sys_set_dont_timeout(true);

    // 检查进程状态
    page->http_on = is_process_running("python");
    page->wifi_on = is_process_running("wpa_supplicant");

    lv_obj_t * obj = page_usb_create_ui(page);
    page->base.obj = obj;
    page->base.on_destroy = page_usb_destroy;

    update_http_button(page);
    update_wifi_button(page);

    return (BasePage *)page;
}