#include <stdio.h>
#include "rmt.h"

void irReceived(irproto brand, uint32_t code, size_t len, rmt_symbol_word_t *item){
	if( code ){
		printf("IR %s, code: %#lx, bits: %d\n",  proto[brand].name, code, len);
	}
	
	if(false){//debug
		printf("Rx%d: ", len);							
		for (uint8_t i=0; i < len ; i++ ) {
			int d0 = item[i].duration0; if(!item[i].level0){d0 *= -1;}
			int d1 = item[i].duration1; if(!item[i].level1){d1 *= -1;}
			printf("%d,%d ", d0, d1);
		}								
		printf("\n");
	}
  
}
void txIR(void* param){

    while (true) {
        sendIR(NEC, 0xC1AAFC03, 32); //protocol, code, bits
        vTaskDelay( 1000 / portTICK_PERIOD_MS );
    }

}
void app_main(void)
{
	xTaskCreate(recvIR, "recvIR", 8000, NULL, 10, NULL);
    xTaskCreate(txIR, "txIR", 8000, NULL, 10, NULL);

}