#include <stdio.h>
#include "rmt.h"
#include "neopixel.h"
#include <esp_log.h>

#include "iot_button.h"

#define NEOPIXEL_PIN 27
#define BUTTON_PIN 39
#define BUTTON_ACTIVE_LEVEL     0
static const char * TAG = "main";

volatile int myhealth = 5;
volatile int myteam = 1;
tNeopixelContext neopixel;

void update() {
    tNeopixel pixel[] =
    {
        { 0, NP_RGB(0, 0,  0) }, /* off */
        { 1, NP_RGB(0,  0, 0) }, /* off */
        { 2, NP_RGB(0,  0, 0) }, /* off */
        { 3, NP_RGB(0,  0,  0) }, /* off */
        { 4, NP_RGB(0,  0,  0) }, /* off */
    };
    neopixel_SetPixel(neopixel, &pixel[0], 5);
    uint32_t color;
    if (myteam == 1) {
        color = NP_RGB(0,  0, 50); //blue
    } else if (myteam == 2) {
        color = NP_RGB(50, 0,  0); //red
    } else {
        color = NP_RGB(0,50,0); //green
    }
    for (int i =0; i < myhealth; i++) {
        pixel[i].rgb = color;
    }
    neopixel_SetPixel(neopixel, &pixel[0], 5);
}

void irReceived(irproto brand, uint32_t code, size_t len, rmt_symbol_word_t *item){
    printf("code=%lx\n", code);
	if(( code & 0xffffff00 )== 0xBA00BA00){
        uint8_t rhealth = code & 0x7;
        uint8_t rteam = (code >> 4) & 0x3;
        printf("rteam=%d rhealth=%d\n", rteam, rhealth);
        if (rhealth != 1 && rteam != myteam && myhealth != 1) {
            myhealth--;
            update();
        }
        if (rteam != myteam) {
            tNeopixel pixel[] =
            {
                { 5, NP_RGB(0, 0,  0) }, /* off */
                { 6, NP_RGB(0,  0, 0) }, /* off */
                { 7, NP_RGB(0,  0, 0) }, /* off */
                { 8, NP_RGB(0,  0,  0) }, /* off */
                { 9, NP_RGB(0,  0,  0) }, /* off */
            };
            neopixel_SetPixel(neopixel, &pixel[0], 5);
            uint32_t color;
            if (rteam == 1) {
                color = NP_RGB(0,  0, 50); //blue
            } else if (rteam == 2) {
                color = NP_RGB(50, 0,  0); //red
            } else {
                color = NP_RGB(0,50,0); //green
            }
            for (int i =0; i < rhealth; i++) {
                pixel[i].rgb = color;
            }
            neopixel_SetPixel(neopixel, &pixel[0], 5);
        }
	}
}

static void button_click_cb(void *arg, void *data)
{
    printf("shoot\n");
   sendIR(NEC, 0xBA00BA00 | myhealth | (myteam << 4), 32); //protocol, code, bits
}

static void button_long_cb(void *arg, void *data)
{
  if (myhealth == 5) {
    if (myteam == 3) {
        myteam = 1;
    } else {
        myteam++;
    }
  } else {
    myhealth = 5;
  }
  update();
}

void app_main(void)
{    
    neopixel = neopixel_Init(25, NEOPIXEL_PIN);
    if(NULL == neopixel)   {
      ESP_LOGE(TAG, "[%s] Initialization failed\n", __func__);
      return;
    }
    update();

    button_config_t btn_cfg = {
        .type = BUTTON_TYPE_GPIO,
        .long_press_time = 2000,
        .gpio_button_config = {
            .gpio_num = BUTTON_PIN,
            .active_level = BUTTON_ACTIVE_LEVEL,
        },
    };
    button_handle_t btn = iot_button_create(&btn_cfg);
    assert(btn);
    esp_err_t err = iot_button_register_cb(btn, BUTTON_PRESS_DOWN, button_click_cb, NULL);
    err |= iot_button_register_cb(btn, BUTTON_LONG_PRESS_START, button_long_cb, NULL);
    ESP_ERROR_CHECK(err);

	xTaskCreate(recvIR, "recvIR", 8000, NULL, 10, NULL);    
    sendIR_init();    

}