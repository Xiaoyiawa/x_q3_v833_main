#include "lvgl/lvgl.h"
#include "lvgl/demos/lv_demos.h"
#include "lv_drivers/display/fbdev.h"
#include "lv_drivers/indev/evdev.h"
#include "lv_lib_100ask/lv_lib_100ask.h"
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <linux/fb.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <pthread.h>
#include <sys/wait.h>
#include <time.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <string.h>
#include "platform/audio_ctrl.h"
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>

// 页面管理器及各个页面模块
#include "pages/page_manager.h"
#include "pages/page_main.h"
#include "pages/page_demo.h"
#include "pages/page_audio.h"
#include "pages/page_file_manager.h"
#include "pages/page_calculator.h"
#include "pages/page_apple.h"
#include "pages/page_image.h"
#include "pages/page_ftp.h"

#define DISP_BUF_SIZE (LV_SCR_WIDTH * LV_SCR_HEIGHT)

#define PATH_MAX_LENGTH 256
char homepath[PATH_MAX_LENGTH] = {0};

int dispd  = 0;                   // 背光
int fbd    = 0;                   // 帧缓冲设备
int powerd = 0;                   // 电源按钮
int homed  = 0;                   // 主页按钮
struct fb_var_screeninfo * vinfo; // 屏幕参数

uint32_t sleepTs      = -1;
uint32_t homeClickTs  = -1;
uint32_t backgroundTs = -1;

bool deepSleep     = false;
bool dontDeepSleep = false;

extern void lcdBrightness(int brightness);
extern void sysSleep(void);
extern void sysWake(void);
extern void sysDeepSleep(void);
extern void setDontDeepSleep(bool b);
extern void switchRobot(void);
extern void switchBackground(void);
extern void switchForeground(void);

extern lv_style_t getFontStyle(const char * filename, uint16_t weight, uint16_t style);

extern uint32_t tick_get(void);

void readKeyPower(void);
void readKeyHome(void);
void lcdInit(void);
void lcdOpen(void);
void lcdClose(void);
void lcdRefresh(void);
void touchOpen(void);
void touchClose(void);

static lv_style_t style_default;

static uint32_t last_screenshot_time = 0;
static uint32_t power_down_time      = 0;
static uint32_t home_down_time       = 0;
static int combo_triggered           = 0;

static void take_screenshot(void)
{
    uint32_t now = tick_get();
    // 1秒内不重复截图
    if(now - last_screenshot_time < 1000) {
        printf("[Screenshot] Ignored (too frequent)\n");
        return;
    }
    last_screenshot_time = now;

    const char * raw_path = "/mnt/UDISK/fb_data.raw";
    const char * shot_dir = "/mnt/UDISK/screenshot";
    char png_path[128];
    time_t t;
    struct tm * tm;
    FILE * raw_fp;
    uint8_t * fb_data;
    long raw_size;
    int ret;

    mkdir(shot_dir, 0755);
    t  = time(NULL);
    tm = localtime(&t);
    snprintf(png_path, sizeof(png_path), "%s/screen_%04d%02d%02d_%02d%02d%02d.png", shot_dir, tm->tm_year + 1900,
             tm->tm_mon + 1, tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec);

    ret = system("dd if=/dev/fb0 of=/mnt/UDISK/fb_data.raw bs=1M 2>/dev/null");
    if(ret != 0) {
        printf("[Screenshot] dd failed\n");
        return;
    }

    raw_fp = fopen(raw_path, "rb");
    if(!raw_fp) {
        printf("[Screenshot] Cannot open raw file\n");
        return;
    }
    fseek(raw_fp, 0, SEEK_END);
    raw_size = ftell(raw_fp);
    fseek(raw_fp, 0, SEEK_SET);
    fb_data = malloc(raw_size);
    if(!fb_data) {
        printf("[Screenshot] malloc failed\n");
        fclose(raw_fp);
        unlink(raw_path);
        return;
    }
    fread(fb_data, 1, raw_size, raw_fp);
    fclose(raw_fp);
    unlink(raw_path);

    AVCodec * codec = avcodec_find_encoder(AV_CODEC_ID_PNG);
    if(!codec) {
        printf("[Screenshot] PNG encoder not found\n");
        free(fb_data);
        return;
    }

    AVCodecContext * c = avcodec_alloc_context3(codec);
    if(!c) {
        printf("[Screenshot] Failed to alloc context\n");
        free(fb_data);
        return;
    }

    c->width     = 240;
    c->height    = 240;
    c->time_base = (AVRational){1, 25};
    c->pix_fmt   = AV_PIX_FMT_RGBA;

    if(avcodec_open2(c, codec, NULL) < 0) {
        printf("[Screenshot] avcodec_open2 failed\n");
        avcodec_free_context(&c);
        free(fb_data);
        return;
    }

    AVFrame * frame = av_frame_alloc();
    frame->width    = c->width;
    frame->height   = c->height;
    frame->format   = c->pix_fmt;
    av_frame_get_buffer(frame, 0);

    // 转换RGBA
    for(int y = 0; y < c->height; y++) {
        uint8_t * src = fb_data + y * c->width * 4;
        uint8_t * dst = frame->data[0] + y * frame->linesize[0];
        for(int x = 0; x < c->width; x++) {
            uint8_t b = src[0];
            uint8_t g = src[1];
            uint8_t r = src[2];
            uint8_t a = src[3];
            dst[0]    = r;
            dst[1]    = g;
            dst[2]    = b;
            dst[3]    = a;
            src += 4;
            dst += 4;
        }
    }

    AVPacket * pkt = av_packet_alloc();
    ret            = avcodec_send_frame(c, frame);
    if(ret >= 0) {
        ret = avcodec_receive_packet(c, pkt);
        if(ret >= 0) {
            FILE * out = fopen(png_path, "wb");
            if(out) {
                fwrite(pkt->data, 1, pkt->size, out);
                fclose(out);
                printf("[Screenshot] Saved: %s\n", png_path);
            } else {
                printf("[Screenshot] Cannot create PNG file\n");
            }
            av_packet_unref(pkt);
        }
    }

    av_packet_free(&pkt);
    av_frame_free(&frame);
    avcodec_free_context(&c);
    free(fb_data);
}

int main(int argc, char * argv[])
{

    printf("ciallo lvgl\n");
#if LV_USE_PERF_MONITOR
    printf("monitor on\n");
#endif

    bool isDaemonMode = true;

    powerd = open("/dev/input/event1", O_RDWR);
    fcntl(powerd, 4, 2048);
    homed = open("/dev/input/event2", O_RDWR);
    fcntl(homed, 4, 2048);

    for(uint32_t i = 0; i < argc; i++) {
        char * arg = argv[i];
        printf("argv[%d] = %s\n", i, arg);
        if(strcmp(arg, "-d") == 0) {
            isDaemonMode = false;
        }

        if(strcmp(arg, "-w") == 0) {
            daemon(1, 0);
            switchBackground();
            while(1) {
                usleep(25000);
                readKeyHome();
            }
        }
    }

    printf("kill robot\n");
    system("killall robotd");
    system("killall robot_run");
    system("killall robot_run_1");
    usleep(100000);

    getcwd(homepath, PATH_MAX_LENGTH);

    if(isDaemonMode) daemon(1, 0);
    // daemon函数将本程序置于后台，脱离终端
    // 若要进行调试，请使用-d参数

    setenv("TZ", "CST-8", 1);
    tzset();

    dispd = open("/dev/disp", O_RDWR);
    fbdev_init();
    fbd = fbdev_get_fbd();
    lcdInit();
    lcdClose();
    lcdOpen();
    lcdBrightness(25);
    touchOpen();

    lv_init();

    static lv_color_t bufA[DISP_BUF_SIZE];
    static lv_color_t bufB[DISP_BUF_SIZE];

    static lv_disp_draw_buf_t disp_buf;
    lv_disp_draw_buf_init(&disp_buf, bufA, bufB, DISP_BUF_SIZE);

    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.draw_buf = &disp_buf;
    disp_drv.flush_cb = fbdev_flush;
    disp_drv.hor_res  = 240;
    disp_drv.ver_res  = 240;
    lv_disp_t * disp  = lv_disp_drv_register(&disp_drv);
    lv_disp_set_default(disp);

    evdev_init();
    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type     = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb  = evdev_read;
    lv_indev_t * indev = lv_indev_drv_register(&indev_drv);

    lv_ffmpeg_init();

    lv_obj_t * screen = lv_obj_create(NULL);
    lv_scr_load(screen);

    lv_freetype_init(128, 4, 0);

    lv_ft_info_t ft_info;
    ft_info.name   = "/mnt/UDISK/lvgl/res/font.ttf";
    ft_info.weight = 16;
    ft_info.style  = FT_FONT_STYLE_NORMAL;
    ft_info.mem    = NULL;

    if(lv_ft_font_init(&ft_info)) {
        lv_theme_t * theme = lv_theme_default_init(disp, lv_palette_main(LV_PALETTE_BLUE),
                                                   lv_palette_main(LV_PALETTE_CYAN), false, ft_info.font);
        theme->font_normal = ft_info.font;
        theme->font_large  = ft_info.font;
        theme->font_small  = ft_info.font; // 为啥子设置不上？
        lv_disp_set_theme(disp, theme);

        lv_style_init(&style_default);
        lv_style_set_text_font(&style_default, ft_info.font);
        lv_obj_add_style(lv_scr_act(), &style_default, 0);
    }

    // 初始化页面管理器并打开主页面（使用兼容函数）
    page_manager_init();
    page_open(main_page_create());

    while(1) {
        readKeyHome();
        if(backgroundTs == -1) {
            readKeyPower();
            if(sleepTs == -1) {
                lv_timer_handler();
                lcdRefresh(); // 放在fbdev里不合适，反而会增大cpu占用且变卡，神金啊
                usleep(5000);
            } else {
                if(dontDeepSleep)
                    sleepTs = tick_get();

                else if(!deepSleep && tick_get() - sleepTs >= 60000)
                    sysDeepSleep();

                usleep(25000);
            }
        } else {
            usleep(25000);
        }
    }

    if(fbd) close(fbd);
    if(dispd) close(dispd);
    if(homed) close(homed);
    if(powerd) close(powerd);
    return 0;
}

/*Set in lv_conf.h as `LV_TICK_CUSTOM_SYS_TIME_EXPR`*/
uint32_t tick_get(void)
{
    static uint32_t start_ms = 0;
    if(start_ms == 0) {
        struct timeval tv_start;
        gettimeofday(&tv_start, NULL);
        start_ms = (tv_start.tv_sec * 1000000 + tv_start.tv_usec) / 1000;
    }

    struct timeval tv_now;
    gettimeofday(&tv_now, NULL);
    uint32_t now_ms;
    now_ms = (tv_now.tv_sec * 1000000 + tv_now.tv_usec) / 1000;

    uint32_t time_ms = now_ms - start_ms;
    return time_ms;
}

void lcdInit(void)
{
    vinfo         = fbdev_get_vinfo();
    vinfo->rotate = 3;
    ioctl(fbd, 0x4601u, vinfo);
}

void lcdOpen(void)
{
    int buffer[8] = {0};
    buffer[1]     = 1;
    ioctl(dispd, 0xFu, buffer);
    printf("[lcd]opened\n");
}

void lcdClose(void)
{
    int buffer[8] = {0};
    ioctl(dispd, 0xFu, buffer);
    printf("[lcd]closed\n");
}

void touchOpen(void)
{
    int tpd = open("/proc/sprocomm_tpInfo", 526338);
    write(tpd, "1", 1u);
    close(tpd);
    printf("[tp]opened\n");
}

void touchClose(void)
{
    int tpd = open("/proc/sprocomm_tpInfo", 526338);
    write(tpd, "0", 1u);
    close(tpd);
    printf("[tp]closed\n");
}

void lcdRefresh(void)
{
    ioctl(fbd, 0x4606u, vinfo);
}

void lcdBrightness(int brightness)
{
    int buffer[8] = {0};
    buffer[1]     = brightness;
    ioctl(dispd, 0x102u, buffer);
}

void readKeyPower(void)
{
    char buffer[16] = {0};
    while(read(powerd, buffer, 0x10u) > 0) {
        if(buffer[10] != 0x74) return;

        if(buffer[12] == 0x00) {
            printf("[key]power_up\n");

            // 判断是否组合键
            uint32_t now = tick_get();
            if(home_down_time > power_down_time && (now - power_down_time < 500)) {
                combo_triggered = 1;
                take_screenshot();
                printf("[key] combo detected, skip sleep/wake\n");
            } else {
                if(sleepTs == -1 && !deepSleep) {
                    sysSleep();
                } else {
                    sysWake();
                }
            }
            power_down_time = 0;
        } else {
            printf("[key]power_down\n");
            power_down_time = tick_get();
        }
    }
}

void readKeyHome(void)
{
    char buffer[16] = {0};
    while(read(homed, buffer, 0x10u) > 0) {
        if(buffer[10] != 0x73) return;

        if(buffer[12] == 0x00) {
            printf("[key]home_up\n");

            if(combo_triggered) {
                combo_triggered = 0;
                printf("[key] combo already handled, skip screenshot\n");
            }

            uint32_t ts = tick_get();
            if(homeClickTs != -1 && ts - homeClickTs <= 300) {
                switchForeground();
                homeClickTs = -1;
            } else {
                homeClickTs = ts;
            }
        } else {
            printf("[key]home_down\n");
            home_down_time = tick_get();
        }
    }
}

void sysWake(void)
{
    if(sleepTs != -1) {
        deepSleep = false;
        sleepTs   = -1;
        touchOpen();
        lcdOpen();
    }
}

void sysSleep(void)
{
    if(sleepTs == -1) {
        deepSleep = false;
        sleepTs   = tick_get();
        touchClose();
        lcdClose();
    }
}

void sysDeepSleep(void)
{
    deepSleep = true;
    // 睡死过去，相当省电
    system("echo \"0\" >/sys/class/rtc/rtc0/wakealarm");
    system("echo \"mem\" > /sys/power/state");

    // 按电源键会醒过来，继续执行下面的代码

    sysWake(); // 那睡觉的起来了嗷（改到这里是为了防止其他醒来的情况，比如插拔usb）
    char buffer[16] = {0};
    while(read(powerd, buffer, 0x10u) > 0); // 清空电源键的缓冲区，因为开机按的电源键也算数
}

void setDontDeepSleep(bool b)
{
    dontDeepSleep = b;
}

void switchRobot(void)
{
    switchBackground();

    // 我没招了，杀vsftpd还能连带着把lvgl的图像给干没
    // 所以我选择在退出的时候顺带也把vsftpd杀了
    system("killall vsftpd");
    system("chmod 777 ./switch_robot");
    system("sh ./switch_robot");
}

void switchBackground(void)
{
    if(backgroundTs != -1) return;
    backgroundTs = tick_get();
    sleepTs      = -1;
    if(fbd) close(fbd);
    if(dispd) close(dispd);
    if(powerd) close(powerd);
}

void switchForeground(void)
{
    if(backgroundTs == -1) return;

    chdir(homepath);
    system("chmod 777 switch_foreground");
    system("sh ./switch_foreground &");
    // 等待自己被脚本杀死，然后开始新的轮回
    // 因为这里确实处理不好设备占用问题，只能把两个全杀了再重启自己
    sleep(114514);
}

lv_style_t getFontStyle(const char * filename, uint16_t weight, uint16_t font_style)
{
    lv_style_t style;
    lv_style_init(&style);

    lv_ft_info_t ft_info;
    ft_info.name   = filename;
    ft_info.weight = weight;
    ft_info.style  = font_style;
    ft_info.mem    = NULL;

    if(lv_ft_font_init(&ft_info)) {
        lv_style_set_text_font(&style, ft_info.font);
    }

    return style;
}