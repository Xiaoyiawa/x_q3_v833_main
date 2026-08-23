#include "page_ai.h"
#include "../cJSON/cJSON.h"
#include "views/ime_helper.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <curl/curl.h>
#include <dirent.h>
#include <unistd.h>

/* ==================== 常量 ==================== */
#define CONFIG_DIR      "./setting/ai_config/"
#define SCREEN_W        240
#define SCREEN_H        240

#define TOP_BAR_H       30
#define BOT_BAR_H       28
#define INPUT_W         200
#define SEND_W          40
#define GAP             2

#define CHAT_Y          (TOP_BAR_H)
#define CHAT_H_NORMAL   (SCREEN_H - TOP_BAR_H - BOT_BAR_H)

#define CURL_BUFFER_SIZE (4 * 1024 * 1024)
#define MAX_AI_BUFFER_SIZE (4 * 1024 * 1024)

/* ==================== 配置项 ==================== */
typedef struct {
    char path[512];
    char name[64];
    char api_url[256];
    char api_key[128];
    char model[64];
    char system_prompt[512];
    int  max_tokens;
    float temperature;
    float top_p;
    char response_format_type[32];
    char user_id[64];
    int  valid;
} ConfigItem;

/* ==================== 全局配置 ==================== */
static ConfigItem *g_configs = NULL;
static int g_config_count = 0;
static int g_current_idx = -1;

static char g_api_url[256]    = "";
static char g_api_key[128]    = "";
static char g_model[64]       = "";
static char g_system_prompt[512] = "";
static int  g_max_tokens      = 0;
static float g_temperature    = -1.0f;
static float g_top_p          = -1.0f;
static char  g_response_format_type[32] = "";
static char  g_user_id[64]    = "";

/* ==================== 页面状态 ==================== */
typedef struct {
    lv_obj_t *screen;
    lv_obj_t *chat_container;
    lv_obj_t *input;
    lv_obj_t *send_btn;
    lv_obj_t *dropdown;
    bool waiting;
    bool ai_streaming;
    lv_obj_t *ai_label;
    char *ai_accumulated;     // 预分配 4MB
} AIPage;

static AIPage *g_page = NULL;

/* ==================== 流式队列 ==================== */
typedef struct chunk_node {
    char *data;
    struct chunk_node *next;
} chunk_node_t;

static chunk_node_t *g_queue_head = NULL;
static chunk_node_t *g_queue_tail = NULL;
static pthread_mutex_t g_queue_mutex = PTHREAD_MUTEX_INITIALIZER;
static int g_queue_len = 0;
static int g_async_pending = 0;

/* ==================== 函数声明 ==================== */
static void ai_back_cb(lv_event_t *e);
static void send_cb(lv_event_t *e);
static void dropdown_cb(lv_event_t *e);
static void scroll_bottom(void);
static void set_waiting(bool w);
static void *api_thread(void *arg);
static void on_api_response_ui_cb(void *data);
static size_t curl_write_cb(void *ptr, size_t size, size_t nmemb, void *userdata);

/* 聊天操作 */
static void add_message(const char *text, lv_color_t color);
static void add_user_message(const char *text);
static void add_warning(const char *text);
static void add_ai_message_start(void);
static void update_ai_message(const char *full_text);
static void finish_ai_message(void);

/* 队列操作 */
static void enqueue_chunk(const char *chunk);
static void clear_queue(void);
static void process_queue(void);

/* ==================== 配置扫描与加载 ==================== */
static int scan_configs(void) {
    DIR *dir = opendir(CONFIG_DIR);
    if (!dir) {
        printf("[AI] Config dir not found: %s\n", CONFIG_DIR);
        return 0;
    }

    struct dirent *entry;
    int count = 0;
    ConfigItem *items = NULL;

    printf("[AI] Scanning config directory: %s\n", CONFIG_DIR);

    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type != DT_REG) continue;
        if (strncmp(entry->d_name, "config_", 7) != 0) continue;
        char *dot = strrchr(entry->d_name, '.');
        if (!dot || strcmp(dot, ".json") != 0) continue;

        char full_path[512];
        snprintf(full_path, sizeof(full_path), "%s%s", CONFIG_DIR, entry->d_name);
        if (access(full_path, R_OK) != 0) continue;

        FILE *f = fopen(full_path, "rb");
        if (!f) continue;
        fseek(f, 0, SEEK_END);
        long sz = ftell(f);
        fseek(f, 0, SEEK_SET);
        char *json_str = malloc(sz + 1);
        fread(json_str, 1, sz, f);
        json_str[sz] = '\0';
        fclose(f);

        cJSON *root = cJSON_Parse(json_str);
        free(json_str);
        if (!root) continue;

        const cJSON *key   = cJSON_GetObjectItem(root, "api_key");
        const cJSON *url   = cJSON_GetObjectItem(root, "api_url");
        const cJSON *model = cJSON_GetObjectItem(root, "model");
        const cJSON *sp    = cJSON_GetObjectItem(root, "system_prompt");
        const cJSON *max_tokens = cJSON_GetObjectItem(root, "max_tokens");
        const cJSON *temperature = cJSON_GetObjectItem(root, "temperature");
        const cJSON *top_p = cJSON_GetObjectItem(root, "top_p");
        const cJSON *rf = cJSON_GetObjectItem(root, "response_format");
        const cJSON *uid = cJSON_GetObjectItem(root, "user_id");

        int valid = 1;
        if (!key || !cJSON_IsString(key) || strlen(key->valuestring) == 0) valid = 0;
        if (!url || !cJSON_IsString(url) || strlen(url->valuestring) == 0) valid = 0;
        if (!model || !cJSON_IsString(model) || strlen(model->valuestring) == 0) valid = 0;

        char display[64];
        snprintf(display, sizeof(display), "%s", entry->d_name + 7);
        char *ext = strrchr(display, '.');
        if (ext) *ext = '\0';

        items = realloc(items, (count + 1) * sizeof(ConfigItem));
        ConfigItem *ci = &items[count];
        memset(ci, 0, sizeof(ConfigItem));
        snprintf(ci->path, sizeof(ci->path), "%s", full_path);
        snprintf(ci->name, sizeof(ci->name), "%s", display);
        ci->valid = valid;
        if (valid) {
            snprintf(ci->api_key, sizeof(ci->api_key), "%s", key->valuestring);
            snprintf(ci->api_url, sizeof(ci->api_url), "%s", url->valuestring);
            snprintf(ci->model, sizeof(ci->model), "%s", model->valuestring);
            if (sp && cJSON_IsString(sp))
                snprintf(ci->system_prompt, sizeof(ci->system_prompt), "%s", sp->valuestring);
            if (max_tokens && cJSON_IsNumber(max_tokens)) ci->max_tokens = max_tokens->valueint;
            if (temperature && cJSON_IsNumber(temperature)) ci->temperature = (float)temperature->valuedouble;
            if (top_p && cJSON_IsNumber(top_p)) ci->top_p = (float)top_p->valuedouble;
            if (rf && cJSON_IsObject(rf)) {
                const cJSON *type = cJSON_GetObjectItem(rf, "type");
                if (type && cJSON_IsString(type))
                    snprintf(ci->response_format_type, sizeof(ci->response_format_type), "%s", type->valuestring);
            }
            if (uid && cJSON_IsString(uid))
                snprintf(ci->user_id, sizeof(ci->user_id), "%s", uid->valuestring);
        }
        count++;
        cJSON_Delete(root);
        printf("[AI] Loaded config: %s (%s)\n", display, valid ? "valid" : "invalid");
    }
    closedir(dir);

    g_configs = items;
    g_config_count = count;
    printf("[AI] Total configs: %d\n", count);
    return count;
}

static void load_config_by_index(int idx) {
    if (idx < 0 || idx >= g_config_count || !g_configs[idx].valid) {
        printf("[AI] Invalid config index %d, clearing global settings\n", idx);
        g_api_url[0] = g_api_key[0] = g_model[0] = g_system_prompt[0] = '\0';
        g_max_tokens = 0;
        g_temperature = -1.0f;
        g_top_p = -1.0f;
        g_response_format_type[0] = '\0';
        g_user_id[0] = '\0';
        return;
    }
    ConfigItem *ci = &g_configs[idx];
    snprintf(g_api_url, sizeof(g_api_url), "%s", ci->api_url);
    snprintf(g_api_key, sizeof(g_api_key), "%s", ci->api_key);
    snprintf(g_model, sizeof(g_model), "%s", ci->model);
    snprintf(g_system_prompt, sizeof(g_system_prompt), "%s", ci->system_prompt);
    g_max_tokens = ci->max_tokens;
    g_temperature = ci->temperature;
    g_top_p = ci->top_p;
    snprintf(g_response_format_type, sizeof(g_response_format_type), "%s", ci->response_format_type);
    snprintf(g_user_id, sizeof(g_user_id), "%s", ci->user_id);
    printf("[AI] Loaded config: %s (model=%s, url=%s)\n", ci->name, g_model, g_api_url);
}

static char* build_dropdown_options(void) {
    if (g_config_count == 0) {
        return strdup("No configs");
    }
    size_t total = 0;
    for (int i = 0; i < g_config_count; i++) {
        total += strlen(g_configs[i].name);
        if (!g_configs[i].valid) total += strlen(" (invalid)");
        total += 1; // newline
    }
    char *opts = malloc(total + 1);
    if (!opts) return NULL;
    opts[0] = '\0';
    for (int i = 0; i < g_config_count; i++) {
        if (i > 0) strcat(opts, "\n");
        strcat(opts, g_configs[i].name);
        if (!g_configs[i].valid) strcat(opts, " (invalid)");
    }
    return opts;
}

/* ==================== 页面创建 ==================== */
BasePage *page_ai_create(void) {
    int cnt = scan_configs();
    if (cnt == 0) {
        printf("[AI] No config files found.\n");
    }

    AIPage *page = calloc(1, sizeof(AIPage));
    g_page = page;

    lv_obj_t *scr = lv_obj_create(lv_scr_act());
    page->screen = scr;
    lv_obj_remove_style_all(scr);
    lv_obj_set_size(scr, SCREEN_W, SCREEN_H);
    lv_obj_set_style_bg_color(scr, lv_color_white(), 0);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    /* 顶部栏 */
    lv_obj_t *bar = lv_obj_create(scr);
    lv_obj_set_width(bar, lv_pct(100));
    lv_obj_set_height(bar, TOP_BAR_H);
    lv_obj_set_pos(bar, 0, 0);
    lv_obj_set_style_pad_all(bar, 0, 0);
    lv_obj_set_style_border_width(bar, 0, 0);

    lv_obj_t *btn_back = lv_btn_create(bar);
    lv_obj_set_size(btn_back, 30, TOP_BAR_H);
    lv_obj_set_pos(btn_back, 0, 0);
    lv_obj_t *lbl_back = lv_label_create(btn_back);
    lv_label_set_text(lbl_back, LV_SYMBOL_LEFT);
    lv_obj_center(lbl_back);
    lv_obj_add_event_cb(btn_back, ai_back_cb, LV_EVENT_CLICKED, NULL);

    page->dropdown = lv_dropdown_create(bar);
    lv_obj_set_pos(page->dropdown, 30 + GAP, 0);
    lv_obj_set_width(page->dropdown, SCREEN_W - 30 - GAP);
    lv_obj_set_height(page->dropdown, TOP_BAR_H);
    lv_obj_set_style_border_width(page->dropdown, 1, 0);
    lv_obj_set_style_border_color(page->dropdown, lv_color_hex(0x888888), 0);
    lv_obj_set_style_radius(page->dropdown, 4, 0);
    lv_obj_set_style_pad_all(page->dropdown, 4, 0);

    char *opts = build_dropdown_options();
    if (opts) {
        lv_dropdown_set_options(page->dropdown, opts);
        free(opts);
    }

    int default_idx = -1;
    for (int i = 0; i < g_config_count; i++) {
        if (g_configs[i].valid) { default_idx = i; break; }
    }
    if (default_idx >= 0) {
        lv_dropdown_set_selected(page->dropdown, default_idx);
        load_config_by_index(default_idx);
        g_current_idx = default_idx;
    } else {
        g_api_url[0] = g_api_key[0] = g_model[0] = '\0';
        g_current_idx = -1;
        printf("[AI] No valid config found\n");
    }
    lv_obj_add_event_cb(page->dropdown, dropdown_cb, LV_EVENT_VALUE_CHANGED, NULL);

    /* 聊天容器（固定高度） */
    page->chat_container = lv_obj_create(scr);
    lv_obj_set_pos(page->chat_container, 0, CHAT_Y);
    lv_obj_set_width(page->chat_container, lv_pct(100));
    lv_obj_set_height(page->chat_container, CHAT_H_NORMAL);
    lv_obj_set_style_pad_all(page->chat_container, 4, 0);
    lv_obj_set_style_border_width(page->chat_container, 1, 0);
    lv_obj_set_style_border_color(page->chat_container, lv_color_hex(0x888888), 0);
    lv_obj_set_flex_flow(page->chat_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_scrollbar_mode(page->chat_container, LV_SCROLLBAR_MODE_AUTO);

    /* 发送按钮 */
    page->send_btn = lv_btn_create(scr);
    lv_obj_set_size(page->send_btn, SEND_W, BOT_BAR_H);
    lv_obj_set_pos(page->send_btn, SCREEN_W - SEND_W, SCREEN_H - BOT_BAR_H);
    lv_obj_set_style_pad_all(page->send_btn, 0, 0);
    lv_obj_set_style_border_width(page->send_btn, 0, 0);
    lv_obj_t *lbl_send = lv_label_create(page->send_btn);
    lv_label_set_text(lbl_send, "Send");
    lv_obj_center(lbl_send);
    lv_obj_add_event_cb(page->send_btn, send_cb, LV_EVENT_CLICKED, NULL);

    /* 输入框（绑定中文输入法） */
    page->input = lv_textarea_create(scr);
    lv_obj_set_pos(page->input, 0, SCREEN_H - BOT_BAR_H);
    lv_obj_set_size(page->input, INPUT_W, BOT_BAR_H);
    lv_obj_set_style_pad_ver(page->input, 0, 0);
    lv_obj_set_style_pad_hor(page->input, 4, 0);
    lv_obj_set_style_outline_width(page->input, 0, 0);
    lv_obj_set_style_shadow_width(page->input, 0, 0);
    lv_obj_set_style_min_height(page->input, BOT_BAR_H, 0);
    lv_obj_set_style_max_height(page->input, BOT_BAR_H, 0);
    lv_textarea_set_placeholder_text(page->input, "Ask something...");
    lv_textarea_set_one_line(page->input, true);

    // 绑定中文输入法
    lv_textarea_bind_ime(page->input);

    /* 显示无效配置警告 */
    for (int i = 0; i < g_config_count; i++) {
        if (!g_configs[i].valid) {
            char warn[128];
            snprintf(warn, sizeof(warn), "Config '%s' missing key fields", g_configs[i].name);
            add_warning(warn);
        }
    }

    return base_page_create(scr);
}

/* ==================== UI 回调 ==================== */
static void ai_back_cb(lv_event_t *e) {
    (void)e;
    printf("[AI] Back button pressed, cleaning up\n");
    clear_queue();
    free(g_page->ai_accumulated);
    g_page->ai_accumulated = NULL;
    g_page->ai_label = NULL;
    g_page = NULL;
    if (g_configs) {
        free(g_configs);
        g_configs = NULL;
        g_config_count = 0;
    }
    page_back();
}

static void dropdown_cb(lv_event_t *e) {
    int idx = lv_dropdown_get_selected(g_page->dropdown);
    if (idx == g_current_idx) return;
    g_current_idx = idx;
    load_config_by_index(idx);
    if (g_configs[idx].valid) {
        char msg[128];
        snprintf(msg, sizeof(msg), "Switched to model: %s", g_configs[idx].model);
        add_message(msg, lv_color_hex(0x808080));
        printf("[AI] Switched to config: %s\n", g_configs[idx].name);
    } else {
        add_warning("Selected config is invalid. Please check fields.");
        printf("[AI] Selected config is invalid\n");
    }
    scroll_bottom();
}

static void send_cb(lv_event_t *e) {
    (void)e;
    if (g_page->waiting || !g_page->input) return;

    if (g_api_url[0] == '\0' || g_api_key[0] == '\0' || g_model[0] == '\0') {
        add_warning("No valid config selected. Please choose a valid model.");
        scroll_bottom();
        return;
    }

    const char *text = lv_textarea_get_text(g_page->input);
    if (!text || strlen(text) == 0) return;

    char *user_input = strdup(text);
    if (!user_input) {
        add_warning("Out of memory");
        return;
    }

    char *prompt = strdup(text);
    lv_textarea_set_text(g_page->input, "");
    add_user_message(user_input);
    free(user_input);

    set_waiting(true);
    scroll_bottom();

    pthread_t tid;
    if (pthread_create(&tid, NULL, api_thread, prompt) != 0) {
        add_warning("Cannot start request");
        set_waiting(false);
        free(prompt);
    } else {
        pthread_detach(tid);
    }
}

/* ==================== 聊天操作 ==================== */
static void add_message(const char *text, lv_color_t color) {
    if (!g_page || !g_page->chat_container) return;
    if (!text || strlen(text) == 0) return;

    lv_obj_t *label = lv_label_create(g_page->chat_container);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_color(label, color, 0);
    lv_obj_set_width(label, lv_pct(100));
    lv_obj_set_style_pad_hor(label, 4, 0);
    lv_obj_set_style_pad_ver(label, 2, 0);
    scroll_bottom();
}

static void add_user_message(const char *text) {
    char buf[1024];
    snprintf(buf, sizeof(buf), "You: %s", text);
    add_message(buf, lv_color_hex(0xADD8E6));
}

static void add_warning(const char *text) {
    char buf[1024];
    snprintf(buf, sizeof(buf), "Warning: %s", text);
    add_message(buf, lv_color_hex(0x800080));
}

static void add_ai_message_start(void) {
    if (!g_page) return;
    g_page->ai_label = lv_label_create(g_page->chat_container);
    lv_obj_set_style_text_color(g_page->ai_label, lv_color_black(), 0);
    lv_obj_set_width(g_page->ai_label, lv_pct(100));
    lv_obj_set_style_pad_hor(g_page->ai_label, 4, 0);
    lv_obj_set_style_pad_ver(g_page->ai_label, 2, 0);
    lv_label_set_text(g_page->ai_label, "AI: ");
    free(g_page->ai_accumulated);
    g_page->ai_accumulated = malloc(MAX_AI_BUFFER_SIZE);
    if (g_page->ai_accumulated) {
        g_page->ai_accumulated[0] = '\0';
        strcpy(g_page->ai_accumulated, "AI: ");
    } else {
        g_page->ai_accumulated = strdup("AI: ");
    }
    g_page->ai_streaming = true;
}

static void update_ai_message(const char *full_text) {
    if (!g_page || !g_page->ai_label || !full_text) return;
    lv_label_set_text(g_page->ai_label, full_text);
    scroll_bottom();
}

static void finish_ai_message(void) {
    if (!g_page) return;
    process_queue();  // 确保所有队列处理完毕
    if (g_page->ai_accumulated) {
        update_ai_message(g_page->ai_accumulated);
    }
    free(g_page->ai_accumulated);
    g_page->ai_accumulated = NULL;
    g_page->ai_label = NULL;
    set_waiting(false);
    g_page->ai_streaming = false;
    printf("[AI] AI reply complete.\n");
}

/* ==================== 队列操作 ==================== */
static void enqueue_chunk(const char *chunk) {
    if (!chunk) return;
    pthread_mutex_lock(&g_queue_mutex);

    chunk_node_t *node = malloc(sizeof(chunk_node_t));
    node->data = strdup(chunk);
    node->next = NULL;
    if (g_queue_tail) {
        g_queue_tail->next = node;
        g_queue_tail = node;
    } else {
        g_queue_head = g_queue_tail = node;
    }
    g_queue_len++;

    if (!g_async_pending) {
        g_async_pending = 1;
        lv_async_call((lv_async_cb_t)process_queue, NULL);
    }
    pthread_mutex_unlock(&g_queue_mutex);
}

static void clear_queue(void) {
    pthread_mutex_lock(&g_queue_mutex);
    chunk_node_t *node = g_queue_head;
    while (node) {
        chunk_node_t *next = node->next;
        free(node->data);
        free(node);
        node = next;
    }
    g_queue_head = g_queue_tail = NULL;
    g_queue_len = 0;
    g_async_pending = 0;
    pthread_mutex_unlock(&g_queue_mutex);
}

static void process_queue(void) {
    pthread_mutex_lock(&g_queue_mutex);
    chunk_node_t *head = g_queue_head;
    g_queue_head = g_queue_tail = NULL;
    g_queue_len = 0;
    g_async_pending = 0;
    pthread_mutex_unlock(&g_queue_mutex);

    chunk_node_t *node = head;
    while (node) {
        if (node->data) {
            if (g_page && g_page->ai_accumulated) {
                size_t current_len = strlen(g_page->ai_accumulated);
                size_t add_len = strlen(node->data);
                if (current_len + add_len < MAX_AI_BUFFER_SIZE) {
                    memcpy(g_page->ai_accumulated + current_len, node->data, add_len + 1);
                    update_ai_message(g_page->ai_accumulated);
                } else {
                    printf("[AI] Warning: AI buffer overflow, discarding chunk\n");
                }
            }
            printf("[AI] Chunk: %s\n", node->data);
        }
        chunk_node_t *next = node->next;
        free(node->data);
        free(node);
        node = next;
    }
}

/* ==================== curl 回调（动态扩容，初始 4MB） ==================== */
static size_t curl_write_cb(void *ptr, size_t size, size_t nmemb, void *userdata) {
    size_t total = size * nmemb;
    char *data = (char*)ptr;
    static char *buffer = NULL;
    static size_t buf_len = 0;
    static size_t buf_cap = 0;

    // 初始化缓冲区（4MB）
    if (!buffer) {
        buffer = malloc(CURL_BUFFER_SIZE);
        if (!buffer) return 0;
        buf_cap = CURL_BUFFER_SIZE;
        buf_len = 0;
    }

    // 扩容
    if (buf_len + total > buf_cap) {
        size_t new_cap = buf_cap * 2;
        if (new_cap < buf_len + total) new_cap = buf_len + total + 4096;
        char *new_buf = realloc(buffer, new_cap);
        if (!new_buf) {
            printf("[AI] curl buffer realloc failed, dropping data\n");
            return total;
        }
        buffer = new_buf;
        buf_cap = new_cap;
    }

    // 追加新数据
    memcpy(buffer + buf_len, data, total);
    buf_len += total;
    buffer[buf_len] = '\0';

    char *line_start = buffer;
    char *line_end;

    // 主解析循环：按 \n\n 分隔
    while ((line_end = strstr(line_start, "\n\n")) != NULL) {
        *line_end = '\0';
        char *sse_line = line_start;
        while (*sse_line == '\n' || *sse_line == '\r') sse_line++;
        if (*sse_line != '\0') {
            if (strncmp(sse_line, "data: ", 6) == 0) {
                const char *json_str = sse_line + 6;
                if (strcmp(json_str, "[DONE]") == 0) {
                    printf("[AI] Received [DONE], processing remaining data\n");
                    // 处理缓冲区中剩余的所有数据（按行分割）
                    char *rem = line_end + 2;
                    while (rem < buffer + buf_len) {
                        char *next = strchr(rem, '\n');
                        if (next) *next = '\0';
                        char *sse_line2 = rem;
                        while (*sse_line2 == '\n' || *sse_line2 == '\r') sse_line2++;
                        if (*sse_line2 != '\0' && strncmp(sse_line2, "data: ", 6) == 0) {
                            const char *json_str2 = sse_line2 + 6;
                            if (strcmp(json_str2, "[DONE]") != 0) {
                                cJSON *root = cJSON_Parse(json_str2);
                                if (root) {
                                    cJSON *choices = cJSON_GetObjectItem(root, "choices");
                                    if (cJSON_IsArray(choices) && cJSON_GetArraySize(choices) > 0) {
                                        cJSON *first = cJSON_GetArrayItem(choices, 0);
                                        cJSON *delta = cJSON_GetObjectItem(first, "delta");
                                        cJSON *content = cJSON_GetObjectItem(delta, "content");
                                        if (cJSON_IsString(content) && content->valuestring) {
                                            char *chunk = strdup(content->valuestring);
                                            printf("[AI] Final chunk: %s\n", chunk);
                                            enqueue_chunk(chunk);
                                            free(chunk);
                                        }
                                    }
                                    cJSON_Delete(root);
                                }
                            }
                        }
                        if (next) {
                            rem = next + 1;
                        } else {
                            break;
                        }
                    }
                    // 清空缓冲区
                    buf_len = 0;
                    buffer[0] = '\0';
                    // 直接调用 finish_ai_message（会触发 process_queue 显示全部内容）
                    lv_async_call((lv_async_cb_t)finish_ai_message, NULL);
                    return total;
                } else {
                    // 普通 chunk
                    cJSON *root = cJSON_Parse(json_str);
                    if (root) {
                        cJSON *choices = cJSON_GetObjectItem(root, "choices");
                        if (cJSON_IsArray(choices) && cJSON_GetArraySize(choices) > 0) {
                            cJSON *first = cJSON_GetArrayItem(choices, 0);
                            cJSON *delta = cJSON_GetObjectItem(first, "delta");
                            cJSON *content = cJSON_GetObjectItem(delta, "content");
                            if (cJSON_IsString(content) && content->valuestring) {
                                char *chunk = strdup(content->valuestring);
                                int len = strlen(chunk);
                                printf("[AI] Received chunk: %.*s%s\n",
                                       len>20?20:len, chunk, len>20?"..." : "");
                                enqueue_chunk(chunk);
                                free(chunk);
                            }
                        }
                        cJSON_Delete(root);
                    } else {
                        printf("[AI] Failed to parse JSON chunk: %s\n", json_str);
                    }
                }
            }
        }
        line_start = line_end + 2;
        if (line_start >= buffer + buf_len) break;
    }

    // 保留剩余未处理的数据到下次调用
    if (line_start < buffer + buf_len) {
        memmove(buffer, line_start, buffer + buf_len - line_start);
        buf_len = buffer + buf_len - line_start;
    } else {
        buf_len = 0;
    }
    buffer[buf_len] = '\0';

    return total;
}

/* ==================== 非流式回调（备用） ==================== */
typedef struct {
    char *text;
    bool  ok;
} AIData;

static void on_api_response_ui_cb(void *data) {
    AIData *ai = (AIData *)data;
    if (ai && g_page) {
        if (ai->ok && ai->text) {
            add_message(ai->text, lv_color_black());
        } else {
            add_warning("Request failed");
        }
        free(ai->text);
        free(ai);
        set_waiting(false);
        scroll_bottom();
    }
}

/* ==================== API 请求线程 ==================== */
static void *api_thread(void *arg) {
    char *prompt = (char *)arg;
    CURL *curl = curl_easy_init();
    if (!curl) {
        printf("[AI] curl_easy_init failed\n");
        AIData *ai = malloc(sizeof(AIData));
        ai->text = strdup("Network error");
        ai->ok = false;
        lv_async_call(on_api_response_ui_cb, ai);
        free(prompt);
        return NULL;
    }

    printf("[AI] Sending request to %s, model=%s, prompt='%s'\n",
           g_api_url, g_model, prompt);

    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "model", g_model);
    cJSON_AddBoolToObject(root, "stream", true);

    cJSON *thinking = cJSON_CreateObject();
    cJSON_AddStringToObject(thinking, "type", "disabled");
    cJSON_AddItemToObject(root, "thinking", thinking);

    cJSON *messages = cJSON_AddArrayToObject(root, "messages");
    if (g_system_prompt[0] != '\0') {
        cJSON *sys = cJSON_CreateObject();
        cJSON_AddStringToObject(sys, "role", "system");
        cJSON_AddStringToObject(sys, "content", g_system_prompt);
        cJSON_AddItemToArray(messages, sys);
        printf("[AI] System prompt: %s\n", g_system_prompt);
    }
    cJSON *usr = cJSON_CreateObject();
    cJSON_AddStringToObject(usr, "role", "user");
    cJSON_AddStringToObject(usr, "content", prompt);
    cJSON_AddItemToArray(messages, usr);

    if (g_max_tokens > 0) cJSON_AddNumberToObject(root, "max_tokens", g_max_tokens);
    if (g_temperature >= 0.0f) cJSON_AddNumberToObject(root, "temperature", g_temperature);
    if (g_top_p >= 0.0f) cJSON_AddNumberToObject(root, "top_p", g_top_p);
    if (g_response_format_type[0]) {
        cJSON *rf = cJSON_CreateObject();
        cJSON_AddStringToObject(rf, "type", g_response_format_type);
        cJSON_AddItemToObject(root, "response_format", rf);
    }
    if (g_user_id[0]) cJSON_AddStringToObject(root, "user_id", g_user_id);

    char *body_str = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    printf("[AI] Request body: %s\n", body_str);

    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    char auth[256];
    snprintf(auth, sizeof(auth), "Authorization: Bearer %s", g_api_key);
    headers = curl_slist_append(headers, auth);

    curl_easy_setopt(curl, CURLOPT_URL, g_api_url);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body_str);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, NULL);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);

    lv_async_call((lv_async_cb_t)add_ai_message_start, NULL);

    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        printf("[AI] curl_easy_perform failed: %s\n", curl_easy_strerror(res));
        lv_async_call((lv_async_cb_t)finish_ai_message, NULL);
        lv_async_call((lv_async_cb_t)add_warning, "Request failed");
        set_waiting(false);
    } else {
        printf("[AI] Request completed successfully\n");
    }

    curl_slist_free_all(headers);
    free(body_str);
    curl_easy_cleanup(curl);
    free(prompt);

    return NULL;
}

/* ==================== 滚动 ==================== */
static void scroll_bottom(void) {
    if (!g_page || !g_page->chat_container) return;
    lv_obj_scroll_to_y(g_page->chat_container, LV_COORD_MAX, LV_ANIM_OFF);
}

/* ==================== 设置等待状态 ==================== */
static void set_waiting(bool w) {
    if (g_page) g_page->waiting = w;
    if (w) printf("[AI] Waiting for response...\n");
    else printf("[AI] Response received or error.\n");
}