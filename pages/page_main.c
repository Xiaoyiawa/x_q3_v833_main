#include "page_main.h"

static lv_timer_t * timer;
static lv_obj_t * label1;

static void btn_demo_click(lv_event_t * e);
static void btn_audio_click(lv_event_t * e);
static void btn_robot_click(lv_event_t * e);
static void btn_file_manager_click(lv_event_t * e);
static void btn_calculator_click(lv_event_t * e);
static void btn_bird_click(lv_event_t * e);
static void btn_apple_click(lv_event_t * e);
static void timer_tick(lv_event_t * e);

lv_obj_t * page_main() {
	lv_obj_t * screen = lv_obj_create(lv_scr_act());
    //lv_obj_remove_style_all(screen);
    lv_obj_set_size(screen, lv_pct(100), lv_pct(100));

    lv_obj_set_flex_flow(screen, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_scroll_dir(screen, LV_DIR_VER);
    

    label1 = lv_label_create(screen);
	lv_label_set_text(label1, "ciallo lvgl");
    lv_obj_align(label1, LV_ALIGN_CENTER, 0, 0);

    timer = lv_timer_create(timer_tick, 250, label1);

    lv_obj_t * btn_robot = lv_btn_create(screen);
    lv_obj_set_size(btn_robot, lv_pct(45), lv_pct(25));
    lv_obj_align(btn_robot, LV_FLEX_ALIGN_CENTER, 0, 0);
    lv_obj_t * btn_label_robot = lv_label_create(btn_robot);
    lv_label_set_text(btn_label_robot, "robot");
    lv_obj_center(btn_label_robot);
    lv_obj_add_event_cb(btn_robot, btn_robot_click, LV_EVENT_CLICKED, NULL);

    lv_obj_t * btn_file_manager = lv_btn_create(screen);
    lv_obj_set_size(btn_file_manager, lv_pct(45), lv_pct(25));
    lv_obj_align(btn_file_manager, LV_FLEX_ALIGN_CENTER, 0, 0);
    lv_obj_t * btn_label_file_manager = lv_label_create(btn_file_manager);
    lv_label_set_text(btn_label_file_manager, "file manager");
    lv_obj_center(btn_label_file_manager);
    lv_obj_add_event_cb(btn_file_manager, btn_file_manager_click, LV_EVENT_CLICKED, NULL);

    lv_obj_t * btn_calculator = lv_btn_create(screen);
    lv_obj_set_size(btn_calculator, lv_pct(45), lv_pct(25));
    lv_obj_align(btn_calculator, LV_FLEX_ALIGN_CENTER, 0, 0);
    lv_obj_t * btn_label_calculator = lv_label_create(btn_calculator);
    lv_label_set_text(btn_label_calculator, "calculator");
    lv_obj_center(btn_label_calculator);
    lv_obj_add_event_cb(btn_calculator, btn_calculator_click, LV_EVENT_CLICKED, NULL);

    
    lv_obj_t * btn_bird = lv_btn_create(screen);
    lv_obj_set_size(btn_bird, lv_pct(45), lv_pct(25));
    lv_obj_align(btn_bird, LV_FLEX_ALIGN_CENTER, 0, 0);
    lv_obj_t * btn_label_bird = lv_label_create(btn_bird);
    lv_label_set_text(btn_label_bird, "flappy bird");
    lv_obj_center(btn_label_bird);
    lv_obj_add_event_cb(btn_bird, btn_bird_click, LV_EVENT_CLICKED, NULL);
    

    lv_obj_t * btn_demo = lv_btn_create(screen);
    lv_obj_set_size(btn_demo, lv_pct(45), lv_pct(25));
    lv_obj_align(btn_demo, LV_FLEX_ALIGN_CENTER, 0, 0);
    lv_obj_t * btn_label_demo = lv_label_create(btn_demo);
    lv_label_set_text(btn_label_demo, "demo page");
    lv_obj_center(btn_label_demo);
    lv_obj_add_event_cb(btn_demo, btn_demo_click, LV_EVENT_CLICKED, NULL);

    /*
    lv_obj_t * btn_apple = lv_btn_create(screen);
    lv_obj_set_size(btn_apple, lv_pct(45), lv_pct(25));
    lv_obj_align(btn_apple, LV_FLEX_ALIGN_CENTER, 0, 0);
    lv_obj_t * btn_label_apple = lv_label_create(btn_apple);
    lv_label_set_text(btn_label_apple, "apple");
    lv_obj_center(btn_label_apple);
    lv_obj_add_event_cb(btn_apple, btn_apple_click, LV_EVENT_CLICKED, NULL);
    */
    return screen;
}

static void btn_demo_click(lv_event_t * e)      //static可以防止同名冲突
{	
    page_open(page_demo(), NULL);
}

static void btn_audio_click(lv_event_t * e) // static可以防止同名冲突
{
    page_open(page_audio("/mnt/app/factory/play_test.wav"), NULL);
}

static void btn_robot_click(lv_event_t * e)
{
    switchRobot();
}

static void btn_file_manager_click(lv_event_t * e)
{
    page_open(page_file_manager(), NULL);
}

static void btn_calculator_click(lv_event_t * e)
{
    page_open(page_calculator(), NULL);
}

static void btn_bird_click(lv_event_t * e)
{
    page_open(page_bird(), NULL);
}

static void btn_apple_click(lv_event_t * e)
{
    page_open(page_apple(), NULL);
}

static void timer_tick(lv_event_t * e)
{
    char * time_text[24];
    struct timeval tv;
    gettimeofday(&tv, NULL);

    struct tm * tm;
    tm = localtime(&tv);

    lv_snprintf(time_text, sizeof(time_text), "%04d-%02d-%02d %02d:%02d:%02d", 
        tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday,
        tm->tm_hour, tm->tm_min, tm->tm_sec);
    lv_label_set_text(label1, time_text);
}
