#include "lv_drv_conf.h"
#include "device_config.h"
#include "lvgl/lvgl.h"

#if USE_MONITOR
#include "lv_drivers/display/monitor.h"
#elif USE_FBDEV
#include "lv_drivers/display/fbdev.h"
#endif

#if USE_KEYBOARD
#include "lv_drivers/indev/keyboard.h"
#elif USE_LIBINPUT
#include "lv_drivers/indev/libinput_drv.h"
#elif USE_EVDEV
#include "lv_drivers/indev/evdev.h"
#endif

#include <dirent.h>
#include <fcntl.h>
#include <stdio.h>
#include <threads.h>
#include <unistd.h>

typedef struct item {
	char * text;
	struct item * next;
} item_t;

typedef struct file_entry {
	struct dirent dir;
	long duration;
	struct file_entry * next;
} file_entry_t;

static item_t * mm_item;
static item_t * rm_item;
static void open_menu(item_t * item_list, lv_event_cb_t ev_handler);
static void open_files_menu(const char * path);
static void mm_event_handler(lv_obj_t * obj, lv_event_t ev);
static void rm_event_handler(lv_obj_t * obj, lv_event_t ev);
static void fl_event_handler(lv_obj_t * obj, lv_event_t ev);
static void fill_arrays();
static void batt_mon(lv_task_t * param);

#define PANEL_HEIGHT 20

#define ACTION_REBOOT "Reboot"
#define ACTION_EXEC_SCRIPT "Execute script"
#define ACTION_POWER_OFF "Power off"

#define ACTION_REBOOT_BOOTLOADER "Reboot to bootloader"
#define ACTION_REBOOT_RECOVERY "Reboot to recovery"
#define ACTION_REBOOT_SYSTEM "Reboot to system"

#define IS_DIR(type) (((type) & 4) == 4)

#ifndef NAME_MAX
#define NAME_MAX 255
#endif

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

static lv_indev_drv_t kp_drv;
static lv_indev_t * kp_indev;

static lv_style_t scr_style;
static lv_style_t txt_style;
static lv_obj_t * options;
static lv_obj_t * batt_label;
static char * file_path;

static bool keypad_read(lv_indev_drv_t * indev_drv, lv_indev_data_t * data) {
#if USE_KEYBOARD
	return keyboard_read(indev_drv, data);
#elif USE_LIBINPUT
	return libinput_read(indev_drv, data);
#elif USE_EVDEV
	return evdev_read(indev_drv, data);
#endif
}

// Main menu event handler
static void mm_event_handler(lv_obj_t * obj, lv_event_t ev) {
	uint32_t key = lv_indev_get_key(kp_indev);
	printf("mm_event: %d\n", key);
	if (ev == LV_EVENT_RELEASED && key == LV_KEY_ENTER) {
		char * btn_label = lv_list_get_btn_text(obj);
		printf("%s\n", btn_label);
		if (strcmp(btn_label, ACTION_REBOOT) == 0) {
			open_menu(rm_item, rm_event_handler);
		} else if (strcmp(btn_label, ACTION_EXEC_SCRIPT) == 0) {
			file_path = (char *) malloc(sizeof(char)*PATH_MAX);
			strcpy(file_path, "/");
			open_files_menu(file_path);
		} else if (strcmp(btn_label, ACTION_POWER_OFF) == 0) {
			lv_list_clean(options);
			lv_obj_t * msgbox = lv_msgbox_create(lv_scr_act(), NULL);
			lv_msgbox_set_text(msgbox, "Unmounting partitions");
#ifndef DEVICE_DEBUG
			umount_partitions();
#endif
			lv_msgbox_set_text(msgbox, "Power off");
			system("poweroff");
		}
	}
}

static const char mm_actions[3][80] = {ACTION_REBOOT, ACTION_EXEC_SCRIPT, ACTION_POWER_OFF};
static const char rm_actions[3][80] = {ACTION_REBOOT_SYSTEM, ACTION_REBOOT_RECOVERY, ACTION_REBOOT_BOOTLOADER};

static void open_menu(item_t * item_list, lv_event_cb_t ev_handler) {
	lv_list_clean(options);
	lv_obj_t * list_btn;
	item_t * tmp = item_list;
	int is_first_item = 1;
	do {
		list_btn = lv_list_add_btn(options, NULL, tmp->text);
		lv_obj_add_style(list_btn, LV_BTN_PART_MAIN, &txt_style);
		lv_obj_set_event_cb(list_btn, ev_handler);
		tmp = tmp->next;
		if (is_first_item) {
			lv_list_focus_btn(options, list_btn);
			is_first_item = 0;
		}
	} while (tmp);
}

static void rm_event_handler(lv_obj_t * obj, lv_event_t ev) {
	uint32_t key = lv_indev_get_key(kp_indev);
	printf("rm_event: %d\n", key);
	if (ev == LV_EVENT_RELEASED) {
		if (key == LV_KEY_ENTER) {
			char * btn_label = lv_list_get_btn_text(obj);
			printf("%s\n", btn_label);
			char * command = (char*) malloc(sizeof(char)*50);
			if (strcmp(btn_label, ACTION_REBOOT_RECOVERY) == 0) {
				command = "reboot-mode recovery";
			} else if (strcmp(btn_label, ACTION_REBOOT_BOOTLOADER) == 0) {
				command = "reboot-mode bootloader";
			} else if (strcmp(btn_label, ACTION_REBOOT_SYSTEM) == 0) {
				command = "reboot";
			} else {
				return;
			}
			lv_list_clean(options);
			lv_obj_t * msgbox = lv_msgbox_create(lv_scr_act(), NULL);
			lv_msgbox_set_text(msgbox, "Unmounting partitions");
#ifndef DEVICE_DEBUG
			umount_partitions();
#endif
			lv_msgbox_set_text(msgbox, "Rebooting");
			system(command);
		} else if (key == LV_KEY_BACKSPACE || key == LV_KEY_ESC) {
			open_menu(mm_item, mm_event_handler);
		}
	}
}

// File list event handler
static void fl_event_handler(lv_obj_t * obj, lv_event_t ev) {
	uint32_t key = lv_indev_get_key(kp_indev);
	printf("fl_event: %d\n", key);
	if (ev == LV_EVENT_RELEASED) {
		if (key == LV_KEY_ENTER) {
			char * btn_txt = lv_list_get_btn_text(obj);
			strcat(file_path, btn_txt);
			strcat(file_path, "/");
			open_files_menu(file_path);
		} else if (key == LV_KEY_BACKSPACE || key == LV_KEY_ESC) {
			open_menu(mm_item, mm_event_handler);
		}
	}
}

static void script_event_handler(lv_obj_t * obj, lv_event_t ev) {
	uint32_t key = lv_indev_get_key(kp_indev);
	printf("scr_event: %d\n", key);
	if (ev == LV_EVENT_RELEASED && key == LV_KEY_ENTER) {
		lv_list_clean(options);
		lv_obj_t * msgbox = lv_msgbox_create(lv_scr_act(), NULL);
		lv_msgbox_set_text(msgbox, "Running script...");
		int exec_code = system(file_path);
		lv_msgbox_set_text(msgbox, (exec_code == 0) ? "Successful" : "Error" );
		lv_msgbox_start_auto_close(msgbox, 2000);
		usleep(1000);
		open_menu(mm_item, mm_event_handler);
	} else {
		fl_event_handler(obj, ev);
	}
}

static void scroll_list_handler(lv_obj_t * obj, lv_event_t ev) {
	uint32_t key = lv_indev_get_key(kp_indev);
	printf("scroll_list_event: %d\n", key);
	if (ev == LV_EVENT_PRESSED) {
		if (key == LV_KEY_UP || key == LV_KEY_RIGHT || key == LV_KEY_NEXT) {
			lv_list_up(obj);
		} else if (key == LV_KEY_DOWN || key == LV_KEY_LEFT || key == LV_KEY_PREV) {
			lv_list_down(obj);
		}
	}
}

static void open_files_menu(const char * path) {
	lv_list_clean(options);
	lv_obj_t * list_btn;
	DIR * dir = opendir(path);
	struct dirent * entry;
	entry = readdir(dir);
	int is_first_item = 1;
	int is_script, is_valid_file;
	char * symbol;
	lv_event_cb_t ev_handler;
	do {
		char *ext = strrchr(entry->d_name, '.');
		is_script = !IS_DIR(entry->d_type) && ext && (strcmp(ext, ".sh") == 0);
		is_valid_file = IS_DIR(entry->d_type) || is_script;

		if (is_valid_file) {
			if (is_script) {
				symbol = LV_SYMBOL_FILE;
				ev_handler = script_event_handler;
				list_btn = lv_list_add_btn(options, symbol, entry->d_name);
				lv_obj_set_event_cb(list_btn, ev_handler);
			} else {
				list_btn = lv_list_add_btn(options, LV_SYMBOL_DIRECTORY, entry->d_name);
				lv_obj_set_event_cb(list_btn, fl_event_handler);
			}
			lv_obj_add_style(list_btn, LV_BTN_PART_MAIN, &txt_style);
			if (is_first_item) {
				lv_list_focus_btn(options, list_btn);
				is_first_item = 0;
			}
		}
		entry = readdir(dir);
	} while (entry);
	closedir(dir);
}

void tick_thrd() {
	for(;;) {
#if USE_MONITOR
		SDL_Delay(5);
#else
		usleep(5); // Sleep for 5 milliseconds
#endif
		lv_tick_inc(5); // Tell LVGL that 5 milliseconds have passed
		lv_task_handler(); // Tell LVGL to do its stuff
	}
}

void fill_arrays() {
	int arr_size;

	mm_item = (item_t *) malloc(sizeof(item_t));
	mm_item->text = mm_actions[0];
	mm_item->next = (item_t *) malloc(sizeof(item_t));

	item_t * tmp = mm_item->next;
	arr_size = 3;
	for (int i = 1; i < arr_size; i++) {
		tmp->text = (char *) malloc(sizeof(char)*80);
		strcpy(tmp->text, mm_actions[i]);
		if (i != arr_size-1) {
			tmp->next = (item_t *) malloc(sizeof(item_t));
		} else {
			tmp->next = NULL;
		}
		tmp = tmp->next;
	}

	rm_item = (item_t *) malloc(sizeof(item_t));
	rm_item->text = rm_actions[0];
	rm_item->next = (item_t *) malloc(sizeof(item_t));

	tmp = rm_item->next;

	arr_size = 3;
	for (int i = 1; i < arr_size; i++) {
		tmp->text = (char *) malloc(sizeof(char)*80);
		strcpy(tmp->text, rm_actions[i]);
		if (i != arr_size-1) {
			tmp->next = (item_t *) malloc(sizeof(item_t));
		} else {
			tmp->next = NULL;
		}
		tmp = tmp->next;
	}
}

int main() {
	thrd_t tick_thrd_t;

	// Initialize LVGL
	lv_init();

#if USE_MONITOR
	monitor_init();
#elif USE_FBDEV
	fbdev_init();
#endif

#if USE_EVDEV
	evdev_init();
#endif

	fill_arrays();
#ifndef DEVICE_DEBUG
	mount_partitions();
#endif

	// Set up buffer
	static lv_disp_buf_t disp_buf;
	static lv_color_t buf[LV_HOR_RES_MAX * LV_VER_RES_MAX];
	lv_disp_buf_init(&disp_buf, buf, NULL, LV_HOR_RES_MAX * LV_VER_RES_MAX);

	// Set up display
	lv_disp_drv_t disp_drv;
	lv_disp_drv_init(&disp_drv);
	disp_drv.buffer = &disp_buf;
#if USE_MONITOR
	disp_drv.flush_cb = monitor_flush;
#elif USE_FBDEV
	disp_drv.flush_cb = fbdev_flush;
#endif
	lv_disp_drv_register(&disp_drv);

	lv_indev_drv_init(&kp_drv);
	kp_drv.type = LV_INDEV_TYPE_KEYPAD;
	kp_drv.read_cb = keypad_read;
	kp_indev = lv_indev_drv_register(&kp_drv);

	lv_group_t * group = lv_group_create();
	lv_indev_set_group(kp_indev, group);

	// Create "ticking" thread
	thrd_create(&tick_thrd_t, (thrd_start_t)tick_thrd, NULL);

	lv_style_init(&scr_style);
	lv_style_set_bg_color(&scr_style, LV_STATE_DEFAULT, LV_COLOR_BLACK);
	lv_style_set_radius(&scr_style, LV_STATE_DEFAULT, 0);
	lv_style_set_border_width(&scr_style, LV_STATE_DEFAULT, 0);

	// TEXT STYLE
	lv_style_init(&txt_style);
	lv_style_copy(&txt_style, &scr_style);
	lv_style_set_text_color(&txt_style, LV_STATE_DEFAULT, LV_COLOR_WHITE);
	lv_style_set_bg_color(&txt_style, LV_STATE_PRESSED, LV_COLOR_BLUE);
	lv_style_set_outline_color(&txt_style, LV_STATE_DEFAULT, LV_COLOR_BLUE);

	lv_style_set_pad_top(&txt_style, LV_STATE_DEFAULT, 5);
	lv_style_set_pad_bottom(&txt_style, LV_STATE_DEFAULT, 5);
	lv_style_set_pad_left(&txt_style, LV_STATE_DEFAULT, 5);
	lv_style_set_pad_right(&txt_style, LV_STATE_DEFAULT, 5);

	options = lv_list_create(lv_scr_act(), NULL);
	lv_group_add_obj(group, options);
	lv_obj_set_pos(options, 0, PANEL_HEIGHT);
	lv_obj_set_size(options, LV_HOR_RES_MAX, LV_VER_RES_MAX-PANEL_HEIGHT);

	lv_obj_add_style(options, LV_LIST_PART_BG, &scr_style);

	batt_label = lv_label_create(lv_scr_act(), NULL);
	lv_obj_align(batt_label, NULL, LV_ALIGN_IN_TOP_RIGHT, 8, 0);

	lv_task_create(batt_mon, 2000, LV_TASK_PRIO_LOW, NULL);

	open_menu(mm_item, mm_event_handler);
	lv_obj_set_event_cb(options, scroll_list_handler);
	// Sleep forever
	for(;;)
		usleep(5000);
}

char batt_buf[4];

static void batt_mon(lv_task_t *param) {
	(void) param;

	int fd = open(SYSFS_BAT_PATH, O_RDONLY);
	if (fd >= 0) {
		read(fd, batt_buf, sizeof(batt_buf)-2);
		lv_label_set_text_fmt(batt_label, "%s %%", batt_buf);
	}
	close(fd);
}
