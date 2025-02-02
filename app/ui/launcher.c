#include "app.h"
#include "app_ui.h"

#include "launcher.h"

#include "hosts/hosts_fragment.h"
#include "settings/settings.h"

#include "lvgl/fonts/material-icons/symbols.h"
#include "ui/settings/basic.h"

typedef struct app_root_fragment {
    lv_fragment_t base;
    app_t *app;
    lv_coord_t col_dsc[5], row_dsc[3];
    struct {
        lv_style_t root;
        lv_style_t action_btn;
        lv_style_t action_btn_label;
    } styles;
    lv_obj_t *nav_content;
} app_root_fragment;

static void constructor(lv_fragment_t *self, void *arg);

static void destructor(lv_fragment_t *self);

static lv_obj_t *create_obj(lv_fragment_t *self, lv_obj_t *container);

static lv_obj_t *nav_btn_create(app_root_fragment *fragment, lv_obj_t *container, const char *txt);

static void launcher_open(app_root_fragment *fragment, const lv_fragment_class_t *cls);

static void launcher_hosts(lv_event_t *e);

static void open_settings(lv_event_t *e);

static void launcher_quit(lv_event_t *e);

const lv_fragment_class_t launcher_fragment_class = {
        .constructor_cb = constructor,
        .destructor_cb = destructor,
        .create_obj_cb = create_obj,
        .instance_size = sizeof(app_root_fragment),
};

static void constructor(lv_fragment_t *self, void *arg) {
    app_ui_fragment_args_t *fargs = arg;
    app_root_fragment *fragment = (app_root_fragment *) self;
    fragment->app = fargs->app;
    fragment->col_dsc[0] = LV_GRID_FR(1);
    fragment->col_dsc[1] = LV_DPX(40);
    fragment->col_dsc[2] = LV_DPX(40);
    fragment->col_dsc[3] = LV_DPX(40);
    fragment->col_dsc[4] = LV_GRID_TEMPLATE_LAST;
    fragment->row_dsc[0] = LV_DPX(40);
    fragment->row_dsc[1] = LV_GRID_FR(1);
    fragment->row_dsc[2] = LV_GRID_TEMPLATE_LAST;

    lv_style_init(&fragment->styles.root);
    lv_style_set_pad_gap(&fragment->styles.root, LV_DPX(10));
    lv_style_set_pad_hor(&fragment->styles.root, LV_DPX(30));
    lv_style_set_pad_ver(&fragment->styles.root, LV_DPX(40));
    lv_style_set_bg_opa(&fragment->styles.root, LV_OPA_COVER);
    lv_style_set_bg_color(&fragment->styles.root, lv_color_black());

    lv_style_init(&fragment->styles.action_btn);
    lv_style_set_radius(&fragment->styles.action_btn, LV_RADIUS_CIRCLE);
    lv_style_init(&fragment->styles.action_btn_label);
    lv_style_set_text_font(&fragment->styles.action_btn_label, fargs->app->ui->iconfont.large);
}

static void destructor(lv_fragment_t *self) {
    app_root_fragment *fragment = (app_root_fragment *) self;
    lv_style_reset(&fragment->styles.action_btn_label);
    lv_style_reset(&fragment->styles.action_btn);
    lv_style_reset(&fragment->styles.root);
}

static lv_obj_t *create_obj(lv_fragment_t *self, lv_obj_t *container) {
    app_root_fragment *fragment = (app_root_fragment *) self;
    lv_obj_t *root = lv_obj_create(container);
    lv_obj_remove_style_all(root);
    lv_obj_add_style(root, &fragment->styles.root, 0);
    lv_obj_set_layout(root, LV_LAYOUT_GRID);
    lv_obj_set_grid_dsc_array(root, fragment->col_dsc, fragment->row_dsc);

    lv_obj_t *title = lv_label_create(root);
    lv_obj_set_style_text_font(title, lv_theme_get_font_large(title), 0);
    lv_label_set_text(title, "IHSplay");
    lv_obj_set_size(title, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_grid_cell(title, LV_GRID_ALIGN_START, 0, 2, LV_GRID_ALIGN_SPACE_AROUND, 0, 1);

    lv_obj_t *btn_settings = nav_btn_create(fragment, root, MAT_SYMBOL_SETTINGS);
    lv_obj_set_grid_cell(btn_settings, LV_GRID_ALIGN_STRETCH, 1, 1, LV_GRID_ALIGN_STRETCH, 0, 1);
    lv_obj_add_event_cb(btn_settings, open_settings, LV_EVENT_CLICKED, fragment);

    lv_obj_t *btn_support = nav_btn_create(fragment, root, MAT_SYMBOL_HELP);
    lv_obj_set_grid_cell(btn_support, LV_GRID_ALIGN_STRETCH, 2, 1, LV_GRID_ALIGN_STRETCH, 0, 1);

    lv_obj_t *btn_quit = nav_btn_create(fragment, root, MAT_SYMBOL_CLOSE);
    lv_obj_set_grid_cell(btn_quit, LV_GRID_ALIGN_STRETCH, 3, 1, LV_GRID_ALIGN_STRETCH, 0, 1);
    lv_obj_add_event_cb(btn_quit, launcher_quit, LV_EVENT_CLICKED, fragment->app);

    lv_obj_t *nav_content = lv_obj_create(root);
    lv_obj_remove_style_all(nav_content);
    lv_obj_set_grid_cell(nav_content, LV_GRID_ALIGN_STRETCH, 0, 4, LV_GRID_ALIGN_STRETCH, 1, 1);
    fragment->nav_content = nav_content;

    lv_obj_set_size(root, LV_PCT(100), LV_PCT(100));

    launcher_open(fragment, &hosts_fragment_class);
    return root;
}

static lv_obj_t *nav_btn_create(app_root_fragment *fragment, lv_obj_t *container, const char *txt) {
    lv_obj_t *btn = lv_btn_create(container);
    lv_obj_add_style(btn, &fragment->styles.action_btn, 0);
    lv_obj_t *label = lv_label_create(btn);
    lv_obj_add_style(label, &fragment->styles.action_btn_label, 0);
    lv_label_set_text(label, txt);
    return btn;
}

static void launcher_open(app_root_fragment *fragment, const lv_fragment_class_t *cls) {
    lv_fragment_manager_t *manager = ((lv_fragment_t *) fragment)->child_manager;
    lv_fragment_t *current = lv_fragment_manager_find_by_container(manager, fragment->nav_content);
    if (current != NULL && current->cls == cls) {
        return;
    }
    lv_fragment_t *f = lv_fragment_create(cls, fragment->app);
    lv_fragment_manager_replace(manager, f, &fragment->nav_content);
}

static void open_settings(lv_event_t *e) {
    app_root_fragment *fragment = lv_event_get_user_data(e);
    app_t *app = fragment->app;
    app_ui_push_fragment(app->ui, &settings_basic_fragment_class, app);
}

static void launcher_quit(lv_event_t *e) {
    app_quit(lv_event_get_user_data(e));
}
