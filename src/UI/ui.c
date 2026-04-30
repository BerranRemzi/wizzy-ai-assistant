#include "ui.h"

lv_obj_t *ui_MENU = NULL;

void ui_init(void)
{
    ui_MENU = lv_obj_create(NULL);
    lv_obj_clear_flag(ui_MENU, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(ui_MENU, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_MENU, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_disp_load_scr(ui_MENU);
}
