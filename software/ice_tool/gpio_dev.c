/*
 * New GPIO access routines kanged from the internet uses /dev/gpio
 */
 
// needed for asprintf()
#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <linux/types.h>
#include <linux/gpio.h>
#include "gpio_dev.h"

#define RPI_GPIODEV "gpiochip0"
#define CONSUMER "gpio_dev"

/**
 * gpiotools_request_linehandle() - request gpio lines in a gpiochip
 * @device_name:	The name of gpiochip without prefix "/dev/",
 *			such as "gpiochip0"
 * @lines:		An array desired lines, specified by offset
 *			index for the associated GPIO device.
 * @nline:		The number of lines to request.
 * @flag:		The new flag for requsted gpio. Reference
 *			"linux/gpio.h" for the meaning of flag.
 * @data:		Default value will be set to gpio when flag is
 *			GPIOHANDLE_REQUEST_OUTPUT.
 * @consumer_label:	The name of consumer, such as "sysfs",
 *			"powerkey". This is useful for other users to
 *			know who is using.
 *
 * Request gpio lines through the ioctl provided by chardev. User
 * could call gpiotools_set_values() and gpiotools_get_values() to
 * read and write respectively through the returned fd. Call
 * gpiotools_release_linehandle() to release these lines after that.
 *
 * Return:		On success return the fd;
 *			On failure return the errno.
 */
int gpiotools_request_linehandle(const char *device_name, unsigned int *lines,
				 unsigned int nlines, unsigned int flag,
				 struct gpiohandle_data *data,
				 const char *consumer_label)
{
	struct gpiohandle_request req;
	char *chrdev_name;
	int fd;
	int i;
	int ret;

	ret = asprintf(&chrdev_name, "/dev/%s", device_name);
	if (ret < 0)
		return -ENOMEM;

	fd = open(chrdev_name, 0);
	if (fd == -1) {
		ret = -errno;
		fprintf(stderr, "Failed to open %s, %s\n",
			chrdev_name, strerror(errno));
		goto exit_close_error;
	}

	for (i = 0; i < nlines; i++)
		req.lineoffsets[i] = lines[i];

	req.flags = flag;
	strcpy(req.consumer_label, consumer_label);
	req.lines = nlines;
	if (flag & GPIOHANDLE_REQUEST_OUTPUT)
		memcpy(req.default_values, data, sizeof(req.default_values));

	ret = ioctl(fd, GPIO_GET_LINEHANDLE_IOCTL, &req);
	if (ret == -1) {
		ret = -errno;
		fprintf(stderr, "Failed to issue %s (%d), %s\n",
			"GPIO_GET_LINEHANDLE_IOCTL", ret, strerror(errno));
	}

exit_close_error:
	if (close(fd) == -1)
		perror("Failed to close GPIO character device file");
	free(chrdev_name);
	return ret < 0 ? ret : req.fd;
}

/**
 * gpiotools_set_values(): Set the value of gpio(s)
 * @fd:			The fd returned by
 *			gpiotools_request_linehandle().
 * @data:		The array of values want to set.
 *
 * Return:		On success return 0;
 *			On failure return the errno.
 */
int gpiotools_set_values(const int fd, struct gpiohandle_data *data)
{
	int ret;

	ret = ioctl(fd, GPIOHANDLE_SET_LINE_VALUES_IOCTL, data);
	if (ret == -1) {
		ret = -errno;
		fprintf(stderr, "Failed to issue %s (%d), %s\n",
			"GPIOHANDLE_SET_LINE_VALUES_IOCTL", ret,
			strerror(errno));
	}

	return ret;
}

/**
 * gpiotools_get_values(): Get the value of gpio(s)
 * @fd:			The fd returned by
 *			gpiotools_request_linehandle().
 * @data:		The array of values get from hardware.
 *
 * Return:		On success return 0;
 *			On failure return the errno.
 */
int gpiotools_get_values(const int fd, struct gpiohandle_data *data)
{
	int ret;

	ret = ioctl(fd, GPIOHANDLE_GET_LINE_VALUES_IOCTL, data);
	if (ret == -1) {
		ret = -errno;
		fprintf(stderr, "Failed to issue %s (%d), %s\n",
			"GPIOHANDLE_GET_LINE_VALUES_IOCTL", ret,
			strerror(errno));
	}

	return ret;
}

/**
 * gpiotools_release_linehandle(): Release the line(s) of gpiochip
 * @fd:			The fd returned by
 *			gpiotools_request_linehandle().
 *
 * Return:		On success return 0;
 *			On failure return the errno.
 */
int gpiotools_release_linehandle(const int fd)
{
	int ret;

	ret = close(fd);
	if (ret == -1) {
		perror("Failed to close GPIO LINEHANDLE device file");
		ret = -errno;
	}

	return ret;
}

int gpio_out_fd, gpio_in_fd;

struct gpiohandle_data gpio_out_data, gpio_in_data;

/*
 * wrapper for init
 */
int GPIOInit(int nout, unsigned int out_lines[], int nin, unsigned int in_lines[])
{
	/* setup output defaults to high */
	memset(&gpio_out_data, 1, sizeof(struct gpiohandle_data));
	
	/* open output with default values */
	gpio_out_fd = gpiotools_request_linehandle(RPI_GPIODEV, out_lines, 
		nout, GPIOHANDLE_REQUEST_OUTPUT, &gpio_out_data, CONSUMER);
	if(gpio_out_fd == -1)
	{
		return 1;
	}
	
	/* open input */
	gpio_in_fd = gpiotools_request_linehandle(RPI_GPIODEV, in_lines, 
		nin, GPIOHANDLE_REQUEST_INPUT, &gpio_in_data, CONSUMER);
	if(gpio_in_fd == -1)
	{
		gpiotools_release_linehandle(gpio_out_fd);
		return 1;
	}
	
	return 0;
}

/*
 * clean up
 */
void GPIOFree(void)
{
	gpiotools_release_linehandle(gpio_out_fd);
	gpiotools_release_linehandle(gpio_in_fd);	
}

/*
 * simplified wrapper for pin read
 */
int GPIORead(int pin)
{
	int ret;
	
	if((ret = gpiotools_get_values(gpio_in_fd, &gpio_in_data)) < 0)
		return ret;
	else
		return gpio_in_data.values[pin];
}

/*
 * simplified wrapper for pin write
 */
int GPIOWrite(int pin, int value)
{
	gpio_out_data.values[pin] = value;
	return gpiotools_set_values(gpio_out_fd, &gpio_out_data);
}
