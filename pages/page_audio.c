#include "page_audio.h"

static lv_obj_t * btn_control_label;
static lv_obj_t * slider_progress;
static lv_obj_t * slider_volume;
static ff_player_t * player;
static lv_timer_t * timer;

static void back_click(lv_event_t * e);
static void control_click(lv_event_t * e);
static void timer_tick(lv_event_t * e);
static void slider_progress_changed(lv_event_t * e);
static void slider_progress_released(lv_event_t * e);
static void slider_volume_changed(lv_event_t * e);
static void slider_volume_released(lv_event_t * e);

lv_obj_t * page_audio(char * filename)
{
    lv_obj_t * screen = lv_obj_create(lv_scr_act());
    lv_obj_remove_style_all(screen);
    lv_obj_set_size(screen, lv_pct(100), lv_pct(100));

    setDontDeepSleep(true);
    system("echo 1 > /dev/spk_crtl");


    player = player_create(); // /mnt/app/factory/play_test.wav
    if(player_open(player, filename) == 0 && player_init_audio(player) == 0) {
        player_resume(player);
    } else
        player = NULL;

    timer  = lv_timer_create(timer_tick, 250, NULL);

    lv_obj_t * btn_control = lv_btn_create(screen);
    lv_obj_set_size(btn_control, lv_pct(40), lv_pct(20));
    lv_obj_align(btn_control, LV_ALIGN_TOP_MID, 0, lv_pct(65));
    btn_control_label = lv_label_create(btn_control);
    lv_label_set_text(btn_control_label, "pause");
    lv_obj_center(btn_control_label);
    lv_obj_add_event_cb(btn_control, control_click, LV_EVENT_CLICKED, player);

    slider_progress = lv_slider_create(screen);
    lv_obj_set_size(slider_progress, lv_pct(80), lv_pct(10));
    lv_obj_align(slider_progress, LV_ALIGN_TOP_MID, 0, lv_pct(20));
    lv_slider_set_range(slider_progress, 0, 100);
    lv_obj_add_event_cb(slider_progress, slider_progress_changed, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_event_cb(slider_progress, slider_progress_released, LV_EVENT_RELEASED, NULL);

    slider_volume = lv_slider_create(screen);
    lv_obj_set_size(slider_volume, lv_pct(80), lv_pct(10));
    lv_obj_align(slider_volume, LV_ALIGN_TOP_MID, 0, lv_pct(40));
    lv_slider_set_range(slider_volume, 0, 100);
    lv_slider_set_value(slider_volume, player_get_volume(player), LV_ANIM_OFF);
    lv_obj_add_event_cb(slider_volume, slider_volume_changed, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_event_cb(slider_volume, slider_volume_released, LV_EVENT_RELEASED, NULL);

    lv_obj_t * btn_back = lv_btn_create(screen);
    lv_obj_set_size(btn_back, lv_pct(25), lv_pct(12));
    lv_obj_align(btn_back, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    lv_obj_t * btn_back_label = lv_label_create(btn_back);
    lv_label_set_text(btn_back_label, "back");
    lv_obj_center(btn_back_label);
    lv_obj_add_event_cb(btn_back, back_click, LV_EVENT_CLICKED, player);

    return screen;
}

static void back_click(lv_event_t * e)
{
    if(timer) lv_timer_del(timer);
    if(player) player_destroy(player);
    system("echo 0 > /dev/spk_crtl");
    setDontDeepSleep(false);
    page_back();
}

static void control_click(lv_event_t * e)
{
    if(!player) return;
    player_state_t state = player_get_state(player);
    if(state == PLAYER_PAUSED) {
        system("echo 1 > /dev/spk_crtl");
        player_resume(player);
    }
    if(state == PLAYER_PLAYING) {
        player_pause(player);
        system("echo 0 > /dev/spk_crtl");
    }
}

static void slider_progress_changed(lv_event_t * e) 
{

}
static void slider_progress_released(lv_event_t * e)
{
    if(!player) return;
    player_state_t state = player_get_state(player);
    if(state == PLAYER_PLAYING || state == PLAYER_PAUSED){
        lv_obj_t * slider = lv_event_get_target(e);
        int value         = lv_slider_get_value(slider);
        player_seek_pct(player, value);
    }
}

static void slider_volume_changed(lv_event_t * e)
{
    lv_obj_t * slider = lv_event_get_target(e);
    int value         = lv_slider_get_value(slider);
    if(!player) return;
    player_state_t state = player_get_state(player);
    if(state == PLAYER_PLAYING || state == PLAYER_PAUSED) {
        player_set_volume(player, value);
    }
}
static void slider_volume_released(lv_event_t * e)
{
    
}

static void timer_tick(lv_event_t * e){
    if(!player) return;
    player_state_t state = player_get_state(player);
    if(state == PLAYER_PLAYING) lv_label_set_text(btn_control_label, "pause");
    if(state == PLAYER_PAUSED) lv_label_set_text(btn_control_label, "resume");
    lv_slider_set_value(slider_progress, player_get_position_pct(player), LV_ANIM_OFF);
}