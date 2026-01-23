#include "page_bird.h"

static lv_timer_t * timer_move;
static lv_obj_t * bird;
static lv_obj_t * tubeA;
static lv_obj_t * tubeB;
static lv_obj_t * touch_area;
static lv_obj_t * btn_restart;
static lv_obj_t * img_logo;
static lv_obj_t * img_over;
static lv_obj_t * img_tap;
static lv_obj_t * label_score;

static int birdY;
static double birdSpeed;
static int tubeAX;
static int tubeAY;
static int tubeBX;
static int tubeBY;
static bool tubeAScored;
static bool tubeBScored;
static bool started;
static bool over;
static int score;

#define SCREEN_WIDTH 240
#define SCREEN_HEIGHT 240
#define BIRD_WIDTH 36
#define BIRD_HEIGHT 24
#define TUBE_WIDTH 56
#define TUBE_HEIGHT 480
#define GAP_HEIGHT 100
#define BIRD_X 48
#define BIRD_Y_INIT 135
#define TUBE_AX_INIT SCREEN_WIDTH
#define TUBE_BX_INIT SCREEN_WIDTH * 1.5 + TUBE_WIDTH * 0.5
#define RAND_MIN (SCREEN_HEIGHT - GAP_HEIGHT) * 0.25
#define RAND_MAX (SCREEN_HEIGHT - GAP_HEIGHT) * 0.75

static void game_init();
static void game_score();
static void game_stop();
static void back_click(lv_event_t * e);
static void touch_pressed(lv_event_t * e);
static void restart_click(lv_event_t * e);
static void timer_move_tick(lv_event_t * e);

static int rand_between(int start, int end);

lv_obj_t * page_bird()
{
    lv_obj_t * screen = lv_obj_create(lv_scr_act());
    lv_obj_remove_style_all(screen);
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(screen, lv_pct(100), lv_pct(100));

    lv_obj_t * background = lv_img_create(screen);
    lv_obj_set_size(background, SCREEN_WIDTH, SCREEN_HEIGHT);
    lv_obj_set_pos(background, 0, 0);
    lv_img_set_src(background, "/mnt/UDISK/lvgl/res/bird/background.png");

    tubeA = lv_img_create(screen);
    lv_obj_set_size(tubeA, TUBE_WIDTH, TUBE_HEIGHT);
    lv_img_set_src(tubeA, "/mnt/UDISK/lvgl/res/bird/tube.png");

    tubeB = lv_img_create(screen);
    lv_obj_set_size(tubeB, TUBE_WIDTH, TUBE_HEIGHT);
    lv_img_set_src(tubeB, "/mnt/UDISK/lvgl/res/bird/tube.png");

    bird = lv_img_create(screen);
    lv_obj_set_size(bird, BIRD_WIDTH, BIRD_HEIGHT);
    lv_img_set_src(bird, "/mnt/UDISK/lvgl/res/bird/bird2.png");

    img_tap = lv_img_create(screen);
    lv_obj_set_size(img_tap, 120, 98);
    lv_obj_align(img_tap, LV_ALIGN_TOP_MID, 0, lv_pct(42));
    lv_img_set_src(img_tap, "/mnt/UDISK/lvgl/res/bird/tap.png");

    img_over = lv_img_create(screen);
    lv_obj_set_size(img_over, 208, 60);
    lv_obj_align(img_over, LV_ALIGN_TOP_MID, 0, lv_pct(20));
    lv_obj_add_flag(img_over, LV_OBJ_FLAG_HIDDEN);
    lv_img_set_src(img_over, "/mnt/UDISK/lvgl/res/bird/over.png");

    img_logo = lv_img_create(screen);
    lv_obj_set_size(img_logo, 178, 48);
    lv_obj_align(img_logo, LV_ALIGN_TOP_MID, 0, lv_pct(16));
    lv_img_set_src(img_logo, "/mnt/UDISK/lvgl/res/bird/logo.png");

    label_score = lv_label_create(screen);
    lv_obj_align(label_score, LV_ALIGN_TOP_MID, 0, lv_pct(5));
    lv_label_set_text(label_score, "0");

    touch_area = lv_obj_create(screen);
    lv_obj_set_size(touch_area, SCREEN_WIDTH, SCREEN_HEIGHT);
    lv_obj_set_pos(touch_area, 0, 0);
    lv_obj_set_style_bg_opa(touch_area, LV_OPA_0, 0);
    lv_obj_set_style_border_opa(touch_area, LV_OPA_0, 0);
    lv_obj_set_style_outline_opa(touch_area, LV_OPA_0, 0);
    lv_obj_set_style_shadow_opa(touch_area, LV_OPA_0, 0);
    lv_obj_add_event_cb(touch_area, touch_pressed, LV_EVENT_PRESSED, NULL);

    btn_restart = lv_img_create(screen);
    lv_obj_set_size(btn_restart, 120, 72);
    lv_obj_align(btn_restart, LV_ALIGN_TOP_MID, 0, lv_pct(55));
    lv_obj_add_flag(btn_restart, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(btn_restart, LV_OBJ_FLAG_CLICKABLE);
    lv_img_set_src(btn_restart, "/mnt/UDISK/lvgl/res/bird/start.png");
    lv_obj_add_event_cb(btn_restart, restart_click, LV_EVENT_CLICKED, NULL);

    lv_obj_t * btn_back = lv_obj_create(screen);
    lv_obj_set_size(btn_back, lv_pct(100), lv_pct(12));
    lv_obj_align(btn_back, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_style_bg_opa(btn_back, LV_OPA_0, 0);
    lv_obj_set_style_border_opa(btn_back, LV_OPA_0, 0);
    lv_obj_set_style_outline_opa(btn_back, LV_OPA_0, 0);
    lv_obj_set_style_shadow_opa(btn_back, LV_OPA_0, 0);
    lv_obj_add_event_cb(btn_back, back_click, LV_EVENT_CLICKED, NULL);

    game_init();
    timer_move = lv_timer_create(timer_move_tick, 30, NULL);
    lv_timer_pause(timer_move);

    return screen;
}

static void timer_move_tick(lv_event_t * e)
{
    // 下方Y坐标均以屏幕底部为零点

    birdSpeed -= 1;
    birdY += birdSpeed;
    if(birdSpeed < -15) birdSpeed = -15;
    if(birdY < BIRD_HEIGHT) birdY = BIRD_HEIGHT;
    if(birdY > SCREEN_HEIGHT) birdY = SCREEN_HEIGHT;

    if(!over) {
        tubeAX -= 3;
        tubeBX -= 3;
    }

    if(tubeAX < -TUBE_WIDTH) {
        tubeAX = SCREEN_WIDTH;
        tubeAY = rand_between(RAND_MIN, RAND_MAX);
        tubeAScored = false;
    }
    if(tubeBX < -TUBE_WIDTH) {
        tubeBX = SCREEN_WIDTH;
        tubeBY = rand_between(RAND_MIN, RAND_MAX);
        tubeBScored = false;
    }

    lv_obj_set_pos(bird, BIRD_X, SCREEN_HEIGHT - birdY);
    lv_obj_set_pos(tubeA, tubeAX, -TUBE_HEIGHT + SCREEN_HEIGHT + (TUBE_HEIGHT - GAP_HEIGHT) / 2 - tubeAY);
    lv_obj_set_pos(tubeB, tubeBX, -TUBE_HEIGHT + SCREEN_HEIGHT + (TUBE_HEIGHT - GAP_HEIGHT) / 2 - tubeBY);

    if(!over) {
        if(BIRD_X - TUBE_WIDTH < tubeAX && tubeAX < BIRD_X + BIRD_WIDTH) {
            if(!(tubeAY < birdY - BIRD_HEIGHT && tubeAY + GAP_HEIGHT > birdY)) {
                over = true;
            }
        }
        if(!tubeAScored && tubeAX < BIRD_X + BIRD_WIDTH / 2) {
            tubeAScored = true;
            game_score();
        }

        if(BIRD_X - TUBE_WIDTH < tubeBX && tubeBX < BIRD_X + BIRD_WIDTH) {
            if(!(tubeBY < birdY - BIRD_HEIGHT && tubeBY + GAP_HEIGHT > birdY)) {
                over = true;
            }
        }
        if(!tubeBScored && tubeBX < BIRD_X + BIRD_WIDTH / 2) {
            tubeBScored = true;
            game_score();
        }
    }

    if(birdY <= BIRD_HEIGHT) {
        over = true;
        game_stop();
    }
}

static void game_init()
{
    srand((unsigned)time(NULL));

    score     = 0;
    lv_label_set_text(label_score, "0");

    birdSpeed = 10;
    birdY     = BIRD_Y_INIT;

    tubeAX = TUBE_AX_INIT;
    tubeBX = TUBE_BX_INIT;
    tubeAY = rand_between(RAND_MIN, RAND_MAX);
    tubeBY = rand_between(RAND_MIN, RAND_MAX);
    tubeAScored = false;
    tubeBScored = false;

    lv_obj_set_pos(bird, BIRD_X, SCREEN_HEIGHT - birdY);
    lv_obj_set_pos(tubeA, tubeAX, tubeAY - (TUBE_HEIGHT / 2));
    lv_obj_set_pos(tubeB, tubeBX, tubeBY - (TUBE_HEIGHT / 2));

    started = false;
    over    = false;
}

static void game_score()
{
    score += 1;
    lv_label_set_text_fmt(label_score, "%d", score);
}

static void game_stop()
{
    started = false;
    lv_timer_pause(timer_move);
    lv_obj_clear_flag(img_over, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(btn_restart, LV_OBJ_FLAG_HIDDEN);
}

static void restart_click(lv_event_t * e)
{
    lv_obj_add_flag(img_over, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(btn_restart, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(img_tap, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(img_logo, LV_OBJ_FLAG_HIDDEN);

    game_init();
}

static void touch_pressed(lv_event_t * e)
{
    if(!over) {
        if(!started) {
            lv_obj_add_flag(img_tap, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(img_logo, LV_OBJ_FLAG_HIDDEN);
            started = true;
            lv_timer_resume(timer_move);
        }
        birdSpeed = 10;
    }
}

static void back_click(lv_event_t * e)
{
    lv_timer_pause(timer_move);
    lv_timer_del(timer_move);
    page_back();
}

static int rand_between(int start, int end)
{
    return rand() % (end - start + 1) + start;
}