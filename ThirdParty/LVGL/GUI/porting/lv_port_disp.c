#include "lv_port_disp.h"
#include "soft_timer.h"
#include "lcd.h"
#include "lcd_init.h"

#define LV_PORT_DISP_BUFFER_LINES 40U

/* 双缓冲各容纳 40 行：一个由 LVGL 绘制时，另一个可由 DMA 发送。 */
static lv_disp_draw_buf_t s_draw_buffer;
static lv_color_t s_buffer_1[LCD_W * LV_PORT_DISP_BUFFER_LINES];
static lv_color_t s_buffer_2[LCD_W * LV_PORT_DISP_BUFFER_LINES];
static lv_disp_drv_t * volatile s_flushing_driver = NULL;
static __IO uint8_t permit_lv_flush = 0;


/* LVGL 等待上一块刷新结束时，主动推进 LCD 的非中断状态机。 */
static void LCD_LVGL_Wait(lv_disp_drv_t *driver)
{
    /* LVGL 等待缓冲区时主动推进 LCD 完成状态，避免空转死等。 */
    (void)driver;
    LCD_Service();
}

/* 该回调由 LCD_Service() 在非中断上下文中调用。 */
static void LCD_LVGL_FlushComplete(void)
{
    /* 该函数在主循环上下文执行，因此可以安全调用 LVGL API。 */
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
    /* flush_cb 接收的是已经裁剪好的脏矩形及其连续 RGB565 像素。 */
    HAL_StatusTypeDef status;

    /* 先保存 driver 再启动 DMA，避免极小区域过快完成导致丢失通知目标。 */
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
        /* DMA 异步搬运；最终由 LCD_Service 调用完成回调。 */
    }
}

void lv_port_disp_init(void)
{
    /* 驱动结构必须 static，因为注册后 LVGL 会长期保存其地址。 */
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
    /* 关闭整屏刷新，让 LVGL 只提交失效区域，这是本项目流畅度的基础。 */
    display_driver.full_refresh = 0U;

    (void)lv_disp_drv_register(&display_driver);
}

void softTimer_0_Callback(void) {
	/* timer 只置许可标志，真正 lv_timer_handler 在主循环执行。 */
	permit_lv_flush = 1;
	softTimer_Stop(0);
}

void lv_flush_proc(void){
	/* 按 LVGL 建议的下次运行时间重新注册一次性软件定时器。 */
	static uint32_t time = 0;
	if(permit_lv_flush == 1) {
		permit_lv_flush = 0;
		time = lv_timer_handler();
		if(time < 1U) time = 1U;
		/* 限制 1~30 ms：既避免忙循环，也保证动画/输入不会长期饿死。 */
		if(time > 30U) time = 30U;
		softTimer_Register(0, time, softTimer_0_Callback);
	}
}

void lv_flush_start(void) {
    /* 上电后主动运行一次 handler，从其返回值建立第一个调度周期。 */
    static uint32_t time = 0;
    time = lv_timer_handler();
	if(time < 1U) time = 1U;
	if(time > 30U) time = 30U;
	softTimer_Register(0, time, softTimer_0_Callback);
}
