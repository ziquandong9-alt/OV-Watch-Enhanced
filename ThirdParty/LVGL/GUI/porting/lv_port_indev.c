/**
 * @file lv_port_indev_templ.c
 *
 */

 /*Copy this file as "lv_port_indev.c" and set this value to "1" to enable content*/
#if 1

/*********************
 *      INCLUDES
 *********************/
#include "lv_port_indev.h"
#include "lvgl.h"
#include "CST816.h"
#include "lcd_init.h"

/* CST816 到 LVGL 指针设备的适配层；当前工程只注册触摸屏一种输入设备。 */

/*********************
 *      DEFINES
 *********************/

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *  STATIC PROTOTYPES
 **********************/

static void touchpad_init(void);
static void touchpad_read(lv_indev_drv_t * indev_drv, lv_indev_data_t * data);
static bool touchpad_is_pressed(void);
static void touchpad_get_xy(lv_coord_t * x, lv_coord_t * y);

/**********************
 *  STATIC VARIABLES
 **********************/
lv_indev_t * indev_touchpad;
extern CST816_Info	CST816_Instance;

/**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

void lv_port_indev_init(void)
{
    /* indev_drv 为 static，注册后 LVGL 会在整个运行期继续引用。 */
    /**
     * Here you will find example implementation of input devices supported by LittelvGL:
     *  - Touchpad
     *  - Mouse (with cursor support)
     *  - Keypad (supports GUI usage only with key)
     *  - Encoder (supports GUI usage only with: left, right, push)
     *  - Button (external buttons to press points on the screen)
     *
     *  The `..._read()` function are only examples.
     *  You should shape them according to your hardware
     */
	 
    static lv_indev_drv_t indev_drv;

    /*------------------
     * Touchpad
     * -----------------*/

    /*Initialize your touchpad if you have*/
    touchpad_init();

    /*Register a touchpad input device*/
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = touchpad_read;
    indev_touchpad = lv_indev_drv_register(&indev_drv);

}

/**********************
 *   STATIC FUNCTIONS
 **********************/

/*------------------
 * Touchpad
 * -----------------*/

/*Initialize your touchpad*/
static void touchpad_init(void)
{
    /* 触摸芯片已在 main 的 USER CODE 中初始化，此处无需重复复位。 */
}

/*Will be called by the library to read the touchpad*/
static void touchpad_read(lv_indev_drv_t *indev_drv,
                          lv_indev_data_t *data)
{
    /* 保留最后坐标：释放状态仍需提供稳定位置，符合 LVGL 输入约定。 */
    static lv_coord_t last_x = 0;
    static lv_coord_t last_y = 0;

    uint8_t finger_num;

    (void)indev_drv;

    /* 一轮只读一次手指数，避免同一判断内重复 I2C 访问。 */
    finger_num = CST816_Get_FingerNum();

    if ((finger_num != 0x00U) && (finger_num != 0xFFU))
    {
        CST816_Get_XY_AXIS();

        last_x = (lv_coord_t)CST816_Instance.X_Pos;
        last_y = (lv_coord_t)CST816_Instance.Y_Pos;

        /* 坐标夹紧到逻辑分辨率，防止通讯毛刺导致 LVGL 命中越界区域。 */
        if (last_x < 0)
            last_x = 0;
        else if (last_x >= LCD_W)
            last_x = LCD_W - 1;

        if (last_y < 0)
            last_y = 0;
        else if (last_y >= LCD_H)
            last_y = LCD_H - 1;

        data->state = LV_INDEV_STATE_PR;
    }
    else
    {
        data->state = LV_INDEV_STATE_REL;
    }

    data->point.x = last_x;
    data->point.y = last_y;
}
/*Return true is the touchpad is pressed*/
static bool touchpad_is_pressed(void)
{
    /* 兼容保留接口；0xFF 被视为 I2C 错误而不是按下。 */
	if(CST816_Get_FingerNum()!=0x00 && CST816_Get_FingerNum()!=0xFF)
	{return true;}
	else
  {return false;}
}

/*Get the x and y coordinates if the touchpad is pressed*/
static void touchpad_get_xy(lv_coord_t * x, lv_coord_t * y)
{
    /* 兼容保留接口；当前实际 read_cb 已直接完成坐标读取。 */
		CST816_Get_XY_AXIS();
    (*x) = CST816_Instance.X_Pos;
    (*y) = CST816_Instance.Y_Pos;
}

#else /*Enable this file at the top*/

/*This dummy typedef exists purely to silence -Wpedantic.*/
typedef int keep_pedantic_happy;
#endif
