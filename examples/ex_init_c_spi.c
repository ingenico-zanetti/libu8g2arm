/*
 Licence: CC0/Public Domain http://creativecommons.org/publicdomain/zero/1.0/
 Author: Adrian Rossiter <adrian@antiprism.com>

 libu8g2arm example showing how to initialise a display U8g2-style using C
*/

#include <libu8g2arm/u8g2.h>
#include <libu8g2arm/u8g2arm.h>

#include <stdio.h>

u8g2_t u8g2;

uint8_t *frameBuffer;

#undef __INVERSE_VIDEO__
//#define __INVERSE_VIDEO__

static void drawValue(u8g2_t *p, int startX, int startY, const char *title, const char *value){
  u8g2_DrawStr(p, startX + 4,  startY + 13, title);
  u8g2_DrawStr(p, startX + 28, startY + 26, value);
  u8g2_DrawRFrame(p, startX, startY, 64, 32, 5);
}

static int gain = 0;

void loop(u8g2_t *p) {
  char str[16];
  
#ifdef __INVERSE_VIDEO__
  memset(frameBuffer, 0xFF, 8 * 128);
#else
  memset(frameBuffer, 0x00, 8 * 128);
#endif

  sprintf(str, "+%2ddB", gain);
  drawValue(p, 0,  0, "GAIN:", str);
  
  sprintf(str, "%4dK", 3300 + gain * 64);
  drawValue(p, 0,  32, "COLOR:", str);
  
  sprintf(str, "%3d", 5 * gain);
  drawValue(p, 0,  64, "ANGLE:", str);
  
  sprintf(str, "%3d", 900 + 5 * gain);
  drawValue(p, 0,  96, "GAMMA:", str);
  u8g2_SendBuffer(p);

  // usleep(100000);

  gain++;
  gain &= 0x3f;
}

int main()
{
  u8x8_t *p_u8x8 = u8g2_GetU8x8(&u8g2);

  // U8g2 Setup for example display with HW I2C
  u8g2_Setup_ssd1309_128x64_noname0_f(&u8g2, U8G2_R1,
                                        u8x8_byte_arm_linux_hw_spi,
                                        u8x8_arm_linux_gpio_and_delay);
  u8x8_SetPin(p_u8x8, U8X8_PIN_SPI_CLOCK, U8X8_PIN_NONE);
  u8x8_SetPin(p_u8x8, U8X8_PIN_SPI_DATA, U8X8_PIN_NONE);
#ifdef __USE_SYSFS__
  u8x8_SetPin(p_u8x8, U8X8_PIN_RESET, 594); // 594 -> GPIO25
  u8x8_SetPin(p_u8x8, U8X8_PIN_DC, 593); // 593 -> GPIO24
#else
  u8x8_SetPin(p_u8x8, U8X8_PIN_RESET, 25); // GPIO25
  u8x8_SetPin(p_u8x8, U8X8_PIN_DC, 24); // GPIO24
#endif

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

  // Draw something to the display
  u8g2_ClearBuffer(&u8g2);                    // clear the internal memory
#ifdef __INVERSE_VIDEO__
  u8g2_SetDrawColor(&u8g2, 0);
#else
  u8g2_SetDrawColor(&u8g2, 1);
#endif
  u8g2_SetFont(&u8g2, u8g2_font_6x13_tf);
  frameBuffer = u8g2_GetBufferPtr(&u8g2);
 int i = 1000;
 while(i--){
	 loop(&u8g2);
 }
  // Leave it on the screen for a time
  // sleep(5);

  // Clear the screen
  //u8g2_ClearBuffer(&u8g2); // clear the internal memory
  //u8g2_SendBuffer(&u8g2);  // transfer internal memory to the display

  return 0;
}
