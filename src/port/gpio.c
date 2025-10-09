/*
   Copyright (c) 2019, Wu Han <wuhanstudio@hust.edu.cn>
                       http://wuhanstudio.cc

   Permission is hereby granted, free of charge, to any person obtaining a
   copy of this software and associated documentation files (the "Software"),
   to deal in the Software without restriction, including without limitation
   the rights to use, copy, modify, merge, publish, distribute, sublicense,
   and/or sell copies of the Software, and to permit persons to whom the
   Software is furnished to do so, subject to the following conditions:

      The above copyright notice and this permission notice shall be included
      in all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
  FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
  IN THE SOFTWARE.
*/

#include "gpio.h"
#include <unistd.h>

#ifdef __USE_SYSFS__

#define GPIO_FILENAME_DEFINE(pin,field) char fileName[255] = {0}; \
        sprintf(fileName, "/sys/class/gpio/gpio%d/%s", pin, field);

static int writeValueToFile(char* fileName, char* buff)
{
    FILE *fp = fopen(fileName, "w");
    if (fp != NULL)
    {
        fwrite(buff, strlen(buff), 1, fp);
        fflush(fp);
        return 0;
    }
    return -1;
}

static int readValueFromFile(char* fileName, char* buff, int len)
{
    int ret = -1;
    FILE *fp = fopen(fileName,"r");
    if (fp == NULL)
    {
        return -1;
    }
    else
    {
        if (fread(buff, sizeof(char), len, fp) > 0)
        {
            ret = 0;
        }
    }
    fclose(fp);
    return ret;
}

static int readIntValueFromFile(char* fileName)
{
    char buff[255];
    memset(buff, 0, sizeof(buff));
    int ret = readValueFromFile(fileName, buff, sizeof(buff) - 1);
    if (ret == 0) {
        return atoi(buff);
    }
    return ret;
}

int exportGPIOPin(int pin)
{
    FILE* fp = fopen("/sys/class/gpio/export", "w");
    if (fp != NULL)
    {
        fprintf(fp, "%d", pin);
        fclose(fp);
        usleep(30000);
        return 0;
    }
    else
    {
        printf("Failed to export pin %d\n", pin);
        return -1;
    }
}

int unexportGPIOPin(int pin)
{
    FILE* fp = fopen("/sys/class/gpio/unexport", "w");
    if (fp != NULL)
    {
        fprintf(fp, "%d", pin);
        fclose(fp);
        usleep(30000);
        return 0;
    }
    else
    {
        printf("Failed to unexport pin %d\n", pin);
        return -1;
    }
}

int getGPIOValue(int pin)
{
    GPIO_FILENAME_DEFINE(pin, "value")
    return readIntValueFromFile(fileName);
}

int setGPIOValue(int pin, int value)
{
    static FILE* fp_gpio[255];
    if(fp_gpio[pin] != NULL)
    {
        fprintf(fp_gpio[pin], "%d", value);
        fflush(fp_gpio[pin]);
        return 0;
    }
    else
    {
        GPIO_FILENAME_DEFINE(pin, "value")
        fp_gpio[pin] = fopen(fileName, "w+");
        if (fp_gpio[pin] != NULL)
        {
            fprintf(fp_gpio[pin], "%d", value);
            fflush(fp_gpio[pin]);
            return 0;
        }
        else
        {
            return -1;
        }
    }
}

int setGPIODirection(int pin, int direction)
{
    char directionStr[10];
    GPIO_FILENAME_DEFINE(pin, "direction")

    if (direction == GPIO_IN)
    {
        strcpy(directionStr, "in");
    }
    else if (direction == GPIO_OUT)
    {
        strcpy(directionStr, "out");
    }
    else
    {
        return -1;
    }
    return writeValueToFile(fileName, directionStr);
}

int getGPIODirection(int pin)
{
    char buff[255] = {0};
    int direction;
    int ret;

    GPIO_FILENAME_DEFINE(pin, "direction")
    ret = readValueFromFile(fileName, buff, sizeof(buff)-1);
    if (ret >= 0)
    {
        if (strncasecmp(buff, "out", 3)==0)
        {
            direction = GPIO_OUT;
        }
        else if (strncasecmp(buff, "in", 2)==0)
        {
            direction = GPIO_IN;
        }
        else
        {
            return -1;
        }
        return direction;
    }
    return ret;
}

#else

#include <gpiod.h>

static struct gpiod_chip *chip = NULL;

static struct {
	struct gpiod_line *line;
	int direction;
	int value;
} gpio_ports[256] = {NULL};

int exportGPIOPin(int pin)
{
	char gpioName[8];
	int offset = pin & 0xFF;
	struct gpiod_line *line = NULL;
	if(NULL == chip)
	{
		chip = gpiod_chip_open_by_name("gpiochip0");
	}
	if(NULL != chip)
	{
		snprintf(gpioName, sizeof(gpioName), "GPIO%d", offset);
		line = gpiod_chip_find_line(chip, gpioName);
		if(NULL != line)
		{
			int cr = gpiod_line_request_input(line, "libu8g2arm");
			if(cr)
			{
				perror("gpio_line_request_input");
			}
		}
	}
	// fprintf(stderr, "%s(%i)=>(chip=%p, line=%p)" "\n", __func__, pin, chip, line);
	gpio_ports[offset].line = line ;
	return(0);
}

int unexportGPIOPin(int pin)
{
	int offset = pin & 0xFF;
	struct gpiod_line *line = gpio_ports[pin & 0xFF].line;
	if(line)
	{
		gpiod_line_release(line);
	}
	// fprintf(stderr, "%s(%i)" "\n", __func__, pin);
	return(0);
}

int getGPIOValue(int pin)
{
	int value = -1;
	struct gpiod_line *line = gpio_ports[pin & 0xFF].line;
	if(NULL != line)
	{
		value = gpiod_line_get_value(line);
	}
	fprintf(stderr, "%s(%i)=>%i" "\n", __func__, pin, value);
	return(value);
}

int setGPIOValue(int pin, int value)
{
	int offset = pin & 0xFF;
	int actualValue = -1;
	struct gpiod_line *line = gpio_ports[offset].line;
	if(NULL != line)
	{
		gpio_ports[offset].value = value == GPIO_HIGH ? 1 : 0;
		actualValue = gpiod_line_set_value(line, value);
		if(-1 == actualValue)
		{
			perror("gpiod_line_set_value");
		}
	}
	// fprintf(stderr, "%s(%i, %i)(line=%p, actualvalue=%i)" "\n", __func__, pin, value, line, actualValue);
	return(0);
}

int setGPIODirection(int pin, int direction)
{
	int offset = pin & 0xFF;
	struct gpiod_line *line = gpio_ports[offset].line;
	if(line)
	{
		gpio_ports[offset].direction = direction;
		if(GPIO_OUT == direction)
		{
			gpiod_line_release(line);
			int cr = gpiod_line_request_output(line, "libu8g2arm", 0);
			if(cr)
			{
				perror("gpio_line_request_input");
			}
		}
		else
		{
			gpiod_line_release(line);
			int cr = gpiod_line_request_input(line, "libu8g2arm");
			if(cr)
			{
				perror("gpio_line_request_input");
			}
		}
	}
	// fprintf(stderr, "%s(%i, %i)" "\n", __func__, pin, direction);
	return(0);
}

int getGPIODirection(int pin)
{
	int direction = 0;
	int offset = pin & 0xFF;
	struct gpiod_line *line = gpio_ports[offset].line;
	if(line)
	{
		direction = gpio_ports[offset].direction;
	}
	// fprintf(stderr, "%s(%i)=>%i" "\n", __func__, pin, direction);
	return(direction);
}

#endif

