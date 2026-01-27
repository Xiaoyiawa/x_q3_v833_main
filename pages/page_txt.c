// page_txt.c
#include "page_txt.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_LINES 12          // 每页最大行数
#define MAX_CHARS_PER_LINE 28  // 每行最大字符数（按英文字符计算）

static void back_click(lv_event_t * e);
static void next_page_click(lv_event_t * e);
static void prev_page_click(lv_event_t * e);
static void update_display(void);
static int is_chinese_char(unsigned char c);
static int is_word_break_char(unsigned char c);  // 新增：判断是否为单词断点字符

static lv_obj_t * screen = NULL;
static lv_obj_t * text_label = NULL;
static lv_obj_t * page_label = NULL;
static char * file_content = NULL;
static long file_size = 0;
static int current_page = 0;
static int total_pages = 0;
static char display_buffer[MAX_LINES * (MAX_CHARS_PER_LINE * 2 + 1) + 1];

// 判断是否为中文字符（UTF-8编码的第一个字节）
static int is_chinese_char(unsigned char c) {
    return (c >= 0xE4 && c <= 0xE9);
}

// 判断是否为单词断点字符（空格、标点等）
static int is_word_break_char(unsigned char c) {
    return (c == ' ' || c == '\t' || c == ',' || c == '.' || c == ';' || c == ':' || 
            c == '!' || c == '?' || c == ')' || c == ']' || c == '}' || 
            c == '(' || c == '[' || c == '{' || c == '\n' || c == '\r');
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
    lv_obj_set_size(btn_next, 45, 29);
    lv_obj_align(btn_next, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
    lv_obj_t * btn_next_label = lv_label_create(btn_next);
    lv_label_set_text(btn_next_label, ">");
    lv_obj_center(btn_next_label);
    lv_obj_add_event_cb(btn_next, next_page_click, LV_EVENT_CLICKED, NULL);
    
    lv_obj_t * btn_prev = lv_btn_create(screen);
    lv_obj_set_size(btn_prev, 45, 29);
    lv_obj_align(btn_prev, LV_ALIGN_BOTTOM_RIGHT, -48, 0);
    lv_obj_t * btn_prev_label = lv_label_create(btn_prev);
    lv_label_set_text(btn_prev_label, "<");
    lv_obj_center(btn_prev_label);
    lv_obj_add_event_cb(btn_prev, prev_page_click, LV_EVENT_CLICKED, NULL);
    
    // 计算总行数 - 考虑中文字符宽度为英文字符的两倍，并且有单词换行
    long pos = 0;
    int total_lines = 0;
    while (pos < file_size) {
        int char_width_count = 0;
        int last_break_pos = -1;  // 最后一个断点位置
        int last_break_width = 0; // 最后一个断点时的宽度
        
        while (pos < file_size && char_width_count < MAX_CHARS_PER_LINE) {
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
            
            // 计算字符宽度
            int char_width = 1;
            if (is_chinese_char(c)) {
                char_width = 2;
                pos += 3;
            } else if (c >= 0x80) {
                // 其他多字节UTF-8字符
                char_width = 1;
                while (pos < file_size && (file_content[pos] & 0xC0) == 0x80) {
                    pos++;
                }
                pos++;
            } else {
                char_width = 1;
                pos++;
            }
            
            char_width_count += char_width;
            
            // 记录断点位置
            if (is_word_break_char(c)) {
                last_break_pos = pos;
                last_break_width = char_width_count;
            }
            
            // 如果到达行尾但没有断点，或者单词太长
            if (char_width_count >= MAX_CHARS_PER_LINE) {
                if (last_break_pos != -1) {
                    // 回退到最后一个断点
                    pos = last_break_pos;
                    char_width_count = last_break_width;
                }
                break;
            }
        }
        
        // 如果一行结束了（非换行符结束）
        if (char_width_count > 0 && char_width_count <= MAX_CHARS_PER_LINE) {
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
    
    // 跳过前面的行
    while (pos < file_size && current_line < current_page * MAX_LINES) {
        int char_width_count = 0;
        int last_break_pos = -1;
        int last_break_width = 0;
        
        while (pos < file_size && char_width_count < MAX_CHARS_PER_LINE) {
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
            
            int char_width = 1;
            if (is_chinese_char(c)) {
                char_width = 2;
                pos += 3;
            } else if (c >= 0x80) {
                char_width = 1;
                while (pos < file_size && (file_content[pos] & 0xC0) == 0x80) {
                    pos++;
                }
                pos++;
            } else {
                char_width = 1;
                pos++;
            }
            
            char_width_count += char_width;
            
            if (is_word_break_char(c)) {
                last_break_pos = pos;
                last_break_width = char_width_count;
            }
            
            if (char_width_count >= MAX_CHARS_PER_LINE) {
                if (last_break_pos != -1) {
                    pos = last_break_pos;
                    char_width_count = last_break_width;
                }
                break;
            }
        }
        
        if (char_width_count > 0 && char_width_count <= MAX_CHARS_PER_LINE) {
            current_line++;
        }
    }
    
    // 填充当前页的内容
    while (pos < file_size && line_count < MAX_LINES) {
        int char_width_count = 0;
        int line_start = buffer_index;
        int line_char_start = pos;  // 当前行开始的字符位置
        int last_break_pos = -1;
        int last_break_buffer_index = -1;
        int last_break_width = 0;
        
        while (pos < file_size && char_width_count < MAX_CHARS_PER_LINE) {
            unsigned char c = (unsigned char)file_content[pos];
            
            if (c == '\n' || c == '\r') {
                // 填充剩余空格
                while (char_width_count < MAX_CHARS_PER_LINE) {
                    display_buffer[buffer_index++] = ' ';
                    char_width_count++;
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
            
            // 记录当前位置，以便可能回退
            int current_buffer_index = buffer_index;
            int current_pos = pos;
            int current_char_width = 1;
            
            // 处理字符
            if (is_chinese_char(c)) {
                current_char_width = 2;
                // 检查是否有足够空间
                if (char_width_count + current_char_width > MAX_CHARS_PER_LINE) {
                    // 没有足够空间，回退到最后一个断点
                    if (last_break_pos != -1) {
                        pos = last_break_pos;
                        buffer_index = last_break_buffer_index;
                        char_width_count = last_break_width;
                    }
                    break;
                }
                
                // 复制中文字符
                for (int i = 0; i < 3 && pos < file_size; i++) {
                    display_buffer[buffer_index++] = file_content[pos++];
                }
            } else if (c >= 0x80) {
                current_char_width = 1;
                if (char_width_count + current_char_width > MAX_CHARS_PER_LINE) {
                    if (last_break_pos != -1) {
                        pos = last_break_pos;
                        buffer_index = last_break_buffer_index;
                        char_width_count = last_break_width;
                    }
                    break;
                }
                
                display_buffer[buffer_index++] = file_content[pos++];
                while (pos < file_size && (file_content[pos] & 0xC0) == 0x80) {
                    display_buffer[buffer_index++] = file_content[pos++];
                }
            } else {
                current_char_width = 1;
                if (char_width_count + current_char_width > MAX_CHARS_PER_LINE) {
                    if (last_break_pos != -1) {
                        pos = last_break_pos;
                        buffer_index = last_break_buffer_index;
                        char_width_count = last_break_width;
                    }
                    break;
                }
                
                display_buffer[buffer_index++] = file_content[pos++];
            }
            
            char_width_count += current_char_width;
            
            // 记录断点位置
            if (is_word_break_char(c)) {
                last_break_pos = pos;
                last_break_buffer_index = buffer_index;
                last_break_width = char_width_count;
            }
            
            // 如果到达行尾
            if (char_width_count >= MAX_CHARS_PER_LINE) {
                if (last_break_pos != -1 && last_break_pos != pos) {
                    // 回退到最后一个断点
                    pos = last_break_pos;
                    buffer_index = last_break_buffer_index;
                    char_width_count = last_break_width;
                }
                break;
            }
        }
        
        // 填充行尾空格
        while (char_width_count < MAX_CHARS_PER_LINE) {
            display_buffer[buffer_index++] = ' ';
            char_width_count++;
        }
        
        // 添加换行符
        if (line_count < MAX_LINES - 1) {
            display_buffer[buffer_index++] = '\n';
        }
        
        line_count++;
    }
    
    // 补齐空行
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