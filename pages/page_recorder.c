#include "page_recorder.h"

extern void sys_set_dont_deep_sleep(bool b);
extern void sys_set_dont_timeout(bool b);

typedef struct RecorderPage RecorderPage;

struct RecorderPage {
    BasePage base;

    lv_obj_t *label_space;
    lv_obj_t *label_status;
    lv_obj_t *label_duration;
    lv_obj_t *label_filename;
    lv_obj_t *btn_record;
    lv_obj_t *btn_record_label;
    lv_obj_t *btn_back;
    lv_obj_t *btn_delete;

    lv_obj_t *dialog_bg;
    lv_obj_t *dialog_box;
    lv_obj_t *dialog_label;
    lv_obj_t *dialog_btn_confirm;
    lv_obj_t *dialog_btn_cancel;
    bool      dialog_showing;

    AlsaRecorder *recorder;
    char current_filename[256];
    lv_timer_t *timer;
    bool file_saved;
};

typedef struct {
    RecorderPage *page;
    bool saved;
} finish_msg_t;

static lv_obj_t *recorder_ui_create(RecorderPage *page);
static void recorder_on_create(void *p);
static void recorder_on_destroy(void *p);
static bool recorder_on_key(void *p, key_code_t key_code, key_action_t key_action);
static void btn_record_cb(lv_event_t *e);
static void btn_back_cb(lv_event_t *e);
static void btn_delete_cb(lv_event_t *e);
static void dialog_confirm_cb(lv_event_t *e);
static void dialog_cancel_cb(lv_event_t *e);
static void timer_cb(lv_timer_t *timer);
static void recorder_finish_cb(void *user_data);
static void recorder_finish_ui_cb(void *user_data);
static int ensure_rec_dir_exists(void);
static void update_space_info(RecorderPage *page);
static void generate_filename(char *buffer, size_t size);
static void update_ui_state(RecorderPage *page);
static void show_delete_dialog(RecorderPage *page);
static void hide_delete_dialog(RecorderPage *page);
static void delete_all_recordings(RecorderPage *page);

BasePage *recorder_page_create(void)
{
    RecorderPage *page = malloc(sizeof(RecorderPage));
    if (!page) return NULL;
    memset(page, 0, sizeof(RecorderPage));

    page->base.obj = recorder_ui_create(page);
    page->base.on_create = recorder_on_create;
    page->base.on_destroy = recorder_on_destroy;
    page->base.on_key = recorder_on_key;

    RECORDER_DEBUG("Page created");
    return (BasePage *)page;
}

static lv_obj_t *recorder_ui_create(RecorderPage *page)
{
    lv_obj_t *scr = lv_obj_create(lv_scr_act());
    lv_obj_remove_style_all(scr);
    lv_obj_set_size(scr, lv_pct(100), lv_pct(100));
    lv_obj_set_style_bg_color(scr, lv_color_white(), 0);

    page->label_space = lv_label_create(scr);
    lv_obj_set_width(page->label_space, lv_pct(90));
    lv_obj_align(page->label_space, LV_ALIGN_TOP_MID, 0, 10);
    lv_obj_set_style_text_align(page->label_space, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(page->label_space, lv_color_hex(0x888888), 0);

    page->label_status = lv_label_create(scr);
    lv_obj_set_width(page->label_status, lv_pct(90));
    lv_obj_align(page->label_status, LV_ALIGN_TOP_MID, 0, 35);
    lv_label_set_text(page->label_status, "状态: 空闲");
    lv_obj_set_style_text_align(page->label_status, LV_TEXT_ALIGN_CENTER, 0);

    page->label_duration = lv_label_create(scr);
    lv_obj_set_width(page->label_duration, lv_pct(90));
    lv_obj_align(page->label_duration, LV_ALIGN_TOP_MID, 0, 58);
    lv_label_set_text(page->label_duration, "时长: 00:00");
    lv_obj_set_style_text_align(page->label_duration, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(page->label_duration, lv_color_hex(0x666666), 0);

    page->btn_record = lv_btn_create(scr);
    lv_obj_set_size(page->btn_record, 145, 65);
    lv_obj_align(page->btn_record, LV_ALIGN_CENTER, 0, 0);
    lv_obj_add_event_cb(page->btn_record, btn_record_cb, LV_EVENT_CLICKED, page);
    page->btn_record_label = lv_label_create(page->btn_record);
    lv_label_set_text(page->btn_record_label, "开始录音");
    lv_obj_center(page->btn_record_label);

    page->label_filename = lv_label_create(scr);
    lv_obj_set_width(page->label_filename, lv_pct(90));
    lv_obj_align(page->label_filename, LV_ALIGN_CENTER, 0, 55);
    lv_label_set_text(page->label_filename, "文件: (未开始)");
    lv_obj_set_style_text_align(page->label_filename, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(page->label_filename, lv_color_hex(0x666666), 0);

    page->btn_back = lv_btn_create(scr);
    lv_obj_set_size(page->btn_back, lv_pct(25), lv_pct(12));
    lv_obj_align(page->btn_back, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    lv_obj_t *lbl_back = lv_label_create(page->btn_back);
    lv_label_set_text(lbl_back, CUSTOM_SYMBOL_BACK "");
    lv_obj_center(lbl_back);
    lv_obj_add_event_cb(page->btn_back, btn_back_cb, LV_EVENT_CLICKED, page);

    page->btn_delete = lv_btn_create(scr);
    lv_obj_set_size(page->btn_delete, 45, 28);
    lv_obj_align(page->btn_delete, LV_ALIGN_BOTTOM_RIGHT, -2, 0);
    lv_obj_t *lbl_delete = lv_label_create(page->btn_delete);
    lv_label_set_text(lbl_delete, "清空");
    lv_obj_center(lbl_delete);
    lv_obj_set_style_bg_color(page->btn_delete, lv_color_hex(0xFF4444), LV_PART_MAIN);
    lv_obj_set_style_text_color(lbl_delete, lv_color_white(), 0);
    lv_obj_add_event_cb(page->btn_delete, btn_delete_cb, LV_EVENT_CLICKED, page);

    page->recorder = alsa_recorder_create("default", 16000, 1, SND_PCM_FORMAT_S16_LE);
    if (!page->recorder) {
        lv_label_set_text(page->label_status, "状态: 初始化失败");
        RECORDER_DEBUG("Failed to create AlsaRecorder");
    } else {
        alsa_recorder_set_finish_callback(page->recorder, recorder_finish_cb, page);
    }

    page->timer = lv_timer_create(timer_cb, 1000, page);
    page->dialog_showing = false;
    page->file_saved = false;
    page->current_filename[0] = '\0';

    update_space_info(page);
    update_ui_state(page);
    return scr;
}

static void recorder_on_create(void *p)
{
    RecorderPage *page = (RecorderPage *)p;
    if (!page) return;

    alsa_mixer_init();

    sys_set_dont_deep_sleep(true);
    sys_set_dont_timeout(true);
    RECORDER_DEBUG("Disabled deep sleep and screen timeout");
}

static void recorder_on_destroy(void *p)
{
    RecorderPage *page = (RecorderPage *)p;
    if (!page) return;

    RECORDER_DEBUG("on_destroy");

    // 如果正在录音，停止录音
    if (alsa_recorder_is_recording(page->recorder)) {
        alsa_recorder_stop(page->recorder);
    }

    // 销毁
    if (page->recorder) {
        alsa_recorder_destroy(page->recorder);
        page->recorder = NULL;
    }

    if (page->timer) {
        lv_timer_del(page->timer);
        page->timer = NULL;
    }

    sys_set_dont_deep_sleep(false);
    sys_set_dont_timeout(false);
    RECORDER_DEBUG("Re-enabled deep sleep and screen timeout");
}

static bool recorder_on_key(void *p, key_code_t key_code, key_action_t key_action)
{
    RecorderPage *page = (RecorderPage *)p;
    if (!page) return false;

    if (key_code == KEY_CODE_HOME) {
        // 只在按下时执行录音开始/停止逻辑
        if (key_action == KEY_ACTION_DOWN) {
            if (!alsa_recorder_is_recording(page->recorder)) {
                if (!ensure_rec_dir_exists()) {
                    lv_label_set_text(page->label_status, "状态: 目录创建失败");
                    RECORDER_DEBUG("Failed to create rec dir");
                    return true;
                }
                generate_filename(page->current_filename, sizeof(page->current_filename));
                int ret = alsa_recorder_start(page->recorder, page->current_filename);
                if (ret == 0) {
                    lv_label_set_text(page->label_status, "状态: 录音中...");
                    page->file_saved = false;
                    RECORDER_DEBUG("Recording started by HOME key: %s", page->current_filename);
                } else {
                    lv_label_set_text(page->label_status, "状态: 启动失败");
                    page->current_filename[0] = '\0';
                    RECORDER_DEBUG("Failed to start recording by HOME key");
                }
            } else {
                alsa_recorder_stop(page->recorder);
                lv_label_set_text(page->label_status, "状态: 停止中...");
                RECORDER_DEBUG("Recording stopped by HOME key");
            }
            update_ui_state(page);
        }
        // 拦截所有 HOME 按键事件，防止系统执行 page_back()
        return true;
    }
    return false;
}

static void btn_record_cb(lv_event_t *e)
{
    RecorderPage *page = (RecorderPage *)lv_event_get_user_data(e);
    if (!page) return;

    if (page->dialog_showing) {
        hide_delete_dialog(page);
    }

    if (!alsa_recorder_is_recording(page->recorder)) {
        if (!ensure_rec_dir_exists()) {
            lv_label_set_text(page->label_status, "状态: 目录创建失败");
            RECORDER_DEBUG("Failed to create rec dir");
            return;
        }
        generate_filename(page->current_filename, sizeof(page->current_filename));
        int ret = alsa_recorder_start(page->recorder, page->current_filename);
        if (ret == 0) {
            lv_label_set_text(page->label_status, "状态: 录音中...");
            page->file_saved = false;
            RECORDER_DEBUG("Recording started: %s", page->current_filename);
        } else {
            lv_label_set_text(page->label_status, "状态: 启动失败");
            page->current_filename[0] = '\0';
            RECORDER_DEBUG("Failed to start recording");
        }
    } else {
        alsa_recorder_stop(page->recorder);
        lv_label_set_text(page->label_status, "状态: 停止中...");
    }
    update_ui_state(page);
}

static void btn_back_cb(lv_event_t *e)
{
    RecorderPage *page = (RecorderPage *)lv_event_get_user_data(e);
    if (!page) return;

    if (page->dialog_showing) {
        hide_delete_dialog(page);
        return;
    }

    if (alsa_recorder_is_recording(page->recorder)) {
        alsa_recorder_stop(page->recorder);
        RECORDER_DEBUG("Recording stopped on back");
    }

    page_back();
}

static void btn_delete_cb(lv_event_t *e)
{
    RecorderPage *page = (RecorderPage *)lv_event_get_user_data(e);
    if (!page) return;
    show_delete_dialog(page);
}

static void dialog_confirm_cb(lv_event_t *e)
{
    RecorderPage *page = (RecorderPage *)lv_event_get_user_data(e);
    if (!page) return;
    hide_delete_dialog(page);
    delete_all_recordings(page);
}

static void dialog_cancel_cb(lv_event_t *e)
{
    RecorderPage *page = (RecorderPage *)lv_event_get_user_data(e);
    if (!page) return;
    hide_delete_dialog(page);
}

static void timer_cb(lv_timer_t *timer)
{
    RecorderPage *page = (RecorderPage *)timer->user_data;
    if (!page) return;

    if (alsa_recorder_is_recording(page->recorder)) {
        unsigned int sec = alsa_recorder_get_duration(page->recorder);
        char text[20];
        snprintf(text, sizeof(text), "时长: %02u:%02u", sec / 60, sec % 60);
        lv_label_set_text(page->label_duration, text);
    }

    update_space_info(page);
}

static void recorder_finish_cb(void *user_data)
{
    RecorderPage *page = (RecorderPage *)user_data;
    if (!page) return;

    finish_msg_t *msg = malloc(sizeof(finish_msg_t));
    if (!msg) return;
    msg->page = page;

    if (page->current_filename[0] != '\0' && access(page->current_filename, F_OK) == 0) {
        msg->saved = true;
    } else {
        msg->saved = false;
    }

    lv_async_call(recorder_finish_ui_cb, msg);
}

static void recorder_finish_ui_cb(void *user_data)
{
    finish_msg_t *msg = (finish_msg_t *)user_data;
    RecorderPage *page = msg->page;

    if (msg->saved) {
        lv_label_set_text(page->label_status, "状态: 已保存");
        page->file_saved = true;
        RECORDER_DEBUG("File saved: %s", page->current_filename);
    } else {
        lv_label_set_text(page->label_status, "状态: 已放弃");
        page->current_filename[0] = '\0';
        page->file_saved = false;
        RECORDER_DEBUG("Recording aborted, no file saved");
    }

    update_ui_state(page);
    update_space_info(page);
    free(msg);
}

static int ensure_rec_dir_exists(void)
{
    struct stat st;
    if (stat("../recorder", &st) == 0) {
        if (S_ISDIR(st.st_mode)) return 1;
    }
    int ret = mkdir("../recorder", 0755);
    if (ret == 0 || errno == EEXIST) return 1;
    RECORDER_DEBUG("mkdir failed: %s", strerror(errno));
    return 0;
}

static void update_space_info(RecorderPage *page)
{
    struct statfs fs;
    if (statfs(".", &fs) == 0) {
        long long free_bytes = (long long)fs.f_bfree * fs.f_bsize;
        double free_mb = free_bytes / (1024.0 * 1024.0);
        char info[64];
        snprintf(info, sizeof(info), "剩余空间: %.1f MB", free_mb);
        lv_label_set_text(page->label_space, info);
    } else {
        lv_label_set_text(page->label_space, "剩余空间: -- MB");
        RECORDER_DEBUG("statfs failed: %s", strerror(errno));
    }
}

static void generate_filename(char *buffer, size_t size)
{
    time_t now = time(NULL);
    struct tm *tm = localtime(&now);
    snprintf(buffer, size, "../recorder/%02d%02d%02d_%02d%02d%02d.mp3",
             tm->tm_year % 100, tm->tm_mon + 1, tm->tm_mday,
             tm->tm_hour, tm->tm_min, tm->tm_sec);
    RECORDER_DEBUG("Generated filename: %s", buffer);
}

static void update_ui_state(RecorderPage *page)
{
    bool recording = alsa_recorder_is_recording(page->recorder);
    if (recording) {
        lv_label_set_text(page->btn_record_label, "停止录音");
        lv_obj_set_style_bg_color(page->btn_record, lv_color_hex(0xFF5555), LV_PART_MAIN);
    } else {
        lv_label_set_text(page->btn_record_label, "开始录音");
        lv_obj_set_style_bg_color(page->btn_record, lv_color_hex(0x4CAF50), LV_PART_MAIN);
    }

    if (page->current_filename[0] != '\0' && !recording) {
        char *fname = strrchr(page->current_filename, '/');
        char display[150];
        snprintf(display, sizeof(display), "文件: %s", fname ? fname + 1 : page->current_filename);
        lv_label_set_text(page->label_filename, display);
    } else if (recording) {
        lv_label_set_text(page->label_filename, "文件: 录音中...");
    } else {
        lv_label_set_text(page->label_filename, "文件: (未开始)");
    }
}

static void show_delete_dialog(RecorderPage *page)
{
    if (page->dialog_showing) return;
    page->dialog_showing = true;

    if (alsa_recorder_is_recording(page->recorder)) {
        alsa_recorder_abort(page->recorder);
        page->file_saved = false;
        page->current_filename[0] = '\0';
        update_ui_state(page);
    }

    lv_obj_t *scr = page->base.obj;

    page->dialog_bg = lv_obj_create(scr);
    lv_obj_set_size(page->dialog_bg, lv_pct(100), lv_pct(100));
    lv_obj_set_style_bg_color(page->dialog_bg, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(page->dialog_bg, LV_OPA_50, 0);
    lv_obj_clear_flag(page->dialog_bg, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_move_foreground(page->dialog_bg);

    page->dialog_box = lv_obj_create(page->dialog_bg);
    lv_obj_set_size(page->dialog_box, 220, 140);
    lv_obj_align(page->dialog_box, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(page->dialog_box, lv_color_white(), 0);
    lv_obj_set_style_radius(page->dialog_box, 10, 0);
    lv_obj_clear_flag(page->dialog_box, LV_OBJ_FLAG_SCROLLABLE);

    page->dialog_label = lv_label_create(page->dialog_box);
    lv_label_set_text(page->dialog_label, "确认删除所有录音文件？\n此操作不可恢复！");
    lv_obj_set_width(page->dialog_label, lv_pct(100));
    lv_obj_align(page->dialog_label, LV_ALIGN_TOP_MID, 0, 10);
    lv_obj_set_style_text_align(page->dialog_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(page->dialog_label, lv_color_hex(0xFF4444), 0);

    page->dialog_btn_confirm = lv_btn_create(page->dialog_box);
    lv_obj_set_size(page->dialog_btn_confirm, 80, 35);
    lv_obj_align(page->dialog_btn_confirm, LV_ALIGN_BOTTOM_LEFT, 10, -10);
    lv_obj_t *lbl_cfm = lv_label_create(page->dialog_btn_confirm);
    lv_label_set_text(lbl_cfm, "确认");
    lv_obj_center(lbl_cfm);
    lv_obj_set_style_bg_color(page->dialog_btn_confirm, lv_color_hex(0xFF4444), LV_PART_MAIN);
    lv_obj_set_style_text_color(lbl_cfm, lv_color_white(), 0);
    lv_obj_add_event_cb(page->dialog_btn_confirm, dialog_confirm_cb, LV_EVENT_CLICKED, page);

    page->dialog_btn_cancel = lv_btn_create(page->dialog_box);
    lv_obj_set_size(page->dialog_btn_cancel, 80, 35);
    lv_obj_align(page->dialog_btn_cancel, LV_ALIGN_BOTTOM_RIGHT, -10, -10);
    lv_obj_t *lbl_ccl = lv_label_create(page->dialog_btn_cancel);
    lv_label_set_text(lbl_ccl, "取消");
    lv_obj_center(lbl_ccl);
    lv_obj_set_style_bg_color(page->dialog_btn_cancel, lv_color_hex(0xCCCCCC), LV_PART_MAIN);
    lv_obj_add_event_cb(page->dialog_btn_cancel, dialog_cancel_cb, LV_EVENT_CLICKED, page);
}

static void hide_delete_dialog(RecorderPage *page)
{
    if (!page->dialog_showing) return;
    page->dialog_showing = false;

    if (page->dialog_bg) {
        lv_obj_del(page->dialog_bg);
        page->dialog_bg = NULL;
        page->dialog_box = NULL;
        page->dialog_label = NULL;
        page->dialog_btn_confirm = NULL;
        page->dialog_btn_cancel = NULL;
    }
}

static void delete_all_recordings(RecorderPage *page)
{
    RECORDER_DEBUG("Deleting all recordings in ../recorder/");
    int ret = system("rm -rf ../recorder/* 2>/dev/null");
    if (ret == 0) {
        lv_label_set_text(page->label_status, "状态: 录音文件已清空");
        page->current_filename[0] = '\0';
        page->file_saved = false;
        update_ui_state(page);
        RECORDER_DEBUG("All files deleted");
    } else {
        lv_label_set_text(page->label_status, "状态: 删除失败");
        RECORDER_DEBUG("rm failed, ret=%d", ret);
    }
    update_space_info(page);
}