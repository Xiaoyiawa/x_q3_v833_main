// page_txt.c
#include "page_txt.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_LINES 9
#define MAX_CHARS_PER_LINE 26

static void back_click(lv_event_t * e);
static void next_page_click(lv_event_t * e);
static void prev_page_click(lv_event_t * e);
static void update_display(void);
static void build_pages(void);
static void page_txt_on_destroy(void * page); // 新增销毁回调

static int get_utf8_char_len(unsigned char c)
{
    if(c < 0x80)
        return 1;
    else if((c & 0xE0) == 0xC0)
        return 2;
    else if((c & 0xF0) == 0xE0)
        return 3;
    else if((c & 0xF8) == 0xF0)
        return 4;
    else
        return 1;
}

// 字符宽度单位：英文字符=1，中文字符=2
static int get_char_width(unsigned char c)
{
    if(c < 0x80)
        return 1; // ASCII
    else
        return 2; // 非ASCII（中文等）
}

// 判断是否为英文字母
static int is_english_letter(unsigned char c)
{
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
}

static lv_obj_t * screen     = NULL;
static lv_obj_t * text_label = NULL;
static lv_obj_t * page_label = NULL;
static char * file_content   = NULL;
static long file_size        = 0;
static int current_page      = 0;
static int total_pages       = 0;
static long * page_starts    = NULL; // 每页起始位置数组

BasePage * page_txt(char * filename)
{
    screen = lv_obj_create(lv_scr_act());
    lv_obj_remove_style_all(screen);
    lv_obj_set_size(screen, lv_pct(100), lv_pct(100));
    lv_obj_set_style_bg_color(screen, lv_color_white(), 0);

    current_page = 0;

    // 释放之前可能残留的 page_starts
    if(page_starts) {
        free(page_starts);
        page_starts = NULL;
    }

    // 读取文件内容
    FILE * fp = fopen(filename, "r");
    if(fp == NULL) {
        // 文件打开失败显示错误信息
        lv_obj_t * error_label = lv_label_create(screen);
        lv_label_set_text(error_label, "error:can not open file!");
        lv_obj_align(error_label, LV_ALIGN_CENTER, 0, 0);

        // 返回按钮
        lv_obj_t * btn_back = lv_btn_create(screen);
        lv_obj_set_size(btn_back, lv_pct(25), lv_pct(12));
        lv_obj_align(btn_back, LV_ALIGN_BOTTOM_LEFT, 0, 0);
        lv_obj_t * btn_back_label = lv_label_create(btn_back);
        lv_label_set_text(btn_back_label, "back");
        lv_obj_center(btn_back_label);
        lv_obj_add_event_cb(btn_back, back_click, LV_EVENT_CLICKED, NULL);

        // 创建 BasePage 包装错误页面（无资源需释放，on_destroy 留空）
        BasePage * page = malloc(sizeof(BasePage));
        if(page) {
            memset(page, 0, sizeof(BasePage));
            page->obj = screen;
            // 无需回调
        }
        return page;
    }

    // 获取文件大小
    fseek(fp, 0, SEEK_END);
    file_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    // 分配内存并读取文件内容
    file_content = (char *)malloc(file_size + 1);
    if(file_content == NULL) {
        fclose(fp);
        lv_obj_t * error_label = lv_label_create(screen);
        lv_label_set_text(error_label, "error:no more memory!");
        lv_obj_align(error_label, LV_ALIGN_CENTER, 0, 0);

        lv_obj_t * btn_back = lv_btn_create(screen);
        lv_obj_set_size(btn_back, lv_pct(25), lv_pct(12));
        lv_obj_align(btn_back, LV_ALIGN_BOTTOM_LEFT, 0, 0);
        lv_obj_t * btn_back_label = lv_label_create(btn_back);
        lv_label_set_text(btn_back_label, "back");
        lv_obj_center(btn_back_label);
        lv_obj_add_event_cb(btn_back, back_click, LV_EVENT_CLICKED, NULL);

        // 创建 BasePage 包装错误页面
        BasePage * page = malloc(sizeof(BasePage));
        if(page) {
            memset(page, 0, sizeof(BasePage));
            page->obj = screen;
        }
        return page;
    }

    size_t read_size        = fread(file_content, 1, file_size, fp);
    file_content[read_size] = '\0';
    fclose(fp);

    build_pages();

    text_label = lv_label_create(screen);
    lv_obj_set_width(text_label, lv_pct(95));
    lv_obj_align(text_label, LV_ALIGN_TOP_MID, 0, 10);
    lv_label_set_long_mode(text_label, LV_LABEL_LONG_WRAP);

    page_label = lv_label_create(screen);
    lv_obj_align(page_label, LV_ALIGN_BOTTOM_MID, 0, -5);

    lv_obj_t * btn_back = lv_btn_create(screen);
    lv_obj_set_size(btn_back, lv_pct(25), lv_pct(12));
    lv_obj_align(btn_back, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    lv_obj_t * btn_back_label = lv_label_create(btn_back);
    lv_label_set_text(btn_back_label, CUSTOM_SYMBOL_BACK "");
    lv_obj_center(btn_back_label);
    lv_obj_add_event_cb(btn_back, back_click, LV_EVENT_CLICKED, NULL);

    lv_obj_t * btn_prev = lv_btn_create(screen);
    lv_obj_set_size(btn_prev, 43, 30);
    lv_obj_align(btn_prev, LV_ALIGN_BOTTOM_RIGHT, -46, 0);
    lv_obj_t * btn_prev_label = lv_label_create(btn_prev);
    lv_label_set_text(btn_prev_label, "<");
    lv_obj_center(btn_prev_label);
    lv_obj_add_event_cb(btn_prev, prev_page_click, LV_EVENT_CLICKED, NULL);

    lv_obj_t * btn_next = lv_btn_create(screen);
    lv_obj_set_size(btn_next, 43, 30);
    lv_obj_align(btn_next, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
    lv_obj_t * btn_next_label = lv_label_create(btn_next);
    lv_label_set_text(btn_next_label, ">");
    lv_obj_center(btn_next_label);
    lv_obj_add_event_cb(btn_next, next_page_click, LV_EVENT_CLICKED, NULL);

    update_display();

    // 创建 BasePage 包装正常页面，设置销毁回调以释放资源
    BasePage * page = malloc(sizeof(BasePage));
    if(page) {
        memset(page, 0, sizeof(BasePage));
        page->obj        = screen;
        page->on_destroy = page_txt_on_destroy; // 注册销毁回调
        // user_data 未使用
    }
    return page;
}

static void build_pages(void)
{
    if(!file_content || file_size == 0) {
        total_pages    = 1;
        page_starts    = (long *)malloc(sizeof(long));
        page_starts[0] = 0;
        return;
    }

    int max_pages = (file_size + 1) / 1 + 1;
    page_starts   = (long *)malloc(sizeof(long) * max_pages);
    if(!page_starts) {
        total_pages    = 1;
        page_starts    = (long *)malloc(sizeof(long));
        page_starts[0] = 0;
        return;
    }

    int page_idx            = 0;
    long pos                = 0;
    page_starts[page_idx++] = 0;

    while(pos < file_size) {
        int line_count    = 0;
        int current_width = 0;

        while(pos < file_size && line_count < MAX_LINES) {
            unsigned char c = file_content[pos];

            // 处理换行符
            if(c == '\n' || c == '\r') {
                // 当前行如果有内容，则结束一行
                if(current_width > 0) {
                    line_count++;
                    if(line_count >= MAX_LINES) break;
                    current_width = 0;
                }

                // 跳过当前换行符
                if(c == '\r' && pos + 1 < file_size && file_content[pos + 1] == '\n') {
                    pos++; // 跳过 '\n'
                }
                pos++; // 跳过当前换行符

                // 处理连续换行符（空行）
                while(pos < file_size && (file_content[pos] == '\n' || file_content[pos] == '\r')) {
                    line_count++;
                    if(line_count >= MAX_LINES) break;

                    if(file_content[pos] == '\r' && pos + 1 < file_size && file_content[pos + 1] == '\n') {
                        pos++;
                    }
                    pos++;
                }
                continue;
            }

            // 处理英文字母（单词完整性）
            if(is_english_letter(c)) {
                // 找出完整单词
                int word_len   = 0;
                int word_width = 0;
                while(pos + word_len < file_size && is_english_letter(file_content[pos + word_len])) {
                    word_width += 1;
                    word_len++;
                }

                // 判断是否能放入当前行
                if(current_width + word_width <= MAX_CHARS_PER_LINE) {
                    // 可以放入
                    current_width += word_width;
                    pos += word_len;
                    continue;
                } else {
                    // 不能放入
                    if(current_width > 0) {
                        // 当前行非空：换行
                        line_count++;
                        if(line_count >= MAX_LINES) {
                            // 当前页已满，下一页从这个单词开始（即 pos 不变）
                            break;
                        }
                        current_width = 0;
                        // 重新处理这个单词（继续循环）
                        continue;
                    } else {
                        // 当前行为空，但单词太长（不可能超过最大行宽，因为每个字母宽度1，最多26个字母）
                        // 按单个字符处理（防止死循环）
                        current_width += 1;
                        pos += 1;
                        continue;
                    }
                }
            }

            // 处理普通字符
            int len   = get_utf8_char_len(c);
            int width = get_char_width(c);

            if(current_width + width > MAX_CHARS_PER_LINE) {
                // 换行
                line_count++;
                if(line_count >= MAX_LINES) {
                    break; // 页满
                }
                current_width = 0;
                // 重新处理当前字符（不增加 pos）
                continue;
            }

            // 放入字符
            current_width += width;
            pos += len;
        }

        // 当前页结束，记录下一页起始位置
        if(pos < file_size) {
            page_starts[page_idx++] = pos;
        } else {
            break; // 已到文件末尾
        }
    }

    total_pages = page_idx;
}

static void update_display(void)
{
    if(!file_content || !text_label || !page_label || !page_starts) return;

    long start_pos = page_starts[current_page];
    if(start_pos >= file_size) {
        start_pos    = 0;
        current_page = 0;
    }

    // 显示缓冲区
    char display_buffer[2048];
    int buf_idx       = 0;
    int line_count    = 0;
    int current_width = 0;

    long i = start_pos;
    while(i < file_size && line_count < MAX_LINES) {
        unsigned char c = file_content[i];

        // 处理换行符
        if(c == '\n' || c == '\r') {
            if(current_width > 0) {
                if(line_count < MAX_LINES - 1 && buf_idx < sizeof(display_buffer) - 1) {
                    display_buffer[buf_idx++] = '\n';
                }
                line_count++;
                current_width = 0;
                if(line_count >= MAX_LINES) break;
            }

            if(c == '\r' && i + 1 < file_size && file_content[i + 1] == '\n') {
                i++;
            }
            i++;

            while(i < file_size && (file_content[i] == '\n' || file_content[i] == '\r')) {
                if(line_count < MAX_LINES - 1 && buf_idx < sizeof(display_buffer) - 1) {
                    display_buffer[buf_idx++] = '\n';
                }
                line_count++;
                if(line_count >= MAX_LINES) break;

                if(file_content[i] == '\r' && i + 1 < file_size && file_content[i + 1] == '\n') {
                    i++;
                }
                i++;
            }
            if(line_count >= MAX_LINES) break;
            continue;
        }

        // 处理英文字母（单词完整性）
        if(is_english_letter(c)) {
            int word_start = i;
            int word_len   = 0;
            int word_width = 0;
            while(i + word_len < file_size && is_english_letter(file_content[i + word_len])) {
                word_width += 1;
                word_len++;
            }

            if(current_width + word_width <= MAX_CHARS_PER_LINE) {
                // 可以放入
                for(int j = 0; j < word_len; j++) {
                    display_buffer[buf_idx++] = file_content[i + j];
                }
                current_width += word_width;
                i += word_len;
                continue;
            } else {
                // 不能放入
                if(current_width > 0) {
                    // 当前行非空：填充空格至最大宽度，然后换行
                    int fill_spaces = MAX_CHARS_PER_LINE - current_width;
                    for(int s = 0; s < fill_spaces; s++) {
                        if(buf_idx < sizeof(display_buffer) - 1) display_buffer[buf_idx++] = ' ';
                    }
                    if(line_count < MAX_LINES - 1 && buf_idx < sizeof(display_buffer) - 1) {
                        display_buffer[buf_idx++] = '\n';
                    }
                    line_count++;
                    if(line_count >= MAX_LINES) break;
                    current_width = 0;
                    // 重新处理这个单词
                    continue;
                } else {
                    // 当前行为空但单词太长（实际不会发生，但保留处理）
                    // 按单个字符处理
                    display_buffer[buf_idx++] = file_content[i];
                    current_width += 1;
                    i += 1;
                    continue;
                }
            }
        }

        // 处理普通字符
        int len   = get_utf8_char_len(c);
        int width = get_char_width(c);

        if(current_width + width > MAX_CHARS_PER_LINE) {
            if(line_count < MAX_LINES - 1 && buf_idx < sizeof(display_buffer) - 1) {
                display_buffer[buf_idx++] = '\n';
            }
            line_count++;
            if(line_count >= MAX_LINES) break;
            current_width = 0;
            continue;
        }

        for(int j = 0; j < len; j++) {
            if(buf_idx < sizeof(display_buffer) - 1) display_buffer[buf_idx++] = file_content[i + j];
        }
        current_width += width;
        i += len;
    }

    display_buffer[buf_idx] = '\0';
    lv_label_set_text(text_label, display_buffer);

    // 更新页码：当前页+1 / 总页数
    char page_info[32];
    snprintf(page_info, sizeof(page_info), "%d/%d", current_page + 1, total_pages);
    lv_label_set_text(page_label, page_info);
}

static void back_click(lv_event_t * e)
{
    // 资源释放由 on_destroy 回调处理，此处仅返回上一页
    page_back();
}

static void next_page_click(lv_event_t * e)
{
    if(current_page + 1 < total_pages) {
        current_page++;
        update_display();
    }
}

static void prev_page_click(lv_event_t * e)
{
    if(current_page > 0) {
        current_page--;
        update_display();
    }
}

// 销毁回调：释放文件内容和分页索引数组
static void page_txt_on_destroy(void * page)
{
    (void)page; // 参数未使用
    if(file_content) {
        free(file_content);
        file_content = NULL;
    }
    if(page_starts) {
        free(page_starts);
        page_starts = NULL;
    }
    // 注意：screen 对象会被 page_back 自动删除，无需额外操作
}