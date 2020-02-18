/*
Copyright (c) 2019 lewis he
This is just a demonstration. Most of the functions are not implemented.
The main implementation is low-power standby.
The off-screen standby (not deep sleep) current is about 4mA.
Select standard motherboard and standard backplane for testing.
Created by Lewis he on October 10, 2019.
*/

#include <TTGO.h>
#include <Arduino.h>
#include <time.h>
#include "gui.h"
#include <WiFi.h>
#include "string.h"
#include <Ticker.h>
#include "FS.h"
#include "SD.h"

//! lewis 2020/02/10 add 2019-nCov
#include "ArduinoJson.h"
#include "HTTPClient.h"

//! lewis 2020/02/14 add mlx90614 thermometer
#include "mlx90614.h"


#define RTC_TIME_ZONE   "CST-8"


LV_FONT_DECLARE(Geometr);
LV_FONT_DECLARE(Ubuntu);
LV_IMG_DECLARE(bg);
LV_IMG_DECLARE(bg1);
LV_IMG_DECLARE(bg2);
LV_IMG_DECLARE(bg3);
LV_IMG_DECLARE(WALLPAPER_1_IMG);
LV_IMG_DECLARE(WALLPAPER_2_IMG);
LV_IMG_DECLARE(WALLPAPER_3_IMG);
LV_IMG_DECLARE(step);
LV_IMG_DECLARE(menu);

LV_IMG_DECLARE(wifi);
LV_IMG_DECLARE(light);
LV_IMG_DECLARE(bluetooth);
LV_IMG_DECLARE(sd);
LV_IMG_DECLARE(setting);
LV_IMG_DECLARE(on);
LV_IMG_DECLARE(off);
LV_IMG_DECLARE(level1);
LV_IMG_DECLARE(level2);
LV_IMG_DECLARE(level3);
LV_IMG_DECLARE(iexit);
LV_IMG_DECLARE(modules);
LV_IMG_DECLARE(CAMERA_PNG);
LV_IMG_DECLARE(nCov);
LV_FONT_DECLARE(chinese_font);
LV_FONT_DECLARE(digital_font);
LV_IMG_DECLARE(thermometer);
LV_IMG_DECLARE(qiut);

extern EventGroupHandle_t g_event_group;
extern QueueHandle_t g_event_queue_handle;

static lv_style_t settingStyle;
static lv_obj_t *mainBar = nullptr;
static lv_obj_t *timeLabel = nullptr;
static lv_obj_t *menuBtn = nullptr;

static uint8_t globalIndex = 0;

static void lv_update_task(struct _lv_task_t *);
static void lv_battery_task(struct _lv_task_t *);
static void updateTime();
static void view_event_handler(lv_obj_t *obj, lv_event_t event);

static void wifi_event_cb();
static void sd_event_cb();
static void setting_event_cb();
static void light_event_cb();
static void bluetooth_event_cb();
static void modules_event_cb();
static void camera_event_cb();
static void wifi_destory();
static void nCov_event_cb();
static void thermometer_event_cb();

class StatusBar
{
    typedef struct {
        bool vaild;
        lv_obj_t *icon;
    } lv_status_bar_t;
public:
    StatusBar()
    {
        memset(_array, 0, sizeof(_array));
    }
    void createIcons(lv_obj_t *par)
    {
        _par = par;

        static lv_style_t barStyle;
        lv_style_copy(&barStyle, &lv_style_plain);    /*Copy a built-in style to initialize the new style*/
        barStyle.body.main_color = LV_COLOR_GRAY;
        barStyle.body.grad_color = LV_COLOR_GRAY;
        barStyle.body.opa = LV_OPA_20;
        barStyle.body.radius = 0;
        barStyle.body.border.color = LV_COLOR_GRAY;
        barStyle.body.border.width = 0;
        barStyle.body.border.opa = LV_OPA_50;
        barStyle.text.color = LV_COLOR_WHITE;
        barStyle.image.color = LV_COLOR_WHITE;
        _bar = lv_cont_create(_par, NULL);
        lv_obj_set_size(_bar,  LV_HOR_RES, _barHeight);
        lv_obj_set_style(_bar, &barStyle);

        _array[0].icon = lv_label_create(_bar, NULL);
        lv_label_set_text(_array[0].icon, "100%");

        _array[1].icon = lv_img_create(_bar, NULL);
        lv_img_set_src(_array[1].icon, LV_SYMBOL_BATTERY_FULL);

        _array[2].icon = lv_img_create(_bar, NULL);
        lv_img_set_src(_array[2].icon, LV_SYMBOL_WIFI);
        lv_obj_set_hidden(_array[2].icon, true);

        _array[3].icon = lv_img_create(_bar, NULL);
        lv_img_set_src(_array[3].icon, LV_SYMBOL_BLUETOOTH);
        lv_obj_set_hidden(_array[3].icon, true);

        //step counter
        _array[4].icon = lv_img_create(_bar, NULL);
        lv_img_set_src(_array[4].icon, &step);
        lv_obj_align(_array[4].icon, _bar, LV_ALIGN_IN_LEFT_MID, 10, 0);

        _array[5].icon = lv_label_create(_bar, NULL);
        lv_label_set_text(_array[5].icon, "0");
        lv_obj_align(_array[5].icon, _array[4].icon, LV_ALIGN_OUT_RIGHT_MID, 5, 0);

        refresh();
    }

    void setStepCounter(uint32_t counter)
    {
        lv_label_set_text(_array[5].icon, String(counter).c_str());
        lv_obj_align(_array[5].icon, _array[4].icon, LV_ALIGN_OUT_RIGHT_MID, 5, 0);
    }

    void updateLevel(int level)
    {
        lv_label_set_text(_array[0].icon, (String(level) + "%").c_str());
        refresh();
    }

    void updateBatteryIcon(lv_icon_battery_t icon)
    {
        const char *icons[6] = {LV_SYMBOL_BATTERY_EMPTY, LV_SYMBOL_BATTERY_1, LV_SYMBOL_BATTERY_2, LV_SYMBOL_BATTERY_3, LV_SYMBOL_BATTERY_FULL, LV_SYMBOL_CHARGE};
        lv_img_set_src(_array[1].icon, icons[icon]);
        refresh();
    }

    void show(lv_icon_status_bar_t icon)
    {
        lv_obj_set_hidden(_array[icon].icon, false);
        refresh();
    }

    void hidden(lv_icon_status_bar_t icon)
    {
        lv_obj_set_hidden(_array[icon].icon, true);
        refresh();
    }
    uint8_t height()
    {
        return _barHeight;
    }
    lv_obj_t *self()
    {
        return _bar;
    }
private:
    void refresh()
    {
        int prev;
        for (int i = 0; i < 4; i++) {
            if (!lv_obj_get_hidden(_array[i].icon)) {
                if (i == LV_STATUS_BAR_BATTERY_LEVEL) {
                    lv_obj_align(_array[i].icon, NULL, LV_ALIGN_IN_RIGHT_MID, 0, 0);
                } else {
                    lv_obj_align(_array[i].icon, _array[prev].icon, LV_ALIGN_OUT_LEFT_MID, iconOffset, 0);
                }
                prev = i;
            }
        }
    };
    lv_obj_t *_bar = nullptr;
    lv_obj_t *_par = nullptr;
    uint8_t _barHeight = 30;
    lv_status_bar_t _array[6];
    const int8_t iconOffset = -5;
};



class MenuBar
{
public:
    typedef struct {
        const char *name;
        void *img;
        void (*event_cb)();
    } lv_menu_config_t;

    MenuBar()
    {
        _cont = nullptr;
        _view = nullptr;
        _exit = nullptr;
        _obj = nullptr;
        _vp = nullptr;
    };
    ~MenuBar() {};

    void createMenu(lv_menu_config_t *config, int count, lv_event_cb_t event_cb, int direction = 1)
    {
        static lv_style_t menuStyle;
        lv_style_copy(&menuStyle, &lv_style_plain);    /*Copy a built-in style to initialize the new style*/
        menuStyle.body.main_color = LV_COLOR_GRAY;
        menuStyle.body.grad_color = LV_COLOR_GRAY;
        menuStyle.body.opa = LV_OPA_0;
        menuStyle.body.radius = 0;
        menuStyle.body.border.color = LV_COLOR_GRAY;
        menuStyle.body.border.width = 0;
        menuStyle.body.border.opa = LV_OPA_50;
        menuStyle.text.color = LV_COLOR_WHITE;
        menuStyle.image.color = LV_COLOR_WHITE;


        _count = count;

        _vp = new lv_point_t [count];

        _obj = new lv_obj_t *[count];

        for (int i = 0; i < count; i++) {
            if (direction) {
                _vp[i].x = 0;
                _vp[i].y = i;
            } else {
                _vp[i].x = i;
                _vp[i].y = 0;
            }
        }

        _cont = lv_cont_create(lv_scr_act(), NULL);
        lv_obj_set_size(_cont, LV_HOR_RES, LV_VER_RES - 30);
        lv_obj_align(_cont, NULL, LV_ALIGN_OUT_BOTTOM_MID, 0, 0);
        lv_obj_set_style(_cont, &menuStyle);

        _view = lv_tileview_create(_cont, NULL);
        lv_tileview_set_valid_positions(_view, _vp, count );
        lv_tileview_set_edge_flash(_view, false);
        lv_obj_align(_view, NULL, LV_ALIGN_CENTER, 0, 0);
        lv_tileview_set_style(_view, LV_TILEVIEW_STYLE_MAIN, &menuStyle);
        lv_page_set_sb_mode(_view, LV_SB_MODE_OFF);

        lv_coord_t _w = lv_obj_get_width(_view) ;
        lv_coord_t _h = lv_obj_get_height(_view);

        for (int i = 0; i < count; i++) {
            _obj[i] = lv_cont_create(_view, _view);
            lv_obj_set_size(_obj[i], _w, _h);

            lv_obj_t *img = lv_img_create(_obj[i], NULL);
            lv_img_set_src(img, config[i].img);
            lv_obj_align(img, _obj[i], LV_ALIGN_CENTER, 0, 0);

            lv_obj_t *label = lv_label_create(_obj[i], NULL);
            lv_label_set_text(label, config[i].name);
            lv_obj_align(label, img, LV_ALIGN_OUT_BOTTOM_MID, 0, 0);


            i == 0 ? lv_obj_align(_obj[i], NULL, LV_ALIGN_CENTER, 0, 0) : lv_obj_align(_obj[i], _obj[i - 1], direction ? LV_ALIGN_OUT_BOTTOM_MID : LV_ALIGN_OUT_RIGHT_MID, 0, 0);

            lv_tileview_add_element(_view, _obj[i]);
            lv_obj_set_click(_obj[i], true);
            lv_obj_set_event_cb(_obj[i], event_cb);
        }

        _exit  = lv_imgbtn_create(lv_scr_act(), NULL);
        lv_imgbtn_set_src(_exit, LV_BTN_STATE_REL, &menu);
        lv_imgbtn_set_src(_exit, LV_BTN_STATE_PR, &menu);
        lv_imgbtn_set_src(_exit, LV_BTN_STATE_TGL_REL, &menu);
        lv_imgbtn_set_src(_exit, LV_BTN_STATE_TGL_PR, &menu);
        lv_obj_align(_exit, NULL, LV_ALIGN_IN_BOTTOM_RIGHT, -20, -20);
        lv_obj_set_event_cb(_exit, event_cb);
        lv_obj_set_top(_exit, true);
    }
    lv_obj_t *exitBtn() const
    {
        return _exit;
    }
    lv_obj_t *self() const
    {
        return _cont;
    }
    void hidden(bool en = true)
    {
        lv_obj_set_hidden(_cont, en);
        lv_obj_set_hidden(_exit, en);
    }
    lv_obj_t *obj(int index) const
    {
        if (index > _count)return nullptr;
        return _obj[index];
    }
private:
    lv_obj_t *_cont, *_view, *_exit, * *_obj;
    lv_point_t *_vp ;
    int _count = 0;
};

MenuBar::lv_menu_config_t _cfg[] = {
    {.name = "WiFi",  .img = (void *) &wifi, .event_cb = wifi_event_cb},
    {.name = "nCov", .img = (void *) &nCov, .event_cb = nCov_event_cb},
    {.name = "Thermometer", .img = (void *) &thermometer, .event_cb = thermometer_event_cb},
    {.name = "Bluetooth",  .img = (void *) &bluetooth, .event_cb = bluetooth_event_cb},
    {.name = "SD Card",  .img = (void *) &sd,  .event_cb = sd_event_cb},
    {.name = "Light",  .img = (void *) &light, /*.event_cb = light_event_cb*/},
    {.name = "Setting",  .img = (void *) &setting, /*.event_cb = setting_event_cb */},
    {.name = "Modules",  .img = (void *) &modules, /*.event_cb = modules_event_cb */},
    {.name = "Camera",  .img = (void *) &CAMERA_PNG, /*.event_cb = camera_event_cb*/ },
};


MenuBar menuBars;
StatusBar bar;

static void event_handler(lv_obj_t *obj, lv_event_t event)
{
    if (event == LV_EVENT_SHORT_CLICKED) {  //!  Event callback Is in here
        if (obj == menuBtn) {
            lv_obj_set_hidden(mainBar, true);
            if (menuBars.self() == nullptr) {
                menuBars.createMenu(_cfg, sizeof(_cfg) / sizeof(_cfg[0]), view_event_handler);
                lv_obj_align(menuBars.self(), bar.self(), LV_ALIGN_OUT_BOTTOM_MID, 0, 0);

            } else {
                menuBars.hidden(false);
            }
        }
    }
}


void setupGui()
{
    lv_style_copy(&settingStyle, &lv_style_plain);    /*Copy a built-in style to initialize the new style*/
    settingStyle.body.main_color = LV_COLOR_GRAY;
    settingStyle.body.grad_color = LV_COLOR_GRAY;
    settingStyle.body.opa = LV_OPA_0;
    settingStyle.body.radius = 0;
    settingStyle.body.border.color = LV_COLOR_GRAY;
    settingStyle.body.border.width = 0;
    settingStyle.body.border.opa = LV_OPA_50;
    settingStyle.text.color = LV_COLOR_WHITE;
    settingStyle.image.color = LV_COLOR_WHITE;

    //Create wallpaper
    void *images[] = {(void *) &bg, (void *) &bg1, (void *) &bg2, (void *) &bg3 };
    lv_obj_t *scr = lv_scr_act();
    lv_obj_t *img_bin = lv_img_create(scr, NULL);  /*Create an image object*/
    srand((int)time(0));
    int r = rand() % 4;
    lv_img_set_src(img_bin, images[r]);
    lv_obj_align(img_bin, NULL, LV_ALIGN_CENTER, 0, 0);

    //! bar
    bar.createIcons(scr);
    updateBatteryLevel();
    lv_icon_battery_t icon = LV_ICON_CALCULATION;

    TTGOClass *ttgo = TTGOClass::getWatch();

    if (ttgo->power->isChargeing()) {
        icon = LV_ICON_CHARGE;
    }
    updateBatteryIcon(icon);

    //! main
    static lv_style_t mainStyle;
    lv_style_copy(&mainStyle, &lv_style_plain);    /*Copy a built-in style to initialize the new style*/
    mainStyle.body.main_color = LV_COLOR_GRAY;
    mainStyle.body.grad_color = LV_COLOR_GRAY;
    mainStyle.body.opa = LV_OPA_0;
    mainStyle.body.radius = 0;
    mainStyle.body.border.color = LV_COLOR_GRAY;
    mainStyle.body.border.width = 0;
    mainStyle.body.border.opa = LV_OPA_50;
    mainStyle.text.color = LV_COLOR_WHITE;
    mainStyle.image.color = LV_COLOR_WHITE;
    mainBar = lv_cont_create(scr, NULL);
    lv_obj_set_size(mainBar,  LV_HOR_RES, LV_VER_RES - bar.height());
    lv_obj_set_style(mainBar, &mainStyle);
    lv_obj_align(mainBar, bar.self(), LV_ALIGN_OUT_BOTTOM_MID, 0, 0);

    //! Time
    static lv_style_t timeStyle;
    lv_style_copy(&timeStyle, &mainStyle);
    timeStyle.text.font = &Ubuntu;

    timeLabel = lv_label_create(mainBar, NULL);
    lv_label_set_style(timeLabel, LV_LABEL_STYLE_MAIN, &timeStyle);
    updateTime();

    //! menu
    static lv_style_t style_pr;
    lv_style_copy(&style_pr, &lv_style_plain);
    style_pr.image.color = LV_COLOR_BLACK;
    style_pr.image.intense = LV_OPA_50;
    style_pr.text.color = lv_color_hex3(0xaaa);

    menuBtn = lv_imgbtn_create(mainBar, NULL);
    lv_imgbtn_set_src(menuBtn, LV_BTN_STATE_REL, &menu);
    lv_imgbtn_set_src(menuBtn, LV_BTN_STATE_PR, &menu);
    lv_imgbtn_set_src(menuBtn, LV_BTN_STATE_TGL_REL, &menu);
    lv_imgbtn_set_src(menuBtn, LV_BTN_STATE_TGL_PR, &menu);
    lv_imgbtn_set_style(menuBtn, LV_BTN_STATE_PR, &style_pr);
    lv_obj_align(menuBtn, mainBar, LV_ALIGN_OUT_BOTTOM_MID, 0, -70);
    lv_obj_set_event_cb(menuBtn, event_handler);

    lv_task_create(lv_update_task, 1000, LV_TASK_PRIO_LOWEST, NULL);
    lv_task_create(lv_battery_task, 30000, LV_TASK_PRIO_LOWEST, NULL);
}

void updateStepCounter(uint32_t counter)
{
    bar.setStepCounter(counter);
}

static void updateTime()
{
    time_t now;
    struct tm  info;
    char buf[64];
    time(&now);
    localtime_r(&now, &info);
    strftime(buf, sizeof(buf), "%H:%M", &info);
    lv_label_set_text(timeLabel, buf);
    lv_obj_align(timeLabel, NULL, LV_ALIGN_IN_TOP_MID, 0, 20);
}

void updateBatteryLevel()
{
    TTGOClass *ttgo = TTGOClass::getWatch();
    int p = ttgo->power->getBattPercentage();
    bar.updateLevel(p);
}

void updateBatteryIcon(lv_icon_battery_t icon)
{
    if (icon >= LV_ICON_CALCULATION) {
        TTGOClass *ttgo = TTGOClass::getWatch();
        int level = ttgo->power->getBattPercentage();
        if (level > 95)icon = LV_ICON_BAT_FULL;
        else if (level > 80)icon = LV_ICON_BAT_3;
        else if (level > 45)icon = LV_ICON_BAT_2;
        else if (level > 20)icon = LV_ICON_BAT_1;
        else icon = LV_ICON_BAT_EMPTY;
    }
    bar.updateBatteryIcon(icon);
}


static void lv_update_task(struct _lv_task_t *data)
{
    updateTime();
}

static void lv_battery_task(struct _lv_task_t *data)
{
    updateBatteryLevel();
}

static void view_event_handler(lv_obj_t *obj, lv_event_t event)
{
    int size = sizeof(_cfg) / sizeof(_cfg[0]);
    if (event == LV_EVENT_SHORT_CLICKED) {
        if (obj == menuBars.exitBtn()) {
            menuBars.hidden();
            lv_obj_set_hidden(mainBar, false);
            return;
        }
        for (int i = 0; i < size; i++) {
            if (obj == menuBars.obj(i)) {
                if (_cfg[i].event_cb != nullptr) {
                    menuBars.hidden();
                    _cfg[i].event_cb();
                }
                return;
            }
        }
    }
}

/*****************************************************************
 *
 *          ! Keyboard Class
 *
 */


class Keyboard
{
public:
    typedef enum {
        KB_EVENT_OK,
        KB_EVENT_EXIT,
    } kb_event_t;

    typedef void (*kb_event_cb)(kb_event_t event);

    Keyboard()
    {
        _kbCont = nullptr;
    };

    ~Keyboard()
    {
        if (_kbCont)
            lv_obj_del(_kbCont);
        _kbCont = nullptr;
    };

    void create(lv_obj_t *parent =  nullptr)
    {
        static lv_style_t kbStyle;
        lv_style_copy(&kbStyle, &lv_style_plain);    /*Copy a built-in style to initialize the new style*/
        kbStyle.body.main_color = LV_COLOR_GRAY;
        kbStyle.body.grad_color = LV_COLOR_GRAY;
        kbStyle.body.opa = LV_OPA_0;
        kbStyle.body.radius = 0;
        kbStyle.body.border.color = LV_COLOR_GRAY;
        kbStyle.body.border.width = 0;
        kbStyle.body.border.opa = LV_OPA_50;
        kbStyle.text.color = LV_COLOR_WHITE;
        kbStyle.image.color = LV_COLOR_WHITE;

        if (parent == nullptr) {
            parent = lv_scr_act();
        }

        _kbCont = lv_cont_create(parent, NULL);
        lv_obj_set_size(_kbCont, LV_HOR_RES, LV_VER_RES - 30);
        lv_obj_align(_kbCont, NULL, LV_ALIGN_CENTER, 0, 0);
        lv_obj_set_style(_kbCont, &kbStyle);

        lv_obj_t *ta = lv_ta_create(_kbCont, NULL);
        lv_obj_set_height(ta, 40);
        lv_ta_set_one_line(ta, true);
        lv_ta_set_pwd_mode(ta, false);
        lv_ta_set_text(ta, "");
        lv_obj_align(ta, _kbCont, LV_ALIGN_IN_TOP_MID, 0, 10);

        lv_obj_t *kb = lv_kb_create(_kbCont, NULL);
        lv_kb_set_map(kb, btnm_mapplus[0]);
        lv_obj_set_height(kb, LV_VER_RES / 3 * 2);
        lv_obj_align(kb, _kbCont, LV_ALIGN_IN_BOTTOM_MID, 0, 0);
        lv_kb_set_ta(kb, ta);

        lv_obj_set_style(ta, &kbStyle);
        lv_obj_set_style(kb, &kbStyle);
        lv_obj_set_event_cb(kb, __kb_event_cb);

        _kb = this;
    }

    void align(const lv_obj_t *base, lv_align_t align, lv_coord_t x = 0, lv_coord_t y = 0)
    {
        lv_obj_align(_kbCont, base, align, x, y);
    }

    static void __kb_event_cb(lv_obj_t *kb, lv_event_t event)
    {
        if (event != LV_EVENT_VALUE_CHANGED && event != LV_EVENT_LONG_PRESSED_REPEAT) return;
        lv_kb_ext_t *ext = (lv_kb_ext_t *)lv_obj_get_ext_attr(kb);
        const char *txt = lv_btnm_get_active_btn_text(kb);
        if (txt == NULL) return;
        static int index = 0;
        if (strcmp(txt, LV_SYMBOL_OK) == 0) {
            strcpy(__buf, lv_ta_get_text(ext->ta));
            if (_kb->_cb != nullptr) {
                _kb->_cb(KB_EVENT_OK);
            }
            return;
        } else if (strcmp(txt, "Exit") == 0) {
            if (_kb->_cb != nullptr) {
                _kb->_cb(KB_EVENT_EXIT);
            }
            return;
        } else if (strcmp(txt, LV_SYMBOL_RIGHT) == 0) {
            index = index + 1 >= sizeof(btnm_mapplus) / sizeof(btnm_mapplus[0]) ? 0 : index + 1;
            lv_kb_set_map(kb, btnm_mapplus[index]);
            return;
        } else if (strcmp(txt, "Del") == 0) {
            lv_ta_del_char(ext->ta);
        } else {
            lv_ta_add_text(ext->ta, txt);
        }
    }

    void setKeyboardEvent(kb_event_cb cb)
    {
        _cb = cb;
    }

    const char *getText()
    {
        return (const char *)__buf;
    }

    void hidden(bool en = true)
    {
        lv_obj_set_hidden(_kbCont, en);
    }

private:
    lv_obj_t *_kbCont = nullptr;
    kb_event_cb _cb = nullptr;
    static const char *btnm_mapplus[10][23];
    static Keyboard *_kb;
    static char __buf[128];
};
char Keyboard::__buf[128];
Keyboard *Keyboard::_kb = nullptr;
const char *Keyboard::btnm_mapplus[10][23] = {
    {
        "a", "b", "c",   "\n",
        "d", "e", "f",   "\n",
        "g", "h", "i",   "\n",
        LV_SYMBOL_OK, "Del", "Exit", LV_SYMBOL_RIGHT, ""
    },
    {
        "j", "k", "l", "\n",
        "n", "m", "o",  "\n",
        "p", "q", "r",  "\n",
        LV_SYMBOL_OK, "Del", "Exit", LV_SYMBOL_RIGHT, ""
    },
    {
        "s", "t", "u",   "\n",
        "v", "w", "x", "\n",
        "y", "z", " ", "\n",
        LV_SYMBOL_OK, "Del", "Exit", LV_SYMBOL_RIGHT, ""
    },
    {
        "A", "B", "C",  "\n",
        "D", "E", "F",   "\n",
        "G", "H", "I",  "\n",
        LV_SYMBOL_OK, "Del", "Exit", LV_SYMBOL_RIGHT, ""
    },
    {
        "J", "K", "L", "\n",
        "N", "M", "O",  "\n",
        "P", "Q", "R", "\n",
        LV_SYMBOL_OK, "Del", "Exit", LV_SYMBOL_RIGHT, ""
    },
    {
        "S", "T", "U",   "\n",
        "V", "W", "X",   "\n",
        "Y", "Z", " ", "\n",
        LV_SYMBOL_OK, "Del", "Exit", LV_SYMBOL_RIGHT, ""
    },
    {
        "1", "2", "3",  "\n",
        "4", "5", "6",  "\n",
        "7", "8", "9",  "\n",
        LV_SYMBOL_OK, "Del", "Exit", LV_SYMBOL_RIGHT, ""
    },
    {
        "0", "+", "-",  "\n",
        "/", "*", "=",  "\n",
        "!", "?", "#",  "\n",
        LV_SYMBOL_OK, "Del", "Exit", LV_SYMBOL_RIGHT, ""
    },
    {
        "<", ">", "@",  "\n",
        "%", "$", "(",  "\n",
        ")", "{", "}",  "\n",
        LV_SYMBOL_OK, "Del", "Exit", LV_SYMBOL_RIGHT, ""
    },
    {
        "[", "]", ";",  "\n",
        "\"", "'", ".", "\n",
        ",", ":",  " ", "\n",
        LV_SYMBOL_OK, "Del", "Exit", LV_SYMBOL_RIGHT, ""
    }
};


/*****************************************************************
 *
 *          ! Switch Class
 *
 */
class Switch
{
public:
    typedef struct {
        const char *name;
        void (*cb)(uint8_t, bool);
    } switch_cfg_t;

    typedef void (*exit_cb)();
    Switch()
    {
        _swCont = nullptr;
    }
    ~Switch()
    {
        if (_swCont)
            lv_obj_del(_swCont);
        _swCont = nullptr;
    }

    void create(switch_cfg_t *cfg, uint8_t count, exit_cb cb, lv_obj_t *parent = nullptr)
    {
        static lv_style_t swlStyle;
        lv_style_copy(&swlStyle, &lv_style_plain);    /*Copy a built-in style to initialize the new style*/
        swlStyle.body.main_color = LV_COLOR_GRAY;
        swlStyle.body.grad_color = LV_COLOR_GRAY;
        swlStyle.body.opa = LV_OPA_0;
        swlStyle.body.radius = 0;
        swlStyle.body.border.color = LV_COLOR_GRAY;
        swlStyle.body.border.width = 0;
        swlStyle.body.border.opa = LV_OPA_50;
        swlStyle.text.color = LV_COLOR_WHITE;
        swlStyle.image.color = LV_COLOR_WHITE;

        if (parent == nullptr) {
            parent = lv_scr_act();
        }
        _exit_cb = cb;

        _swCont = lv_cont_create(parent, NULL);
        lv_obj_set_size(_swCont, LV_HOR_RES, LV_VER_RES - 30);
        lv_obj_align(_swCont, NULL, LV_ALIGN_CENTER, 0, 0);
        lv_obj_set_style(_swCont, &swlStyle);

        _count = count;
        _sw = new lv_obj_t *[count];
        _cfg = new switch_cfg_t [count];

        memcpy(_cfg, cfg, sizeof(switch_cfg_t) * count);

        lv_obj_t *prev = nullptr;
        for (int i = 0; i < count; i++) {
            lv_obj_t *la1 = lv_label_create(_swCont, NULL);
            lv_label_set_text(la1, cfg[i].name);
            i == 0 ? lv_obj_align(la1, NULL, LV_ALIGN_IN_TOP_LEFT, 30, 20) : lv_obj_align(la1, prev, LV_ALIGN_OUT_BOTTOM_MID, 0, 20);
            _sw[i] = lv_imgbtn_create(_swCont, NULL);
            lv_imgbtn_set_src(_sw[i], LV_BTN_STATE_REL, &off);
            lv_imgbtn_set_src(_sw[i], LV_BTN_STATE_PR, &off);
            lv_imgbtn_set_src(_sw[i], LV_BTN_STATE_TGL_REL, &off);
            lv_imgbtn_set_src(_sw[i], LV_BTN_STATE_TGL_PR, &off);
            lv_imgbtn_set_toggle(_sw[i], true);
            lv_obj_align(_sw[i], la1, LV_ALIGN_OUT_RIGHT_MID, 80, 0);
            lv_obj_set_event_cb(_sw[i], __switch_event_cb);
            prev = la1;
        }

        _exitBtn = lv_imgbtn_create(_swCont, NULL);
        lv_imgbtn_set_src(_exitBtn, LV_BTN_STATE_REL, &iexit);
        lv_imgbtn_set_src(_exitBtn, LV_BTN_STATE_PR, &iexit);
        lv_imgbtn_set_src(_exitBtn, LV_BTN_STATE_TGL_REL, &iexit);
        lv_imgbtn_set_src(_exitBtn, LV_BTN_STATE_TGL_PR, &iexit);
        lv_obj_align(_exitBtn, _swCont, LV_ALIGN_IN_BOTTOM_MID, 0, -5);
        lv_obj_set_event_cb(_exitBtn, __switch_event_cb);

        _switch = this;
    }

    void align(const lv_obj_t *base, lv_align_t align, lv_coord_t x = 0, lv_coord_t y = 0)
    {
        lv_obj_align(_swCont, base, align, x, y);
    }

    void hidden(bool en = true)
    {
        lv_obj_set_hidden(_swCont, en);
    }

    static void __switch_event_cb(lv_obj_t *obj, lv_event_t event)
    {
        if (event == LV_EVENT_SHORT_CLICKED) {
            if (obj == _switch->_exitBtn) {
                if ( _switch->_exit_cb != nullptr) {
                    _switch->_exit_cb();
                    return;
                }
            }
        }

        if (event == LV_EVENT_VALUE_CHANGED) {
            for (int i = 0; i < _switch->_count ; i++) {
                lv_obj_t *sw = _switch->_sw[i];
                if (obj == sw) {
                    const void *src =  lv_imgbtn_get_src(sw, LV_BTN_STATE_REL);
                    const void *dst = src == &off ? &on : &off;
                    bool en = src == &off;
                    lv_imgbtn_set_src(sw, LV_BTN_STATE_REL, dst);
                    lv_imgbtn_set_src(sw, LV_BTN_STATE_PR, dst);
                    lv_imgbtn_set_src(sw, LV_BTN_STATE_TGL_REL, dst);
                    lv_imgbtn_set_src(sw, LV_BTN_STATE_TGL_PR, dst);
                    if (_switch->_cfg[i].cb != nullptr) {
                        _switch->_cfg[i].cb(i, en);
                    }
                    return;
                }
            }
        }
    }

    void setStatus(uint8_t index, bool en)
    {
        if (index > _count)return;
        lv_obj_t *sw = _sw[index];
        const void *dst =  en ? &on : &off;
        lv_imgbtn_set_src(sw, LV_BTN_STATE_REL, dst);
        lv_imgbtn_set_src(sw, LV_BTN_STATE_PR, dst);
        lv_imgbtn_set_src(sw, LV_BTN_STATE_TGL_REL, dst);
        lv_imgbtn_set_src(sw, LV_BTN_STATE_TGL_PR, dst);
    }

private:
    static Switch *_switch;
    lv_obj_t *_swCont = nullptr;
    uint8_t _count;
    lv_obj_t **_sw = nullptr;
    switch_cfg_t *_cfg = nullptr;
    lv_obj_t *_exitBtn = nullptr;
    exit_cb _exit_cb = nullptr;
};

Switch *Switch::_switch = nullptr;


/*****************************************************************
 *
 *          ! Preload Class
 *
 */
class Preload
{
public:
    Preload()
    {
        _preloadCont = nullptr;
    }
    ~Preload()
    {
        if (_preloadCont == nullptr) return;
        lv_obj_del(_preloadCont);
        _preloadCont = nullptr;
    }
    void create(lv_obj_t *parent = nullptr)
    {
        if (parent == nullptr) {
            parent = lv_scr_act();
        }
        if (_preloadCont == nullptr) {
            static lv_style_t plStyle;
            lv_style_copy(&plStyle, &lv_style_plain);    /*Copy a built-in style to initialize the new style*/
            plStyle.body.main_color = LV_COLOR_GRAY;
            plStyle.body.grad_color = LV_COLOR_GRAY;
            plStyle.body.opa = LV_OPA_0;
            plStyle.body.radius = 0;
            plStyle.body.border.color = LV_COLOR_GRAY;
            plStyle.body.border.width = 0;
            plStyle.body.border.opa = LV_OPA_50;
            plStyle.text.color = LV_COLOR_WHITE;
            plStyle.image.color = LV_COLOR_WHITE;

            static lv_style_t style;
            lv_style_copy(&style, &lv_style_plain);
            style.line.width = 10;                         /*10 px thick arc*/
            style.line.color = LV_COLOR_GREEN;       /*Blueish arc color*/
            style.body.border.color = LV_COLOR_WHITE; /*Gray background color*/
            style.body.border.width = 10;
            style.body.padding.left = 0;
            _preloadCont = lv_cont_create(parent, NULL);
            lv_obj_set_size(_preloadCont, LV_HOR_RES, LV_VER_RES - 30);
            lv_obj_align(_preloadCont, NULL, LV_ALIGN_OUT_BOTTOM_MID, 0, 0);
            lv_obj_set_style(_preloadCont, &plStyle);

            lv_obj_t *preload = lv_preload_create(_preloadCont, NULL);
            lv_obj_set_size(preload, lv_obj_get_width(_preloadCont) / 2, lv_obj_get_height(_preloadCont) / 2);
            lv_preload_set_style(preload, LV_PRELOAD_STYLE_MAIN, &style);
            lv_obj_align(preload, _preloadCont, LV_ALIGN_CENTER, 0, 0);
        }
    }
    void align(const lv_obj_t *base, lv_align_t align, lv_coord_t x = 0, lv_coord_t y = 0)
    {
        lv_obj_align(_preloadCont, base, align, x, y);
    }

    void hidden(bool en = true)
    {
        lv_obj_set_hidden(_preloadCont, en);
    }

private:
    lv_obj_t *_preloadCont = nullptr;
};


/*****************************************************************
 *
 *          ! List Class
 *
 */

class List
{
public:
    typedef void(*list_event_cb)(const char *);
    List()
    {
    }
    ~List()
    {
        if (_listCont == nullptr) return;
        lv_obj_del(_listCont);
        _listCont = nullptr;
    }
    void create(lv_obj_t *parent = nullptr, lv_style_t *style = nullptr)
    {
        if (parent == nullptr) {
            parent = lv_scr_act();
        }
        if (_listCont == nullptr) {
            static lv_style_t listStyle;

            if (style != nullptr) {
                lv_style_copy(&listStyle, style);
            } else {
                lv_style_copy(&listStyle, &lv_style_plain);    /*Copy a built-in style to initialize the new style*/
                listStyle.body.main_color = LV_COLOR_GRAY;
                listStyle.body.grad_color = LV_COLOR_GRAY;
                listStyle.body.opa = LV_OPA_0;
                listStyle.body.radius = 0;
                listStyle.body.border.color = LV_COLOR_GRAY;
                listStyle.body.border.width = 0;
                listStyle.body.border.opa = LV_OPA_50;
                listStyle.text.color = LV_COLOR_WHITE;
                listStyle.image.color = LV_COLOR_WHITE;
                listStyle.text.font = &chinese_font;
            }

            _listCont = lv_list_create(lv_scr_act(), NULL);
            lv_obj_set_size(_listCont, LV_HOR_RES, LV_VER_RES - 30);
            lv_list_set_style(_listCont, LV_LIST_STYLE_BG, &listStyle);
            lv_obj_align(_listCont, NULL, LV_ALIGN_CENTER, 0, 0);
        }
        _list = this;
    }
    //! lewis add
    void setStyle( lv_list_style_t type, lv_style_t *style)
    {
        lv_list_set_style(_listCont, type, style);
    }
    //! lewis
    lv_obj_t *add(const char *txt, void *imgsrc = (void *)LV_SYMBOL_WIFI)
    {
        lv_obj_t *btn = lv_list_add_btn(_listCont, imgsrc, txt);
        lv_obj_set_event_cb(btn, __list_event_cb);
        return btn;
    }

    void align(const lv_obj_t *base, lv_align_t align, lv_coord_t x = 0, lv_coord_t y = 0)
    {
        lv_obj_align(_listCont, base, align, x, y);
    }

    void hidden(bool en = true)
    {
        lv_obj_set_hidden(_listCont, en);
    }

    static void __list_event_cb(lv_obj_t *obj, lv_event_t event)
    {
        if (event == LV_EVENT_SHORT_CLICKED) {
            const char *txt = lv_list_get_btn_text(obj);
            if (_list->_cb != nullptr) {
                _list->_cb(txt);
            }
        }
    }
    void setListCb(list_event_cb cb)
    {
        _cb = cb;
    }
private:
    lv_obj_t *_listCont = nullptr;
    static List *_list ;
    list_event_cb _cb = nullptr;
};
List *List::_list = nullptr;

/*****************************************************************
 *
 *          ! Task Class
 *
 */
class Task
{
public:
    Task()
    {
        _handler = nullptr;
        _cb = nullptr;
    }
    ~Task()
    {
        if ( _handler == nullptr)return;
        Serial.println("Free Task Func");
        lv_task_del(_handler);
        _handler = nullptr;
        _cb = nullptr;
    }

    void create(lv_task_cb_t cb, uint32_t period = 1000, lv_task_prio_t prio = LV_TASK_PRIO_LOW)
    {
        _handler = lv_task_create(cb,  period,  prio, NULL);
    };

private:
    lv_task_t *_handler = nullptr;
    lv_task_cb_t _cb = nullptr;
};

/*****************************************************************
 *
 *          ! MesBox Class
 *
 */

class MBox
{
public:
    MBox()
    {
        _mbox = nullptr;
    }
    ~MBox()
    {
        if (_mbox == nullptr)return;
        lv_obj_del(_mbox);
        _mbox = nullptr;
    }

    void create(const char *text, lv_event_cb_t event_cb, const char **btns = nullptr, lv_obj_t *par = nullptr)
    {
        if (_mbox != nullptr)return;
        lv_obj_t *p = par == nullptr ? lv_scr_act() : par;
        _mbox = lv_mbox_create(p, NULL);
        lv_mbox_set_text(_mbox, text);
        if (btns == nullptr) {
            static const char *defBtns[] = {"Ok", ""};
            lv_mbox_add_btns(_mbox, defBtns);
        } else {
            lv_mbox_add_btns(_mbox, btns);
        }
        lv_obj_set_width(_mbox, LV_HOR_RES - 40);
        lv_obj_set_event_cb(_mbox, event_cb);
        lv_obj_align(_mbox, NULL, LV_ALIGN_CENTER, 0, 0);
    }

    void setData(void *data)
    {
        lv_obj_set_user_data(_mbox, data);
    }

    void *getData()
    {
        return lv_obj_get_user_data(_mbox);
    }

    void setBtn(const char **btns)
    {
        lv_mbox_add_btns(_mbox, btns);
    }

private:
    lv_obj_t *_mbox = nullptr;
};




/*****************************************************************
 *
 *          ! GLOBAL VALUE
 *
 */
static Keyboard *kb = nullptr;
static Switch *sw = nullptr;
static Preload *pl = nullptr;
static List *list = nullptr;
static Task *task = nullptr;
static Ticker *gTicker = nullptr;
static MBox *mbox = nullptr;

static char ssid[64], password[64];

/*****************************************************************
 *
 *          !WIFI EVENT
 *
 */
void wifi_connect_status(bool result)
{
    if (gTicker != nullptr) {
        delete gTicker;
        gTicker = nullptr;
    }
    if (kb != nullptr) {
        delete kb;
        kb = nullptr;
    }
    if (sw != nullptr) {
        delete sw;
        sw = nullptr;
    }
    if (pl != nullptr) {
        delete pl;
        pl = nullptr;
    }
    if (result) {
        bar.show(LV_STATUS_BAR_WIFI);
    } else {
        bar.hidden(LV_STATUS_BAR_WIFI);
    }
    menuBars.hidden(false);
}


void wifi_kb_event_cb(Keyboard::kb_event_t event)
{
    if (event == 0) {
        kb->hidden();
        Serial.println(kb->getText());
        strlcpy(password, kb->getText(), sizeof(password));
        pl->hidden(false);
        WiFi.mode(WIFI_STA);
        WiFi.disconnect();
        WiFi.begin(ssid, password);
        gTicker = new Ticker;
        gTicker->once_ms(5 * 1000, []() {
            wifi_connect_status(false);
        });
    } else if (event == 1) {
        delete kb;
        delete sw;
        delete pl;
        pl = nullptr;
        kb = nullptr;
        sw = nullptr;
        menuBars.hidden(false);
    }
}

static void wifi_sync_mbox_cb(lv_task_t *t)
{
    static  struct tm timeinfo;
    bool ret = false;
    static int retry = 0;
    configTzTime(RTC_TIME_ZONE, "pool.ntp.org");
    while (1) {
        ret = getLocalTime(&timeinfo);
        if (!ret) {
            Serial.printf("get ntp fail,retry : %d \n", retry++);
        } else {
            //! del preload
            delete pl;
            pl = nullptr;

            char format[256];
            snprintf(format, sizeof(format), "Time acquisition is:%d-%d-%d/%d:%d:%d, Whether to synchronize?", timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday, timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
            Serial.println(format);
            delete task;
            task = nullptr;

            //! mbox
            static const char *btns[] = {"Ok", "Cancle", ""};
            mbox = new MBox;
            mbox->create(format, [](lv_obj_t *obj, lv_event_t event) {
                if (event == LV_EVENT_VALUE_CHANGED) {
                    const char *txt =  lv_mbox_get_active_btn_text(obj);
                    if (!strcmp(txt, "Ok")) {

                        //!sync to rtc
                        struct tm *info =  (struct tm *)mbox->getData();
                        Serial.printf("read use data = %d:%d:%d - %d:%d:%d \n", info->tm_year + 1900, info->tm_mon + 1, info->tm_mday, info->tm_hour, info->tm_min, info->tm_sec);

                        TTGOClass *ttgo = TTGOClass::getWatch();
                        ttgo->rtc->setDateTime(info->tm_year + 1900, info->tm_mon + 1, info->tm_mday, info->tm_hour, info->tm_min, info->tm_sec);
                    } else if (!strcmp(txt, "Cancle")) {
                        //!cancle
                        // Serial.println("Cancle press");
                    }
                    delete mbox;
                    mbox = nullptr;
                    sw->hidden(false);
                }
            });
            mbox->setBtn(btns);
            mbox->setData(&timeinfo);
            return;
        }
    }
}




void wifi_sw_event_cb(uint8_t index, bool en)
{
    switch (index) {
    case 0:
        if (en) {
            WiFi.begin();
        } else {
            WiFi.disconnect();
            bar.hidden(LV_STATUS_BAR_WIFI);
        }
        break;
    case 1:
        sw->hidden();
        pl = new Preload;
        pl->create();
        pl->align(bar.self(), LV_ALIGN_OUT_BOTTOM_MID);
        WiFi.disconnect();
        WiFi.scanNetworks(true);
        break;
    case 2:
        if (!WiFi.isConnected()) {
            //TODO pop-up window
            Serial.println("WiFi is no connect");
            return;
        } else {
            if (task != nullptr) {
                Serial.println("task is runing ...");
                return;
            }
            task = new Task;
            task->create(wifi_sync_mbox_cb);
            sw->hidden();
            pl = new Preload;
            pl->create();
            pl->align(bar.self(), LV_ALIGN_OUT_BOTTOM_MID);
        }
        break;
    default:
        break;
    }
}

void wifi_list_cb(const char *txt)
{
    strlcpy(ssid, txt, sizeof(ssid));
    delete list;
    list = nullptr;
    kb = new Keyboard;
    kb->create();
    kb->align(bar.self(), LV_ALIGN_OUT_BOTTOM_MID);
    kb->setKeyboardEvent(wifi_kb_event_cb);
}

void wifi_list_add(const char *ssid)
{
    if (list == nullptr) {
        pl->hidden();
        list = new List;
        list->create();
        list->align(bar.self(), LV_ALIGN_OUT_BOTTOM_MID);
        list->setListCb(wifi_list_cb);
    }
    list->add(ssid);
}


static void wifi_event_cb()
{
    Switch::switch_cfg_t cfg[3] = {{"Switch", wifi_sw_event_cb}, {"Scan", wifi_sw_event_cb}, {"NTP Sync", wifi_sw_event_cb}};
    sw = new Switch;
    sw->create(cfg, 3, []() {
        delete sw;
        sw = nullptr;
        menuBars.hidden(false);
    });
    sw->align(bar.self(), LV_ALIGN_OUT_BOTTOM_MID);
    sw->setStatus(0, WiFi.isConnected());
}


static void wifi_destory()
{
    Serial.printf("globalIndex:%d\n", globalIndex);
    switch (globalIndex) {
    //! wifi management main
    case 0:
        menuBars.hidden(false);
        delete sw;
        sw = nullptr;
        break;
    //! wifi ap list
    case 1:
        if (list != nullptr) {
            delete list;
            list = nullptr;
        }
        if (gTicker != nullptr) {
            delete gTicker;
            gTicker = nullptr;
        }
        if (kb != nullptr) {
            delete kb;
            kb = nullptr;
        }
        if (pl != nullptr) {
            delete pl;
            pl = nullptr;
        }
        sw->hidden(false);
        break;
    //! wifi keyboard
    case 2:
        if (gTicker != nullptr) {
            delete gTicker;
            gTicker = nullptr;
        }
        if (kb != nullptr) {
            delete kb;
            kb = nullptr;
        }
        if (pl != nullptr) {
            delete pl;
            pl = nullptr;
        }
        sw->hidden(false);
        break;
    case 3:
        break;
    default:
        break;
    }
    globalIndex--;
}


/*****************************************************************
 *
 *          !SETTING EVENT
 *
 */
static void setting_event_cb()
{


}


/*****************************************************************
 *
 *          ! LIGHT EVENT
 *
 */
static void light_sw_event_cb(uint8_t index, bool en)
{
    //Add lights that need to be controlled
}

static void light_event_cb()
{
    const uint8_t cfg_count = 4;
    Switch::switch_cfg_t cfg[cfg_count] = {
        {"light1", light_sw_event_cb},
        {"light2", light_sw_event_cb},
        {"light3", light_sw_event_cb},
        {"light4", light_sw_event_cb},
    };
    sw = new Switch;
    sw->create(cfg, cfg_count, []() {
        delete sw;
        sw = nullptr;
        menuBars.hidden(false);
    });

    sw->align(bar.self(), LV_ALIGN_OUT_BOTTOM_MID);

    //Initialize switch status
    for (int i = 0; i < cfg_count; i++) {
        sw->setStatus(i, 0);
    }
}


/*****************************************************************
 *
 *          ! MBOX EVENT
 *
 */
static lv_obj_t *mbox1 = nullptr;

static void create_mbox(const char *txt, lv_event_cb_t event_cb)
{
    if (mbox1 != nullptr)return;
    static const char *btns[] = {"Ok", ""};
    mbox1 = lv_mbox_create(lv_scr_act(), NULL);
    lv_mbox_set_text(mbox1, txt);
    lv_mbox_add_btns(mbox1, btns);
    lv_obj_set_width(mbox1, LV_HOR_RES - 40);
    lv_obj_set_event_cb(mbox1, event_cb);
    lv_obj_align(mbox1, NULL, LV_ALIGN_CENTER, 0, 0);
}

static void destory_mbox()
{
    if (pl != nullptr) {
        delete pl;
        pl = nullptr;
    }
    if (list != nullptr) {
        delete list;
        list = nullptr;
    }
    if (mbox1 != nullptr) {
        lv_obj_del(mbox1);
        mbox1 = nullptr;
    }
}

/*****************************************************************
 *
 *          ! SD CARD EVENT
 *          ! Audio & Image
 */


enum PlayState {
    Play_PLAY,
    Play_PAUSE,
    Play_NEXT,
    Play_PREV,
    Play_STOP,
};

enum PlayFomart {
    Play_MP3,
    Play_WAV,
    Play_NONE,
};

#ifdef  AUDIO_PLAY_ENABLE
#include "AudioFileSourceSD.h"
#include "AudioFileSourceID3.h"
#include "AudioGeneratorMP3.h"
#include "AudioOutputI2SNoDAC.h"
#include "AudioGeneratorWAV.h"
#include "AudioOutputI2S.h"

//! audio object
static AudioGeneratorWAV *audio_wav = nullptr;
static AudioGeneratorMP3 *audio_mp3 = nullptr;
static AudioFileSourceSD *audio_file = nullptr;
static AudioFileSourceID3 *audio_id3 = nullptr;

#ifdef AUDIO_PLAY_OUTPUT_I2S
static AudioOutputI2S *audio_out = nullptr;
#else
static AudioOutput *audio_out = nullptr;
#endif /*AUDIO_PLAY_OUTPUT_I2S*/

static PlayState aduio_state = Play_STOP;
static PlayFomart aduio_fomart = Play_NONE;

static bool audio_play_init()
{
    if (audio_file != nullptr)return true;

    audio_file = new AudioFileSourceSD();
    audio_id3 = new AudioFileSourceID3(audio_file);
    //! External DAC decoding
#ifdef AUDIO_PLAY_OUTPUT_I2S
    audio_out = new AudioOutputI2S();
    audio_out->SetPinout(TWATCH_DAC_IIS_BCK, TWATCH_DAC_IIS_WS, TWATCH_DAC_IIS_DOUT);
#else
    audio_out = new AudioOutputI2S(0, 1);
#endif
    audio_mp3 = new AudioGeneratorMP3();
    audio_wav = new AudioGeneratorWAV();

    //! Turn on the ldo3 power
    TTGOClass *watch = TTGOClass::getWatch();
    watch->enableLDO3();
    return true;
}

bool audio_play_start(String filename)
{
    audio_play_init();
    Serial.print("Play music name:");
    Serial.println(filename);

    if (filename.endsWith(".mp3")) {
        if (!audio_file->open(filename.c_str())) {
            Serial.println("Failed to open file");
            return false;
        }
        if (!audio_mp3->begin(audio_id3, audio_out)) {
            Serial.println("Failed to begin mp3 file");

            return false;
        }
        aduio_state = Play_PLAY;
        aduio_fomart = Play_MP3;
    } else if (filename.endsWith(".wav")) {

        if (!audio_file->open(filename.c_str())) {
            Serial.println("Failed to open file");
            return false;
        }
        if (!audio_wav->begin(audio_file, audio_out)) {
            Serial.println("Failed to begin wav file");
            return false;
        }
        aduio_state = Play_PLAY;
        aduio_fomart = Play_WAV;

    } else {
        aduio_state = Play_STOP;
        aduio_fomart = Play_NONE;
        return false;
    }
    return true;
}

static void aduio_play_handle()
{
    switch (aduio_fomart) {
    case Play_MP3:
        if (audio_mp3->isRunning()) {
            if (!audio_mp3->loop()) {
                audio_mp3->stop();
                aduio_state = Play_STOP;
            }
        }
        break;
    case Play_WAV:
        if (audio_wav->isRunning()) {
            if (!audio_wav->loop()) {
                audio_wav->stop();
                aduio_state = Play_STOP;
            }
        }
        break;
    default:
        break;
    }
}

void audio_play_loop()
{
    switch (aduio_state) {
    case Play_PLAY:
        aduio_play_handle();
        break;
    case Play_PAUSE:
        break;
    case Play_NEXT:
        break;
    case Play_PREV:
        break;
    case Play_STOP:
        break;
    default:
        break;
    }
}






static void audio_play_task(void *args)
{
    Serial.println("audio_play_task start");
    for (;;) {
        audio_play_loop();
    }
}

#endif /*AUDIO_PLAY_ENABLE*/


static const char *btns[] = {"Reload", "Quit", ""};

static bool is_sd_mount()
{
    TTGOClass *watch = TTGOClass::getWatch();
    return watch->sdcard_begin();
}

static void sd_mbox_event_cb(lv_obj_t *obj, lv_event_t event)
{
    if (event == LV_EVENT_VALUE_CHANGED) {
        const char *btn = lv_mbox_get_active_btn_text(obj);
        if (!strcmp(btn, "Quit")) {
            destory_mbox();
            menuBars.hidden(false);
        } else if (!strcmp(btn, "Reload")) {
            destory_mbox();
            if (is_sd_mount()) {
                Serial.println("Reload PASS");
                menuBars.hidden(false);
            } else {
                create_mbox("Reload Failed", sd_mbox_event_cb);
                lv_mbox_add_btns(mbox1, btns);
            }
        }
    }
}


static void sd_list_file(fs::FS &fs, const char *dirname = "/", uint8_t levels = 0);

static void sd_list_file(fs::FS &fs, const char *dirname, uint8_t levels)
{
    Serial.printf("Listing directory: %s\n", dirname);

    File root = fs.open(dirname);
    if (!root) {
        Serial.println("Failed to open directory");
        return;
    }
    if (    !root.isDirectory()) {
        Serial.println("Not a directory");
        return;
    }

    File file = root.openNextFile();
    while (file) {
        if (file.isDirectory()) {
            // Serial.print("  DIR : ");
            // Serial.println(file.name());
            if (levels) {
                sd_list_file(fs, file.name(), levels - 1);
            }
        } else {
            String filename = file.name();
            if (filename.endsWith(".mp3") ||
                    filename.endsWith(".WAV") ||
                    filename.endsWith(".MP3") ||
                    filename.endsWith(".wav")) {
                Serial.println(filename);
                list->add(filename.c_str(), (void *)LV_SYMBOL_FILE);
            }
            // Serial.print("  FILE: ");
            // Serial.print(file.name());
            // Serial.print("  SIZE: ");
            // Serial.println(file.size());
        }
        file = root.openNextFile();
    }
}

static void sd_list_event_cb(const char *text)
{
    Serial.println(text);
    // disableCore0WDT();
    // disableCore1WDT();
    if (audio_play_start(String(text))) {
        // xTaskCreate(audio_play_task, "audio", 8192, nullptr, 1, NULL);
    } else {
        Serial.println("Failed to paly music");
    }
}


static void sd_event_cb()
{
    if (!is_sd_mount()) {
        create_mbox("No SD card detected", sd_mbox_event_cb);
        lv_mbox_add_btns(mbox1, btns);
        return;
    }

    list = new List;
    list->create();
    list->align(bar.self(), LV_ALIGN_OUT_BOTTOM_MID);
    list->setListCb(sd_list_event_cb);

    sd_list_file(SD);

}

/*****************************************************************
*
*          ! Modules EVENT
*
*/
static void modules_event_cb()
{
}


/*****************************************************************
*
*          ! Camera EVENT
*
*/

static void camera_event_cb()
{

}

/*****************************************************************
*
*          ! nCov EVENT
*
*/
static const char *province[] = {
    "北京市",
    "上海市",
    "天津市",
    "重庆市",
    "湖北省",
    "湖南省",
    "广东省",
    "广西省",
    "河北省",
    "山西省",
    "辽宁省",
    "吉林省",
    "江苏省",
    "浙江省",
    "安徽省",
    "福建省",
    "江西省",
    "山东省",
    "河南省",
    "海南省",
    "四川省",
    "贵州省",
    "云南省",
    "陕西省",
    "甘肃省",
    "青海省",
    "黑龙江省",
    "台湾",
    "香港",
    "澳门",
    "西藏自治区",
    "内蒙古自治区",
    "宁夏回族自治区",
    "新疆维吾尔自治区",
};

typedef struct  {
    String city;
    int confirmedCount;
    int suspectedCount;
    int curedCount;
    int deadCount;
    int locationId;
} nCovArray_t;

typedef struct {
    nCovArray_t province;
    nCovArray_t *cities;
    int cities_size;
} nCovData_t;

lv_obj_t *nCov_Page = nullptr;
nCovData_t nCov_data;
bool capture_status = false;
bool capture_done = false;
lv_task_t *nCov_task_handle ;
StaticJsonDocument<2048 * 10> doc;

int url_encode(const char *src, const int srcLen, char *result, const int resultLen)
{
    int index = 0;
    char ch;

    if ((src == NULL) || (result == NULL) || (srcLen <= 0) || (resultLen <= 0)) {
        return 0;
    }
    for (int i = 0; (i < srcLen) && (index < resultLen); ++i) {
        ch = src[i];
        if (((ch >= 'A') && (ch < 'Z')) ||
                ((ch >= 'a') && (ch < 'z')) ||
                ((ch >= '0') && (ch < '9'))) {
            result[index++] = ch;
        } else if (ch == ' ') {
            result[index++] = '+';
        } else if (ch == '.' || ch == '-' || ch == '_' || ch == '*') {
            result[index++] = ch;
        } else {
            if (index + 3 < resultLen) {
                sprintf(result + index, "%%%02X", (unsigned char)ch);
                index += 3;
            } else {
                return 0;
            }
        }
    }
    result[index] = '\0';
    return index;
}



static bool nCov_capture_data(const char *prov)
{
    char buf[128] = {0};
    String url = "https://lab.isaaclin.cn/nCoV/api/area?latest=1&province=";
    if (url_encode(prov, strlen(prov), buf, 128) <= 0) {
        return false;
    }
    url += buf;
    HTTPClient http;

    http.begin( url);
    Serial.println(url);
    http.setTimeout(10000);
    int httpCode = http.GET();
    if (httpCode != HTTP_CODE_OK) {
        Serial.printf("HTTP_CODE_ERROR %d\n", httpCode);
        http.end();
        return false;
    }

    String payload = http.getString();
    http.end();
    Serial.println(payload);

    DeserializationError error = deserializeJson(doc, payload);
    if (error) {
        Serial.print(F("deserializeJson() failed: "));
        Serial.println(error.c_str());
        return false;
    }

    JsonObject results = doc["results"][0];
    if (results["country"] == NULL) {
        Serial.println("Search failed\n");
        return false;
    }

    nCov_data.province.city = results["provinceShortName"].as<String>();
    nCov_data.province.confirmedCount = results["confirmedCount"];;
    nCov_data.province.suspectedCount = results["suspectedCount"];
    nCov_data.province.curedCount = results["curedCount"];
    nCov_data.province.deadCount = results["deadCount"];


    nCov_data.cities_size =  results["cities"].size();
    Serial.printf("menory [%u] \n", nCov_data.cities_size);
    nCov_data.cities = new nCovArray_t [nCov_data.cities_size];
    if (nCov_data.cities == nullptr) {
        Serial.printf("menory [%u] failed\n", nCov_data.cities_size);
        return false;
    }

    JsonArray clities = results["cities"];
    for (int i = 0; i < nCov_data.cities_size; ++i) {
        nCov_data.cities[i].city = clities[i]["cityName"].as<String>();
        nCov_data.cities[i].confirmedCount = clities[i]["confirmedCount"].as<int>();
        nCov_data.cities[i].suspectedCount = clities[i]["suspectedCount"].as<int>();
        nCov_data.cities[i].curedCount = clities[i]["curedCount"].as<int>();
        nCov_data.cities[i].deadCount = clities[i]["deadCount"].as<int>();
        nCov_data.cities[i].locationId = clities[i]["locationId"].as<int>();
    }

    // for (int i = 0; i < nCov_data.cities_size; ++i) {
    //     Serial.print("city:");
    //     Serial.print(nCov_data.cities[i].city );
    //     Serial.println();
    //     Serial.print("confirmedCount:");
    //     Serial.print(nCov_data.cities[i].confirmedCount );
    //     Serial.println();
    //     Serial.print("suspectedCount:");
    //     Serial.print(nCov_data.cities[i].suspectedCount );
    //     Serial.println();
    //     Serial.print("curedCount:");
    //     Serial.print(nCov_data.cities[i].curedCount );
    //     Serial.println();
    //     Serial.print("deadCount:");
    //     Serial.print(nCov_data.cities[i].deadCount );
    //     Serial.println();
    //     Serial.print("locationId:");
    //     Serial.print(nCov_data.cities[i].locationId );
    //     Serial.println();
    // }
    return true;
}

static void nCov_data_event_cb(lv_obj_t *obj, lv_event_t event)
{
    if (event == LV_EVENT_SHORT_CLICKED) {
        if (nCov_Page != nullptr ) {
            lv_obj_del(nCov_Page);
            delete [] nCov_data.cities;
            nCov_data.cities = nullptr;

            delete list;
            list = nullptr;
            menuBars.hidden(false);
        }
    }
}


static void ex_table_create(nCovData_t *data)
{
    if (data == nullptr )return;
    /*Create a normal cell style*/
    static lv_style_t style_cell1;
    lv_style_copy(&style_cell1, &lv_style_plain);
    style_cell1.body.border.width = 1;
    style_cell1.body.border.color = LV_COLOR_BLACK;
    style_cell1.text.font = &chinese_font;

    /*Crealte a header cell style*/
    static lv_style_t style_cell2;
    lv_style_copy(&style_cell2, &lv_style_plain);
    style_cell2.body.border.width = 1;
    style_cell2.body.border.color = LV_COLOR_BLACK;
    style_cell2.body.main_color = LV_COLOR_SILVER;
    style_cell2.body.grad_color = LV_COLOR_SILVER;
    style_cell2.text.font = &chinese_font;


    static lv_style_t page_style;
    lv_style_copy(&page_style, &lv_style_plain);    /*Copy a built-in style to initialize the new style*/
    page_style.body.main_color = LV_COLOR_GRAY;
    page_style.body.grad_color = LV_COLOR_GRAY;
    page_style.body.opa = LV_OPA_0;
    page_style.body.radius = 0;
    page_style.body.border.color = LV_COLOR_GRAY;
    page_style.body.border.width = 0;
    page_style.body.border.opa = LV_OPA_50;
    page_style.text.color = LV_COLOR_WHITE;
    page_style.image.color = LV_COLOR_WHITE;

    nCov_Page = lv_page_create(lv_scr_act(), NULL);
    lv_page_set_edge_flash(nCov_Page, false);
    lv_obj_set_size(nCov_Page, 240, 210);
    lv_page_set_sb_mode(nCov_Page, LV_SB_MODE_OFF);
    lv_obj_set_event_cb(nCov_Page, nCov_data_event_cb);
    lv_page_set_style(nCov_Page, LV_PAGE_STYLE_BG, &page_style);
    lv_obj_align(nCov_Page, bar.self(), LV_ALIGN_OUT_BOTTOM_MID, 0, 0);

    lv_obj_t *table = lv_table_create(nCov_Page, NULL);

    lv_table_set_style(table, LV_TABLE_STYLE_BG, &page_style);

    lv_table_set_style(table, LV_TABLE_STYLE_CELL1, &style_cell1);
    lv_table_set_style(table, LV_TABLE_STYLE_CELL2, &style_cell2);
    lv_table_set_style(table, LV_TABLE_STYLE_BG, &lv_style_transp_tight);
    lv_table_set_col_cnt(table, 4);
    lv_table_set_row_cnt(table, data->cities_size + 1);
    lv_obj_align(table, NULL, LV_ALIGN_CENTER, 0, 0);
    lv_page_glue_obj(table, true);

    lv_table_set_col_width(table, 0, 240 / 4);
    lv_table_set_col_width(table, 1, 240 / 4);
    lv_table_set_col_width(table, 2, 240 / 4);
    lv_table_set_col_width(table, 3, 240 / 4);

    /*Make the cells of the first row center aligned */
    lv_table_set_cell_align(table, 0, 0, LV_LABEL_ALIGN_CENTER);
    lv_table_set_cell_align(table, 0, 1, LV_LABEL_ALIGN_CENTER);
    lv_table_set_cell_align(table, 0, 2, LV_LABEL_ALIGN_CENTER);
    lv_table_set_cell_align(table, 0, 3, LV_LABEL_ALIGN_CENTER);

    /*Make the cells of the first row TYPE = 2 (use `style_cell2`) */
    lv_table_set_cell_type(table, 0, 0, 2);
    lv_table_set_cell_type(table, 0, 1, 2);
    lv_table_set_cell_type(table, 0, 2, 2);
    lv_table_set_cell_type(table, 0, 3, 2);

    lv_table_set_cell_value(table, 0, 0, "城市");
    lv_table_set_cell_value(table, 0, 1, "疑似");
    lv_table_set_cell_value(table, 0, 2, "治愈");
    lv_table_set_cell_value(table, 0, 3, "死亡");

    lv_table_set_cell_value(table, 1, 0, String(data->province.city).c_str());
    lv_table_set_cell_value(table, 1, 1, String(data->province.confirmedCount).c_str());
    lv_table_set_cell_value(table, 1, 2, String(data->province.curedCount).c_str());
    lv_table_set_cell_value(table, 1, 3, String(data->province.deadCount).c_str());

    for (int i = 2; i < data->cities_size + 2; ++i) {
        lv_table_set_cell_value(table, i, 0, data->cities[i - 2].city.c_str());
        lv_table_set_cell_value(table, i, 1, String(data->cities[i - 2].confirmedCount).c_str());
        lv_table_set_cell_value(table, i, 2, String(data->cities[i - 2].curedCount).c_str());
        lv_table_set_cell_value(table, i, 3, String(data->cities[i - 2].deadCount).c_str());
    }
}



static void capture_task(void *arg)
{
    for (;;) {
        if (arg) {
            capture_status = nCov_capture_data((const char *)arg);
            capture_done = true;
            Serial.println("nCov_capture_data done");
        } else {
            capture_done = true;
            capture_status = false;
        }
        vTaskDelete(NULL);
    }
}

static void nCov_list_cb(const char *txt)
{
    Serial.println(txt);
    list->hidden();
    pl = new Preload;
    pl->create();
    pl->align(bar.self(), LV_ALIGN_OUT_BOTTOM_MID);
    xTaskCreate(capture_task, "capture", 2048 * 4, (void *)txt, 10, NULL);
    nCov_task_handle = lv_task_create([](lv_task_t *t) {
        if (capture_done) {
            if (pl != nullptr) {
                delete pl;
                pl = nullptr;
            }
            if (capture_status) {
                ex_table_create(&nCov_data);
            } else {
                create_mbox("Server no response", [](lv_obj_t *obj, lv_event_t event) {
                    if (event == LV_EVENT_SHORT_CLICKED) {
                        destory_mbox();
                        menuBars.hidden(false);
                    }
                });
            }
            capture_status = false;
            capture_done =  false;
            lv_task_del(nCov_task_handle);
        }
    }, 1000, LV_TASK_PRIO_LOW, NULL);
}

static void nCov_event_cb()
{
    if (!WiFi.isConnected()) {
        create_mbox("WiFi is not connect", [](lv_obj_t *obj, lv_event_t event) {
            if (event == LV_EVENT_SHORT_CLICKED) {
                destory_mbox();
                menuBars.hidden(false);
            }
        });
        return;
    } else {
        static lv_style_t listStyle;
        lv_style_copy(&listStyle, &lv_style_plain_color);    /*Copy a built-in style to initialize the new style*/
        listStyle.text.font = &chinese_font;
        listStyle.body.padding.top = 30;
        listStyle.body.padding.bottom = 30;

        if (list == nullptr) {
            list = new List;
            list->create();
            list->align(bar.self(), LV_ALIGN_OUT_BOTTOM_MID);
            list->setListCb(nCov_list_cb);
        }
        for (int i = 0; i < sizeof(province) / sizeof(province[0]); ++i) {
            lv_obj_t *btn = list->add(province[i], nullptr);
            lv_obj_t *label  = lv_list_get_btn_label(btn);
            if (label != NULL) {
                lv_label_set_long_mode(label, LV_LABEL_LONG_DOT);
            }
        }

        list->setStyle(LV_LIST_STYLE_BTN_REL, &listStyle);
        list->setStyle(LV_LIST_STYLE_BTN_PR, &listStyle);
        list->setStyle(LV_LIST_STYLE_BTN_TGL_REL, &listStyle);
        list->setStyle(LV_LIST_STYLE_BTN_TGL_PR, &listStyle);
    }
}


/*****************************************************************
*
*          ! Thermometer EVENT
*
*/
static lv_task_t *temperature_handle = nullptr;
static lv_obj_t *temp_cont = nullptr;
static lv_obj_t *temp_label = nullptr;
static lv_obj_t *milieu_label = nullptr;

static void temperature_task(lv_task_t *t)
{
    char buf[128] = {0};
    // float val = rand() % 20;
    // snprintf(buf, sizeof(buf), "%.2f", val);

    snprintf(buf, sizeof(buf), "%.2f", mlx90614_read_object());
    lv_label_set_text(temp_label, buf);
    lv_obj_align(temp_label, temp_cont, LV_ALIGN_CENTER, 0, -30);

    snprintf(buf, sizeof(buf), "%.2f", mlx90614_read_ambient());
    lv_label_set_text(milieu_label, buf);
    lv_obj_align(milieu_label, temp_cont, LV_ALIGN_CENTER, 0, 45);

    lv_disp_trig_activity(NULL);
}

static void thermometer_event_cb()
{
    TTGOClass *watch = TTGOClass::getWatch();
    bool r = watch->deviceProbe(0x51);
    Serial.printf(" probe : %d\n", r);

    r = watch->deviceProbe(0x5A);
    Serial.printf(" probe : %d\n", r);

    if (!mlx90614_probe()) {
        create_mbox("No detected Sensor", [](lv_obj_t *obj, lv_event_t event) {
            if (event == LV_EVENT_SHORT_CLICKED) {
                destory_mbox();
                menuBars.hidden(false);
            }
        });
        return ;
    }

    if (temperature_handle) {
        return;
    }

    static lv_style_t cont_style;
    lv_style_copy(&cont_style, &lv_style_plain);
    cont_style.body.main_color = LV_COLOR_GRAY;
    cont_style.body.grad_color = LV_COLOR_GRAY;
    cont_style.body.opa = LV_OPA_0;
    cont_style.body.radius = 0;
    cont_style.body.border.color = LV_COLOR_GRAY;
    cont_style.body.border.width = 0;
    cont_style.body.border.opa = LV_OPA_50;
    cont_style.text.color = LV_COLOR_WHITE;
    cont_style.image.color = LV_COLOR_WHITE;

    temp_cont = lv_cont_create(lv_scr_act(), NULL);
    lv_obj_set_style(temp_cont, &cont_style);
    lv_obj_set_size(temp_cont, LV_HOR_RES, LV_VER_RES - 30);
    lv_obj_align(temp_cont, bar.self(), LV_ALIGN_OUT_BOTTOM_MID, 0, 0);

    static lv_style_t temp_style;
    lv_style_copy(&temp_style, &lv_style_plain);
    temp_style.text.font = &digital_font;
    temp_style.text.color = LV_COLOR_WHITE;

    temp_label = lv_label_create(temp_cont, NULL);
    lv_label_set_text(temp_label, "00.00");
    lv_obj_set_style(temp_label, &temp_style);
    lv_obj_align(temp_label, temp_cont, LV_ALIGN_CENTER, 0, -30);

    milieu_label = lv_label_create(temp_cont, NULL);
    lv_label_set_text(milieu_label, "00.00");
    lv_obj_set_style(milieu_label, &temp_style);
    lv_obj_align(milieu_label, temp_cont, LV_ALIGN_CENTER, 0, 45);

    static lv_style_t obj_style;
    lv_style_copy(&obj_style, &lv_style_plain);
    obj_style.text.font = &chinese_font;
    obj_style.text.color = LV_COLOR_WHITE;

    lv_obj_t *obj_label = lv_label_create(temp_cont, NULL);
    lv_label_set_text(obj_label, "对象温度:");
    lv_obj_set_style(obj_label, &obj_style);
    lv_obj_align(obj_label, temp_cont, LV_ALIGN_IN_TOP_LEFT, 15, 30);


    lv_obj_t *ambient_label = lv_label_create(temp_cont, NULL);
    lv_label_set_text(ambient_label, "环境温度:");
    lv_obj_set_style(ambient_label, &obj_style);
    lv_obj_align(ambient_label, temp_cont, LV_ALIGN_IN_LEFT_MID, 15, 10);


    lv_obj_t *img_btn = lv_img_create(temp_cont, NULL);
    lv_img_set_src(img_btn, &qiut);
    lv_obj_align(img_btn, temp_cont, LV_ALIGN_IN_TOP_RIGHT, -15, 10);
    lv_obj_set_click(img_btn, true);
    lv_obj_set_event_cb(img_btn, [](lv_obj_t *obj, lv_event_t event) {
        if (event == LV_EVENT_SHORT_CLICKED) {
            if (temperature_handle) {
                lv_task_del(temperature_handle);
                temperature_handle = nullptr;
                lv_obj_del(temp_cont);
                temp_cont = nullptr;
            }
            menuBars.hidden(false);
        }
    });
    temperature_handle = lv_task_create(temperature_task, 1000, LV_TASK_PRIO_LOW, nullptr);
}


//! 2020/2/17 lewis add
#include "bluetooth.h"

/*****************************************************************
 *
 *          ! SOIL INTERFACE
 *
 */
static lv_obj_t *_soilCont = nullptr;
static lv_obj_t *_soilText = nullptr;
static lv_obj_t *_exitBtn = nullptr;

static void destory_soil()
{
    if (_soilCont != nullptr) {
        lv_obj_del(_soilCont);
        _soilCont = nullptr;
    }
}

static void create_soil()
{
    if (_soilCont != nullptr) {
        return;
    }
    Serial.println("Create SOIL interface");
    static lv_style_t soilStyle;
    lv_style_copy(&soilStyle, &lv_style_plain);    /*Copy a built-in style to initialize the new style*/
    soilStyle.body.main_color = LV_COLOR_GRAY;
    soilStyle.body.grad_color = LV_COLOR_GRAY;
    soilStyle.body.opa = LV_OPA_0;
    soilStyle.body.radius = 0;
    soilStyle.body.border.color = LV_COLOR_GRAY;
    soilStyle.body.border.width = 0;
    soilStyle.body.border.opa = LV_OPA_50;
    soilStyle.text.color = LV_COLOR_WHITE;
    soilStyle.image.color = LV_COLOR_WHITE;

    _soilCont = lv_cont_create(lv_scr_act(), NULL);
    lv_obj_set_size(_soilCont, LV_HOR_RES, LV_VER_RES - 30);
    lv_obj_align(_soilCont, bar.self(), LV_ALIGN_OUT_BOTTOM_MID, 0, 0);
    lv_obj_set_style(_soilCont, &soilStyle);

    _soilText = lv_label_create(_soilCont, NULL);
    lv_label_set_long_mode(_soilText, LV_LABEL_LONG_BREAK);     /*Break the long lines*/
    lv_label_set_recolor(_soilText, true);                      /*Enable re-coloring by commands in the text*/
    lv_label_set_align(_soilText, LV_LABEL_ALIGN_CENTER);       /*Center aligned lines*/
    lv_label_set_text(_soilText, "\n\nHumidity:0.00\n Temperature:0.00\n soil:0%");
    lv_obj_set_width(_soilText, LV_HOR_RES - 20);
    lv_obj_align(_soilText, _soilCont, LV_ALIGN_CENTER, 0, -50);

    _exitBtn = lv_imgbtn_create(_soilCont, NULL);
    lv_imgbtn_set_src(_exitBtn, LV_BTN_STATE_REL, &iexit);
    lv_imgbtn_set_src(_exitBtn, LV_BTN_STATE_PR, &iexit);
    lv_imgbtn_set_src(_exitBtn, LV_BTN_STATE_TGL_REL, &iexit);
    lv_imgbtn_set_src(_exitBtn, LV_BTN_STATE_TGL_PR, &iexit);
    lv_obj_align(_exitBtn, NULL, LV_ALIGN_IN_BOTTOM_MID, 0, -10);
    lv_obj_set_event_cb(_exitBtn, [](lv_obj_t *obj, lv_event_t event) {
        if (event == LV_EVENT_SHORT_CLICKED) {
            buletooth_notify_en(false);
            destory_soil();
            menuBars.hidden(false);
        }
    });
    buletooth_notify_en(true);
}

bool update_soil_data(const char *text)
{
    if (_soilCont != nullptr) {
        lv_label_set_text(_soilText, text);
        lv_obj_align(_soilText, _soilCont, LV_ALIGN_CENTER, 0, -50);
        return true;
    }
    return false;
}

/*****************************************************************
 *
 *          ! BLUETOOTH EVENT
 *
 */
extern SemaphoreHandle_t xSysSemaphore ;

void bluetooth_diconnect_cb()
{
    bar.hidden(LV_STATUS_BAR_BLUETOOTH);
    if (_soilCont != nullptr) {
        destory_soil();
        create_mbox("Device disconnect", [](lv_obj_t *obj, lv_event_t event) {
            if (event == LV_EVENT_VALUE_CHANGED) {
                if (mbox1 != nullptr) {
                    lv_obj_del(mbox1);
                    mbox1 = nullptr;
                }
                menuBars.hidden(false);
            }
        });
    }
}

void bluetooth_mbox_event_cb(lv_obj_t *obj, lv_event_t event)
{
    if (event == LV_EVENT_VALUE_CHANGED) {
        destory_mbox();
        int type = bluetooth_get_connect_type();
        Serial.printf("[bluetooth]type:%d\n", type);
        if (type < 0) {
            if (sw != nullptr) {
                sw->hidden(false);
            } else {
                menuBars.hidden(false);
            }
            return;
        }
        switch (type) {
        case 0:
            delete sw;
            sw = nullptr;
            create_soil();
            break;
        case 1:
            break;
        default:
            break;
        }
    }
}

void bluetooth_icon_hidden()
{
    bar.hidden(LV_STATUS_BAR_BLUETOOTH);
}

void bluetooth_list_cb(const char *txt)
{

    list->hidden();
    pl->hidden(false);

    Ble_Connect_t ret = bluetooth_connect(txt);

    delete list;
    list = nullptr;
    if (pl != nullptr) {
        delete pl;
        pl = nullptr;
    }

    if (ret == BLE_DEVICE_NO_SUPPORT) {
        create_mbox("Connect device no support", bluetooth_mbox_event_cb);
    } else if (ret == BLE_DEVICE_DISCONNECT) {
        create_mbox("Connect device fail", bluetooth_mbox_event_cb);
    } else {
        create_mbox("Connect device success", bluetooth_mbox_event_cb);
        bar.show(LV_STATUS_BAR_BLUETOOTH);
    }
}


void bluetooth_list_add(const char *dev)
{
    if (dev == NULL) {
        xSemaphoreTake(xSysSemaphore, portMAX_DELAY);
        delete pl;
        pl = nullptr;
        Serial.println("[E] Device is null");
        create_mbox("No valid Bluetooth devices found", bluetooth_mbox_event_cb);
        xSemaphoreGive(xSysSemaphore);
        return;
    }

    if (list == nullptr) {
        pl->hidden();
        list = new List;
        list->create();
        list->align(bar.self(), LV_ALIGN_OUT_BOTTOM_MID);
        list->setListCb(bluetooth_list_cb);
    }
    list->add(dev, (void *)LV_SYMBOL_BLUETOOTH);
}

static void bluetooth_event_cb()
{
    int type = bluetooth_get_connect_type();

    if (type == 0) {
        create_soil();
        return;
    } else if (type == 1) {

        return;
    }
    pl = new Preload;
    pl->create();
    pl->align(bar.self(), LV_ALIGN_OUT_BOTTOM_MID);
    bluetooth_start();
}
