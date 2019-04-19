/*
 * temperv14.c By Steffan Slot (dev-random.net) based on the version 
 * below by Juan Carlos.
 * 
 * pcsensor.c by Juan Carlos Perez (c) 2011 (cray@isp-sl.com)
 * based on Temper.c by Robert Kavaler (c) 2009 (relavak.com)
 * All rights reserved.
 *
 * Temper driver for linux. The driver will work with
 * TEMPerUSB V1.4 devices from RDing (www.PCsensor.com).
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

static unsigned char uTemperature[] = { 0x01, 0x80, 0x33, 0x01, 0x00, 0x00, 0x00, 0x00 };

static int loopExitFlag = 1;
static int debug = 0;
static int seconds = 5;
static int format = 0;
static int mrtg = 0;
static int deviceNumber = -1;
static float delta = 0;
int currentDevice = 0;

uint8_t bmRequestType = 0x21;
uint8_t bRequest = 0x09;
uint16_t wValueControl = 0x0201;
uint16_t wIndexControl = 0x0000;
uint16_t wValueXfer = 0x0200;
uint16_t wIndexXfer = 0x0001;
uint16_t wLength = 0x0002;


static int detect_temper_device(libusb_device *dev)
/**
 * Identify if a given USB device is a TEMPerUSB v1.4
 *\param dev device to be identified
 *\returns 1 if the device is recognized, 0 otherwise
 */
{
	struct libusb_device_descriptor desc;
	int r = libusb_get_device_descriptor(dev, &desc);
	if (r < 0) {
		if (0 != debug) {fprintf(stdout, "Failure do get device descriptor\n");}
		return 0;
	}
	if (0 != debug) {fprintf(stdout, "VendorID: %04X ProductID: %04X\n", desc.idVendor, desc.idProduct);}
	if (desc.idVendor == VENDOR_ID && desc.idProduct == PRODUCT_ID){
		if (0 != debug) {fprintf(stdout, "Device recognized\n");}
		return 1;
	}
	return 0;
}

static int claim_temper_interface(libusb_device_handle *dev_handle)
/**
 * Wrapper around libusb_claim_interface for the two interfaces
 * of the TEMPerUSB device
 *\param dev_handle libusb_dev_handle on the TEMPerUSB
 *\returns libusb_claim_interface error code
 */
{
	int r = 0;
	for (int i = 0; i < 2; i++){
		r = libusb_claim_interface(dev_handle, i);
		if (r < 0){
			if (r == LIBUSB_ERROR_BUSY){
				libusb_detach_kernel_driver(dev_handle, i);
				r = libusb_claim_interface(dev_handle, i);
			}
		}
	}
	if ((0 != debug) && (0 != r)) {fprintf(stdout, "USB interface could not be claimed. Permission issue?\n");}
	return r;
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
	int r;

	libusb_device **devs;
	libusb_context *ctx = NULL;
	ssize_t cnt;

	unsigned char question[reqIntLen];
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
				deviceNumber = -1;
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

	libusb_init(&ctx);

	libusb_set_option(ctx, LIBUSB_OPTION_LOG_LEVEL, 3 + debug);

	// Loop around continuous polling
	do {
		libusb_device_handle *dev_handle = NULL;

		(void) signal(SIGINT, ex_program);

		cnt = libusb_get_device_list(ctx, &devs);
		if (cnt < 0)
			return (int) cnt;
		currentDevice = 0;
		// loop on all USB devices found
		for (i = 0; i < cnt; i++){
			// check if the USB device is a TemperUSB device
			if (1 == detect_temper_device(devs[i])){
				// Are we polling all devices one by one or just a specific one
				if ((-1 == deviceNumber) || (currentDevice == deviceNumber)){
					r = libusb_open(devs[i], &dev_handle);
					if ((0 != debug) && (0 != r)) {
						fprintf(stdout, "libusb_open error %d?\n", r);
					}
					claim_temper_interface(dev_handle);

					r = libusb_control_transfer(dev_handle, bmRequestType, bRequest, wValueControl, wIndexControl, question, wLength, timeout);
					if ((0 != debug) && (0 > r)) {fprintf(stdout, "Error code on configuring the device %d?\n", r);}
					r = libusb_control_transfer(dev_handle, bmRequestType, bRequest, wValueXfer, wIndexXfer, uTemperature, reqIntLen, timeout);
					if ((0 != debug) && (0 > r)) {fprintf(stdout, "Error code on requesting the transfer %d?\n", r);}
					bzero(answer, reqIntLen);
					r = libusb_interrupt_transfer(dev_handle, 0x82, answer, reqIntLen, NULL, timeout);
					if ((0 != debug) && (0 > r)) {fprintf(stdout, "Error code on transfer %d?\n", r);}

					tempc = ((answer[3] & 0xFF) + (answer[2] << 8)) * (125.0 / 32000.0) + delta;

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

							printf("Device %d Temperature %.2fF %.2fC\n", (currentDevice),
									(9.0 / 5.0 * tempc + 32.0), tempc);
						}
					}

					// release the interfaces and the handle
					for (int i = 0; i < 2; i++) libusb_release_interface(dev_handle, 0);
					libusb_close(dev_handle);
				}
				currentDevice++;
			}
		}

		if (!loopExitFlag)
			sleep(seconds);
	} while (!loopExitFlag);

	libusb_exit(ctx);
	return 0;
}
