#include "network.h"
#include "main.h"          /* 包含 fbd, dispd, powerd, homed 声明 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <fcntl.h>
#include <dirent.h>
#include <errno.h>
#include <time.h>
#include "../cJSON/cJSON.h"

/* ==================== 常量 ==================== */
#define WPA_CONF_FILE       "/tmp/dendro/wpa_supplicant.conf"
#define WPA_CTRL_DIR        "/var/run/wpa_supplicant"
#define SAVE_FILE           "./setting/wifi_save.json"
#define WPA_CLI             "wpa_cli"
#define WPA_SUPPLICANT      "wpa_supplicant"
#define UDHCPC              "udhcpc"

#define SCAN_TIMEOUT        10
#define CONNECT_TIMEOUT     15

/* ==================== 静态状态 ==================== */
static bool g_wifi_enabled = false;
static wifi_state_t g_state = WIFI_STATE_DISABLED;
static char g_connected_ssid[64] = "";
static char g_ip_address[16] = "";
static pid_t g_udhcpc_pid = -1;
static char g_last_error[128] = "";

/* ==================== 辅助函数 ==================== */

/* 执行命令并获取输出（popen），始终返回非空字符串（失败时返回空字符串） */
static char* exec_cmd(const char *cmd) {
    printf("[Network] exec_cmd: %s\n", cmd);
    fflush(stdout);

    FILE *fp = popen(cmd, "r");
    if (!fp) {
        printf("[Network] popen failed for: %s\n", cmd);
        fflush(stdout);
        return strdup("");
    }
    char *result = malloc(4096);
    if (!result) {
        printf("[Network] malloc failed in exec_cmd\n");
        fflush(stdout);
        pclose(fp);
        return strdup("");
    }
    result[0] = '\0';
    char buf[256];
    while (fgets(buf, sizeof(buf), fp)) {
        strncat(result, buf, 4095 - strlen(result) - 1);
    }
    pclose(fp);
    printf("[Network] exec_cmd output: %s\n", result);
    fflush(stdout);
    return result;
}

/* 执行命令（不获取输出） */
static int exec_cmd_simple(const char *cmd) {
    printf("[Network] exec_cmd_simple: %s\n", cmd);
    fflush(stdout);
    int ret = system(cmd);
    printf("[Network] exec_cmd_simple returned: %d\n", ret);
    fflush(stdout);
    return ret;
}

/* 检查进程是否运行 */
static bool is_process_running(const char *name) {
    char cmd[64];
    snprintf(cmd, sizeof(cmd), "pidof %s", name);
    char *out = exec_cmd(cmd);
    bool running = (out && strlen(out) > 0 && out[0] != '\0');
    free(out);
    printf("[Network] is_process_running(%s): %d\n", name, running);
    fflush(stdout);
    return running;
}

/* 创建 wpa_supplicant.conf 模板 */
static int create_wpa_conf_template(void) {
    printf("[Network] Creating wpa_supplicant.conf template\n");
    fflush(stdout);
    FILE *fp = fopen(WPA_CONF_FILE, "w");
    if (!fp) {
        printf("[Network] Failed to create %s\n", WPA_CONF_FILE);
        fflush(stdout);
        return -1;
    }
    fprintf(fp, "ctrl_interface=%s\n", WPA_CTRL_DIR);
    fprintf(fp, "update_config=1\n");
    fclose(fp);
    printf("[Network] Template created\n");
    fflush(stdout);
    return 0;
}

/* 根据 SSID 和密码生成完整的配置文件内容（返回动态分配的字符串） */
static char* generate_wpa_conf(const char *ssid, const char *password, bool encrypted) {
    printf("[Network] generate_wpa_conf: ssid=%s, encrypted=%d\n", ssid, encrypted);
    fflush(stdout);
    char *conf = malloc(1024);
    if (!conf) {
        printf("[Network] malloc failed in generate_wpa_conf\n");
        fflush(stdout);
        return NULL;
    }
    if (encrypted && password) {
        snprintf(conf, 1024,
                 "ctrl_interface=%s\n"
                 "update_config=1\n"
                 "network={\n"
                 "    ssid=\"%s\"\n"
                 "    psk=\"%s\"\n"
                 "}\n",
                 WPA_CTRL_DIR, ssid, password);
    } else {
        snprintf(conf, 1024,
                 "ctrl_interface=%s\n"
                 "update_config=1\n"
                 "network={\n"
                 "    ssid=\"%s\"\n"
                 "    key_mgmt=NONE\n"
                 "}\n",
                 WPA_CTRL_DIR, ssid);
    }
    printf("[Network] Generated config:\n%s\n", conf);
    fflush(stdout);
    return conf;
}

/* 写入配置文件 */
static int write_wpa_conf(const char *ssid, const char *password, bool encrypted) {
    printf("[Network] write_wpa_conf: ssid=%s\n", ssid);
    fflush(stdout);
    char *content = generate_wpa_conf(ssid, password, encrypted);
    if (!content) return -1;
    FILE *fp = fopen(WPA_CONF_FILE, "w");
    if (!fp) {
        printf("[Network] Failed to open %s for writing\n", WPA_CONF_FILE);
        fflush(stdout);
        free(content);
        return -1;
    }
    fprintf(fp, "%s", content);
    fclose(fp);
    free(content);
    printf("[Network] Config written successfully\n");
    fflush(stdout);
    return 0;
}

/* 从 JSON 读取已保存网络列表 */
static cJSON* load_saved_networks(void) {
    printf("[Network] load_saved_networks\n");
    fflush(stdout);
    FILE *fp = fopen(SAVE_FILE, "r");
    if (!fp) {
        printf("[Network] No saved networks file\n");
        fflush(stdout);
        return NULL;
    }
    fseek(fp, 0, SEEK_END);
    long sz = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    char *json_str = malloc(sz + 1);
    if (!json_str) {
        printf("[Network] malloc failed for JSON\n");
        fflush(stdout);
        fclose(fp);
        return NULL;
    }
    fread(json_str, 1, sz, fp);
    json_str[sz] = '\0';
    fclose(fp);
    cJSON *root = cJSON_Parse(json_str);
    free(json_str);
    printf("[Network] load_saved_networks: %s\n", root ? "OK" : "FAIL");
    fflush(stdout);
    return root;
}

/* 保存网络到 JSON */
static int save_network_to_json(const char *ssid, const char *password) {
    printf("[Network] save_network_to_json: ssid=%s\n", ssid);
    fflush(stdout);
    cJSON *root = load_saved_networks();
    if (!root) {
        root = cJSON_CreateObject();
        cJSON_AddArrayToObject(root, "wifi_save");
    }
    cJSON *arr = cJSON_GetObjectItem(root, "wifi_save");
    if (!arr) {
        arr = cJSON_AddArrayToObject(root, "wifi_save");
    }
    int found = 0;
    cJSON *item;
    cJSON_ArrayForEach(item, arr) {
        cJSON *s = cJSON_GetObjectItem(item, "ssid");
        if (s && cJSON_IsString(s) && strcmp(s->valuestring, ssid) == 0) {
            cJSON *p = cJSON_GetObjectItem(item, "passwd");
            if (p) cJSON_SetValuestring(p, password);
            found = 1;
            break;
        }
    }
    if (!found) {
        cJSON *new_item = cJSON_CreateObject();
        cJSON_AddStringToObject(new_item, "ssid", ssid);
        cJSON_AddStringToObject(new_item, "passwd", password);
        cJSON_AddItemToArray(arr, new_item);
    }
    char *json_str = cJSON_Print(root);
    cJSON_Delete(root);
    if (!json_str) {
        printf("[Network] cJSON_Print failed\n");
        fflush(stdout);
        return -1;
    }
    FILE *fp = fopen(SAVE_FILE, "w");
    if (!fp) {
        printf("[Network] Failed to open %s for writing\n", SAVE_FILE);
        fflush(stdout);
        free(json_str);
        return -1;
    }
    fprintf(fp, "%s", json_str);
    fclose(fp);
    free(json_str);
    printf("[Network] save_network_to_json OK\n");
    fflush(stdout);
    return 0;
}

/* ==================== SSID 解析函数 ==================== */

/**
 * 解析 wpa_cli 输出的 SSID 字段
 * 将形如 "\xf0\x9f\x94\xa5..." 的十六进制转义序列转换为 UTF-8 字符串
 * 如果全是 \x00，则返回 "隐藏的网络"
 * 返回值：动态分配的字符串，调用者需 free
 */
static char* parse_ssid_field(const char *raw_ssid) {
    if (!raw_ssid) return strdup("");
    
    int is_all_zero = 1;
    int has_hex_escape = 0;
    size_t len = strlen(raw_ssid);
    
    if (len == 0) return strdup("");
    
    for (size_t i = 0; i < len; i++) {
        if (raw_ssid[i] == '\\' && raw_ssid[i+1] == 'x' && i+3 < len) {
            has_hex_escape = 1;
            if (raw_ssid[i+2] == '0' && raw_ssid[i+3] == '0') {
                i += 3;
                continue;
            } else {
                is_all_zero = 0;
                break;
            }
        } else if (raw_ssid[i] != '\0' && raw_ssid[i] != ' ' && raw_ssid[i] != '\t' && 
                   raw_ssid[i] != '\n' && raw_ssid[i] != '\r') {
            is_all_zero = 0;
            break;
        }
    }
    
    if (is_all_zero && len > 0) {
        return strdup("隐藏的网络");
    }
    
    if (!has_hex_escape) {
        char *result = strdup(raw_ssid);
        if (!result) return strdup("");
        char *start = result;
        while (*start == ' ' || *start == '\t') start++;
        char *end = start + strlen(start) - 1;
        while (end > start && (*end == ' ' || *end == '\t' || *end == '\n' || *end == '\r')) end--;
        *(end + 1) = '\0';
        if (start != result) {
            memmove(result, start, strlen(start) + 1);
        }
        return result;
    }
    
    char *output = malloc(len + 1);
    if (!output) return strdup(raw_ssid);
    size_t out_idx = 0;
    
    for (size_t i = 0; i < len && out_idx < len; i++) {
        if (raw_ssid[i] == '\\' && raw_ssid[i+1] == 'x' && i+3 < len) {
            char hex_byte[3] = {raw_ssid[i+2], raw_ssid[i+3], '\0'};
            unsigned char byte = (unsigned char)strtol(hex_byte, NULL, 16);
            if (byte != 0) {
                output[out_idx++] = (char)byte;
            }
            i += 3;
        } else {
            output[out_idx++] = raw_ssid[i];
        }
    }
    output[out_idx] = '\0';
    
    if (out_idx == 0) {
        free(output);
        return strdup("隐藏的网络");
    }
    
    return output;
}

/* ==================== 公共接口实现 ==================== */

int network_wifi_init(void) {
    printf("[Network] network_wifi_init called\n");
    fflush(stdout);

    if (network_wifi_is_enabled()) {
        printf("[Network] WiFi already enabled\n");
        fflush(stdout);
        return 0;
    }

    printf("[Network] Loading drivers...\n");
    fflush(stdout);
    exec_cmd_simple("insmod /lib/modules/4.9.118/xradio_mac.ko");
    exec_cmd_simple("insmod /lib/modules/4.9.118/xradio_core.ko");
    exec_cmd_simple("insmod /lib/modules/4.9.118/xradio_wlan.ko");
    usleep(200000);

    if (create_wpa_conf_template() != 0) {
        snprintf(g_last_error, sizeof(g_last_error), "创建配置文件失败");
        printf("[Network] Failed to create config template\n");
        fflush(stdout);
        return -1;
    }

    printf("[Network] Starting wpa_supplicant...\n");
    fflush(stdout);
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "%s -B -i wlan0 -c %s", WPA_SUPPLICANT, WPA_CONF_FILE);
    int ret = exec_cmd_simple(cmd);
    if (ret != 0) {
        snprintf(g_last_error, sizeof(g_last_error), "启动 wpa_supplicant 失败");
        printf("[Network] wpa_supplicant start failed, ret=%d\n", ret);
        fflush(stdout);
        return -1;
    }
    usleep(500000);

    g_wifi_enabled = true;
    g_state = WIFI_STATE_SCAN_DONE;
    snprintf(g_last_error, sizeof(g_last_error), "");
    printf("[Network] WiFi initialized successfully\n");
    fflush(stdout);
    return 0;
}

void network_wifi_deinit(void) {
    printf("[Network] network_wifi_deinit called\n");
    fflush(stdout);
    if (!g_wifi_enabled) {
        printf("[Network] WiFi not enabled, nothing to deinit\n");
        fflush(stdout);
        return;
    }
    exec_cmd_simple("killall udhcpc 2>/dev/null");
    exec_cmd_simple("killall wpa_supplicant 2>/dev/null");
    g_wifi_enabled = false;
    g_state = WIFI_STATE_DISABLED;
    g_connected_ssid[0] = '\0';
    g_ip_address[0] = '\0';
    snprintf(g_last_error, sizeof(g_last_error), "");
    printf("[Network] WiFi deinitialized\n");
    fflush(stdout);
}

bool network_wifi_is_enabled(void) {
    printf("[Network] network_wifi_is_enabled called\n");
    fflush(stdout);
    char *out = exec_cmd("pidof wpa_supplicant");
    bool running = false;
    if (out) {
        running = (strlen(out) > 0 && out[0] != '\0');
        free(out);
    } else {
        printf("[Network] exec_cmd returned NULL, treat as not running\n");
        fflush(stdout);
    }
    if (running != g_wifi_enabled) {
        g_wifi_enabled = running;
        if (!running) {
            g_state = WIFI_STATE_DISABLED;
            g_connected_ssid[0] = '\0';
            g_ip_address[0] = '\0';
        } else {
            g_state = WIFI_STATE_SCAN_DONE;
        }
    }
    printf("[Network] WiFi enabled: %d\n", g_wifi_enabled);
    fflush(stdout);
    return g_wifi_enabled;
}

int network_wifi_scan(wifi_ap_t **aps, int *count) {
    printf("[Network] network_wifi_scan called\n");
    fflush(stdout);

    if (!g_wifi_enabled) {
        snprintf(g_last_error, sizeof(g_last_error), "WiFi 未开启");
        printf("[Network] WiFi not enabled, scan aborted\n");
        fflush(stdout);
        return -1;
    }

    g_state = WIFI_STATE_SCANNING;
    printf("[Network] Starting scan...\n");
    fflush(stdout);
    exec_cmd_simple("wpa_cli -i wlan0 scan");

    int wait_count = 0;
    while (wait_count < SCAN_TIMEOUT * 2) {
        char *out = exec_cmd("wpa_cli -i wlan0 scan_results 2>/dev/null");
        if (out) {
            if (strstr(out, "bssid / frequency") != NULL) {
                free(out);
                break;
            }
            free(out);
        }
        usleep(500000);
        wait_count++;
    }

    char *result = exec_cmd("wpa_cli -i wlan0 scan_results 2>/dev/null");
    if (!result || strlen(result) == 0) {
        snprintf(g_last_error, sizeof(g_last_error), "扫描无结果");
        printf("[Network] No scan results\n");
        fflush(stdout);
        free(result);
        return -1;
    }

    wifi_ap_t *ap_list = NULL;
    int ap_count = 0;
    char *line = strtok(result, "\n");
    while (line) {
        if (strstr(line, "bssid / frequency") != NULL ||
            strstr(line, "Selected interface") != NULL) {
            line = strtok(NULL, "\n");
            continue;
        }
        char bssid[20] = "", freq[10] = "", signal[10] = "", flags[128] = "", ssid[64] = "";
        char *fields[6];
        int field_count = 0;
        char *saveptr;
        char *tok = strtok_r(line, "\t", &saveptr);
        while (tok && field_count < 6) {
            fields[field_count++] = tok;
            tok = strtok_r(NULL, "\t", &saveptr);
        }
        if (field_count >= 5) {
            strncpy(bssid, fields[0], sizeof(bssid)-1);
            strncpy(freq, fields[1], sizeof(freq)-1);
            strncpy(signal, fields[2], sizeof(signal)-1);
            strncpy(flags, fields[3], sizeof(flags)-1);
            strncpy(ssid, fields[4], sizeof(ssid)-1);
            if (strlen(ssid) > 0 && strcmp(ssid, "SSID") != 0) {
                ap_list = realloc(ap_list, (ap_count + 1) * sizeof(wifi_ap_t));
                wifi_ap_t *ap = &ap_list[ap_count];
                
                char *parsed_ssid = parse_ssid_field(ssid);
                strncpy(ap->ssid, parsed_ssid, sizeof(ap->ssid)-1);
                free(parsed_ssid);
                
                strncpy(ap->bssid, bssid, sizeof(ap->bssid)-1);
                ap->signal = atoi(signal);
                strncpy(ap->flags, flags, sizeof(ap->flags)-1);
                ap->encrypted = (strstr(flags, "WPA") != NULL || strstr(flags, "WEP") != NULL);
                ap->saved = network_wifi_is_saved(ap->ssid);
                ap_count++;
                printf("[Network] Found AP: %s, signal %d, encrypted %d\n", ap->ssid, ap->signal, ap->encrypted);
                fflush(stdout);
            }
        }
        line = strtok(NULL, "\n");
    }
    free(result);

    *aps = ap_list;
    *count = ap_count;
    g_state = WIFI_STATE_SCAN_DONE;
    snprintf(g_last_error, sizeof(g_last_error), "");
    printf("[Network] Scan done, found %d APs\n", ap_count);
    fflush(stdout);
    return 0;
}

/* 内部连接函数 */
static int do_connect(const char *ssid, const char *password, bool encrypted) {
    printf("[Network] do_connect: ssid=%s, encrypted=%d\n", ssid, encrypted);
    fflush(stdout);

    if (!g_wifi_enabled) {
        snprintf(g_last_error, sizeof(g_last_error), "WiFi 未开启");
        printf("[Network] WiFi not enabled, connect aborted\n");
        fflush(stdout);
        return -1;
    }

    g_state = WIFI_STATE_CONNECTING;
    strncpy(g_connected_ssid, ssid, sizeof(g_connected_ssid)-1);

    /* 停止当前 wpa_supplicant */
    printf("[Network] Stopping existing wpa_supplicant...\n");
    fflush(stdout);
    exec_cmd_simple("killall wpa_supplicant 2>/dev/null");
    usleep(200000);

    /* 写入配置文件 */
    if (write_wpa_conf(ssid, password, encrypted) != 0) {
        snprintf(g_last_error, sizeof(g_last_error), "写入配置文件失败");
        printf("[Network] write_wpa_conf failed\n");
        fflush(stdout);
        g_state = WIFI_STATE_FAILED;
        return -1;
    }

    /* 启动 wpa_supplicant */
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "%s -B -i wlan0 -c %s", WPA_SUPPLICANT, WPA_CONF_FILE);
    if (exec_cmd_simple(cmd) != 0) {
        snprintf(g_last_error, sizeof(g_last_error), "启动 wpa_supplicant 失败");
        printf("[Network] wpa_supplicant start failed\n");
        fflush(stdout);
        g_state = WIFI_STATE_FAILED;
        return -1;
    }
    usleep(500000);

    /* 等待连接成功（最多 15 秒） */
    int wait_count = 0;
    while (wait_count < CONNECT_TIMEOUT * 2) {
        char *status = exec_cmd("wpa_cli -i wlan0 status 2>/dev/null");
        if (status) {
            if (strstr(status, "wpa_state=COMPLETED") != NULL) {
                free(status);
                break;
            }
            free(status);
        }
        usleep(500000);
        wait_count++;
    }

    /* 检查最终状态 */
    char *status = exec_cmd("wpa_cli -i wlan0 status 2>/dev/null");
    if (!status || strstr(status, "wpa_state=COMPLETED") == NULL) {
        snprintf(g_last_error, sizeof(g_last_error), "连接失败");
        printf("[Network] Connection failed (wpa_state not COMPLETED)\n");
        fflush(stdout);
        free(status);
        g_state = WIFI_STATE_FAILED;
        return -1;
    }
    free(status);
    printf("[Network] wpa_state=COMPLETED\n");
    fflush(stdout);

    /* 连接成功 */
    g_state = WIFI_STATE_CONNECTED;
    snprintf(g_last_error, sizeof(g_last_error), "");

    /* 异步获取 IP（后台运行 udhcpc，不阻塞） */
    pid_t pid = fork();
    if (pid == 0) {
        close(fbd);
        close(dispd);
        close(powerd);
        close(homed);
        close(0);
        close(1);
        close(2);
        setsid();
        execlp("udhcpc", "udhcpc", "-i", "wlan0", "-R", "-H", "AlphaEgg", NULL);
        exit(0);
    } else if (pid > 0) {
        g_udhcpc_pid = pid;
    }

    /* 尝试读取已有 IP */
    char *ip_out = exec_cmd("ifconfig wlan0 2>/dev/null | grep 'inet addr:'");
    if (!ip_out) {
        ip_out = exec_cmd("ifconfig wlan0 2>/dev/null | grep 'inet '");
    }
    if (ip_out) {
        char *ip_start = strstr(ip_out, "inet addr:");
        if (!ip_start) ip_start = strstr(ip_out, "inet ");
        if (ip_start) {
            ip_start += ip_start[0] == 'i' ? 10 : 5;
            sscanf(ip_start, "%s", g_ip_address);
        }
        free(ip_out);
    }

    printf("[Network] Connection successful, IP: %s\n", g_ip_address);
    fflush(stdout);

    /* 异步执行 ntpd 校准时间 */
    pid_t pid_ntp = fork();
    if (pid_ntp == 0) {
        close(fbd);
        close(dispd);
        close(powerd);
        close(homed);
        close(0);
        close(1);
        close(2);
        setsid();
        execlp("./tools/busybox_1", "busybox_1", "ntpd", "-p", "ntp.aliyun.com", "-qNn", NULL);
        exit(0);
    } else if (pid_ntp > 0) {
        printf("[Network] NTPD started in background (pid=%d)\n", pid_ntp);
        fflush(stdout);
    }

    return 0;
}

int network_wifi_connect_open(const char *ssid) {
    printf("[Network] network_wifi_connect_open: %s\n", ssid);
    fflush(stdout);
    return do_connect(ssid, NULL, false);
}

int network_wifi_connect_secure(const char *ssid, const char *password) {
    printf("[Network] network_wifi_connect_secure: %s\n", ssid);
    fflush(stdout);
    return do_connect(ssid, password, true);
}

wifi_state_t network_wifi_get_state(void) {
    return g_state;
}

const char *network_wifi_get_connected_ssid(void) {
    return g_connected_ssid;
}

const char *network_wifi_get_ip(void) {
    return g_ip_address;
}

const char *network_wifi_get_last_error(void) {
    return g_last_error;
}

bool network_wifi_is_saved(const char *ssid) {
    printf("[Network] network_wifi_is_saved: %s\n", ssid);
    fflush(stdout);
    cJSON *root = load_saved_networks();
    if (!root) return false;
    cJSON *arr = cJSON_GetObjectItem(root, "wifi_save");
    if (!arr) { cJSON_Delete(root); return false; }
    cJSON *item;
    cJSON_ArrayForEach(item, arr) {
        cJSON *s = cJSON_GetObjectItem(item, "ssid");
        if (s && cJSON_IsString(s) && strcmp(s->valuestring, ssid) == 0) {
            cJSON_Delete(root);
            printf("[Network] Network is saved\n");
            fflush(stdout);
            return true;
        }
    }
    cJSON_Delete(root);
    printf("[Network] Network not saved\n");
    fflush(stdout);
    return false;
}

const char *network_wifi_get_saved_password(const char *ssid) {
    static char password[128] = "";
    printf("[Network] network_wifi_get_saved_password: %s\n", ssid);
    fflush(stdout);
    cJSON *root = load_saved_networks();
    if (!root) return NULL;
    cJSON *arr = cJSON_GetObjectItem(root, "wifi_save");
    if (!arr) { cJSON_Delete(root); return NULL; }
    cJSON *item;
    cJSON_ArrayForEach(item, arr) {
        cJSON *s = cJSON_GetObjectItem(item, "ssid");
        if (s && cJSON_IsString(s) && strcmp(s->valuestring, ssid) == 0) {
            cJSON *p = cJSON_GetObjectItem(item, "passwd");
            if (p && cJSON_IsString(p)) {
                strncpy(password, p->valuestring, sizeof(password)-1);
                cJSON_Delete(root);
                printf("[Network] Found saved password\n");
                fflush(stdout);
                return password;
            }
        }
    }
    cJSON_Delete(root);
    printf("[Network] No saved password\n");
    fflush(stdout);
    return NULL;
}

int network_wifi_save(const char *ssid, const char *password) {
    printf("[Network] network_wifi_save: %s\n", ssid);
    fflush(stdout);
    return save_network_to_json(ssid, password);
}

int network_wifi_remove_saved(const char *ssid) {
    printf("[Network] network_wifi_remove_saved: %s\n", ssid);
    fflush(stdout);

    cJSON *root = load_saved_networks();
    if (!root) {
        snprintf(g_last_error, sizeof(g_last_error), "没有已保存的网络");
        printf("[Network] No saved networks file\n");
        fflush(stdout);
        return -1;
    }

    cJSON *arr = cJSON_GetObjectItem(root, "wifi_save");
    if (!arr) {
        cJSON_Delete(root);
        return -1;
    }

    int found = 0;
    cJSON *item;
    cJSON_ArrayForEach(item, arr) {
        cJSON *s = cJSON_GetObjectItem(item, "ssid");
        if (s && cJSON_IsString(s) && strcmp(s->valuestring, ssid) == 0) {
            found = 1;
            break;
        }
    }

    if (!found) {
        cJSON_Delete(root);
        snprintf(g_last_error, sizeof(g_last_error), "网络 '%s' 未找到", ssid);
        printf("[Network] SSID not found: %s\n", ssid);
        fflush(stdout);
        return -1;
    }

    cJSON *new_arr = cJSON_CreateArray();
    if (!new_arr) {
        cJSON_Delete(root);
        return -1;
    }

    cJSON_ArrayForEach(item, arr) {
        cJSON *s = cJSON_GetObjectItem(item, "ssid");
        if (s && cJSON_IsString(s) && strcmp(s->valuestring, ssid) != 0) {
            cJSON *new_item = cJSON_Duplicate(item, 1);
            if (new_item) {
                cJSON_AddItemToArray(new_arr, new_item);
            }
        }
    }

    cJSON_ReplaceItemInObject(root, "wifi_save", new_arr);

    char *json_str = cJSON_Print(root);
    cJSON_Delete(root);

    if (!json_str) {
        snprintf(g_last_error, sizeof(g_last_error), "保存失败");
        printf("[Network] cJSON_Print failed\n");
        fflush(stdout);
        return -1;
    }

    FILE *fp = fopen(SAVE_FILE, "w");
    if (!fp) {
        snprintf(g_last_error, sizeof(g_last_error), "打开文件失败");
        printf("[Network] Failed to open %s for writing\n", SAVE_FILE);
        fflush(stdout);
        free(json_str);
        return -1;
    }

    fprintf(fp, "%s", json_str);
    fclose(fp);
    free(json_str);

    snprintf(g_last_error, sizeof(g_last_error), "");
    printf("[Network] Removed saved network: %s\n", ssid);
    fflush(stdout);
    return 0;
}

void network_wifi_disconnect(void) {
    printf("[Network] network_wifi_disconnect\n");
    fflush(stdout);
    exec_cmd_simple("wpa_cli -i wlan0 disconnect 2>/dev/null");
    g_state = WIFI_STATE_SCAN_DONE;
    g_connected_ssid[0] = '\0';
    g_ip_address[0] = '\0';
    snprintf(g_last_error, sizeof(g_last_error), "");
}