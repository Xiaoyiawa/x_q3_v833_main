// page_txt.c
#include "page_txt.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_LINES 12          // 每页最大行数
#define MAX_CHARS_PER_LINE 31  // 每行最大字符数
#define PAGE_SIZE (MAX_LINES * MAX_CHARS_PER_LINE)  // 每页最大字符数

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
    
    // 添加：每次打开文件时重置为第一页
    current_page = 0;  // 确保每次打开都从第一页开始
    
    // 读取文件内容
    FILE *fp = fopen(filename, "r");
    if (fp == NULL) {
        // 文件打开失败，显示错误信息
        lv_obj_t * error_label = lv_label_create(screen);
        lv_label_set_text(error_label, "无法打开文件");
        lv_obj_align(error_label, LV_ALIGN_CENTER, 0, 0);
        
        // 添加返回按钮
        lv_obj_t * btn_back = lv_btn_create(screen);
        lv_obj_set_size(btn_back, lv_pct(25), lv_pct(12));
        lv_obj_align(btn_back, LV_ALIGN_BOTTOM_LEFT, 0, 0);
        lv_obj_t * btn_back_label = lv_label_create(btn_back);
        lv_label_set_text(btn_back_label, "back");
        lv_obj_center(btn_back_label);
        lv_obj_add_event_cb(btn_back, back_click, LV_EVENT_CLICKED, NULL);
        
        return screen;
    }
    
    // 获取文件大小
    fseek(fp, 0, SEEK_END);
    file_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    
    // 分配内存并读取文件内容
    file_content = (char *)malloc(file_size + 1);
    if (file_content == NULL) {
        fclose(fp);
        // 内存分配失败
        lv_obj_t * error_label = lv_label_create(screen);
        lv_label_set_text(error_label, "内存不足");
        lv_obj_align(error_label, LV_ALIGN_CENTER, 0, 0);
        
        // 添加返回按钮
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
    file_content[read_size] = '\0';  // 确保字符串以NULL结尾
    fclose(fp);
    
    // 计算总页数
    total_pages = (file_size + PAGE_SIZE - 1) / PAGE_SIZE;
    if (total_pages == 0) total_pages = 1;
    
    // 创建文本显示标签
    text_label = lv_label_create(screen);
    lv_obj_set_width(text_label, lv_pct(95));
    lv_obj_align(text_label, LV_ALIGN_TOP_MID, 0, 10);
    lv_label_set_long_mode(text_label, LV_LABEL_LONG_WRAP);
    
    // 创建页码标签（显示百分比）- 放在最底部向上5个像素
    page_label = lv_label_create(screen);
    lv_obj_align(page_label, LV_ALIGN_BOTTOM_MID, 0, -5);
    
    // 左下角返回按钮 - 使用原来的大小，不向上移动
    lv_obj_t * btn_back = lv_btn_create(screen);
    lv_obj_set_size(btn_back, lv_pct(25), lv_pct(12));
    lv_obj_align(btn_back, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    lv_obj_t * btn_back_label = lv_label_create(btn_back);
    lv_label_set_text(btn_back_label, "back");
    lv_obj_center(btn_back_label);
    lv_obj_add_event_cb(btn_back, back_click, LV_EVENT_CLICKED, NULL);
    
    // 翻页按钮 - 左箭头，大小为back按钮的1/2
    lv_obj_t * btn_prev = lv_btn_create(screen);
    lv_obj_set_size(btn_prev, 45, 27);  // back按钮100x50的1/2
    lv_obj_align(btn_prev, LV_ALIGN_BOTTOM_RIGHT, -48, 0);  // 向右53像素（50宽度+3间隔），不向上移动
    lv_obj_t * btn_prev_label = lv_label_create(btn_prev);
    lv_label_set_text(btn_prev_label, "<");
    lv_obj_center(btn_prev_label);
    lv_obj_add_event_cb(btn_prev, prev_page_click, LV_EVENT_CLICKED, NULL);
    
    // 翻页按钮 - 右箭头，大小为back按钮的1/2
    lv_obj_t * btn_next = lv_btn_create(screen);
    lv_obj_set_size(btn_next, 45, 27);  // back按钮100x50的1/2
    lv_obj_align(btn_next, LV_ALIGN_BOTTOM_RIGHT, 0, 0);  // 右下角，不向上移动
    lv_obj_t * btn_next_label = lv_label_create(btn_next);
    lv_label_set_text(btn_next_label, ">");
    lv_obj_center(btn_next_label);
    lv_obj_add_event_cb(btn_next, next_page_click, LV_EVENT_CLICKED, NULL);
    
    // 初始显示第一页
    update_display();
    
    return screen;
}

static void update_display(void) {
    if (!file_content || !text_label || !page_label) return;
    
    // 计算当前页的起始位置
    long start_pos = current_page * PAGE_SIZE;
    if (start_pos >= file_size) {
        start_pos = 0;
        current_page = 0;
    }
    
    // 准备显示缓冲区
    memset(display_buffer, 0, sizeof(display_buffer));
    int buffer_index = 0;
    int line_count = 0;
    int char_count = 0;
    
    // 严格限制：每行31个字符，最多12行
    for (long i = start_pos; i < file_size && line_count < MAX_LINES; i++) {
        char c = file_content[i];
        
        // 处理换行符和回车符
        if (c == '\n' || c == '\r') {
            // 填充当前行到31个字符
            while (char_count < MAX_CHARS_PER_LINE && buffer_index < sizeof(display_buffer) - 1) {
                display_buffer[buffer_index++] = ' ';
                char_count++;
            }
            
            // 跳过连续的换行符和回车符
            while ((i + 1 < file_size) && (file_content[i + 1] == '\n' || file_content[i + 1] == '\r')) {
                i++;
            }
            
            // 检查是否已经达到最大行数
            if (line_count >= MAX_LINES - 1) {
                break;
            }
            
            // 只有行数未满且字符数达到31时才换行
            if (char_count == MAX_CHARS_PER_LINE && line_count < MAX_LINES - 1) {
                display_buffer[buffer_index++] = '\n';
                line_count++;
                char_count = 0;
            }
            
            continue;
        }
        
        // 添加字符到缓冲区
        if (char_count < MAX_CHARS_PER_LINE && buffer_index < sizeof(display_buffer) - 1) {
            display_buffer[buffer_index++] = c;
            char_count++;
        }
        
        // 当一行达到31个字符时，换行
        if (char_count >= MAX_CHARS_PER_LINE) {
            if (line_count < MAX_LINES - 1 && buffer_index < sizeof(display_buffer) - 1) {
                display_buffer[buffer_index++] = '\n';
            }
            
            line_count++;
            char_count = 0;
            
            // 如果已经达到最大行数，停止处理
            if (line_count >= MAX_LINES) {
                break;
            }
        }
    }
    
    // 处理最后一行的填充（如果最后一行不足31个字符）
    if (char_count > 0 && line_count < MAX_LINES) {
        while (char_count < MAX_CHARS_PER_LINE && buffer_index < sizeof(display_buffer) - 1) {
            display_buffer[buffer_index++] = ' ';
            char_count++;
        }
        line_count++;
    }
    
    // 确保字符串以NULL结尾
    display_buffer[buffer_index] = '\0';
    
    // 更新文本显示
    lv_label_set_text(text_label, display_buffer);
    
    // 计算并显示百分比
    char page_info[32];
    int percent = 0;
    if (total_pages > 0) {
        percent = (current_page * 100) / total_pages;
    }
    if (current_page >= total_pages - 1) {
        percent = 100;  // 最后一页显示100%
    }
    snprintf(page_info, sizeof(page_info), "%d%%", percent);
    lv_label_set_text(page_label, page_info);
}

static void back_click(lv_event_t * e) {
    // 释放文件内容内存
    if (file_content) {
        free(file_content);
        file_content = NULL;
    }
    page_back();
}

static void next_page_click(lv_event_t * e) {
    // 翻到下一页
    current_page++;
    if (current_page >= total_pages) {
        current_page = total_pages - 1;  // 保持在最后一页
    }
    update_display();
}

static void prev_page_click(lv_event_t * e) {
    // 翻到上一页
    current_page--;
    if (current_page < 0) {
        current_page = 0;  // 保持在第一页
    }
    update_display();
}