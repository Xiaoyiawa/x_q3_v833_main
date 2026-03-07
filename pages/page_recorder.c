#include "page_recorder.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <signal.h>
#include <sys/types.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>

static lv_obj_t *scr, *btn_record, *label_btn, *label_status, *label_filename, *label_duration, *label_space, *btn_back,
    *btn_delete;
static lv_obj_t *dialog_bg = NULL, *dialog_box = NULL, *dialog_label = NULL, *dialog_btn_confirm = NULL,
                *dialog_btn_cancel = NULL;

static int is_recording = 0, dialog_showing = 0;
static time_t recording_start_time    = 0;
static lv_timer_t * duration_timer    = NULL;
static char current_filename[128]     = "";
static pid_t arecord_pid              = 0;
static volatile int fix_complete_flag = 0;

static void create_ui(void);
static void btn_record_cb(lv_event_t * e);
static void btn_back_cb(lv_event_t * e);
static void btn_delete_cb(lv_event_t * e);
static void btn_confirm_cb(lv_event_t * e);
static void btn_cancel_cb(lv_event_t * e);
static void start_recording(void);
static void stop_recording(void);
static void kill_arecord_delayed(int seconds);
static int ensure_rec_dir_exists(void);
static void update_status_display(void);
static void update_space_info(void);
static void duration_timer_cb(lv_timer_t * timer);
static void generate_filename(char * buffer, size_t size);
static void show_delete_dialog(void);
static void hide_delete_dialog(void);
static void delete_all_recordings(void);

// ---------- FFmpeg WAV 文件修复函数 ----------
static int fix_wav_file(const char * input_file, const char * output_file)
{
    AVFormatContext *in_ctx = NULL, *out_ctx = NULL;
    int ret = -1;

    ret = avformat_open_input(&in_ctx, input_file, NULL, NULL);
    if(ret < 0) {
        printf("[Rec Fix] Error: cannot open input file %s\n", input_file);
        return ret;
    }

    ret = avformat_find_stream_info(in_ctx, NULL);
    if(ret < 0) {
        printf("[Rec Fix] Error: cannot find stream info\n");
        goto cleanup;
    }

    ret = avformat_alloc_output_context2(&out_ctx, NULL, "wav", output_file);
    if(ret < 0) {
        printf("[Rec Fix] Error: cannot create output context\n");
        goto cleanup;
    }

    for(unsigned int i = 0; i < in_ctx->nb_streams; i++) {
        AVStream * in_stream  = in_ctx->streams[i];
        AVStream * out_stream = avformat_new_stream(out_ctx, NULL);
        if(!out_stream) {
            printf("[Rec Fix] Error: cannot create output stream\n");
            ret = -1;
            goto cleanup;
        }
        ret = avcodec_parameters_copy(out_stream->codecpar, in_stream->codecpar);
        if(ret < 0) {
            printf("[Rec Fix] Error: cannot copy codec parameters\n");
            goto cleanup;
        }
        out_stream->codecpar->codec_tag = 0;
    }

    ret = avio_open(&out_ctx->pb, output_file, AVIO_FLAG_WRITE);
    if(ret < 0) {
        printf("[Rec Fix] Error: cannot open output file %s\n", output_file);
        goto cleanup;
    }

    ret = avformat_write_header(out_ctx, NULL);
    if(ret < 0) {
        printf("[Rec Fix] Error: cannot write header\n");
        goto cleanup;
    }

    AVPacket pkt;
    while(1) {
        ret = av_read_frame(in_ctx, &pkt);
        if(ret < 0) break;

        av_packet_rescale_ts(&pkt, in_ctx->streams[pkt.stream_index]->time_base,
                             out_ctx->streams[pkt.stream_index]->time_base);

        ret = av_interleaved_write_frame(out_ctx, &pkt);
        av_packet_unref(&pkt);
        if(ret < 0) break;
    }

    av_write_trailer(out_ctx);
    printf("[Rec Fix] Success: %s -> %s\n", input_file, output_file);
    ret = 0;

cleanup:
    if(in_ctx) avformat_close_input(&in_ctx);
    if(out_ctx) {
        if(out_ctx->pb) avio_closep(&out_ctx->pb);
        avformat_free_context(out_ctx);
    }
    return ret;
}

// ---------- 信号处理函数 ----------
static void sigusr1_handler(int sig)
{
    (void)sig;
    fix_complete_flag = 1;
}

// ---------- UI 创建 ----------
lv_obj_t * page_recorder_create(void)
{
    printf("[Rec] create\n");
    scr = lv_obj_create(lv_scr_act());
    lv_obj_remove_style_all(scr);
    lv_obj_set_size(scr, lv_pct(100), lv_pct(100));
    lv_obj_set_style_bg_color(scr, lv_color_white(), 0);

    is_recording = dialog_showing = 0;
    recording_start_time          = 0;
    duration_timer                = NULL;
    current_filename[0]           = '\0';
    arecord_pid                   = 0;
    fix_complete_flag             = 0;

    // 注册 SIGUSR1 处理函数
    struct sigaction sa;
    sa.sa_handler = sigusr1_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGUSR1, &sa, NULL);

    create_ui();
    return scr;
}

static void create_ui(void)
{
    // 存储空间
    label_space = lv_label_create(scr);
    lv_obj_set_width(label_space, lv_pct(90));
    lv_obj_align(label_space, LV_ALIGN_TOP_MID, 0, 10);
    lv_obj_set_style_text_align(label_space, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(label_space, lv_color_hex(0x888888), 0);
    update_space_info();

    // 状态
    label_status = lv_label_create(scr);
    lv_obj_set_width(label_status, lv_pct(90));
    lv_obj_align(label_status, LV_ALIGN_TOP_MID, 0, 35);
    lv_label_set_text(label_status, "状态: 空闲");
    lv_obj_set_style_text_align(label_status, LV_TEXT_ALIGN_CENTER, 0);

    // 时长
    label_duration = lv_label_create(scr);
    lv_obj_set_width(label_duration, lv_pct(90));
    lv_obj_align(label_duration, LV_ALIGN_TOP_MID, 0, 58);
    lv_label_set_text(label_duration, "时长: 00:00");
    lv_obj_set_style_text_align(label_duration, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(label_duration, lv_color_hex(0x666666), 0);

    // 主按钮
    btn_record = lv_btn_create(scr);
    lv_obj_set_size(btn_record, 145, 65);
    lv_obj_align(btn_record, LV_ALIGN_CENTER, 0, 0);
    lv_obj_add_event_cb(btn_record, btn_record_cb, LV_EVENT_CLICKED, NULL);
    label_btn = lv_label_create(btn_record);
    lv_label_set_text(label_btn, "开始录音");
    lv_obj_center(label_btn);

    // 文件名
    label_filename = lv_label_create(scr);
    lv_obj_set_width(label_filename, lv_pct(90));
    lv_obj_align(label_filename, LV_ALIGN_CENTER, 0, 55);
    lv_label_set_text(label_filename, "文件: (未开始)");
    lv_obj_set_style_text_align(label_filename, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(label_filename, lv_color_hex(0x666666), 0);

    // 返回按钮
    btn_back = lv_btn_create(scr);
    lv_obj_set_size(btn_back, lv_pct(25), lv_pct(12));
    lv_obj_align(btn_back, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    lv_obj_t * lbl_back = lv_label_create(btn_back);
    lv_label_set_text(lbl_back, CUSTOM_SYMBOL_BACK "");
    lv_obj_center(lbl_back);
    lv_obj_add_event_cb(btn_back, btn_back_cb, LV_EVENT_CLICKED, NULL);

    // 清空按钮
    btn_delete = lv_btn_create(scr);
    lv_obj_set_size(btn_delete, 45, 28);
    lv_obj_align(btn_delete, LV_ALIGN_BOTTOM_RIGHT, -2, 0);
    lv_obj_t * lbl_delete = lv_label_create(btn_delete);
    lv_label_set_text(lbl_delete, "清空");
    lv_obj_center(lbl_delete);
    lv_obj_set_style_bg_color(btn_delete, lv_color_hex(0xFF4444), LV_PART_MAIN);
    lv_obj_set_style_text_color(lbl_delete, lv_color_white(), 0);
    lv_obj_add_event_cb(btn_delete, btn_delete_cb, LV_EVENT_CLICKED, NULL);

    // 创建持续运行的定时器（每秒触发）
    duration_timer = lv_timer_create(duration_timer_cb, 1000, NULL);

    update_status_display();
}

static void start_recording(void)
{
    printf("[Rec] start\n");
    if(dialog_showing) hide_delete_dialog();
    if(!ensure_rec_dir_exists()) {
        lv_label_set_text(label_status, "状态: 目录创建失败");
        return;
    }
    generate_filename(current_filename, sizeof(current_filename));

    char cmd[512];
    snprintf(cmd, sizeof(cmd),
             "/mnt/UDISK/tools/bin/arecord -D hw:0,0 -f S16_LE -r 16000 -c 1 '%s' & echo $! > /tmp/arecord.pid",
             current_filename);
    printf("[Rec] cmd: %s\n", cmd);

    int ret = system(cmd);
    if(ret == -1) {
        lv_label_set_text(label_status, "状态: 启动失败");
        current_filename[0] = '\0';
        return;
    }

    FILE * fp = fopen("/tmp/arecord.pid", "r");
    if(fp) {
        fscanf(fp, "%d", &arecord_pid);
        fclose(fp);
        printf("[Rec] arecord PID = %d\n", arecord_pid);
    }

    recording_start_time = time(NULL);
    is_recording         = 1;
    lv_label_set_text(label_status, "状态: 录音中...");
    update_status_display();
}

static void stop_recording(void)
{
    if(!is_recording) return;
    printf("[Rec] stop\n");
    is_recording = 0;
    lv_label_set_text(label_status, "状态: 编码中...");

    // 注意：不删除定时器，让定时器继续运行以便检查修复标志
    lv_label_set_text(label_duration, "时长: 00:00");

    // 延迟杀死 arecord 进程
    kill_arecord_delayed(3);

    // 创建子进程进行 FFmpeg 修复
    pid_t fix_pid = fork();
    if(fix_pid == 0) {
        // 子进程
        sleep(4); // 等待 arecord 完全退出

        char temp_filename[128];
        snprintf(temp_filename, sizeof(temp_filename), "%s.tmp.wav", current_filename);

        printf("[Rec Fix Child] Start fixing %s...\n", current_filename);
        int fix_ret = fix_wav_file(current_filename, temp_filename);
        if(fix_ret == 0) {
            rename(temp_filename, current_filename);
            printf("[Rec Fix Child] Fix success, replaced original file.\n");
        } else {
            printf("[Rec Fix Child] Fix failed, keeping original file.\n");
            unlink(temp_filename);
        }

        kill(getppid(), SIGUSR1);
        _exit(0);
    } else if(fix_pid > 0) {
        printf("[Rec] forked fix process %d\n", fix_pid);
    } else {
        printf("[Rec] fork fix process failed\n");
    }

    update_space_info();
    update_status_display();
}

// 子进程延迟杀 arecord
static void kill_arecord_delayed(int seconds)
{
    pid_t pid = fork();
    if(pid == 0) {
        printf("[Rec child] delay %ds then kill -INT arecord (PID %d)\n", seconds, arecord_pid);
        sleep(seconds);
        if(arecord_pid > 0) {
            kill(arecord_pid, SIGINT);
        } else {
            system("killall -INT arecord 2>/dev/null");
        }
        _exit(0);
    } else if(pid > 0) {
        printf("[Rec] forked child %d\n", pid);
    } else {
        printf("[Rec] fork failed\n");
    }
}

// ---------- 回调 ----------
static void btn_record_cb(lv_event_t * e)
{
    if(!is_recording)
        start_recording();
    else
        stop_recording();
}

static void btn_back_cb(lv_event_t * e)
{
    if(dialog_showing) {
        hide_delete_dialog();
        return;
    }
    if(is_recording) stop_recording();
    page_back();
}

static void btn_delete_cb(lv_event_t * e)
{
    show_delete_dialog();
}

static void btn_confirm_cb(lv_event_t * e)
{
    hide_delete_dialog();
    delete_all_recordings();
}

static void btn_cancel_cb(lv_event_t * e)
{
    hide_delete_dialog();
}

// ---------- 对话框 ----------
static void show_delete_dialog(void)
{
    if(dialog_showing) return;
    printf("[Rec] show dialog\n");
    dialog_showing = 1;
    if(is_recording) stop_recording();

    dialog_bg = lv_obj_create(scr);
    lv_obj_set_size(dialog_bg, lv_pct(100), lv_pct(100));
    lv_obj_set_style_bg_color(dialog_bg, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(dialog_bg, LV_OPA_50, 0);
    lv_obj_clear_flag(dialog_bg, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_move_foreground(dialog_bg);

    dialog_box = lv_obj_create(dialog_bg);
    lv_obj_set_size(dialog_box, 220, 140);
    lv_obj_align(dialog_box, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(dialog_box, lv_color_white(), 0);
    lv_obj_set_style_radius(dialog_box, 10, 0);
    lv_obj_clear_flag(dialog_box, LV_OBJ_FLAG_SCROLLABLE);

    dialog_label = lv_label_create(dialog_box);
    lv_label_set_text(dialog_label, "确认删除所有录音文件？\n此操作不可恢复！");
    lv_obj_set_width(dialog_label, lv_pct(100));
    lv_obj_align(dialog_label, LV_ALIGN_TOP_MID, 0, 10);
    lv_obj_set_style_text_align(dialog_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(dialog_label, lv_color_hex(0xFF4444), 0);

    dialog_btn_confirm = lv_btn_create(dialog_box);
    lv_obj_set_size(dialog_btn_confirm, 80, 35);
    lv_obj_align(dialog_btn_confirm, LV_ALIGN_BOTTOM_LEFT, 10, -10);
    lv_obj_t * lbl_cfm = lv_label_create(dialog_btn_confirm);
    lv_label_set_text(lbl_cfm, "确认");
    lv_obj_center(lbl_cfm);
    lv_obj_set_style_bg_color(dialog_btn_confirm, lv_color_hex(0xFF4444), LV_PART_MAIN);
    lv_obj_set_style_text_color(lbl_cfm, lv_color_white(), 0);
    lv_obj_add_event_cb(dialog_btn_confirm, btn_confirm_cb, LV_EVENT_CLICKED, NULL);

    dialog_btn_cancel = lv_btn_create(dialog_box);
    lv_obj_set_size(dialog_btn_cancel, 80, 35);
    lv_obj_align(dialog_btn_cancel, LV_ALIGN_BOTTOM_RIGHT, -10, -10);
    lv_obj_t * lbl_ccl = lv_label_create(dialog_btn_cancel);
    lv_label_set_text(lbl_ccl, "取消");
    lv_obj_center(lbl_ccl);
    lv_obj_set_style_bg_color(dialog_btn_cancel, lv_color_hex(0xCCCCCC), LV_PART_MAIN);
    lv_obj_add_event_cb(dialog_btn_cancel, btn_cancel_cb, LV_EVENT_CLICKED, NULL);
}

static void hide_delete_dialog(void)
{
    if(!dialog_showing) return;
    printf("[Rec] hide dialog\n");
    dialog_showing = 0;
    if(dialog_bg) {
        lv_obj_del(dialog_bg);
        dialog_bg = dialog_box = dialog_label = dialog_btn_confirm = dialog_btn_cancel = NULL;
    }
}

// ---------- 工具函数 ----------
static int ensure_rec_dir_exists(void)
{
    int ret = system("mkdir -p /mnt/UDISK/rec");
    printf("[Rec] mkdir ret=%d\n", ret);
    return ret == 0;
}

static void update_space_info(void)
{
    FILE * fp = popen("df -k /mnt/UDISK | tail -1 | awk '{print $2,$3}'", "r");
    if(fp) {
        int total_kb, used_kb;
        if(fscanf(fp, "%d %d", &total_kb, &used_kb) == 2) {
            float total_mb = total_kb / 1024.0, used_mb = used_kb / 1024.0;
            int percent = (int)(used_mb / total_mb * 100);
            char info[64];
            snprintf(info, sizeof(info), "存储: %.1f/%.1f MB (%d%%)", used_mb, total_mb, percent);
            lv_label_set_text(label_space, info);
        }
        pclose(fp);
    } else {
        lv_label_set_text(label_space, "存储: --/-- MB (--%)");
    }
}

static void generate_filename(char * buffer, size_t size)
{
    time_t now     = time(NULL);
    struct tm * tm = localtime(&now);
    snprintf(buffer, size, "/mnt/UDISK/rec/%02d%02d%02d_%02d%02d%02d.wav", tm->tm_year % 100, tm->tm_mon + 1,
             tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec);
    printf("[Rec] filename: %s\n", buffer);
}

static void update_status_display(void)
{
    if(is_recording) {
        lv_label_set_text(label_btn, "停止录音");
        lv_obj_set_style_bg_color(btn_record, lv_color_hex(0xFF5555), LV_PART_MAIN);
    } else {
        lv_label_set_text(label_btn, "开始录音");
        lv_obj_set_style_bg_color(btn_record, lv_color_hex(0x4CAF50), LV_PART_MAIN);
    }
    if(current_filename[0]) {
        char * fname = strrchr(current_filename, '/');
        char display[150];
        snprintf(display, sizeof(display), "文件: %s", fname ? fname + 1 : current_filename);
        lv_label_set_text(label_filename, display);
    } else {
        lv_label_set_text(label_filename, "文件: (未开始)");
    }
}

static void duration_timer_cb(lv_timer_t * timer)
{
    // 如果正在录音，更新时长显示
    if(is_recording) {
        time_t now = time(NULL);
        long sec   = now - recording_start_time;
        char text[20];
        snprintf(text, sizeof(text), "时长: %02ld:%02ld", sec / 60, sec % 60);
        lv_label_set_text(label_duration, text);
    }

    // 检查修复完成标志
    if(fix_complete_flag) {
        fix_complete_flag = 0;
        // 只有当不在录音状态时，才将状态改为“已保存”
        if(!is_recording) {
            lv_label_set_text(label_status, "状态: 已保存录音文件");
        }
    }
}

static void delete_all_recordings(void)
{
    printf("[Rec] delete all\n");
    lv_label_set_text(label_status, "状态: 正在删除...");
    int ret = system("rm -f /mnt/UDISK/rec/* 2>/dev/null");
    if(ret == 0) {
        lv_label_set_text(label_status, "状态: 录音文件已清空");
        current_filename[0] = '\0';
        lv_label_set_text(label_filename, "文件: (未开始)");
    } else {
        lv_label_set_text(label_status, "状态: 删除失败");
    }
    update_space_info();
}