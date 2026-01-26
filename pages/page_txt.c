// page_txt.c
#include "page_txt.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_LINES 12          // 每页最大行数
#define MAX_CHARS_PER_LINE 25  // 每行最大字符数

static void back_click(lv_event_t * e);
static void next_page_click(lv_event_t * e);
static void prev_page_click(lv_event_t * e);
static void update_display(void);
static int is_utf8_start(unsigned char c);  // 添加函数声明
static int is_utf8_continuation(unsigned char c);  // 添加函数声明

static lv_obj_t * screen = NULL;
static lv_obj_t * text_label = NULL;
static lv_obj_t * page_label = NULL;
static char * file_content = NULL;
static long file_size = 0;
static int current_page = 0;
static int total_pages = 0;
static char display_buffer[MAX_LINES * (MAX_CHARS_PER_LINE * 4 + 1) + 1];  // UTF-8可能需要更多空间

// 判断字符是否为UTF-8中文字符的起始字节
static int is_utf8_start(unsigned char c) {
    return (c >= 0xE0 && c <= 0xEF);  // UTF-8 3字节字符（中文）的起始字节
}

// 判断字符是否为UTF-8中文字符的后续字节
static int is_utf8_continuation(unsigned char c) {
    return (c >= 0x80 && c <= 0xBF);
}

lv_obj_t * page_txt(char * filename) {
    screen = lv_obj_create(lv_scr_act());
    lv_obj_remove_style_all(screen);
    lv_obj_set_size(screen, lv_pct(100), lv_pct(100));
    lv_obj_set_style_bg_color(screen, lv_color_white(), 0);
    
    current_page = 0;
    
    FILE *fp = fopen(filename, "r");
    if (fp == NULL) {
        lv_obj_t * error_label = lv_label_create(screen);
        lv_label_set_text(error_label, "无法打开文件");
        lv_obj_align(error_label, LV_ALIGN_CENTER, 0, 0);
        
        lv_obj_t * btn_back = lv_btn_create(screen);
        lv_obj_set_size(btn_back, lv_pct(25), lv_pct(12));
        lv_obj_align(btn_back, LV_ALIGN_BOTTOM_LEFT, 0, 0);
        lv_obj_t * btn_back_label = lv_label_create(btn_back);
        lv_label_set_text(btn_back_label, "back");
        lv_obj_center(btn_back_label);
        lv_obj_add_event_cb(btn_back, back_click, LV_EVENT_CLICKED, NULL);
        
        return screen;
    }
    
    fseek(fp, 0, SEEK_END);
    file_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    
    file_content = (char *)malloc(file_size + 1);
    if (file_content == NULL) {
        fclose(fp);
        lv_obj_t * error_label = lv_label_create(screen);
        lv_label_set_text(error_label, "内存不足");
        lv_obj_align(error_label, LV_ALIGN_CENTER, 0, 0);
        
        lv_obj_t * btn_back = lv_btn_create(screen);
        lv_obj_set_size(btn_back, lv_pct(25), lv_pct(12));
        lv_obj_align(btn_back, LV_ALIGN_BOTTOM_LEFT, 0, 0);
        lv_obj_t * btn_back_label = lv_label_create(btn_back);
        lv_label_set_text(btn_back_label, "back");
        lv_obj_center(btn_back_label);
        lv_obj_add_event_cb(btn_back, back_click, LV_EVENT_CLICKED, NULL);
        
        return screen;
    }
    
    size_t read_size = fread(file_content, 1, file_size, fp);
    file_content[read_size] = '\0';
    fclose(fp);
    
    text_label = lv_label_create(screen);
    lv_obj_set_width(text_label, lv_pct(95));
    lv_obj_align(text_label, LV_ALIGN_TOP_MID, 0, 10);
    lv_label_set_long_mode(text_label, LV_LABEL_LONG_WRAP);
    
    extern lv_font_t HarmonyOS_16;
    lv_obj_set_style_text_font(text_label, &HarmonyOS_16, 0);
    
    page_label = lv_label_create(screen);
    lv_obj_align(page_label, LV_ALIGN_BOTTOM_MID, 0, -5);
    
    lv_obj_t * btn_back = lv_btn_create(screen);
    lv_obj_set_size(btn_back, lv_pct(25), lv_pct(12));
    lv_obj_align(btn_back, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    lv_obj_t * btn_back_label = lv_label_create(btn_back);
    lv_label_set_text(btn_back_label, "back");
    lv_obj_center(btn_back_label);
    lv_obj_add_event_cb(btn_back, back_click, LV_EVENT_CLICKED, NULL);
    
    lv_obj_t * btn_next = lv_btn_create(screen);
    lv_obj_set_size(btn_next, 50, 25);
    lv_obj_align(btn_next, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
    lv_obj_t * btn_next_label = lv_label_create(btn_next);
    lv_label_set_text(btn_next_label, ">");
    lv_obj_center(btn_next_label);
    lv_obj_add_event_cb(btn_next, next_page_click, LV_EVENT_CLICKED, NULL);
    
    lv_obj_t * btn_prev = lv_btn_create(screen);
    lv_obj_set_size(btn_prev, 50, 25);
    lv_obj_align(btn_prev, LV_ALIGN_BOTTOM_RIGHT, -53, 0);
    lv_obj_t * btn_prev_label = lv_label_create(btn_prev);
    lv_label_set_text(btn_prev_label, "<");
    lv_obj_center(btn_prev_label);
    lv_obj_add_event_cb(btn_prev, prev_page_click, LV_EVENT_CLICKED, NULL);
    
    // 计算总行数 - 正确处理UTF-8编码
    long pos = 0;
    int total_lines = 0;
    while (pos < file_size) {
        int line_length = 0;
        int in_word = 0;
        
        // 计算一行的长度
        while (pos < file_size && line_length < MAX_CHARS_PER_LINE) {
            unsigned char c = (unsigned char)file_content[pos];
            
            if (c == '\n' || c == '\r') {
                total_lines++;
                pos++;
                while (pos < file_size && (file_content[pos] == '\n' || file_content[pos] == '\r')) {
                    pos++;
                    total_lines++;
                }
                break;
            }
            
            // 处理UTF-8字符
            if (is_utf8_start(c)) {
                // UTF-8中文字符（3字节）
                pos += 3;
                line_length += 1;  // 只算1个字符位置
            } else if (c >= 0x80) {
                // 其他UTF-8字符，跳过
                pos++;
            } else {
                // ASCII字符
                pos++;
                line_length++;
            }
            
            // 检查是否在单词中（简化逻辑）
            if (c == ' ' || c == '\t' || c == ',' || c == '.' || c == ';' || c == ':' || 
                c == '!' || c == '?' || c == ')' || c == ']' || c == '}') {
                in_word = 0;
            } else if (c >= 32) {  // 可打印字符
                in_word = 1;
            }
            
            // 如果到达行尾但还在单词中间，简单处理
            if (line_length >= MAX_CHARS_PER_LINE && in_word) {
                // 简单换行，不做复杂回退
                break;
            }
        }
        
        if (line_length > 0) {
            total_lines++;
        }
    }
    
    total_pages = (total_lines + MAX_LINES - 1) / MAX_LINES;
    if (total_pages == 0) total_pages = 1;
    
    update_display();
    
    return screen;
}

static void update_display(void) {
    if (!file_content || !text_label || !page_label) return;
    
    memset(display_buffer, 0, sizeof(display_buffer));
    
    int buffer_index = 0;
    int line_count = 0;
    long pos = 0;
    int current_line = 0;
    
    // 跳过前面的行，直到到达当前页的开始
    while (pos < file_size && current_line < current_page * MAX_LINES) {
        int line_length = 0;
        
        while (pos < file_size && line_length < MAX_CHARS_PER_LINE) {
            unsigned char c = (unsigned char)file_content[pos];
            
            if (c == '\n' || c == '\r') {
                current_line++;
                pos++;
                while (pos < file_size && (file_content[pos] == '\n' || file_content[pos] == '\r')) {
                    pos++;
                    current_line++;
                }
                break;
            }
            
            // 处理UTF-8字符
            if (is_utf8_start(c)) {
                pos += 3;
                line_length += 1;
            } else if (c >= 0x80) {
                pos++;
            } else {
                pos++;
                line_length++;
            }
        }
        
        if (line_length > 0) {
            current_line++;
        }
    }
    
    // 填充当前页的内容
    while (pos < file_size && line_count < MAX_LINES) {
        int line_length = 0;
        int line_start = buffer_index;
        
        while (pos < file_size && line_length < MAX_CHARS_PER_LINE) {
            unsigned char c = (unsigned char)file_content[pos];
            
            if (c == '\n' || c == '\r') {
                // 填充剩余空格
                while (line_length < MAX_CHARS_PER_LINE) {
                    display_buffer[buffer_index++] = ' ';
                    line_length++;
                }
                pos++;
                line_count++;
                while (pos < file_size && (file_content[pos] == '\n' || file_content[pos] == '\r')) {
                    pos++;
                    if (line_count < MAX_LINES) {
                        if (line_count > 0) {
                            display_buffer[buffer_index++] = '\n';
                        }
                        for (int i = 0; i < MAX_CHARS_PER_LINE && buffer_index < sizeof(display_buffer) - 1; i++) {
                            display_buffer[buffer_index++] = ' ';
                        }
                        line_count++;
                    }
                }
                break;
            }
            
            // 处理字符并添加到缓冲区
            if (is_utf8_start(c)) {
                // UTF-8中文字符，复制3个字节
                for (int i = 0; i < 3 && pos < file_size; i++) {
                    display_buffer[buffer_index++] = file_content[pos++];
                }
                line_length += 1;
            } else if (is_utf8_continuation(c)) {
                // UTF-8后续字节，跳过（不应该单独出现）
                pos++;
            } else {
                // ASCII字符
                display_buffer[buffer_index++] = file_content[pos++];
                line_length++;
            }
            
            // 检查是否需要换行
            if (line_length >= MAX_CHARS_PER_LINE) {
                // 填充行尾空格
                while (line_length < MAX_CHARS_PER_LINE) {
                    display_buffer[buffer_index++] = ' ';
                    line_length++;
                }
                break;
            }
        }
        
        // 如果是因为字符数达到限制而结束，填充行尾
        if (line_length < MAX_CHARS_PER_LINE && pos < file_size && 
            file_content[pos] != '\n' && file_content[pos] != '\r') {
            while (line_length < MAX_CHARS_PER_LINE) {
                display_buffer[buffer_index++] = ' ';
                line_length++;
            }
        }
        
        // 添加换行符（如果不是最后一行）
        if (line_count < MAX_LINES - 1 && line_length == MAX_CHARS_PER_LINE) {
            display_buffer[buffer_index++] = '\n';
        }
        
        if (line_length == MAX_CHARS_PER_LINE) {
            line_count++;
        }
    }
    
    // 如果行数不足，用空行补齐
    if (line_count < MAX_LINES) {
        for (int i = line_count; i < MAX_LINES; i++) {
            if (i > 0) {
                display_buffer[buffer_index++] = '\n';
            }
            for (int j = 0; j < MAX_CHARS_PER_LINE && buffer_index < sizeof(display_buffer) - 1; j++) {
                display_buffer[buffer_index++] = ' ';
            }
        }
    }
    
    display_buffer[buffer_index] = '\0';
    
    lv_label_set_text(text_label, display_buffer);
    
    char page_info[32];
    int percent = 0;
    if (total_pages > 0) {
        percent = (current_page * 100) / total_pages;
    }
    if (current_page >= total_pages - 1) {
        percent = 100;
    }
    snprintf(page_info, sizeof(page_info), "%d%%", percent);
    lv_label_set_text(page_label, page_info);
}

static void back_click(lv_event_t * e) {
    if (file_content) {
        free(file_content);
        file_content = NULL;
    }
    page_back();
}

static void next_page_click(lv_event_t * e) {
    current_page++;
    if (current_page >= total_pages) {
        current_page = total_pages - 1;
    }
    update_display();
}

static void prev_page_click(lv_event_t * e) {
    current_page--;
    if (current_page < 0) {
        current_page = 0;
    }
    update_display();
}