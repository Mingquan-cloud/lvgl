#include <Windows.h>
#include "lvgl.h"
#include "examples/lv_examples.h"
#include "demos/lv_demos.h"
#include <stdio.h>
#include <stdbool.h>

// 菜单项结构体，支持图标、文字、数字和子菜单
typedef struct menu_item_s {
	const char* icon_text;              // 可为 LV_SYMBOL_* 或任意字符串
	const char* label_text;             // 菜单文字
	int32_t number;                     // 数字，<0 表示无
	bool disabled;                      // 是否禁用
	const struct menu_item_s* children; // 二级菜单项数组
	uint32_t child_count;               // 二级菜单项数量
	bool is_back;                       // 是否为“返回”项（不再使用）
} menu_item_t;

// 前置声明
static void init_menu_styles(void);
static lv_obj_t* create_menu_panel(lv_obj_t* parent);
static void build_level_items(lv_obj_t* panel, const menu_item_t* items, uint32_t count, lv_group_t* group, bool prepend_back);
static void on_menu_item_event(lv_event_t* e);
static void on_screen_key_event(lv_event_t* e);
// 新增：左右两列交互
static void preview_level2_for(const menu_item_t* parent_item);
static void enter_level2_focus(void);
static void exit_level2_focus(void);

// 全局样式与对象
static lv_style_t g_style_item;
static lv_style_t g_style_item_focused;
static lv_style_t g_style_item_disabled;

static lv_obj_t* g_menu_root = NULL;
static lv_obj_t* g_menu_level1 = NULL;
static lv_obj_t* g_menu_level2 = NULL;

static lv_group_t* g_group_level1 = NULL;
static lv_group_t* g_group_level2 = NULL;

static lv_indev_t* g_keypad_indev = NULL;
static lv_indev_t* g_encoder_indev = NULL;

static const menu_item_t* g_current_level2_parent = NULL;
static lv_obj_t* g_last_focused_level1_btn = NULL;
static bool g_in_level2_focus = false;

// 示例菜单数据
static const menu_item_t SETTINGS_CHILDREN[] = {
	{ LV_SYMBOL_EYE_OPEN, "Brightness", 80, false, NULL, 0, false },
	{ LV_SYMBOL_AUDIO,    "Volume", 50, false, NULL, 0, false },
	{ LV_SYMBOL_EDIT,     "Language", -1, false, NULL, 0, false },
};

static const menu_item_t ABOUT_CHILDREN[] = {
	{ LV_SYMBOL_BARS,  "Version", 123, false, NULL, 0, false },
	{ LV_SYMBOL_OK,    "License", -1, false, NULL, 0, false },
};

static const menu_item_t TOP_LEVEL_ITEMS[] = {
	{ LV_SYMBOL_SETTINGS,   "Settings", -1, false, SETTINGS_CHILDREN, sizeof(SETTINGS_CHILDREN) / sizeof(SETTINGS_CHILDREN[0]), false },
	{ LV_SYMBOL_WIFI,       "Network", 3,  true,  NULL, 0, false }, // 示例禁用项
	{ LV_SYMBOL_FILE,       "File", -1, false, NULL, 0, false },
	{ LV_SYMBOL_HOME,       "About", -1, false, ABOUT_CHILDREN, sizeof(ABOUT_CHILDREN) / sizeof(ABOUT_CHILDREN[0]), false },
};

static void init_menu_styles(void)
{
	lv_style_init(&g_style_item);
	lv_style_set_radius(&g_style_item, 6);
	lv_style_set_bg_opa(&g_style_item, LV_OPA_0);
	lv_style_set_border_width(&g_style_item, 0);
	lv_style_set_pad_left(&g_style_item, 12);
	lv_style_set_pad_right(&g_style_item, 12);
	lv_style_set_pad_top(&g_style_item, 8);
	lv_style_set_pad_bottom(&g_style_item, 8);

	lv_style_init(&g_style_item_focused);
	lv_style_set_bg_opa(&g_style_item_focused, LV_OPA_100);
	lv_style_set_bg_color(&g_style_item_focused, lv_palette_main(LV_PALETTE_BLUE));

	lv_style_init(&g_style_item_disabled);
	lv_style_set_text_opa(&g_style_item_disabled, LV_OPA_40);
}

static lv_obj_t* create_menu_panel(lv_obj_t* parent)
{
	lv_obj_t* panel = lv_obj_create(parent);
	lv_obj_set_size(panel, lv_pct(100), lv_pct(100));
	lv_obj_center(panel);
	lv_obj_set_style_bg_opa(panel, LV_OPA_0, 0);
	lv_obj_set_style_border_width(panel, 0, 0);
	lv_obj_set_style_pad_all(panel, 8, 0);
	lv_obj_set_flex_flow(panel, LV_FLEX_FLOW_COLUMN);
	lv_obj_set_flex_align(panel, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
	lv_obj_set_scroll_dir(panel, LV_DIR_VER);
	return panel;
}

static void set_children_text_color(lv_obj_t* btn, lv_color_t color)
{
	uint32_t count = lv_obj_get_child_cnt(btn);
	for (uint32_t i = 0; i < count; i++) {
		lv_obj_t* child = lv_obj_get_child(btn, i);
		lv_obj_set_style_text_color(child, color, 0);
	}
}

static void on_menu_item_event(lv_event_t* e)
{
	lv_event_code_t code = lv_event_get_code(e);
	lv_obj_t* obj = lv_event_get_target(e);
	const menu_item_t* item = (const menu_item_t*)lv_event_get_user_data(e);
	lv_obj_t* parent = lv_obj_get_parent(obj);

	if (code == LV_EVENT_FOCUSED) {
		set_children_text_color(obj, lv_color_white());
		if (parent == g_menu_level1) {
			g_last_focused_level1_btn = obj;
			if (item && item->children && item->child_count > 0) {
				preview_level2_for(item);
			} else {
				// 没有子项则清空右侧
				build_level_items(g_menu_level2, NULL, 0, g_group_level2, false);
			}
		}
		return;
	}
	if (code == LV_EVENT_DEFOCUSED) {
		set_children_text_color(obj, lv_color_black());
		return;
	}

	if (code == LV_EVENT_KEY) {
		uint32_t key = lv_event_get_key(e);
		if (key != LV_KEY_ENTER) return;
		// 继续按回车按键处理，与点击相同
	} else if (code != LV_EVENT_CLICKED) {
		return;
	}

	if (!item) return;
	if (item->disabled) {
		return;
	}

	// 点击或回车：
	if (parent == g_menu_level1) {
		// 仅当一级被点击/回车时，才进入右侧焦点
		enter_level2_focus();
		return;
	}

	// parent == g_menu_level2 的点击根据业务自行处理
}

static void on_screen_key_event(lv_event_t* e)
{
	if (lv_event_get_code(e) != LV_EVENT_KEY) return;
	uint32_t key = lv_event_get_key(e);
	if (key == LV_KEY_ESC) {
		if (g_in_level2_focus) {
			exit_level2_focus();
		}
	}
}

static lv_obj_t* create_menu_button(lv_obj_t* parent, const menu_item_t* item)
{
	lv_obj_t* btn = lv_btn_create(parent);
	lv_obj_set_width(btn, lv_pct(100));
	lv_obj_set_height(btn, 48);
	lv_obj_set_flex_flow(btn, LV_FLEX_FLOW_ROW);
	lv_obj_set_flex_align(btn, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
	lv_obj_add_style(btn, &g_style_item, LV_PART_MAIN);
	lv_obj_add_style(btn, &g_style_item_focused, LV_PART_MAIN | LV_STATE_FOCUSED);
	lv_obj_add_style(btn, &g_style_item_disabled, LV_PART_MAIN | LV_STATE_DISABLED);
	lv_obj_add_flag(btn, LV_OBJ_FLAG_SCROLL_ON_FOCUS);
	lv_obj_clear_flag(btn, LV_OBJ_FLAG_SCROLLABLE);

	if (item && item->disabled) {
		lv_obj_add_state(btn, LV_STATE_DISABLED);
	}

	if (item && item->icon_text && item->icon_text[0]) {
		lv_obj_t* icon = lv_label_create(btn);
		lv_label_set_text(icon, item->icon_text);
	}

	if (item && item->label_text) {
		lv_obj_t* label = lv_label_create(btn);
		lv_label_set_text(label, item->label_text);
		lv_obj_set_flex_grow(label, 1);
	}

	if (item && item->number >= 0) {
		char buf[16];
		lv_snprintf(buf, sizeof(buf), "%d", item->number);
		lv_obj_t* num = lv_label_create(btn);
		lv_label_set_text(num, buf);
	}

	lv_obj_add_event_cb(btn, on_menu_item_event, LV_EVENT_ALL, (void*)item);

	return btn;
}

static void build_level_items(lv_obj_t* panel, const menu_item_t* items, uint32_t count, lv_group_t* group, bool prepend_back)
{
	LV_UNUSED(prepend_back);
	lv_obj_clean(panel);
	if (group) {
		lv_group_remove_all_objs(group);
	}

	lv_obj_t* first_focusable = NULL;

	for (uint32_t i = 0; i < count; i++) {
		const menu_item_t* item = &items[i];
		lv_obj_t* btn = create_menu_button(panel, item);
		if (group) lv_group_add_obj(group, btn);
		if (!first_focusable && !item->disabled) first_focusable = btn;
	}

	if (group && first_focusable) {
		lv_group_focus_obj(first_focusable);
	}
}

static void preview_level2_for(const menu_item_t* parent_item)
{
	g_current_level2_parent = parent_item;
	if (!parent_item || parent_item->child_count == 0 || !parent_item->children) {
		build_level_items(g_menu_level2, NULL, 0, g_group_level2, false);
		return;
	}
	build_level_items(g_menu_level2, parent_item->children, parent_item->child_count, g_group_level2, false);
}

static void enter_level2_focus(void)
{
	g_in_level2_focus = true;
	if (g_keypad_indev) lv_indev_set_group(g_keypad_indev, g_group_level2);
	if (g_encoder_indev) lv_indev_set_group(g_encoder_indev, g_group_level2);
}

static void exit_level2_focus(void)
{
	g_in_level2_focus = false;
	if (g_keypad_indev) lv_indev_set_group(g_keypad_indev, g_group_level1);
	if (g_encoder_indev) lv_indev_set_group(g_encoder_indev, g_group_level1);
	if (g_last_focused_level1_btn) {
		lv_group_focus_obj(g_last_focused_level1_btn);
	} else if (g_group_level1) {
		lv_group_focus_next(g_group_level1);
	}
}

int main()
{
	lv_init();

	int32_t zoom_level = 100;
	bool allow_dpi_override = false;
	bool simulator_mode = false;
	lv_display_t* display = lv_windows_create_display(
		L"LVGL Display Window",
		800,
		480,
		zoom_level,
		allow_dpi_override,
		simulator_mode);
	if (!display)
	{
		return -1;
	}

	lv_lock();

	lv_indev_t* pointer_device = lv_windows_acquire_pointer_indev(display);
	if (!pointer_device)
	{
		return -1;
	}

	lv_indev_t* keypad_device = lv_windows_acquire_keypad_indev(display);
	if (!keypad_device)
	{
		return -1;
	}

	lv_indev_t* encoder_device = lv_windows_acquire_encoder_indev(display);
	if (!encoder_device)
	{
		return -1;
	}

	// 保存输入设备以便切换分组
	g_keypad_indev = keypad_device;
	g_encoder_indev = encoder_device;

	// 初始化样式与菜单容器
	init_menu_styles();

	// 根容器：左右两列
	g_menu_root = lv_obj_create(lv_scr_act());
	lv_obj_set_size(g_menu_root, lv_pct(100), lv_pct(100));
	lv_obj_set_style_bg_opa(g_menu_root, LV_OPA_0, 0);
	lv_obj_set_style_border_width(g_menu_root, 0, 0);
	lv_obj_set_style_pad_all(g_menu_root, 8, 0);
	lv_obj_set_flex_flow(g_menu_root, LV_FLEX_FLOW_ROW);
	lv_obj_set_flex_align(g_menu_root, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);

	g_menu_level1 = create_menu_panel(g_menu_root);
	g_menu_level2 = create_menu_panel(g_menu_root);

	// 左列固定宽度，右列自适应
	lv_obj_set_width(g_menu_level1, 260);
	lv_obj_set_height(g_menu_level1, lv_pct(100));
	lv_obj_set_flex_grow(g_menu_level1, 0);
	lv_obj_set_width(g_menu_level2, lv_pct(100));
	lv_obj_set_height(g_menu_level2, lv_pct(100));
	lv_obj_set_flex_grow(g_menu_level2, 1);

	// 创建焦点组并绑定键盘/编码器（初始在一级）
	g_group_level1 = lv_group_create();
	g_group_level2 = lv_group_create();
	lv_group_set_wrap(g_group_level1, true);
	lv_group_set_wrap(g_group_level2, true);
	lv_indev_set_group(keypad_device, g_group_level1);
	lv_indev_set_group(encoder_device, g_group_level1);

	// 构建一级菜单
	build_level_items(g_menu_level1, TOP_LEVEL_ITEMS, sizeof(TOP_LEVEL_ITEMS) / sizeof(TOP_LEVEL_ITEMS[0]), g_group_level1, false);

	// 监听 ESC 返回
	lv_obj_add_event_cb(lv_scr_act(), on_screen_key_event, LV_EVENT_KEY, NULL);

	lv_unlock();

	while (1)
	{
		uint32_t time_till_next = lv_timer_handler();
		lv_delay_ms(time_till_next);
	}

	return 0;
}
