/*
https://github.com/junkfix/esp32-rmt-ir
*/
#include "rmt.h"


uint32_t nec_check(rmt_symbol_word_t *item, size_t len);
bool rc5_bit(uint32_t d, uint32_t v);
bool checkbit(rmt_symbol_word_t * item, uint16_t high, uint16_t low);
void fill_item(rmt_symbol_word_t *item, uint16_t high, uint16_t low, bool bit);

size_t rmt_encode_ir(rmt_encoder_t *encoder, rmt_channel_handle_t channel, const void *primary_data, size_t data_size, rmt_encode_state_t *ret_state);

esp_err_t rmt_del_ir_encoder(rmt_encoder_t *encoder) ;

esp_err_t rmt_ir_encoder_reset(rmt_encoder_t *encoder);

uint8_t irRxPin = 32;
uint8_t irTxPin = 26;

const ir_protocol_t proto[PROTO_COUNT] = {
	[UNK]  = {    0,    0,    0,    0,   0,   0,   0, 0,     0, "UNK"  },
	[NEC]  = { 9000, 4500,  560, 1690, 560, 560, 560, 0, 38000, "NEC"  },
};

const uint8_t  bitMargin = 120;

volatile uint8_t irTX = 0;
volatile uint8_t irRX = 0;

void recvIR(void* param){
	rmt_rx_done_event_data_t rx_data;
	QueueHandle_t rx_queue = xQueueCreate(1, sizeof(rx_data));

	for(;;){
		if(irTX){vTaskDelay( 2 / portTICK_PERIOD_MS ); continue;}
		irRX = 1;
		rmt_channel_handle_t rx_channel = NULL;
		rmt_symbol_word_t symbols[64];
		
		rmt_receive_config_t rx_config = {
			.signal_range_min_ns = 1250,
			.signal_range_max_ns = 12000000,
		};
		
		rmt_rx_channel_config_t rx_ch_conf = {
			.gpio_num = irRxPin,
			.clk_src = RMT_CLK_SRC_DEFAULT,
			.resolution_hz = 1000000,
			.mem_block_symbols = 64,
		};
		
		rmt_new_rx_channel(&rx_ch_conf, &rx_channel);
		rmt_rx_event_callbacks_t cbs = {
			.on_recv_done = irrx_done,
		};

		rmt_rx_register_event_callbacks(rx_channel, &cbs, rx_queue);
		rmt_enable(rx_channel);
		rmt_receive(rx_channel, symbols, sizeof(symbols), &rx_config);
		for(;;){
			if(irTX){break;}

			if (xQueueReceive(rx_queue, &rx_data, pdMS_TO_TICKS(1000)) == pdPASS){
				
				size_t len = rx_data.num_symbols;
				rmt_symbol_word_t *rx_items = rx_data.received_symbols;

				if(len > 11){
					uint32_t rcode = 0; irproto rproto = UNK;
					if( (rcode = nec_check(rx_items, len)) ){
						rproto = NEC;
					}
					irReceived(rproto, rcode, len, rx_items);
				}

				rmt_receive(rx_channel, symbols, sizeof(symbols), &rx_config);
			}
		}
		rmt_disable(rx_channel);
		rmt_del_channel(rx_channel);
		irRX = 0;
	}
	vQueueDelete(rx_queue);
	vTaskDelete(NULL);
}

rmt_channel_handle_t tx_channel = NULL;
rmt_encoder_handle_t encoder_handle = NULL;
void sendIR_init() {
	
	rmt_tx_channel_config_t txconf = {
		.gpio_num = irTxPin,
		.clk_src = RMT_CLK_SRC_DEFAULT,
		.resolution_hz = 1000000, // 1MHz resolution, 1 tick = 1us
		.mem_block_symbols = 64,
		.trans_queue_depth = 4,
	};
	
	if(rmt_new_tx_channel(&txconf, &tx_channel) != ESP_OK) {
		return;
	}
	float duty = 0.50;
	
	rmt_carrier_config_t carrier_cfg = {
		.frequency_hz = proto[NEC].frequency,
		.duty_cycle = duty,
	};
	
	rmt_apply_carrier(tx_channel, &carrier_cfg);

	rmt_ir_encoder_t *ir_encoder = (rmt_ir_encoder_t *)calloc(1, sizeof(rmt_ir_encoder_t));
	ir_encoder->base.encode = rmt_encode_ir;
	ir_encoder->base.del = rmt_del_ir_encoder;
	ir_encoder->base.reset = rmt_ir_encoder_reset;

	rmt_copy_encoder_config_t copy_encoder_config = {};
	rmt_new_copy_encoder(&copy_encoder_config, &ir_encoder->copy_encoder);

	encoder_handle = &ir_encoder->base;

}

void sendIR(irproto brand, uint32_t code, uint8_t bits) {

	sendir_t codetx = {brand, code, bits};
	rmt_enable(tx_channel);
	rmt_transmit_config_t tx_config = {
		.loop_count = 0,
	};
	rmt_transmit(tx_channel, encoder_handle, &codetx, sizeof(codetx), &tx_config);
	rmt_tx_wait_all_done(tx_channel, portMAX_DELAY);	
	rmt_disable(tx_channel);
	//rmt_del_channel(tx_channel);
	//rmt_del_encoder(encoder_handle);
}


uint32_t nec_check(rmt_symbol_word_t *item, size_t len){
	const uint8_t  totalData = 34;
	if(len < totalData ){
		return 0;
	}
	const uint32_t m = 0x80000000;
	uint32_t code = 0;
	for(uint8_t i = 0; i < totalData; i++){
		if(i == 0){//header
			if(!checkbit(&item[i], proto[NEC].header_high, proto[NEC].header_low)){return 0;}
		}else if(i == 33){//footer
			if(!checkbit(&item[i], proto[NEC].footer_high, proto[NEC].footer_low)){return 0;}
		}else if(checkbit(&item[i], proto[NEC].one_high, proto[NEC].one_low)){
			code |= (m >> (i - 1) );
		}else if(!checkbit(&item[i], proto[NEC].zero_high, proto[NEC].zero_low)){
			//Serial.printf("BitError i:%d\n",i);
			return 0;
		}
	}
	return code;
}


bool rc5_bit(uint32_t d, uint32_t v) {
	return (d < (v + bitMargin)) && (d > (v - bitMargin));
}


bool checkbit(rmt_symbol_word_t  * item, uint16_t high, uint16_t low){
	return item->level0 == 0 && item->level1 != 0 &&
		item->duration0 < (high + bitMargin) && item->duration0 > (high - bitMargin) &&
		item->duration1 < (low + bitMargin) && item->duration1 > (low - bitMargin);
}
bool irrx_done(rmt_channel_handle_t channel, const rmt_rx_done_event_data_t *edata, void *udata){
	BaseType_t h = pdFALSE;
	QueueHandle_t q = (QueueHandle_t)udata;
	xQueueSendFromISR(q, edata, &h);
	return h == pdTRUE;
}



size_t rmt_encode_ir(rmt_encoder_t *encoder, rmt_channel_handle_t channel, const void *primary_data, size_t data_size, rmt_encode_state_t *ret_state) {
	rmt_ir_encoder_t *ir_encoder = __containerof(encoder, rmt_ir_encoder_t, base);
	
	rmt_encode_state_t fstate = RMT_ENCODING_RESET;
	
	size_t encoded_symbols = 0;
	rmt_encoder_handle_t copy_encoder = ir_encoder->copy_encoder;
	
	const sendir_t *send_data = (const sendir_t *)primary_data;
	ir_protocol_t protocol = proto[send_data->irtype];
	

	if (ir_encoder->state == 0) {
		if(protocol.header_high>0){
			rmt_symbol_word_t header_symbol;
			fill_item(&header_symbol, protocol.header_high, protocol.header_low, 0);
			encoded_symbols += copy_encoder->encode(copy_encoder, channel, &header_symbol, sizeof(rmt_symbol_word_t), &fstate);
		}else{
			fstate = RMT_ENCODING_COMPLETE;
		}
		
		if (fstate & RMT_ENCODING_COMPLETE) {
			ir_encoder->state = 1;
			ir_encoder->bit_index = 0;
		}
		if (fstate & RMT_ENCODING_MEM_FULL) {
			fstate = RMT_ENCODING_MEM_FULL;
			*ret_state = fstate;
			return encoded_symbols;
		}
	}

	if (ir_encoder->state == 1) {
	
		uint8_t rcspecial = 0;
		
		rmt_symbol_word_t one_symbol;
		fill_item(&one_symbol, protocol.one_high, protocol.one_low, rcspecial);

		rmt_symbol_word_t zero_symbol;
		fill_item(&zero_symbol, protocol.zero_high, protocol.zero_low, 0);

		for (uint8_t i = ir_encoder->bit_index; i < send_data->bits; i++) {
			if (send_data->ircode & (1 << (send_data->bits - 1 - i))) {
				encoded_symbols += copy_encoder->encode(copy_encoder, channel, &one_symbol, sizeof(rmt_symbol_word_t), &fstate);
			} else {
				encoded_symbols += copy_encoder->encode(copy_encoder, channel, &zero_symbol, sizeof(rmt_symbol_word_t), &fstate);
			}
			if (fstate & RMT_ENCODING_MEM_FULL) {
				fstate = RMT_ENCODING_MEM_FULL;
				ir_encoder->bit_index = i + 1;
				*ret_state = fstate;
				return encoded_symbols;
			}
		}
		ir_encoder->state = 2;
	}

	if (ir_encoder->state == 2) {
		if(protocol.footer_high>0){
			rmt_symbol_word_t end_symbol;
			fill_item(&end_symbol, protocol.footer_high, protocol.footer_low, 0);
			encoded_symbols += copy_encoder->encode(copy_encoder, channel, &end_symbol, sizeof(rmt_symbol_word_t), &fstate);
		}else{
			fstate = (rmt_encode_state_t)((int)fstate | RMT_ENCODING_COMPLETE);
		}
		if (fstate & RMT_ENCODING_COMPLETE) {
			ir_encoder->state = 0;
			*ret_state = RMT_ENCODING_COMPLETE;
		}
	}

	return encoded_symbols;
}

void fill_item(rmt_symbol_word_t  * item, uint16_t high, uint16_t low, bool bit){
	item->level0 = !bit;
	item->duration0 = high;
	item->level1 = bit;
	item->duration1 = low;
}

esp_err_t rmt_del_ir_encoder(rmt_encoder_t *encoder) {
	rmt_ir_encoder_t *ir_encoder = __containerof(encoder, rmt_ir_encoder_t, base);
	rmt_del_encoder(ir_encoder->copy_encoder);
	free(ir_encoder);
	return ESP_OK;
}

esp_err_t rmt_ir_encoder_reset(rmt_encoder_t *encoder) {
	rmt_ir_encoder_t *ir_encoder = __containerof(encoder, rmt_ir_encoder_t, base);
	rmt_encoder_reset(ir_encoder->copy_encoder);
	ir_encoder->state = 0;
	ir_encoder->bit_index = 0;
	return ESP_OK;
}
