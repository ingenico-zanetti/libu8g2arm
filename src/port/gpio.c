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

static const char *const chip_path = "/dev/gpiochip0";
static struct gpiod_chip *chip = NULL;

/* Request a line as input. */
static struct gpiod_line_request *request_input_line( unsigned int offset, const char *consumer)
{
        struct gpiod_request_config *req_cfg = NULL;
        struct gpiod_line_request *request = NULL;
        struct gpiod_line_settings *settings;
        struct gpiod_line_config *line_cfg;
        int ret;

        settings = gpiod_line_settings_new();
        if (!settings)
                goto close_chip;

        gpiod_line_settings_set_direction(settings, GPIOD_LINE_DIRECTION_INPUT);

        line_cfg = gpiod_line_config_new();
        if (!line_cfg)
                goto free_settings;

        ret = gpiod_line_config_add_line_settings(line_cfg, &offset, 1, settings);
        if (ret)
                goto free_line_config;

        if (consumer) {
                req_cfg = gpiod_request_config_new();
                if (!req_cfg)
                        goto free_line_config;

                gpiod_request_config_set_consumer(req_cfg, consumer);
        }

        request = gpiod_chip_request_lines(chip, req_cfg, line_cfg);

        gpiod_request_config_free(req_cfg);

free_line_config:
        gpiod_line_config_free(line_cfg);

free_settings:
        gpiod_line_settings_free(settings);

close_chip:

        return request;
}

static int reconfigure_as_output_line(struct gpiod_line_request *request,
                                      unsigned int offset,
                                      enum gpiod_line_value value)
{
        struct gpiod_line_settings *settings;
        struct gpiod_line_config *line_cfg;
        int ret = -1;

        settings = gpiod_line_settings_new();
        if (!settings)
                return -1;

        gpiod_line_settings_set_direction(settings,
                                          GPIOD_LINE_DIRECTION_OUTPUT);
        gpiod_line_settings_set_output_value(settings, value);

        line_cfg = gpiod_line_config_new();
        if (!line_cfg)
                goto free_settings;

        ret = gpiod_line_config_add_line_settings(line_cfg, &offset, 1, settings);
        if (ret)
                goto free_line_config;

        ret = gpiod_line_request_reconfigure_lines(request, line_cfg);

free_line_config:
        gpiod_line_config_free(line_cfg);

free_settings:
        gpiod_line_settings_free(settings);

        return ret;
}

static struct {
	struct gpiod_line_request *request;
	int line_offset;
	int direction;
	int value;
} gpio_ports[256] = { NULL };

int exportGPIOPin(int pin)
{
	char gpioName[8];
	int offset = pin & 0xFF;
	int line_offset = -1;
	if(NULL == chip)
	{
		chip = gpiod_chip_open(chip_path);
	}
	if(NULL != chip)
	{
		snprintf(gpioName, sizeof(gpioName), "GPIO%d", offset);
		line_offset = gpiod_chip_get_line_offset_from_name(chip, gpioName);
		if(-1 != line_offset)
		{
			gpio_ports[offset].line_offset = line_offset;
			gpio_ports[offset].request = request_input_line(offset, gpioName);
		}
		else
		{
			perror("gpiod_chip_get_line_offset_from_name");
		}
	}
	else
	{
		perror("gpiod_chip_open(chip_path)");
	}
	// fprintf(stderr, "%s(%i)=>(chip=%p, line_offset=%i, request=%p)" "\n", __func__, pin, chip, line_offset, gpio_ports[offset].request);
	return(0);
}

int unexportGPIOPin(int pin)
{
	int offset = pin & 0xFF;
	int line_offset = gpio_ports[offset].line_offset;
	if(-1 != line_offset)
	{
		gpio_ports[offset].line_offset = -1;
	}
	// fprintf(stderr, "%s(%i)" "\n", __func__, pin);
	return(0);
}

int getGPIOValue(int pin)
{
	int value = -1;
	int offset = pin & 0xFF;
	int line_offset = gpio_ports[offset].line_offset;
	if(-1 != line_offset)
	{
		value = gpiod_line_request_get_value(gpio_ports[offset].request, gpio_ports[offset].line_offset);
	}
	// fprintf(stderr, "%s(%i)=>%i" "\n", __func__, pin, value);
	return(value);
}

int setGPIOValue(int pin, int value)
{
	int actualValue = value;
	int offset = pin & 0xFF;
	int line_offset = gpio_ports[offset].line_offset;
	// fprintf(stderr, "%s(%i, %i)(request=%p, line=%i, actualvalue=%i)" "\n", __func__, pin, value, gpio_ports[offset].request, line_offset, actualValue);
	if(-1 != line_offset)
	{
		gpiod_line_request_set_value(gpio_ports[offset].request, line_offset, value);
		// perror("gpiod_line_request_set_value()");
	}
	return(0);
}

int setGPIODirection(int pin, int direction)
{
	int offset = pin & 0xFF;
	int line_offset = gpio_ports[offset].line_offset;
	// fprintf(stderr, "%s(%i, %i)" "\n", __func__, pin, direction);
	if(direction){
		reconfigure_as_output_line(gpio_ports[offset].request, line_offset, 0);
		// perror("reconfigure_as_output_line");
	}
	return(0);
}

int getGPIODirection(int pin)
{
	int direction = 0;
	int offset = pin & 0xFF;
	int line_offset = gpio_ports[offset].line_offset;
	if(-1 != line_offset)
	{
		direction = gpio_ports[offset].direction;
	}
	// fprintf(stderr, "%s(%i)=>%i" "\n", __func__, pin, direction);
	return(direction);
}

#endif

