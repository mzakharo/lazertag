/*
https://github.com/junkfix/esp32-rmt-ir
*/

#ifndef ir_rmt_esp32
#define ir_rmt_esp32

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/rmt_rx.h"
#include "driver/rmt_tx.h"
#include "driver/rmt_encoder.h"

extern uint8_t irRxPin;
extern uint8_t irTxPin;

typedef enum  { UNK, NEC, PROTO_COUNT } irproto;


typedef struct {
	rmt_encoder_t base;
	rmt_encoder_t *copy_encoder;
	uint8_t bit_index;
	int state;
} rmt_ir_encoder_t;

typedef struct {
	irproto irtype;
	uint32_t ircode;
	uint8_t bits;
} sendir_t;

typedef struct {
	uint16_t header_high;
	uint16_t header_low;
	uint16_t one_high;
	uint16_t one_low;
	uint16_t zero_high;
	uint16_t zero_low;
	uint16_t footer_high;
	uint8_t footer_low;
	uint16_t frequency;
	const char* name;
} ir_protocol_t;

extern const ir_protocol_t proto[PROTO_COUNT];

extern void irReceived(irproto brand, uint32_t code, size_t len, rmt_symbol_word_t *item);
void sendIR(irproto brand, uint32_t code, uint8_t bits);

IRAM_ATTR bool irrx_done(rmt_channel_handle_t channel, const rmt_rx_done_event_data_t *edata, void *udata);

void recvIR(void* param);

#endif
