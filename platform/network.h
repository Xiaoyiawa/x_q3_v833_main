#ifndef PLATFORM_NETWORK_H
#define PLATFORM_NETWORK_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>

/* WiFi 网络信息结构体 */
typedef struct {
    char ssid[64];
    char bssid[18];
    int signal;              /* 信号强度（负数，如 -45） */
    char flags[128];         /* 加密标志，如 [WPA2-PSK-CCMP][ESS] */
    bool encrypted;          /* 是否有加密 */
    bool saved;              /* 是否已保存 */
} wifi_ap_t;

/* WiFi 状态 */
typedef enum {
    WIFI_STATE_DISABLED = 0,
    WIFI_STATE_SCANNING,
    WIFI_STATE_SCAN_DONE,
    WIFI_STATE_CONNECTING,
    WIFI_STATE_GETTING_IP,
    WIFI_STATE_CONNECTED,
    WIFI_STATE_FAILED
} wifi_state_t;

/* 初始化 WiFi 模块（加载驱动、启动 wpa_supplicant） */
int network_wifi_init(void);

/* 关闭 WiFi（kill wpa_supplicant 和 udhcpc） */
void network_wifi_deinit(void);

/* 检查 WiFi 是否已启用 */
bool network_wifi_is_enabled(void);

/* 扫描 WiFi 网络（阻塞约 10 秒） */
int network_wifi_scan(wifi_ap_t **aps, int *count);

/* 连接 WiFi（无加密） */
int network_wifi_connect_open(const char *ssid);

/* 连接 WiFi（WPA/WPA2 加密） */
int network_wifi_connect_secure(const char *ssid, const char *password);

/* 获取当前连接状态 */
wifi_state_t network_wifi_get_state(void);

/* 获取当前连接的 SSID */
const char *network_wifi_get_connected_ssid(void);

/* 获取当前 IP 地址 */
const char *network_wifi_get_ip(void);

/* 检查网络是否已保存 */
bool network_wifi_is_saved(const char *ssid);

/* 获取已保存网络的密码 */
const char *network_wifi_get_saved_password(const char *ssid);

/* 保存网络到配置文件 */
int network_wifi_save(const char *ssid, const char *password);

/* 删除已保存的网络（从 wifi_save.json 中移除） */
int network_wifi_remove_saved(const char *ssid);

/* 取消当前连接 */
void network_wifi_disconnect(void);

/* 获取最后一次错误的详细信息 */
const char *network_wifi_get_last_error(void);

#ifdef __cplusplus
}
#endif

#endif /* PLATFORM_NETWORK_H */