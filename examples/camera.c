/*
 Licence: CC0/Public Domain http://creativecommons.org/publicdomain/zero/1.0/
 Author: Adrian Rossiter <adrian@antiprism.com>

 libu8g2arm example showing how to initialise a display U8g2-style using C
*/

#include <libu8g2arm/u8g2.h>
#include <libu8g2arm/u8g2arm.h>

#include <stdio.h>
#include <sys/utsname.h>
#include <sys/time.h>

u8g2_t u8g2;

uint8_t *frameBuffer;

#undef __INVERSE_VIDEO__
// #define __INVERSE_VIDEO__

#define NO_EDGE (0)
#define FALLING_EDGE (-1)
#define RAISING_EDGE (1)

typedef struct {
	int zeroLength;
	int oneLength;
	int zeroCount;
	int oneCount;
	int currentValue;
	int pinNumber;
	int edge;
	char name[8];
}Debouncer ;

void debouncerInit(Debouncer *debouncer, int gpio, const char *pinName, int length0, int length1){
	debouncer->pinNumber = gpio;
	strncpy(debouncer->name, pinName, 7); debouncer->name[7] = '\0';
	debouncer->zeroLength = length0;
	debouncer->oneLength  = length1;
	debouncer->zeroCount = 0;
	debouncer->oneCount  = 0;
	debouncer->currentValue = -1;
	debouncer->edge = NO_EDGE;
	exportGPIOPin(gpio);
}

int debouncerUpdate(Debouncer *debouncer){
	int edge = NO_EDGE;
	if(0 == getGPIOValue(debouncer->pinNumber)){
		debouncer->oneCount = 0;
		if(debouncer->zeroCount < debouncer->zeroLength){
			debouncer->zeroCount++;
			if(debouncer->zeroCount == debouncer->zeroLength){
				if(1 == debouncer->currentValue){
					edge = FALLING_EDGE;
				}
				debouncer->currentValue = 0;
			}
		}
	}else{
		debouncer->zeroCount = 0;
		if(debouncer->oneCount < debouncer->oneLength){
			debouncer->oneCount++;
			if(debouncer->oneCount == debouncer->oneLength){
				if(0 == debouncer->currentValue){
					edge = RAISING_EDGE;
				}
				debouncer->currentValue = 1;
			}
		}
	}
	debouncer->edge = edge;
	if(edge){
		// fprintf(stderr, "%s(%s):edge=%d" "\n", __func__, debouncer->name, edge);
	}
	return(edge);
}

void debouncerPrintf(Debouncer *debouncer, const char *title){
	fprintf(stderr, "%s: {.0L=%d, .1L=%d, .0C=%d, .1C=%d, .current=%2d}" "\n",
			title,
			debouncer->zeroLength,
			debouncer->oneLength,
			debouncer->zeroCount,
			debouncer->oneCount,
			debouncer->currentValue);
}

#define GUI_EVENT_SINGLE_CLICK (1)
#define GUI_EVENT_DOUBLE_CLICK (2)
#define GUI_EVENT_LONG_CLICK (3)
#define GUI_EVENT_CCW (4)
#define GUI_EVENT_UP (GUI_EVENT_CCW)
#define GUI_EVENT_CW (5)
#define GUI_EVENT_DOWN (GUI_EVENT_CW)

void guiEvent(int event){
	(void)event;
	fprintf(stderr, "%s(%d)" "\n", __func__, event);
}

void gui(u8g2_t *p){
	(void)p;

	Debouncer enc3Switch;
	Debouncer enc3Data;
	Debouncer enc3Clock;
	debouncerInit(&enc3Switch, 17, "ENC3 SW", 3, 3);
	debouncerInit(&enc3Data, 27, "ENC3 DT", 3, 3);
	debouncerInit(&enc3Clock, 22, "ENC3 CLK", 3, 3);

	uint64_t counter = 0;
	uint64_t singleClickPending = 0;
	uint64_t buttonPress;
	uint64_t buttonRelease;

	for(;;){
		usleep(1000);
		counter++;
		debouncerUpdate(&enc3Switch);
		debouncerUpdate(&enc3Data);
		debouncerUpdate(&enc3Clock);
		switch(enc3Switch.edge){
			case NO_EDGE:
				if(singleClickPending && ((counter - singleClickPending) > 500)){
					// fprintf(stderr, "=> single click" "\n");
					singleClickPending = 0;
					guiEvent(GUI_EVENT_SINGLE_CLICK);
				}
			default:
				break;
			case FALLING_EDGE:
				// fprintf(stderr, "ENC3Switch: Falling" "\n");
				buttonPress = counter;
				break;
			case RAISING_EDGE:
				{
					// fprintf(stderr, "ENC3Switch: Raising" "\n");
					uint64_t duration = counter - buttonPress;
					if(duration > 1000){
						// fprintf(stderr, "> 1s => long click" "\n");
						singleClickPending = 0;
						guiEvent(GUI_EVENT_LONG_CLICK);
					}else{
						// Detect double click
						uint64_t delta = counter - buttonRelease;
						if(delta < 500){
							// 2 button releases in less than 500ms
							// fprintf(stderr, "=> double click" "\n");
							singleClickPending = 0;
						guiEvent(GUI_EVENT_DOUBLE_CLICK);
						}else{
							// fprintf(stderr, "=> single click pending" "\n");
							singleClickPending = counter;
						}

					}
					buttonRelease = counter;
				}

				break;
		}
		if(RAISING_EDGE == enc3Clock.edge){
			if(0 == enc3Data.currentValue){
				// fprintf(stderr, "ENC3:  CW step" "\n");
				guiEvent(GUI_EVENT_CW);
			}else if(1 == enc3Data.currentValue){
				// fprintf(stderr, "ENC3: CCW step" "\n");
				guiEvent(GUI_EVENT_CCW);
			}
		}
	}
}


int main(int argc, const char *argv[])
{
  uint8_t *frameBuffer;

  u8x8_t *p_u8x8 = u8g2_GetU8x8(&u8g2);

  // U8g2 Setup for example display with HW I2C
  u8g2_Setup_ssd1309_128x64_noname0_f(&u8g2, U8G2_R1,
                                        u8x8_byte_arm_linux_hw_spi,
                                        u8x8_arm_linux_gpio_and_delay);
  u8x8_SetPin(p_u8x8, U8X8_PIN_SPI_CLOCK, U8X8_PIN_NONE);
  u8x8_SetPin(p_u8x8, U8X8_PIN_SPI_DATA, U8X8_PIN_NONE);
  u8x8_SetPin(p_u8x8, U8X8_PIN_RESET, 25); // GPIO25
  u8x8_SetPin(p_u8x8, U8X8_PIN_DC, 24); // GPIO24

  // U8g2arm needs an extra call for hardware I2C to set the bus number
  // (and for hardware SPI to set the bus number and CS number)
  if (!u8g2arm_arm_init_hw_spi(p_u8x8, /* bus_number = */ 0, /* CS number */ 0, /* speed MHz */ 8)) {
    fprintf(stderr, "could not initialise SPI device");
    exit(1);
  }

  // U8g2 begin
  u8g2_InitDisplay(&u8g2);
  u8g2_ClearDisplay(&u8g2);
  u8g2_SetPowerSave(&u8g2, 0);
  frameBuffer = u8g2_GetBufferPtr(&u8g2);

  // Draw something to the display
  u8g2_ClearBuffer(&u8g2);                    // clear the internal memory
#ifdef __INVERSE_VIDEO__
  u8g2_SetDrawColor(&u8g2, 0);
#else
  u8g2_SetDrawColor(&u8g2, 1);
#endif
  u8g2_t *p = &u8g2;
  if(argc > 1){
	  FILE *logo = fopen(argv[1], "rb");
	  if(logo){
		  char ligne[128 + 2];
		  // char *line = NULL;
		  int y = 0;
		  while(fgets(ligne, sizeof(ligne) - 1, logo)){
			  size_t l = strlen(ligne);
			  if(l < 65){
				  fprintf(stderr, "line too long: %ld" "\n", l);
			  }else{
				  for(int offset = 0 ; offset < 64 ; offset++){
					  int bank = (offset >> 3);
					  int bit = 1 << (offset & 7);
					  unsigned char octet = frameBuffer[bank * 128 + (127 - y)];
					  if(ligne[offset] != ' '){
						  octet |= bit;
					  }else{
						  octet &= ~bit;
					  }
					  octet = frameBuffer[bank * 128 + (127 - y)] = octet;
				  }
			  }
			  y++;
			  if(128 == y){
				  break;
			  }
		  }
		  fclose(logo);
	  }
  }else{
	  u8g2_SetFont(&u8g2, u8g2_font_6x13_tf);
	  frameBuffer = u8g2_GetBufferPtr(&u8g2);
	  u8g2_DrawStr(p, 16, 16, "Five Pi");
	  u8g2_DrawStr(p, 8, 32, "A Pi 5");
	  u8g2_DrawStr(p, 16, 48, "Video");
	  u8g2_DrawStr(p, 24, 64, "Camera");
	  struct utsname buf;
	  uname(&buf);
	  u8g2_DrawStr(p, 0, 80, buf.nodename);
  }
  u8g2_SendBuffer(p);
  gui(p);

  return 0;
}
