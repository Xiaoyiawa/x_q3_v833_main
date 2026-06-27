#include "page_2048.h"
#include "../cJSON/cJSON.h"
#include <sys/stat.h>
#include <errno.h>

#define SAVE_PATH "./setting/2048.json"


static void reset_click(lv_event_t * e);
static void game_value_changed(lv_event_t * e);
static void page_2048_destroy(void * page);
static void update_score(Page2048 * p);
static void save_game_state(Page2048 * p);
static bool load_game_state(Page2048 * p);
static void init_new_game_and_save(Page2048 * p);

BasePage * page_2048_create(void)
{
    Page2048 * p = malloc(sizeof(Page2048));
    if (!p) return NULL;
    memset(p, 0, sizeof(Page2048));

    lv_obj_t * screen = lv_obj_create(lv_scr_act());
    lv_obj_remove_style_all(screen);
    lv_obj_set_size(screen, lv_pct(100), lv_pct(100));
    lv_obj_set_style_bg_color(screen, lv_color_white(), 0);

    lv_obj_t * reset_btn = lv_btn_create(screen);
    lv_obj_set_size(reset_btn, lv_pct(24), lv_pct(12));
    lv_obj_align(reset_btn, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_t * reset_label = lv_label_create(reset_btn);
    lv_label_set_text(reset_label, "reset");
    lv_obj_center(reset_label);
    lv_obj_add_event_cb(reset_btn, reset_click, LV_EVENT_CLICKED, p);

    p->score_label = lv_label_create(screen);
    lv_obj_align(p->score_label, LV_ALIGN_TOP_RIGHT, -20, 5);
    lv_label_set_text(p->score_label, "0");
    lv_obj_set_style_text_font(p->score_label, lv_theme_get_font_large(NULL), 0);
    lv_obj_set_style_text_color(p->score_label, lv_color_black(), 0);

    lv_obj_t * game_container = lv_obj_create(screen);
    lv_obj_remove_style_all(game_container);
    lv_obj_set_size(game_container, 205, 205);
    lv_obj_center(game_container);
    lv_obj_set_pos(game_container, lv_obj_get_x_aligned(game_container), lv_obj_get_y_aligned(game_container) + 13);
    lv_obj_set_style_bg_opa(game_container, LV_OPA_TRANSP, 0);
    lv_obj_set_flex_flow(game_container, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(game_container, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    p->game = lv_100ask_2048_create(game_container);
    lv_obj_set_size(p->game, lv_pct(100), lv_pct(100));
    lv_obj_set_style_pad_all(p->game, 0, 0);
    lv_obj_set_style_border_width(p->game, 0, 0);

    // 加载存档，若失败则初始化新游戏
    if (!load_game_state(p)) {
        init_new_game_and_save(p);
    }
    update_score(p);

    lv_obj_add_event_cb(p->game, game_value_changed, LV_EVENT_VALUE_CHANGED, p);

    p->base.obj = screen;
    p->base.on_destroy = page_2048_destroy;

    sys_set_dont_timeout(true);

    return (BasePage*)p;
}

// 初始化新游戏并保存
static void init_new_game_and_save(Page2048 * p)
{
    lv_100ask_2048_set_new_game(p->game);
    update_score(p);
    save_game_state(p);
}

// 保存游戏状态到json文件
static void save_game_state(Page2048 * p)
{
    if (!p) return;
    uint16_t matrix[4][4];
    lv_100ask_2048_get_matrix(p->game, matrix);
    uint16_t score = lv_100ask_2048_get_score(p->game);

    cJSON *root = cJSON_CreateObject();
    if (!root) return;

    cJSON *grid = cJSON_CreateArray();
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            cJSON_AddItemToArray(grid, cJSON_CreateNumber(matrix[i][j]));
        }
    }
    cJSON_AddItemToObject(root, "grid", grid);
    cJSON_AddNumberToObject(root, "score", score);

    char *json_str = cJSON_Print(root);
    if (json_str) {
        mkdir("./setting", 0755);
        FILE *fp = fopen(SAVE_PATH, "w");
        if (fp) {
            fputs(json_str, fp);
            fclose(fp);
        }
        free(json_str);
    }
    cJSON_Delete(root);
}

// 加载游戏状态...
static bool load_game_state(Page2048 * p)
{
    FILE *fp = fopen(SAVE_PATH, "r");
    if (!fp) {
        return false;   // 文件不存在，杀！
    }

    fseek(fp, 0, SEEK_END);
    long len = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    char *data = malloc(len + 1);
    if (!data) {
        fclose(fp);
        return false;
    }
    fread(data, 1, len, fp);
    data[len] = '\0';
    fclose(fp);

    cJSON *root = cJSON_Parse(data);
    free(data);
    if (!root) {
        return false;   // json解析失败，杀！
    }

    cJSON *grid = cJSON_GetObjectItem(root, "grid");
    cJSON *score_json = cJSON_GetObjectItem(root, "score");
    bool valid = false;
    if (grid && cJSON_IsArray(grid) && score_json) {
        int array_size = cJSON_GetArraySize(grid);
        if (array_size == 4 * 4) {
            uint16_t matrix[4][4];
            int idx = 0;
            int all_zero = 1;
            for (int i = 0; i < 4; i++) {
                for (int j = 0; j < 4; j++) {
                    cJSON *item = cJSON_GetArrayItem(grid, idx++);
                    matrix[i][j] = (uint16_t)cJSON_GetNumberValue(item);
                    if (matrix[i][j] != 0) all_zero = 0;
                }
            }
            if (!all_zero) {
                uint16_t score = (uint16_t)cJSON_GetNumberValue(score_json);
                lv_100ask_2048_set_matrix(p->game, matrix);
                lv_100ask_2048_set_score(p->game, score);
                lv_100ask_2048_refresh(p->game);
                update_score(p);
                valid = true;
            }
        }
    }
    cJSON_Delete(root);

    // 全是0你玩个蛋
    return valid;
}

static void reset_click(lv_event_t * e)
{
    Page2048 * p = (Page2048*)lv_event_get_user_data(e);
    if (!p) return;
    lv_100ask_2048_set_new_game(p->game);
    update_score(p);
    save_game_state(p);
}

static void game_value_changed(lv_event_t * e)
{
    Page2048 * p = (Page2048*)lv_event_get_user_data(e);
    if (!p) return;
    update_score(p);
    save_game_state(p);
}

static void update_score(Page2048 * p)
{
    if (!p) return;
    uint16_t score = lv_100ask_2048_get_score(p->game);
    char buf[16];
    snprintf(buf, sizeof(buf), "%u", score);
    lv_label_set_text(p->score_label, buf);
}

static void page_2048_destroy(void * page)
{
    Page2048 * p = (Page2048*)page;
    if (!p) return;

    // 保存
    save_game_state(p);

    lv_obj_remove_event_cb(p->game, game_value_changed);
    lv_obj_t * reset_btn = lv_obj_get_child(p->base.obj, 0);
    if (reset_btn) {
        lv_obj_remove_event_cb(reset_btn, reset_click);
    }

    sys_set_dont_timeout(false);
    //free(p); // 气死我了加这个会崩
}