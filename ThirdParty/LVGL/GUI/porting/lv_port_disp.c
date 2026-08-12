#include "lv_port_disp.h"
#include "soft_timer.h"
#include "lcd.h"
#include "lcd_init.h"

#define LV_PORT_DISP_BUFFER_LINES 20U

static lv_disp_draw_buf_t s_draw_buffer;
static lv_color_t s_buffer_1[LCD_W * LV_PORT_DISP_BUFFER_LINES];
static lv_color_t s_buffer_2[LCD_W * LV_PORT_DISP_BUFFER_LINES];
static lv_disp_drv_t * volatile s_flushing_driver = NULL;
static __IO uint8_t permit_lv_flush = 0;


/* LVGL 等待上一块刷新结束时，主动推进 LCD 的非中断状态机。 */
static void LCD_LVGL_Wait(lv_disp_drv_t *driver)
{
    (void)driver;
    LCD_Service();
}

/* 该回调由 LCD_Service() 在非中断上下文中调用。 */
static void LCD_LVGL_FlushComplete(void)
{
    lv_disp_drv_t *driver = s_flushing_driver;

    s_flushing_driver = NULL;

    if (driver != NULL)
    {
        lv_disp_flush_ready(driver);
    }
}

static void LCD_LVGL_Flush(lv_disp_drv_t *driver,
                           const lv_area_t *area,
                           lv_color_t *color_buffer)
{
    HAL_StatusTypeDef status;

    /* 必须先保存 driver，避免极小区域 DMA 很快完成。 */
    s_flushing_driver = driver;

    status = LCD_Color_Fill_DMA((uint16_t)area->x1,
                                (uint16_t)area->y1,
                                (uint16_t)area->x2,
                                (uint16_t)area->y2,
                                (const uint16_t *)color_buffer);

    if (status != HAL_OK)
    {
        s_flushing_driver = NULL;
        lv_disp_flush_ready(driver);
    }
    else
    {
        /* DMA 搬运像素；等待期间由 LCD_Service() 处理完成通知。 */
        /* Completion is reported asynchronously by LCD_LVGL_FlushComplete. */
    }
}

void lv_port_disp_init(void)
{
    static lv_disp_drv_t display_driver;

    LCD_Set_Flush_Complete_Callback(LCD_LVGL_FlushComplete);

    lv_disp_draw_buf_init(&s_draw_buffer,
                          s_buffer_1,
                          s_buffer_2,
                          LCD_W * LV_PORT_DISP_BUFFER_LINES);

    lv_disp_drv_init(&display_driver);
    display_driver.hor_res = LCD_W;
    display_driver.ver_res = LCD_H;
    display_driver.flush_cb = LCD_LVGL_Flush;
    display_driver.wait_cb = LCD_LVGL_Wait;
    display_driver.draw_buf = &s_draw_buffer;
    display_driver.full_refresh = 0U;

    (void)lv_disp_drv_register(&display_driver);
}

void softTimer_0_Callback(void) {
	permit_lv_flush = 1;
	softTimer_Stop(0);
}

void lv_flush_proc(void){
	static uint32_t time = 0;
	if(permit_lv_flush == 1) {
		permit_lv_flush = 0;
		time = lv_timer_handler();
		if(time < 1U) time = 1U;
		if(time > 30U) time = 30U;
		softTimer_Register(0, time, softTimer_0_Callback);
	}
}

void lv_flush_start(void) {
    static uint32_t time = 0;
    time = lv_timer_handler();
	if(time < 1U) time = 1U;
	if(time > 30U) time = 30U;
	softTimer_Register(0, time, softTimer_0_Callback);
}
