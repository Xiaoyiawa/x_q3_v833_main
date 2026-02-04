// page_txt.c
#include "page_txt.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LINES_PER_PAGE      9
#define MAX_WIDTH_PER_LINE  26     // 以英文字符为单位，每行28个字符

// 放大比例：英文=2单位，中文=3单位，每行总单位数=28*2=56
#define ENGLISH_WIDTH_UNITS 2
#define CHINESE_WIDTH_UNITS 3
#define LINE_TOTAL_UNITS    (MAX_WIDTH_PER_LINE * ENGLISH_WIDTH_UNITS)  // 56个单位

static lv_obj_t * scr = NULL;
static lv_obj_t * label_text = NULL;
static lv_obj_t * label_percent = NULL;
static lv_obj_t * btn_back = NULL;
static lv_obj_t * btn_prev = NULL;
static lv_obj_t * btn_next = NULL;

static char * file_buf = NULL;
static long   file_len = 0;
static int    cur_page  = 0;
static int    total_pages = 1;

static void btn_back_cb(lv_event_t * e);
static void btn_prev_cb(lv_event_t * e);
static void btn_next_cb(lv_event_t * e);
static void render_current_page(void);
static int  calc_total_pages(void);
static int  utf8_char_width_and_advance(const char ** ppos, long remain);

lv_obj_t * page_txt(char * filename)
{
    cur_page = 0;
    file_buf = NULL;
    file_len = 0;
    total_pages = 1;

    scr = lv_obj_create(lv_scr_act());
    lv_obj_remove_style_all(scr);
    lv_obj_set_size(scr, lv_pct(100), lv_pct(100));
    lv_obj_set_style_bg_color(scr, lv_color_white(), 0);

    // 文本区域
    label_text = lv_label_create(scr);
    lv_obj_set_width(label_text, lv_pct(94));
    lv_obj_align(label_text, LV_ALIGN_TOP_MID, 0, 8);
    lv_label_set_long_mode(label_text, LV_LABEL_LONG_WRAP);

    // 百分比
    label_percent = lv_label_create(scr);
    lv_obj_align(label_percent, LV_ALIGN_BOTTOM_MID, 0, -6);   // 向上偏移 -6 px

    // back 按钮
    btn_back = lv_btn_create(scr);
    lv_obj_set_size(btn_back, lv_pct(25), lv_pct(12));
    lv_obj_align(btn_back, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    lv_obj_t * lbl_back = lv_label_create(btn_back);
    lv_label_set_text(lbl_back, "back");
    lv_obj_center(lbl_back);
    lv_obj_add_event_cb(btn_back, btn_back_cb, LV_EVENT_CLICKED, NULL);

    // 翻页按钮
    btn_prev = lv_btn_create(scr);
    lv_obj_set_size(btn_prev, 45, 28);
    lv_obj_align(btn_prev, LV_ALIGN_BOTTOM_RIGHT, -48, 0);
    lv_obj_t * lbl_prev = lv_label_create(btn_prev);
    lv_label_set_text(lbl_prev, "<");
    lv_obj_center(lbl_prev);
    lv_obj_add_event_cb(btn_prev, btn_prev_cb, LV_EVENT_CLICKED, NULL);

    btn_next = lv_btn_create(scr);
    lv_obj_set_size(btn_next, 45, 28);
    lv_obj_align(btn_next, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
    lv_obj_t * lbl_next = lv_label_create(btn_next);
    lv_label_set_text(lbl_next, ">");
    lv_obj_center(lbl_next);
    lv_obj_add_event_cb(btn_next, btn_next_cb, LV_EVENT_CLICKED, NULL);

    // 读取文件
    FILE * fp = fopen(filename, "rb");
    if (!fp) {
        lv_label_set_text(label_text, "无法打开文件");
        lv_label_set_text(label_percent, "0%");
        return scr;
    }

    fseek(fp, 0, SEEK_END);
    file_len = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    file_buf = malloc(file_len + 1);
    if (!file_buf) {
        fclose(fp);
        lv_label_set_text(label_text, "内存不足");
        lv_label_set_text(label_percent, "0%");
        return scr;
    }

    size_t nread = fread(file_buf, 1, file_len, fp);
    file_buf[nread] = '\0';
    fclose(fp);

    total_pages = calc_total_pages();

    render_current_page();

    return scr;
}

static void btn_back_cb(lv_event_t * e)
{
    if (file_buf) {
        free(file_buf);
        file_buf = NULL;
    }
    page_back();
}

static void btn_prev_cb(lv_event_t * e)
{
    if (cur_page > 0) {
        cur_page--;
        render_current_page();
    }
}

static void btn_next_cb(lv_event_t * e)
{
    if (cur_page < total_pages - 1) {
        cur_page++;
        render_current_page();
    }
}

static void render_current_page(void)
{
    if (!file_buf || file_len == 0) return;

    // 分配足够的缓冲区
    char page_buf[LINES_PER_PAGE * (MAX_WIDTH_PER_LINE * 3 + 2) + 1];
    page_buf[0] = '\0';

    int line_cnt = 0;
    int target_line_start = cur_page * LINES_PER_PAGE;
    int current_line = 0;
    const char * p = file_buf;
    const char * file_end = file_buf + file_len;

    // 跳过前面页的行
    while (p < file_end && current_line < target_line_start) {
        if (*p == '\n') {
            current_line++;
            p++;
            continue;
        }
        
        // 处理一行，直到遇到换行符或文件结束
        int units_used = 0;
        while (p < file_end && *p != '\n' && units_used < LINE_TOTAL_UNITS) {
            long remain = file_end - p;
            int char_units = utf8_char_width_and_advance(&p, remain);
            units_used += char_units;
        }
        
        // 如果是因为换行符结束这一行，跳过换行符
        if (p < file_end && *p == '\n') {
            current_line++;
            p++;
        } else if (units_used >= LINE_TOTAL_UNITS) {
            // 因为达到宽度限制结束这一行
            current_line++;
        }
    }

    // 渲染当前页的10行
    while (line_cnt < LINES_PER_PAGE && p < file_end) {
        char line[MAX_WIDTH_PER_LINE * 3 + 1] = ""; // UTF-8字符最多3字节
        int units_used = 0;
        const char * line_start = p;
        
        // 构建一行
        while (p < file_end && units_used < LINE_TOTAL_UNITS) {
            if (*p == '\n') {
                // 遇到换行符，空行
                p++;
                break;
            }
            
            const char * before = p;
            long remain = file_end - p;
            int char_units = utf8_char_width_and_advance(&p, remain);
            
            if (units_used + char_units <= LINE_TOTAL_UNITS) {
                // 复制字符
                int char_len = p - before;
                strncat(line, before, char_len);
                units_used += char_units;
            } else {
                // 字符超出宽度限制，回退
                p = before;
                break;
            }
        }
        
        // 将行添加到页面缓冲区
        if (line_cnt > 0) {
            strcat(page_buf, "\n");
        }
        strcat(page_buf, line);
        line_cnt++;
    }
    
    // 补空行
    while (line_cnt < LINES_PER_PAGE) {
        if (line_cnt > 0) {
            strcat(page_buf, "\n");
        }
        line_cnt++;
    }

    lv_label_set_text(label_text, page_buf);

    // 更新百分比
    int percent = 0;
    if (total_pages <= 1) {
        percent = 100;
    } else {
        percent = (cur_page * 100) / (total_pages - 1);
        if (cur_page >= total_pages - 1) {
            percent = 100;
        }
    }
    
    char txt[16];
    snprintf(txt, sizeof(txt), "%d%%", percent);
    lv_label_set_text(label_percent, txt);
}

static int calc_total_pages(void)
{
    if (!file_buf || file_len == 0) return 1;

    int lines = 0;
    const char * p = file_buf;
    const char * file_end = file_buf + file_len;

    while (p < file_end) {
        if (*p == '\n') {
            // 换行符算作一行
            lines++;
            p++;
            continue;
        }
        
        // 处理一行中的字符
        int units_used = 0;
        while (p < file_end && *p != '\n' && units_used < LINE_TOTAL_UNITS) {
            long remain = file_end - p;
            int char_units = utf8_char_width_and_advance(&p, remain);
            units_used += char_units;
        }
        
        // 如果是因为换行符结束，换行符会在下一次循环中处理
        // 如果因为达到宽度限制结束，算作一行
        if (units_used >= LINE_TOTAL_UNITS) {
            lines++;
        }
    }

    // 处理最后一行（如果最后一行不是以换行符结束）
    if (p > file_buf && *(p-1) != '\n') {
        lines++;
    }

    if (lines == 0) return 1;
    return (lines + LINES_PER_PAGE - 1) / LINES_PER_PAGE;
}

static int utf8_char_width_and_advance(const char ** ppos, long remain)
{
    const unsigned char * p = (const unsigned char *)*ppos;

    if (remain <= 0) return ENGLISH_WIDTH_UNITS;  // 默认返回英文宽度

    // ASCII字符（英文）- 2个单位
    if (*p < 0x80) {
        (*ppos)++;
        return ENGLISH_WIDTH_UNITS;
    }
    // 3字节UTF-8字符（中文）- 3个单位
    else if (*p >= 0xE0 && *p <= 0xEF && remain >= 3) {
        (*ppos) += 3;
        return CHINESE_WIDTH_UNITS;
    }
    // 2字节UTF-8字符 - 2个单位（和英文一样宽）
    else if (*p >= 0xC0 && *p <= 0xDF && remain >= 2) {
        (*ppos) += 2;
        return ENGLISH_WIDTH_UNITS;
    }
    // 其他情况（包括4字节UTF-8等），按英文宽度处理
    else {
        (*ppos)++;
        return ENGLISH_WIDTH_UNITS;
    }
}