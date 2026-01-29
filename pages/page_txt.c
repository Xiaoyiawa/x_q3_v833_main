// 部分用deepseek写的，代码写的惨不忍睹
#include "page_txt.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_LINES 12
#define MAX_CHARS_PER_LINE 28

static void back_click(lv_event_t * e);
static void next_page_click(lv_event_t * e);
static void prev_page_click(lv_event_t * e);
static void update_display(void);
static int get_char_width_and_copy(unsigned char c, long *pos, long file_size, char *content, char *buffer, int *buf_index);

static lv_obj_t * screen = NULL;
static lv_obj_t * text_label = NULL;
static lv_obj_t * page_label = NULL;
static char * file_content = NULL;
static long file_size = 0;
static int current_page = 0;
static int total_pages = 0;
static char display_buffer[MAX_LINES * (MAX_CHARS_PER_LINE * 3 + 1) + 1];

// 获取字符宽度并复制到缓冲区
// 返回：字符宽度（英文字符=1，中文字符=2）
static int get_char_width_and_copy(unsigned char c, long *pos, long file_size, char *content, char *buffer, int *buf_index) {
    int width = 1;
    long start_pos = *pos;
    
    if (c >= 0xE0 && c <= 0xEF) {  // UTF-8 3字节字符（包括中文）
        width = 2;  // 中文字符宽度为2
        if (*pos + 2 < file_size) {
            // 复制3个字节
            for (int i = 0; i < 3; i++) {
                if (*pos < file_size) {
                    buffer[(*buf_index)++] = content[(*pos)++];
                }
            }
        } else {
            // 文件结束
            (*pos) = file_size;
        }
    } else if (c >= 0xC0 && c <= 0xDF) {  // UTF-8 2字节字符
        width = 1;  // 按1个宽度处理
        if (*pos + 1 < file_size) {
            // 复制2个字节
            for (int i = 0; i < 2; i++) {
                if (*pos < file_size) {
                    buffer[(*buf_index)++] = content[(*pos)++];
                }
            }
        } else {
            (*pos) = file_size;
        }
    } else if (c >= 0x80) {  // 其他多字节字符（应该不会出现）
        width = 1;
        // 跳过无效的多字节字符
        (*pos)++;
    } else {  // ASCII字符
        width = 1;
        buffer[(*buf_index)++] = content[(*pos)++];
    }
    
    return width;
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
        lv_label_set_text(error_label, "error:Unknown");
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
        lv_label_set_text(error_label, "error:Out Of Memory!");
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
    lv_obj_set_size(btn_next, 45, 28);
    lv_obj_align(btn_next, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
    lv_obj_t * btn_next_label = lv_label_create(btn_next);
    lv_label_set_text(btn_next_label, ">");
    lv_obj_center(btn_next_label);
    lv_obj_add_event_cb(btn_next, next_page_click, LV_EVENT_CLICKED, NULL);
    
    lv_obj_t * btn_prev = lv_btn_create(screen);
    lv_obj_set_size(btn_prev, 45, 28);
    lv_obj_align(btn_prev, LV_ALIGN_BOTTOM_RIGHT, -48, 0);
    lv_obj_t * btn_prev_label = lv_label_create(btn_prev);
    lv_label_set_text(btn_prev_label, "<");
    lv_obj_center(btn_prev_label);
    lv_obj_add_event_cb(btn_prev, prev_page_click, LV_EVENT_CLICKED, NULL);
    
    // 初始显示
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
    
    // 1. 先跳过前面页的行
    int lines_to_skip = current_page * MAX_LINES;
    int skipped_lines = 0;
    
    while (pos < file_size && skipped_lines < lines_to_skip) {
        int char_width_count = 0;
        int in_word = 0;
        int last_space_pos = -1;
        int last_space_width = 0;
        
        while (pos < file_size && char_width_count < MAX_CHARS_PER_LINE) {
            unsigned char c = (unsigned char)file_content[pos];
            
            if (c == '\n' || c == '\r') {
                skipped_lines++;
                pos++;
                // 跳过连续换行符
                while (pos < file_size && (file_content[pos] == '\n' || file_content[pos] == '\r')) {
                    pos++;
                    skipped_lines++;
                }
                break;
            }
            
            // 记录空格位置（好的断点）
            if (c == ' ' || c == '\t') {
                last_space_pos = pos;
                last_space_width = char_width_count;
            }
            
            // 检查是否单词字符
            if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || 
                (c >= '0' && c <= '9') || c == '_' || c == '-') {
                in_word = 1;
            } else if (c != ' ' && c != '\t') {
                in_word = 0;
            }
            
            // 获取字符宽度
            int char_width = 1;
            long temp_pos = pos;
            int temp_buf_index = 0;
            char temp_buf[10];
            
            if (c >= 0xE0 && c <= 0xEF) {  // 中文字符
                char_width = 2;
                if (pos + 2 < file_size) {
                    pos += 3;
                } else {
                    pos = file_size;
                }
            } else if (c >= 0xC0 && c <= 0xDF) {  // 2字节UTF-8
                if (pos + 1 < file_size) {
                    pos += 2;
                } else {
                    pos = file_size;
                }
            } else if (c >= 0x80) {
                // 跳过无效字符
                pos++;
            } else {
                pos++;
            }
            
            char_width_count += char_width;
            
            // 检查是否需要换行
            if (char_width_count >= MAX_CHARS_PER_LINE) {
                if (in_word && last_space_pos != -1 && last_space_pos > 0) {
                    // 回退到最后一个空格
                    pos = last_space_pos + 1;
                    char_width_count = last_space_width;
                }
                break;
            }
        }
        
        // 如果一行结束了（非换行符结束）
        if (char_width_count > 0) {
            skipped_lines++;
        }
    }
    
    // 2. 填充当前页内容
    while (pos < file_size && line_count < MAX_LINES) {
        int char_width_count = 0;
        int line_start_buffer = buffer_index;
        long line_start_pos = pos;
        int last_space_buffer = -1;
        int last_space_width = 0;
        long last_space_pos = -1;
        int in_word = 0;
        
        // 构建一行
        while (pos < file_size && char_width_count < MAX_CHARS_PER_LINE) {
            unsigned char c = (unsigned char)file_content[pos];
            
            if (c == '\n' || c == '\r') {
                // 填充行尾空格
                while (char_width_count < MAX_CHARS_PER_LINE) {
                    display_buffer[buffer_index++] = ' ';
                    char_width_count++;
                }
                pos++;
                line_count++;
                // 处理连续换行符
                while (pos < file_size && (file_content[pos] == '\n' || file_content[pos] == '\r')) {
                    pos++;
                    if (line_count < MAX_LINES) {
                        if (line_count > 0) {
                            display_buffer[buffer_index++] = '\n';
                        }
                        // 添加空行
                        for (int i = 0; i < MAX_CHARS_PER_LINE && buffer_index < sizeof(display_buffer) - 1; i++) {
                            display_buffer[buffer_index++] = ' ';
                        }
                        line_count++;
                    }
                }
                break;
            }
            
            // 记录空格位置（好的断点）
            if (c == ' ' || c == '\t') {
                last_space_buffer = buffer_index;
                last_space_width = char_width_count;
                last_space_pos = pos;
                in_word = 0;
            }
            
            // 检查是否在单词中
            int is_word_char = 0;
            if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || 
                (c >= '0' && c <= '9') || c == '_' || c == '-') {
                is_word_char = 1;
                in_word = 1;
            } else if (c != ' ' && c != '\t') {
                in_word = 0;
            }
            
            // 计算字符宽度
            int char_width = 1;
            if (c >= 0xE0 && c <= 0xEF) {  // 中文字符
                char_width = 2;
                if (pos + 2 < file_size) {
                    for (int i = 0; i < 3; i++) {
                        display_buffer[buffer_index++] = file_content[pos++];
                    }
                } else {
                    pos = file_size;
                }
            } else if (c >= 0xC0 && c <= 0xDF) {  // 2字节UTF-8
                char_width = 1;
                if (pos + 1 < file_size) {
                    for (int i = 0; i < 2; i++) {
                        display_buffer[buffer_index++] = file_content[pos++];
                    }
                } else {
                    pos = file_size;
                }
            } else if (c >= 0x80) {
                // 跳过无效字符
                display_buffer[buffer_index++] = '?';
                pos++;
            } else {
                char_width = 1;
                display_buffer[buffer_index++] = file_content[pos++];
            }
            
            char_width_count += char_width;
            
            // 检查是否需要换行
            if (char_width_count >= MAX_CHARS_PER_LINE) {
                // 需要换行
                if (in_word && last_space_buffer != -1 && last_space_pos > line_start_pos) {
                    // 回退到最后一个空格
                    buffer_index = last_space_buffer;
                    // 填充当前行剩余部分
                    while (last_space_width < MAX_CHARS_PER_LINE) {
                        display_buffer[buffer_index++] = ' ';
                        last_space_width++;
                    }
                    // 重置位置到空格后
                    pos = last_space_pos + 1;
                } else {
                    // 没有找到好的空格，就在当前位置换行
                    // 不需要回退，已经在行尾
                }
                break;
            }
        }
        
        // 填充行尾空格（如果是因为字符限制结束）
        if (char_width_count < MAX_CHARS_PER_LINE && pos < file_size && 
            file_content[pos] != '\n' && file_content[pos] != '\r') {
            while (char_width_count < MAX_CHARS_PER_LINE) {
                display_buffer[buffer_index++] = ' ';
                char_width_count++;
            }
        }
        
        // 添加换行符（如果不是最后一行）
        if (line_count < MAX_LINES - 1 && char_width_count == MAX_CHARS_PER_LINE) {
            display_buffer[buffer_index++] = '\n';
        }
        
        if (char_width_count > 0) {
            line_count++;
        }
    }
    
    // 3. 如果行数不足，用空行补齐
    if (line_count < MAX_LINES) {
        for (int i = line_count; i < MAX_LINES; i++) {
            if (i > 0) {
                display_buffer[buffer_index++] = '\n';
            }
            // 添加空格行
            for (int j = 0; j < MAX_CHARS_PER_LINE && buffer_index < sizeof(display_buffer) - 1; j++) {
                display_buffer[buffer_index++] = ' ';
            }
        }
    }
    
    display_buffer[buffer_index] = '\0';
    
    // 4. 更新显示
    lv_label_set_text(text_label, display_buffer);
    
    // 5. 计算百分比（简化计算）
    char page_info[32];
    int percent = 0;
    
    // 重新计算总页数
    long temp_pos = 0;
    int total_lines = 0;
    
    while (temp_pos < file_size) {
        int char_width_count = 0;
        
        while (temp_pos < file_size && char_width_count < MAX_CHARS_PER_LINE) {
            unsigned char c = (unsigned char)file_content[temp_pos];
            
            if (c == '\n' || c == '\r') {
                total_lines++;
                temp_pos++;
                while (temp_pos < file_size && (file_content[temp_pos] == '\n' || file_content[temp_pos] == '\r')) {
                    temp_pos++;
                    total_lines++;
                }
                break;
            }
            
            // 计算字符宽度
            if (c >= 0xE0 && c <= 0xEF) {
                char_width_count += 2;
                temp_pos += 3;
            } else if (c >= 0xC0 && c <= 0xDF) {
                char_width_count += 1;
                temp_pos += 2;
            } else if (c >= 0x80) {
                char_width_count += 1;
                temp_pos++;
            } else {
                char_width_count += 1;
                temp_pos++;
            }
            
            if (char_width_count >= MAX_CHARS_PER_LINE) {
                break;
            }
        }
        
        if (char_width_count > 0) {
            total_lines++;
        }
    }
    
    total_pages = (total_lines + MAX_LINES - 1) / MAX_LINES;
    if (total_pages == 0) total_pages = 1;
    
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
