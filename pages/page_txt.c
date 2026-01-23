// page_txt.c
#include "page_txt.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_LINES 11
#define MAX_CHARS_PER_LINE 31

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
static char display_buffer[(MAX_LINES * (MAX_CHARS_PER_LINE + 1)) + 10];

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
    
    int total_chars = 0;
    for (long i = 0; i < file_size; i++) {
        if (file_content[i] != '\n' && file_content[i] != '\r') {
            total_chars++;
        }
    }
    
    int chars_per_page = MAX_LINES * MAX_CHARS_PER_LINE;
    total_pages = (total_chars + chars_per_page - 1) / chars_per_page;
    if (total_pages == 0) total_pages = 1;
    
    text_label = lv_label_create(screen);
    lv_obj_set_size(text_label, 235, 205);
    lv_obj_align(text_label, LV_ALIGN_TOP_MID, 0, 10);
    lv_label_set_long_mode(text_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_align(text_label, LV_TEXT_ALIGN_LEFT, 0);
    
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
    lv_obj_set_size(btn_next, 45, 27);
    lv_obj_align(btn_next, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
    lv_obj_t * btn_next_label = lv_label_create(btn_next);
    lv_label_set_text(btn_next_label, ">");
    lv_obj_center(btn_next_label);
    lv_obj_add_event_cb(btn_next, next_page_click, LV_EVENT_CLICKED, NULL);
    
    lv_obj_t * btn_prev = lv_btn_create(screen);
    lv_obj_set_size(btn_prev, 45, 27);
    lv_obj_align(btn_prev, LV_ALIGN_BOTTOM_RIGHT, -48, 0);
    lv_obj_t * btn_prev_label = lv_label_create(btn_prev);
    lv_label_set_text(btn_prev_label, "<");
    lv_obj_center(btn_prev_label);
    lv_obj_add_event_cb(btn_prev, prev_page_click, LV_EVENT_CLICKED, NULL);
    
    update_display();
    
    return screen;
}

static void update_display(void) {
    if (!file_content || !text_label || !page_label) return;
    
    memset(display_buffer, 0, sizeof(display_buffer));
    
    int chars_per_page = MAX_LINES * MAX_CHARS_PER_LINE;
    int start_char_index = current_page * chars_per_page;
    int buffer_index = 0;
    int line_count = 0;
    int char_in_line = 0;
    int total_chars_processed = 0;
    
    for (long i = 0; i < file_size && line_count < MAX_LINES; i++) {
        char c = file_content[i];
        
        if (c == '\n' || c == '\r') {
            continue;
        }
        
        total_chars_processed++;
        
        if (total_chars_processed > start_char_index) {
            display_buffer[buffer_index++] = c;
            char_in_line++;
            
            if (char_in_line >= MAX_CHARS_PER_LINE) {
                if (line_count < MAX_LINES - 1) {
                    display_buffer[buffer_index++] = '\n';
                }
                line_count++;
                char_in_line = 0;
                
                if (line_count >= MAX_LINES) {
                    break;
                }
            }
        }
    }
    
    if (char_in_line > 0 && char_in_line < MAX_CHARS_PER_LINE) {
        while (char_in_line < MAX_CHARS_PER_LINE && buffer_index < sizeof(display_buffer) - 1) {
            display_buffer[buffer_index++] = ' ';
            char_in_line++;
        }
        line_count++;
    }
    
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