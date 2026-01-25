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

static lv_obj_t * screen = NULL;
static lv_obj_t * text_label = NULL;
static lv_obj_t * page_label = NULL;
static char * file_content = NULL;
static long file_size = 0;
static int current_page = 0;
static int total_pages = 0;
static char display_buffer[MAX_LINES * (MAX_CHARS_PER_LINE + 1) + 1];  // 加1用于换行符

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
    
    // 修改为使用 unifont_16 字体
    extern lv_font_t unifont_16;  // 声明外部字体
    lv_obj_set_style_text_font(text_label, &unifont_16, 0);
    
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
    
    // 计算总行数 - 将换行符也算作一行
    long pos = 0;
    int total_lines = 0;
    while (pos < file_size) {
        int line_length = 0;
        int in_word = 0;
        
        // 计算一行的长度
        while (pos < file_size && line_length < MAX_CHARS_PER_LINE) {
            char c = file_content[pos];
            
            if (c == '\n' || c == '\r') {
                // 遇到换行符，结束当前行
                total_lines++;
                pos++;
                // 每个换行符都算一行
                while (pos < file_size && (file_content[pos] == '\n' || file_content[pos] == '\r')) {
                    pos++;
                    total_lines++;  // 每个换行符都算一行
                }
                break;
            }
            
            line_length++;
            pos++;
            
            // 检查空格或标点，可以作为断行点
            if (c == ' ' || c == '\t' || c == ',' || c == '.' || c == ';' || c == ':' || 
                c == '!' || c == '?' || c == ')' || c == ']' || c == '}') {
                in_word = 0;
            } else {
                in_word = 1;
            }
            
            // 如果到达行尾但还在单词中间，回退到单词开始
            if (line_length >= MAX_CHARS_PER_LINE && in_word) {
                // 向前查找单词开始
                int word_start = pos - 1;
                while (word_start > 0 && word_start > pos - line_length) {
                    char prev_c = file_content[word_start];
                    if (prev_c == ' ' || prev_c == '\t' || prev_c == '\n' || prev_c == '\r' ||
                        prev_c == ',' || prev_c == '.' || prev_c == ';' || prev_c == ':' || 
                        prev_c == '!' || prev_c == '?' || prev_c == '(' || prev_c == '[' || prev_c == '{' ||
                        prev_c == ')' || prev_c == ']' || prev_c == '}') {
                        break;
                    }
                    word_start--;
                }
                
                if (word_start > pos - line_length) {
                    pos = word_start + 1;
                }
                break;
            }
        }
        
        // 如果一行结束了（非换行符结束）
        if (line_length > 0 && line_length <= MAX_CHARS_PER_LINE) {
            total_lines++;
        }
    }
    
    // 计算总页数
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
        int in_word = 0;
        
        while (pos < file_size && line_length < MAX_CHARS_PER_LINE) {
            char c = file_content[pos];
            
            if (c == '\n' || c == '\r') {
                current_line++;  // 换行符算作一行
                pos++;
                while (pos < file_size && (file_content[pos] == '\n' || file_content[pos] == '\r')) {
                    pos++;
                    current_line++;  // 每个换行符都算一行
                }
                break;
            }
            
            line_length++;
            pos++;
            
            if (c == ' ' || c == '\t' || c == ',' || c == '.' || c == ';' || c == ':' || 
                c == '!' || c == '?' || c == ')' || c == ']' || c == '}') {
                in_word = 0;
            } else {
                in_word = 1;
            }
            
            if (line_length >= MAX_CHARS_PER_LINE && in_word) {
                int word_start = pos - 1;
                while (word_start > 0 && word_start > pos - line_length) {
                    char prev_c = file_content[word_start];
                    if (prev_c == ' ' || prev_c == '\t' || prev_c == '\n' || prev_c == '\r' ||
                        prev_c == ',' || prev_c == '.' || prev_c == ';' || prev_c == ':' || 
                        prev_c == '!' || prev_c == '?' || prev_c == '(' || prev_c == '[' || prev_c == '{' ||
                        prev_c == ')' || prev_c == ']' || prev_c == '}') {
                        break;
                    }
                    word_start--;
                }
                
                if (word_start > pos - line_length) {
                    pos = word_start + 1;
                }
                break;
            }
        }
        
        if (line_length > 0 && line_length <= MAX_CHARS_PER_LINE) {
            current_line++;
        }
    }
    
    // 填充当前页的内容
    while (pos < file_size && line_count < MAX_LINES) {
        int line_length = 0;
        int line_start = buffer_index;
        int in_word = 0;
        
        while (pos < file_size && line_length < MAX_CHARS_PER_LINE) {
            char c = file_content[pos];
            
            if (c == '\n' || c == '\r') {
                // 填充剩余空格
                while (line_length < MAX_CHARS_PER_LINE) {
                    display_buffer[buffer_index++] = ' ';
                    line_length++;
                }
                pos++;
                line_count++;  // 换行符算作一行
                while (pos < file_size && (file_content[pos] == '\n' || file_content[pos] == '\r')) {
                    pos++;
                    // 如果是新的一行，且还没有达到最大行数，添加空行
                    if (line_count < MAX_LINES) {
                        // 添加换行符
                        if (line_count > 0) {
                            display_buffer[buffer_index++] = '\n';
                        }
                        // 添加新的一行（25个空格）
                        for (int i = 0; i < MAX_CHARS_PER_LINE && buffer_index < sizeof(display_buffer) - 1; i++) {
                            display_buffer[buffer_index++] = ' ';
                        }
                        line_count++;
                    }
                }
                break;
            }
            
            display_buffer[buffer_index++] = c;
            line_length++;
            pos++;
            
            if (c == ' ' || c == '\t' || c == ',' || c == '.' || c == ';' || c == ':' || 
                c == '!' || c == '?' || c == ')' || c == ']' || c == '}') {
                in_word = 0;
            } else {
                in_word = 1;
            }
            
            if (line_length >= MAX_CHARS_PER_LINE && in_word) {
                // 回退到单词开始
                int word_start = pos - 1;
                while (word_start > pos - line_length) {
                    char prev_c = file_content[word_start];
                    if (prev_c == ' ' || prev_c == '\t' || prev_c == '\n' || prev_c == '\r' ||
                        prev_c == ',' || prev_c == '.' || prev_c == ';' || prev_c == ':' || 
                        prev_c == '!' || prev_c == '?' || prev_c == '(' || prev_c == '[' || prev_c == '{' ||
                        prev_c == ')' || prev_c == ']' || prev_c == '}') {
                        break;
                    }
                    word_start--;
                }
                
                if (word_start > pos - line_length) {
                    // 回退缓冲区指针
                    buffer_index = line_start + (word_start - (pos - line_length));
                    // 填充当前行剩余部分为空格
                    while (line_length < MAX_CHARS_PER_LINE) {
                        display_buffer[buffer_index++] = ' ';
                        line_length++;
                    }
                    // 重置位置到单词开始
                    pos = word_start + 1;
                }
                break;
            }
        }
        
        // 填充行尾空格（如果是因为字符数达到限制而结束）
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
            // 添加25个空格
            for (int j = 0; j < MAX_CHARS_PER_LINE && buffer_index < sizeof(display_buffer) - 1; j++) {
                display_buffer[buffer_index++] = ' ';
            }
        }
    }
    
    // 确保字符串以NULL结尾
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