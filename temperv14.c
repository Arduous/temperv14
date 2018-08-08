/*
 * temperv14.c By Steffan Slot (dev-random.net) based on the version 
 * below by Juan Carlos.
 * 
 * pcsensor.c by Juan Carlos Perez (c) 2011 (cray@isp-sl.com)
 * based on Temper.c by Robert Kavaler (c) 2009 (relavak.com)
 * All rights reserved.
 *
 * Temper driver for linux. This program can be compiled either as a library
 * or as a standalone program (-DUNIT_TEST). The driver will work with some
 * TEMPer usb devices from RDing (www.PCsensor.com).
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 * 
 * THIS SOFTWARE IS PROVIDED BY Juan Carlos Perez ''AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL Robert kavaler BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * 
 */
/*
 * Modified from original version (0.0.1) by Patrick Skillen (pskillen@gmail.com)
 * - 2013-01-10 - If outputting only deg C or dec F, do not print the date (to make scripting easier)
 */
/*
 * 2013-08-19
 * Modified from abobe version for better name, and support to 
 * subtract degrees in Celsius for some TEMPer devices that displayed
 * too much.
 *
 */
/* 2016-03-09 Modified to V0.0.2 by Roland Fritz
 * Added multiple device support (-d)
 * Added added arg (-a)
 *
 */
/* 2018-08-06 V1.1 Samuel Progin
 * Code clean-up, refactoring and formatting
 * 2 Warnings fixed
 *
 */
/* 2018-08-06 V2.0 Samuel Progin
 * Migrating from libusb to libusb-1.0
 *
 */

#include <libusb-1.0/libusb.h>
#include <stdio.h>
#include <time.h>
#include <ctype.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <signal.h> 
#include <getopt.h>

#define VERSION "2.0"

#define VENDOR_ID  0x0c45
#define PRODUCT_ID 0x7401

#define INTERFACE1 0x00
#define INTERFACE2 0x01

const static int reqIntLen = 8;
const static int timeout = 5000; /* timeout in ms */

const static char uTemperature[] = { 0x01, 0x80, 0x33, 0x01, 0x00, 0x00, 0x00,
		0x00 };

static int loopExitFlag = 1;
static int debug = 0;
static int seconds = 5;
static int format = 0;
static int mrtg = 0;
static int deviceNumber = 0;
static float delta = 0;
int totalDevices = 0;

uint8_t bmRequestType = 0x21;
uint8_t bRequest = 0x09;
uint16_t wValueControl = 0x0201;
uint16_t wIndexControl = 0x0000;
uint16_t wValueXfer = 0x0200;
uint16_t wIndexXfer = 0x0001;
uint16_t wLength = 0x0002;


void bad(const char *why) {
	fprintf(stderr, "Fatal error> %s\n", why);
	exit(17);
}


static int open_device_temperusb(libusb_device *dev, libusb_device_handle **dev_handles)
{
	struct libusb_device_descriptor desc;
	int r = libusb_get_device_descriptor(dev, &desc);
	if (r < 0) {
		fprintf(stderr, "failed to get device descriptor");
		return 0;
	}
	if (desc.idVendor == VENDOR_ID && desc.idProduct == PRODUCT_ID){
		r = libusb_open(dev, dev_handles);
		return 1;
	}
	return 0;
}



void ex_program(int sig) {
	loopExitFlag = 1;

	(void) signal(SIGINT, SIG_DFL);
}

int main(int argc, char **argv) {

	float tempc;
	int c;
	struct tm *local;
	time_t t;
	int i;
	int devNo = 1;
	//libusb_device_handle **dev_handle[10];
	libusb_device_handle *dev_handle;

	libusb_device **devs;
	libusb_context *ctx = NULL;
	int r;
	ssize_t cnt;

	unsigned char question[reqIntLen];

	int temperature;
	unsigned char answer[reqIntLen];

	while ((c = getopt(argc, argv, "mfcvha::d::l::")) != -1)
			switch (c) {
			case 'v':
				debug = 1;
				break;
			case 'c':
				format = 1; //Celsius
				break;
			case 'f':
				format = 2; //Fahrenheit
				break;
			case 'm':
				mrtg = 1;
				break;
			case 'd':
				if (optarg != NULL) {
					if (1 == !sscanf(optarg, "%i", &deviceNumber)) {
						fprintf(stderr, "Error: '%s' is not numeric.\n", optarg);
						exit(EXIT_FAILURE);
					}
				} else {
					deviceNumber = 0;
				}
				break;
			case 'a':
				if (optarg != NULL) {
					if (1 == !sscanf(optarg, "%f", &delta)) {
						fprintf(stderr, "Error: '%s' is not numeric.\n", optarg);
						exit(EXIT_FAILURE);
					}
				}
				break;
			case 'l':
				if (optarg != NULL) {
					if (1 == !sscanf(optarg, "%i", &seconds)) {
						fprintf(stderr, "Error: '%s' is not numeric.\n", optarg);
						exit(EXIT_FAILURE);
					} else {
						loopExitFlag = 0;
						break;
					}
				} else {
					loopExitFlag = 0;
					seconds = 5;
					break;
				}
			case '?':
			case 'h':
				printf("TemperUSB reader version %s\n", VERSION);
				printf("      Available options:\n");
				printf("          -h help\n");
				printf("          -v verbose\n");
				printf(
						"          -l[n] loop every 'n' seconds, default value is 5s\n");
				printf(
						"          -a[n.n] add a delta of 'n.n' degrees Celsius (may be negative)\n");
				printf("          -c output only in Celsius\n");
				printf("          -f output only in Fahrenheit\n");
				printf("          -m output for mrtg integration\n");
				printf("          -d[n] show only device 'n'\n");

				exit(EXIT_FAILURE);
			default:
				if (isprint(optopt))
					fprintf(stderr, "Unknown option `-%c'.\n", optopt);
				else
					fprintf(stderr, "Unknown option character `\\x%x'.\n", optopt);
				exit(EXIT_FAILURE);
			}

		if (optind < argc) {
			fprintf(stderr, "Non-option ARGV-elements, try -h for help.\n");
			exit(EXIT_FAILURE);
		}

	r = libusb_init(&ctx);
	if (r < 0)
		return r;

	libusb_set_debug(ctx, 3);

	cnt = libusb_get_device_list(ctx, &devs);
	if (cnt < 0)
		return (int) cnt;
	for (i = 0; i < cnt; i++){
		totalDevices += open_device_temperusb(devs[i], &dev_handle);
	}

	//dev_handle = libusb_open_device_with_vid_pid(ctx, VENDOR_ID, PRODUCT_ID);

	for (i=0; i < totalDevices; i++){

		r = libusb_claim_interface(dev_handle, 0);
		if (r < 0){
			if (r == LIBUSB_ERROR_BUSY){
				libusb_detach_kernel_driver(dev_handle, 0);
				}
			else
				return r;
			}

		r = libusb_claim_interface(dev_handle, 1);
			if (r < 0){
				if (r == LIBUSB_ERROR_BUSY){
					libusb_detach_kernel_driver(dev_handle, 1);
				}
				else
					return r;
			}

	}
	libusb_free_device_list(devs, cnt);

	do {
		(void) signal(SIGINT, ex_program);
		for (i=0; i < totalDevices; i++){
			r = libusb_control_transfer(dev_handle, bmRequestType, bRequest, wValueControl, wIndexControl, question, wLength, timeout);

			r = libusb_control_transfer(dev_handle, bmRequestType, bRequest, wValueXfer, wIndexXfer, (unsigned char *) uTemperature, reqIntLen, timeout);
			bzero(answer, reqIntLen);
			r = libusb_interrupt_transfer(dev_handle, 0x82, answer, reqIntLen, NULL, timeout);

			temperature = (answer[3] & 0xFF) + (answer[2] << 8);
			tempc = temperature * (125.0 / 32000.0);

			tempc = (tempc + delta);

				t = time(NULL);
				local = localtime(&t);

				if (mrtg) {
					if (format == 2) {
						printf("%.2f\n", (9.0 / 5.0 * tempc + 32.0));
						printf("%.2f\n", (9.0 / 5.0 * tempc + 32.0));
					} else {
						printf("%.2f\n", tempc);
						printf("%.2f\n", tempc);
					}

					printf("%02d:%02d\n", local->tm_hour, local->tm_min);

					printf("pcsensor\n");
				} else {
					if (format == 2) {
						printf("%.2f\n", (9.0 / 5.0 * tempc + 32.0));
					} else if (format == 1) {
						printf("%.2f\n", tempc);
					} else {
						printf("%04d/%02d/%02d %02d:%02d:%02d ",
								local->tm_year + 1900, local->tm_mon + 1,
								local->tm_mday, local->tm_hour, local->tm_min,
								local->tm_sec);

						printf("Device %d Temperature %.2fF %.2fC\n", (devNo - 1),
								(9.0 / 5.0 * tempc + 32.0), tempc);
					}

				}
		}
			if (!loopExitFlag)
				sleep(seconds);
	} while (!loopExitFlag);

	for (i=0; i < totalDevices; i++){
		r = libusb_release_interface(dev_handle, 0); //release the claimed interface
		r = libusb_release_interface(dev_handle, 1); //release the claimed interface
		libusb_close(dev_handle);

	}

	libusb_exit(ctx);
	return 0;



/*



	do {

		int i = 1;


		while ((lvr_winusb = setup_libusb_access(i)) != NULL) {
			if ((deviceNumber == i++) | (deviceNumber == 0)) {

				(void) signal(SIGINT, ex_program);

				ini_control_transfer(lvr_winusb);

				control_transfer(lvr_winusb, uTemperature);
				interrupt_read(lvr_winusb);

				control_transfer(lvr_winusb, uIni1);
				interrupt_read(lvr_winusb);

				control_transfer(lvr_winusb, uIni2);
				interrupt_read(lvr_winusb);
				interrupt_read(lvr_winusb);

				control_transfer(lvr_winusb, uTemperature);
				interrupt_read_temperature(lvr_winusb, &tempc);


				usb_release_interface(lvr_winusb, INTERFACE1);
				usb_release_interface(lvr_winusb, INTERFACE2);
			}
			usb_close(lvr_winusb);
		}
		if (!loopExitFlag)
			sleep(seconds);
	} while (!loopExitFlag);
*/
	return 0;
}
