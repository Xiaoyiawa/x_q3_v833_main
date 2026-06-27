#include "page_file_manager.h"

#include <stdio.h>
#include <sys/stat.h>
#include <stdbool.h>
#include "page_apple.h"
#include "page_audio.h"
#include "page_midi.h"
#include "page_image.h"
#include "page_txt.h"
#include "platform/str_utils.h"
#include "views/custom_msgbox.h"

typedef enum { FILE_OPERATION_NONE = 0, FILE_OPERATION_CUT, FILE_OPERATION_COPY } file_operation_t;

typedef struct
{
    BasePage base;
    lv_obj_t * file_explorer;
    lv_obj_t * container_act;
    lv_obj_t * label_file_name;
    char file_current[LV_100ASK_FILE_EXPLORER_PATH_MAX_LEN];
    char file_clipboard[LV_100ASK_FILE_EXPLORER_PATH_MAX_LEN];
    file_operation_t file_operation;
} FileManagerPage;

static const char * btn_txts[] = {"YES", "NO", NULL};

static lv_obj_t * page_file_manager_obj(FileManagerPage * page);
static bool is_blacklisted(const char * path);
static void explorer_event_handler(lv_event_t * e);
static void back_click(lv_event_t * e);
static void container_act_click(lv_event_t * e);
static void act_cut_click(lv_event_t * e);
static void act_copy_click(lv_event_t * e);
static void act_paste_click(lv_event_t * e);
static void act_delete_click(lv_event_t * e);
static void act_msgbox_delete(lv_event_t * e);
static void act_msgbox_paste(lv_event_t * e);
static bool page_file_manager_on_key(void * p, key_code_t key_code, key_action_t key_action);

static bool is_blacklisted(const char * path)
{
    char tmp[LV_100ASK_FILE_EXPLORER_PATH_MAX_LEN];
    strcpy(tmp, path);

    // 去除末尾多余的斜杠（但保留根目录的单个斜杠）
    size_t len = strlen(tmp);
    if (len > 1 && tmp[len-1] == '/') {
        tmp[len-1] = '\0';
    }

    // 1. 根目录本身
    if (strcmp(tmp, "/") == 0) return true;

    // 2. 根目录下的系统文件夹（禁止目录本身及其所有子内容）
    // 这些是常见的系统关键目录，禁止任何操作
    if (strcmp(tmp, "/bin") == 0 || str_begin_with(tmp, "/bin/", true)) return true;
    if (strcmp(tmp, "/www") == 0 || str_begin_with(tmp, "/boot/", true)) return true;
    if (strcmp(tmp, "/dev") == 0 || str_begin_with(tmp, "/dev/", true)) return true;
    if (strcmp(tmp, "/etc") == 0 || str_begin_with(tmp, "/etc/", true)) return true;
    if (strcmp(tmp, "/lib") == 0 || str_begin_with(tmp, "/lib/", true)) return true;
    if (strcmp(tmp, "/proc") == 0 || str_begin_with(tmp, "/proc/", true)) return true;
    if (strcmp(tmp, "/sbin") == 0 || str_begin_with(tmp, "/sbin/", true)) return true;
    if (strcmp(tmp, "/sys") == 0 || str_begin_with(tmp, "/sys/", true)) return true;
    if (strcmp(tmp, "/usr") == 0 || str_begin_with(tmp, "/usr/", true)) return true;
    if (strcmp(tmp, "/var") == 0 || str_begin_with(tmp, "/var/", true)) return true;
    if (strcmp(tmp, "/root") == 0 || str_begin_with(tmp, "/root/", true)) return true;
    if (strcmp(tmp, "/data") == 0 || str_begin_with(tmp, "/data/", true)) return true;
    if (strcmp(tmp, "/overlay") == 0 || str_begin_with(tmp, "/overlay/", true)) return true;
    if (strcmp(tmp, "/home") == 0 || str_begin_with(tmp, "/home/", true)) return true;
    if (strcmp(tmp, "/tmp") == 0 || str_begin_with(tmp, "/tmp/", true)) return true;
    if (strcmp(tmp, "/squashfs") == 0 || str_begin_with(tmp, "/squashfs/", true)) return true;
    if (strcmp(tmp, "/run") == 0 || str_begin_with(tmp, "/run/", true)) return true;
    if (strcmp(tmp, "/rom") == 0 || str_begin_with(tmp, "/rom/", true)) return true;

    if (strcmp(tmp, "/mnt") == 0) return true;
    if (strcmp(tmp, "/mnt/UDISK") == 0) return true;
    if (strcmp(tmp, "/mnt/UDISK/startup.sh") == 0) return true;
    if (strcmp(tmp, "/mnt/UDISK/lvgl") == 0 || str_begin_with(tmp, "/mnt/UDISK/lvgl/", true)) return true;
    if (strcmp(tmp, "/mnt/sdcard") == 0) return true;

    /* 7. /mnt/app 规则：
      - /mnt/app 本身禁止
      - /mnt/app/ 下的其他子目录禁止，但 /mnt/app/dendro 整棵子树允许 */
    if (str_begin_with(tmp, "/mnt/app", true)) {
        if (strcmp(tmp, "/mnt/app") == 0) return true;
        if (strcmp(tmp, "/mnt/app/dendro") == 0 || str_begin_with(tmp, "/mnt/app/dendro/", true)) return false; // 例外允许
        return true; // 其他 /mnt/app/* 禁止
    }

    // 其余所有路径允许操作
    return false;
}

static bool is_file_safe(char * file_name)
{
    return !is_blacklisted(file_name);
}

static bool is_directory_safe(char * file_name)
{
    return !is_blacklisted(file_name);
}

static bool is_directory(char * file_name)
{
    struct stat s_buf;

    if(stat(file_name, &s_buf) == 0) {
        if(S_ISDIR(s_buf.st_mode)) {
            return true;
        }
    } else {
        perror("is_directory");
        return false;
    }
    return false;
}

BasePage * page_file_manager_create(void)
{
    FileManagerPage * page = malloc(sizeof(FileManagerPage));
    if(!page) return NULL;
    memset(page, 0, sizeof(FileManagerPage));

    page->base.obj        = page_file_manager_obj(page);
    page->base.on_key     = page_file_manager_on_key;
    return (BasePage *)page;
}

lv_obj_t * page_file_manager_obj(FileManagerPage * page)
{
    lv_obj_t * screen = lv_obj_create(lv_scr_act());
    lv_obj_remove_style_all(screen);
    lv_obj_set_size(screen, lv_pct(100), lv_pct(100));

    lv_obj_t * file_explorer = lv_100ask_file_explorer_create(screen);
    lv_obj_add_event_cb(file_explorer, explorer_event_handler, 
							LV_EVENT_ALL, page);
    lv_100ask_file_explorer_open_dir(file_explorer, "//mnt");
    page->file_explorer = file_explorer;

    lv_obj_t * btn_back = lv_btn_create(screen);
    lv_obj_set_size(btn_back, lv_pct(25), lv_pct(12));
    lv_obj_align(btn_back, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_t * btn_back_label = lv_label_create(btn_back);
    lv_label_set_text(btn_back_label, CUSTOM_SYMBOL_BACK "");
    lv_obj_center(btn_back_label);
    lv_obj_add_event_cb(btn_back, back_click, LV_EVENT_CLICKED, page);

    lv_obj_t * container_act = lv_obj_create(screen);
    lv_obj_remove_style_all(container_act);
    lv_obj_clear_flag(container_act, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(container_act, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_size(container_act, lv_pct(100), lv_pct(88));
    lv_obj_set_style_bg_opa(container_act, LV_OPA_60, LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(container_act, lv_color_black(), LV_STATE_DEFAULT);
    lv_obj_add_flag(container_act, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_event_cb(container_act, container_act_click, LV_EVENT_CLICKED, page);
    page->container_act = container_act;

    lv_obj_t * list_act = lv_obj_create(container_act);
    lv_obj_align(list_act, LV_ALIGN_TOP_RIGHT, 0, 0);
    lv_obj_set_size(list_act, lv_pct(75), lv_pct(100));
    lv_obj_set_flex_flow(list_act, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_scroll_dir(list_act, LV_DIR_VER);

    lv_obj_t * label_file_name = lv_label_create(list_act);
    lv_label_set_long_mode(label_file_name, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_width(label_file_name, lv_pct(100));
    page->label_file_name = label_file_name;

    lv_obj_t * btn_cut = lv_btn_create(list_act);
    lv_obj_set_size(btn_cut, lv_pct(100), lv_pct(22));
    lv_obj_t * btn_label_cut = lv_label_create(btn_cut);
    lv_label_set_text(btn_label_cut, "剪贴");
    lv_obj_center(btn_label_cut);
    lv_obj_add_event_cb(btn_cut, act_cut_click, LV_EVENT_CLICKED, page);

    lv_obj_t * btn_copy = lv_btn_create(list_act);
    lv_obj_set_size(btn_copy, lv_pct(100), lv_pct(22));
    lv_obj_t * btn_label_copy = lv_label_create(btn_copy);
    lv_label_set_text(btn_label_copy, "复制");
    lv_obj_center(btn_label_copy);
    lv_obj_add_event_cb(btn_copy, act_copy_click, LV_EVENT_CLICKED, page);

    lv_obj_t * btn_paste = lv_btn_create(list_act);
    lv_obj_set_size(btn_paste, lv_pct(100), lv_pct(22));
    lv_obj_t * btn_label_paste = lv_label_create(btn_paste);
    lv_label_set_text(btn_label_paste, "粘贴");
    lv_obj_center(btn_label_paste);
    lv_obj_add_event_cb(btn_paste, act_paste_click, LV_EVENT_CLICKED, page);

    lv_obj_t * btn_delete = lv_btn_create(list_act);
    lv_obj_set_size(btn_delete, lv_pct(100), lv_pct(22));
    lv_obj_set_style_bg_color(btn_delete, lv_palette_main(LV_PALETTE_RED), LV_STATE_DEFAULT);
    lv_obj_t * btn_label_delete = lv_label_create(btn_delete);
    lv_label_set_text(btn_label_delete, "删除");
    lv_obj_center(btn_label_delete);
    lv_obj_add_event_cb(btn_delete, act_delete_click, LV_EVENT_CLICKED, page);

    return screen;
}

static void explorer_event_handler(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t * obj       = lv_event_get_target(e);
    FileManagerPage * page = (FileManagerPage *)e->user_data;

    // file_explorer获取的路径形如 "//mnt/UDISK/lvgl/"
    // 将其指针向前移动一位，路径变为 "/mnt/UDISK/lvgl/"
    char * cur_path = lv_100ask_file_explorer_get_cur_path(obj) + 1;
    char * sel_fn   = lv_100ask_file_explorer_get_sel_fn(obj);
    char * file_name = &page->file_current[0];
    lv_snprintf(file_name, sizeof(page->file_current), "%s%s", cur_path, sel_fn);

    if(code == LV_EVENT_CLICKED) {
        printf("[file_manager] clicked %s\n", file_name);

        if(str_end_with(file_name, ".png", false) || str_end_with(file_name, ".jpg", false) ||
            str_end_with(file_name, ".jpeg", false) || str_end_with(file_name, ".bmp", false) ||
            str_end_with(file_name, ".gif", false)) 
            {
                page_open(page_image_create(file_name));
            }

        if(str_end_with(file_name, ".mp3", false) || str_end_with(file_name, ".wav", false) ||
            str_end_with(file_name, ".ogg", false) || str_end_with(file_name, ".m4a", false) ||
            str_end_with(file_name, ".aac", false) || str_end_with(file_name, ".pcm", false))
            {
                page_open(page_audio_create(file_name));
            }
            
        if(str_end_with(file_name, ".mp4", false) || str_end_with(file_name, ".avi", false)) {
            page_open(page_video_create(file_name));
        }

        if(str_end_with(file_name, ".mid", false) || str_end_with(file_name, ".midi", false)) {
            page_open(page_midi_create(file_name));
        }

        if(str_end_with(file_name, ".txt", false) || str_end_with(file_name, ".json", false) || 
            str_end_with(file_name, ".conf", false) || str_end_with(file_name, ".log", false) ||
            str_end_with(file_name, ".cfg", false) || str_end_with(file_name, ".sh", false)) {
            page_open(page_txt_create(file_name));
        }   
    }

    if(code == LV_EVENT_LONG_PRESSED) {
        printf("[file_manager] long-pressed %s\n", file_name);
        lv_label_set_text(page->label_file_name, sel_fn);
        lv_obj_clear_flag(page->container_act, LV_OBJ_FLAG_HIDDEN);
    }
}

static void act_cut_click(lv_event_t * e)
{
    FileManagerPage * page = (FileManagerPage *)e->user_data;

    if(!is_file_safe(page->file_current)) {
        custom_toast_create("不允许的操作! ");
        return;
    }

    strcpy(page->file_clipboard, page->file_current);
    page->file_operation = FILE_OPERATION_CUT;
    lv_obj_add_flag(page->container_act, LV_OBJ_FLAG_HIDDEN);
    custom_toast_create("已剪贴");
}

static void act_copy_click(lv_event_t * e)
{
    FileManagerPage * page = (FileManagerPage *)e->user_data;
    strcpy(page->file_clipboard, page->file_current);
    page->file_operation = FILE_OPERATION_COPY;
    lv_obj_add_flag(page->container_act, LV_OBJ_FLAG_HIDDEN);
    custom_toast_create("已复制");
}

static void act_paste_click(lv_event_t * e)
{
    FileManagerPage * page = (FileManagerPage *)e->user_data;

    if(!is_directory(page->file_current)) {
        custom_toast_create("请选择一个文件夹来粘贴");
        return;
    }

    if(!is_directory_safe(page->file_current)) {
        custom_toast_create("不允许的操作! ");
        return;
    }

    lv_obj_t * mbox = NULL;
    switch (page->file_operation)
    {
    case FILE_OPERATION_CUT:
        mbox = custom_msgbox_create("移动文件:", 
                page->file_clipboard,
                btn_txts, false);
        break;
        
    case FILE_OPERATION_COPY:
        mbox = custom_msgbox_create("复制文件:", 
                page->file_clipboard,
                btn_txts, false);
        break;
    
    default:
        custom_toast_create("剪贴板是空的");
        break;
    }

    if (mbox) lv_obj_add_event_cb(mbox, act_msgbox_paste, LV_EVENT_VALUE_CHANGED, page);
}

static void act_delete_click(lv_event_t * e)
{
    FileManagerPage * page = (FileManagerPage *)e->user_data;
    if(!is_file_safe(page->file_current)) {
        custom_toast_create("不允许的操作!");
        return;
    }
    lv_obj_t * mbox = custom_msgbox_create("要删除文件吗？", 
                lv_100ask_file_explorer_get_sel_fn(page->file_explorer),
                btn_txts, false);
    lv_obj_add_event_cb(mbox, act_msgbox_delete, LV_EVENT_VALUE_CHANGED, page);
}

static void act_msgbox_delete(lv_event_t * e)
{
    FileManagerPage * page = (FileManagerPage *)e->user_data;
    // 请注意：这里的消息是冒泡上来的，target获取到的是里面的btn_matrix
    lv_obj_t * msgbox = lv_obj_get_parent(lv_event_get_target(e));
    char * txt = lv_msgbox_get_active_btn_text(msgbox);

    if(strcmp(txt, "YES") == 0) {
        char * file_name = &page->file_current[0];
        int cmd_length   = 10 + strlen(file_name);
        char * cmd       = malloc(cmd_length);
        if(cmd == NULL) { perror("malloc"); exit(EXIT_FAILURE); }
        lv_snprintf(cmd, cmd_length, "rm -rf \"%s\"", file_name);

        printf("[file_manager] %s\n", cmd);
        char toast_msg[512];
        lv_snprintf(toast_msg, sizeof(toast_msg), "已删除 %s", file_name);
        custom_toast_create(toast_msg);
        system(cmd);
        free(cmd);

        lv_100ask_file_explorer_refresh(page->file_explorer);
        lv_obj_add_flag(page->container_act, LV_OBJ_FLAG_HIDDEN);
    }
    lv_msgbox_close_async(msgbox);
}

static void act_msgbox_paste(lv_event_t * e)
{
    FileManagerPage * page = (FileManagerPage *)e->user_data;
    // 请注意：这里的消息是冒泡上来的，target获取到的是里面的btn_matrix
    lv_obj_t * msgbox = lv_obj_get_parent(lv_event_get_target(e));
    char * txt = lv_msgbox_get_active_btn_text(msgbox);

    if (strcmp(txt, "YES") == 0) {
        char * file_current   = &page->file_current[0];
        char * file_clipboard = &page->file_clipboard[0];

        char * cmd;
        int cmd_length;

        switch(page->file_operation) {
            case FILE_OPERATION_CUT:
                cmd_length = 12 + strlen(file_clipboard) + strlen(file_current);
                cmd        = malloc(cmd_length);
                if(cmd == NULL) { perror("malloc"); exit(EXIT_FAILURE); }
                lv_snprintf(cmd, cmd_length, "mv -f \"%s\" \"%s\"", file_clipboard, file_current);
                break;
            case FILE_OPERATION_COPY:
                cmd_length = 15 + strlen(file_clipboard) + strlen(file_current);
                cmd        = malloc(cmd_length);
                if(cmd == NULL) { perror("malloc"); exit(EXIT_FAILURE); }
                lv_snprintf(cmd, cmd_length, "cp -rf \"%s\" \"%s\"", file_clipboard, file_current);
                break;
            default: custom_toast_create("未知的操作!"); return;
        }

        printf("[file_manager] %s\n", cmd);
        custom_toast_create(cmd);
        system(cmd);
        free(cmd);

        lv_100ask_file_explorer_refresh(page->file_explorer);
        lv_obj_add_flag(page->container_act, LV_OBJ_FLAG_HIDDEN);
        lv_msgbox_close_async(msgbox);
        page->file_operation = FILE_OPERATION_NONE;
    }
    else {
        lv_msgbox_close_async(msgbox);
    }
}

static void back_click(lv_event_t * e)
{
    FileManagerPage * page = (FileManagerPage *)e->user_data;
    if(!lv_obj_has_flag(page->container_act, LV_OBJ_FLAG_HIDDEN)) 
        lv_obj_add_flag(page->container_act, LV_OBJ_FLAG_HIDDEN);
    else page_back();
}

static void container_act_click(lv_event_t * e)
{
    FileManagerPage * page = (FileManagerPage *)e->user_data;
    lv_obj_add_flag(page->container_act, LV_OBJ_FLAG_HIDDEN);
}

static bool page_file_manager_on_key(void * p, key_code_t key_code, key_action_t key_action)
{
    if(!p) return false;
    if(key_code != KEY_CODE_HOME) return false;
    if(key_action == KEY_ACTION_DOWN) return true;
    // KEY_CODE_HOME & KEY_ACTION_UP

    FileManagerPage * page = (FileManagerPage *)p;

    if(!lv_obj_has_flag(page->container_act, LV_OBJ_FLAG_HIDDEN)) {
        lv_obj_add_flag(page->container_act, LV_OBJ_FLAG_HIDDEN);
        return true;
    }

    char * cur_path        = lv_100ask_file_explorer_get_cur_path(page->file_explorer);

    //printf("%s\n", cur_path);
    if(strcmp(cur_path, "//") == 0) return false;

    char parent_path[LV_100ASK_FILE_EXPLORER_PATH_MAX_LEN];
    strcpy(parent_path, cur_path);

    // 路径的最后还有一个斜杠，需要截取两次
    char * last_slash = strrchr(parent_path, '/');
    if(last_slash) *last_slash = '\0';
    last_slash = strrchr(parent_path, '/');
    if(last_slash) *last_slash = '\0';
    
    lv_100ask_file_explorer_open_dir(page->file_explorer, parent_path);

    return true;
}