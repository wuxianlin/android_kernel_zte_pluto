/**********************************************************************************
 * Si8240 Linux Driver
 *
 * Copyright (C) 2011-2012 Silicon Image Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation version 2.
 *
 * This program is distributed .as is. WITHOUT ANY WARRANTY of any
 * kind, whether express or implied; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE.  See the
 * GNU General Public License for more details.
 *
 **********************************************************************************/

/**
 * @file siidvrtest.c
 *
 * @brief
 *   The MHL TX device driver simple test application
 *
 *****************************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <errno.h>
#include <sys/uio.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <pthread.h>

#include "mhl_linuxdrv_ioctl.h"


// Convert a value specified in milliseconds to nanoseconds
#define MSEC_TO_NSEC(x)	(x * 1000000UL)

// Define interval used to poll driver for events to 40ms.
#define POLL_INTERVAL_MS	40

// MHL defined timeout between RCP messages.
#define T_RCP_WAIT_MS		1000


#define KEYCODE_STR_SIZE 	6


static pthread_mutex_t			cmdLock = PTHREAD_MUTEX_INITIALIZER;
static uint8_t					gControlFlags = 0;
//static uint8_t					pollThreadExitFlag = 0;
static uint8_t					sendKeyCode = 0xFF;
//static uint8_t					devCapReadOffset = 0xFF;
static mhlTxReadDevCap_t		gMhlReadDevCapPacket;
static mhlTxScratchPadAccess_t	gMhlScratchPadAccessPacket;

// controlFlags definitions
#define CF_EXIT					0x01	// inform polling thread to exit
#define CF_DEVCAP_CMD			0x02	// read value of a Device Capabilities register
#define CF_SCRATCH_PAD_READ		0x04	// read from Scratch Pad register(s)
#define CF_SCRATCH_PAD_WRITE	0x08	// write to Scratch Pad register(s)


// State of MHL connection
static uint8_t	mhlState = 0;
#define MHL_STATE_CONNECTED	0x01
#define MHL_STATE_RCP_READY	0x02
#define MHL_STATE_RCP_IP		0x04
#define MHL_STATE_RCP_SEND_ACK	0x08
#define MHL_STATE_RCP_NACK_RECV	0x10


// Function executed by driver event polling thread.
void* PollDriverEvents(void *ptr);


int gMhlDrv;


// Utility function to retrieve one or more values entered
// on the command line.
void getCmdLineParams(long int *valuesArray, uint8_t *arrayLen)
{
	int			idx = 0;
	char		*token;


	token = strtok(NULL, " ");
	while((idx < *arrayLen) && token != NULL) {

		valuesArray[idx] = strtol(token, NULL, 0);
		idx++;
		token = strtok(NULL, " ");
	}

	*arrayLen = idx;		// return number of parameters successfully parsed
}



int main(int argc, char **argv) 
{
	pthread_t	pollThread;
	int			status;
	int			idx;
	char		*token;
	char		*cmdLine;
	int			cmdLineLen;
	int			bytesEntered;
	long int	paramArray[20];
	uint8_t		paramArrayLen;
	bool		bExit = false;
	bool		bShowCmds;
	char		mhl_linuxdrv_name[20] = "/dev/siI-833x";


	printf("Welcome to the Silicon Image Transmitter test application\n");

	// Parse the command line
	idx = 1;
	while(idx < argc) {

		// check for test device name switch
		if(strncmp("-d", argv[idx], 2) == 0) {
			idx++;
			if(idx >= argc) {
				idx = 0;
				break;
			}

			if(strlen(argv[idx]) > 14) {
				idx = 0;
				break;
			} else {
				sprintf(mhl_linuxdrv_name, "/dev/%s", argv[idx]);
			}
			idx++;
			continue;
		}

		// Oh Oh, invalid command line detected.
		idx = 0;
		break;
	}

	if(idx == 0) {
		printf("Invalid command line detected!\n");
		printf("Usage:\n");
		printf("  %s [-d device_name]\n\n", argv[0]);
		printf("  -d\n");
		printf("      replace the default name of the device to be tested with\n");
		printf("      device_name\n");
		return 0;
	}

	gMhlDrv = open(mhl_linuxdrv_name, O_RDONLY);
  
	if (gMhlDrv < 0)
	{
		printf("Error opening %s!\n", mhl_linuxdrv_name);
		return errno;
	}

	cmdLineLen = 20;
	cmdLine = (char *)malloc(cmdLineLen);

	status = pthread_create(&pollThread, NULL, PollDriverEvents, NULL);

	bShowCmds = true;
	do {

		if(bShowCmds) {
			printf("Command key codes:\n");
			printf("    E                              = exit program\n");
			printf("    S <key code>                   = send remote control code\n");
			printf("    D <reg offset>                 = read Device Capability register\n");
			printf("    W <reg offset> <reg_value(s)>  = write specified values to scratch pad\n");
			printf("                                     registers starting at <reg offset>\n");
			printf("    R <reg offset> <count>         = read <count> number of scratch pad\n");
			printf("                                     registers starting at <reg offset>\n");
			bShowCmds = false;
		}

		bytesEntered = getline(&cmdLine, &cmdLineLen, stdin);

		token = strtok(cmdLine, " ");
		if(token == NULL)
			break;

		switch(toupper(token[0])) {
			case 'E':			// Exit program
				bExit = true;
				break;

			case 'S':			// Send an RCP key code
				paramArrayLen = 2;
				getCmdLineParams(paramArray, &paramArrayLen);

				if(paramArrayLen > 1) {
					printf("Too many parameters for command!");
					bShowCmds = true;

				} else if(paramArray[0] > 0xFF) {
					printf("Invalid key code 0x%lx entered!\n",
							paramArray[0]);
				} else {

					pthread_mutex_lock(&cmdLock);
					sendKeyCode = (uint8_t)paramArray[0];
					pthread_mutex_unlock(&cmdLock);
				}
				break;

			case 'D':		// Read a Device Capability register
				paramArrayLen = 2;
				getCmdLineParams(paramArray, &paramArrayLen);

				if(paramArrayLen > 1) {
					printf("Too many parameters for command!");
					bShowCmds = true;

				} else if(paramArray[0] > 0x0F) {
					printf("Invalid register offset: 0x%lx entered!\n",
							paramArray[0]);

				} else {
						pthread_mutex_lock(&cmdLock);
						gMhlReadDevCapPacket.regNum = (uint8_t)paramArray[0];
						gMhlReadDevCapPacket.regValue = 0;
						gControlFlags |= CF_DEVCAP_CMD;
						pthread_mutex_unlock(&cmdLock);
				}
				break;

			case 'W':		// Write to the Scratch Pad registers
printf("case W:\n");
				paramArrayLen = 18;
				getCmdLineParams(paramArray, &paramArrayLen);

				if((paramArrayLen < 2) || (paramArrayLen > 17)) {
					printf("Wrong number of parameters for command!");
					bShowCmds = true;

				} else if(paramArray[0] > 0x3F) {
					printf("Invalid register offset: 0x%lx entered!\n",
							paramArray[0]);

				} else if(paramArray[0] + paramArrayLen > 0x40) {
					printf("Parameters exceed scratch pad register's"\
							"address range!\n");

				} else {
					for(idx = 1; idx < paramArrayLen; idx++) {
						if(paramArray[idx] > 0xFF) {
							printf("Register value #%d (0x%lx) is too large!\n",
									idx, paramArray[idx]);
							break;
						}
					}
					if(idx < paramArrayLen)
						break;

					pthread_mutex_lock(&cmdLock);

					gMhlScratchPadAccessPacket.offset = paramArray[0];
					gMhlScratchPadAccessPacket.length = paramArrayLen -1;
					for(idx = 1; idx < paramArrayLen; idx++) {
						gMhlScratchPadAccessPacket.data[idx - 1] = (uint8_t)paramArray[idx];
					}
					gControlFlags |= CF_SCRATCH_PAD_WRITE;

					pthread_mutex_unlock(&cmdLock);
				}
				break;

			case 'R':			// Read from the Scratch Pad registers
printf("case R:\n");
				paramArrayLen = 3;
				getCmdLineParams(paramArray, &paramArrayLen);

				if(paramArrayLen != 2) {
					printf("Wrong number of parameters for command!");
					bShowCmds = true;

				} else if(paramArray[0] >= MAX_SCRATCH_PAD_TRANSFER_SIZE) {
					printf("Invalid register offset: 0x%lx entered!\n",
							paramArray[0]);

				} else if(paramArray[1] > 0x10) {
					printf("Invalid register count: %ld entered!\n",
							paramArray[1]);

				} else if(paramArray[0] + paramArray[1] > MAX_SCRATCH_PAD_TRANSFER_SIZE) {
					printf("Transfer parameters exceed Scratch Pad "\
						   "register address space\n");

				} else {
					pthread_mutex_lock(&cmdLock);

					gMhlScratchPadAccessPacket.offset = paramArray[0];
					gMhlScratchPadAccessPacket.length = paramArray[1];
					for(idx = 0; idx < MAX_SCRATCH_PAD_TRANSFER_SIZE; idx++) {
						gMhlScratchPadAccessPacket.data[idx] = 0;
					}
					gControlFlags |= CF_SCRATCH_PAD_READ;

					pthread_mutex_unlock(&cmdLock);
				}
				break;

			default:
				printf("Unrecognized command entered!\n\n");
				bShowCmds = true;
				break;
		}

	} while(bExit == false);

	if(cmdLine != NULL)
		free(cmdLine);

  // Signal driver event polling thread to quit.
  gControlFlags |= CF_EXIT;
  pthread_join(pollThread, NULL);

  close(gMhlDrv);
  return 0;
}



void* PollDriverEvents(void *ptr)
{
	struct timespec		ts;
	mhlTxEventPacket_t	mhlEventPacket;
	uint8_t				keyCodeInProcess;
	uint16_t			rcpTimeout;
	uint8_t				retryCount = 0;
	uint8_t				idx;
	int					status;


	ts.tv_sec = 0;
	ts.tv_nsec = MSEC_TO_NSEC(POLL_INTERVAL_MS);


	// Determine the current state of the MHL connection.
	status = ioctl(gMhlDrv,SII_MHL_GET_MHL_CONNECTION_STATUS, &mhlEventPacket );

	if(status < 0)
	{
		printf("SII_MHL_GET_MHL_CONNECTION_STATUS ioctl failed, status: %d\n", errno);
		mhlState = 0;
	}
	else
	{
		if(mhlEventPacket.event == MHL_TX_EVENT_RCP_READY) {
			mhlState = MHL_STATE_CONNECTED | MHL_STATE_RCP_READY;

		} else if(mhlEventPacket.event == MHL_TX_EVENT_CONNECTION) {
			mhlState = MHL_STATE_CONNECTED;

		} else {
			// Assume no MHL connection currently exists.
			mhlState = 0;
		}
	}

	while(!(gControlFlags & CF_EXIT))
	{
		nanosleep(&ts, NULL);

		if(mhlState & MHL_STATE_RCP_IP)
		{
			// Check if outstanding RCP request has timed out.
			if(rcpTimeout > POLL_INTERVAL_MS)
			{
				rcpTimeout -= POLL_INTERVAL_MS;
			}
			else
			{
				// Request has timed out.  We are allowed to retry the request
				// but we don't.  Instead just reset our state to indicate that
				// there is no longer an RCP request in process.
				mhlState &= (MHL_STATE_CONNECTED | MHL_STATE_RCP_READY);
				printf("Attempt to send RCP keycode: 0x%02x timed out\n",
						keyCodeInProcess);
			}
		}

		status = ioctl(gMhlDrv,SII_MHL_GET_MHL_TX_EVENT, &mhlEventPacket );

		if(status < 0)
		{
			printf("SII_MHL_GET_MHL_TX_EVENT ioctl failed, status: %d\n", errno);
		}
		else
		{
			pthread_mutex_lock(&cmdLock);

			switch(mhlEventPacket.event)
			{
				case MHL_TX_EVENT_NONE:				// No new events
					break;

				case MHL_TX_EVENT_DISCONNECTION:	// MHL connection has been lost
					printf("Disconnection event received\n");
					mhlState = 0;
					retryCount = 0;
					break;

				case MHL_TX_EVENT_CONNECTION:		// MHL connection has been established
					printf("Connection event received\n");
					mhlState = MHL_STATE_CONNECTED;
					break;

				case MHL_TX_EVENT_RCP_READY:		// MHL connection is ready for RCP
					printf("Connection ready for RCP event received\n");
					mhlState |= MHL_STATE_RCP_READY;
					break;

				case MHL_TX_EVENT_RCP_RECEIVED:		// Received an RCP. Key Code in eventParam
					printf("RCP event received, key code: 0x%02x\n",
							mhlEventPacket.eventParam);
					// For now, we just positively acknowledge any key codes received.
					mhlState |= MHL_STATE_RCP_SEND_ACK | MHL_STATE_RCP_IP;
					break;

				case MHL_TX_EVENT_RCPK_RECEIVED:	// Received an RCPK message
					if((mhlState & MHL_STATE_RCP_IP) &&
						(mhlEventPacket.eventParam == keyCodeInProcess))
					{
						printf("RCPK received for sent key code: 0x%02x\n",
								mhlEventPacket.eventParam);
						// Received positive acknowledgment for key code
						// we sent so update our state to indicate that the
						// key code send is complete.
						mhlState &= ~(MHL_STATE_RCP_IP | MHL_STATE_RCP_NACK_RECV);
					}
					else
					{
						printf("Unexpected RCPK event received, key code: 0x%02x\n",
								mhlEventPacket.eventParam);
					}
					break;

				case MHL_TX_EVENT_RCPE_RECEIVED:	// Received an RCPE message
					if(mhlState & MHL_STATE_RCP_IP)
					{
						printf("RCPE status 0x%02x received for sent key code: 0x%02x\n",
								mhlEventPacket.eventParam, keyCodeInProcess);
						// Remember the key code we sent was rejected by the
						// downstream device.
						mhlState |= MHL_STATE_RCP_NACK_RECV;

						// Reset the timeout to wait for the RCPK we should
						// get from the downstream device.
						rcpTimeout = T_RCP_WAIT_MS;
					}
					else
					{
						printf("Unexpected RCPE event received\n");
					}
					break;

				default:
					printf("Unknown event code: %d \n", mhlEventPacket.event);
			}

			if(mhlState & MHL_STATE_RCP_SEND_ACK)
			{
				status = ioctl(gMhlDrv,SII_MHL_RCP_SEND_ACK, mhlEventPacket.eventParam );

				if(status < 0)
				{
					printf("SII_MHL_RCP_SEND_ACK ioctl failed, status: %d\n", errno);
				}

				// Flag we no longer need to send an ack.
				mhlState &= ~(MHL_STATE_RCP_SEND_ACK | MHL_STATE_RCP_IP);
			}
			else if((mhlState & (MHL_STATE_RCP_READY | MHL_STATE_RCP_IP))
				== (MHL_STATE_RCP_READY))
			{
				// MHL connection is in a state where we can initiate the
				// sending of a Remote Control Protocol (RCP) code.  So if the
				// user has entered one go ahead and send it.
				if(sendKeyCode != 0xFF)
				{
					status = ioctl(gMhlDrv,SII_MHL_RCP_SEND, sendKeyCode );
					if(status < 0)
					{
						printf("SII_MHL_RCP_SEND ioctl failed, status: %d\n", errno);
					}
					else
					{
						// Flag we're in the process of sending a RCP code.
						mhlState |= MHL_STATE_RCP_IP;
						keyCodeInProcess = sendKeyCode;
						rcpTimeout = T_RCP_WAIT_MS;
					}
					sendKeyCode = 0xFF;

				} else if(gControlFlags & CF_DEVCAP_CMD) {

					// Perform requested read of a Device Capability register.
					status = ioctl(gMhlDrv,SII_MHL_GET_DEV_CAP_VALUE, &gMhlReadDevCapPacket);

					if(status == 0) {
						printf("Device Capability register %02x = %02x\n",
								gMhlReadDevCapPacket.regNum,
								gMhlReadDevCapPacket.regValue);

						gControlFlags &= ~CF_DEVCAP_CMD;
						retryCount = 0;

					} else if(errno == EAGAIN) {

						retryCount++;
						if(retryCount < 4) {
							printf("Retrying Device Capability register read\n");

						} else {
							printf("Device Capability register read failed, "\
								   "retries exhausted\n");
							retryCount = 0;
						}

					} else {
						printf("Device Capability register read failed, status: %d\n",
								errno);
						gControlFlags &= ~CF_DEVCAP_CMD;
						retryCount = 0;
					}

				} else if(gControlFlags & CF_SCRATCH_PAD_READ) {

					// Perform requested read of the Scratch Pad register(s).
					status = ioctl(gMhlDrv,SII_MHL_READ_SCRATCH_PAD, &gMhlScratchPadAccessPacket);

					if(status == 0) {

						printf("Scratch Pad register data read");
						for(idx = 0; idx < gMhlScratchPadAccessPacket.length; idx++) {
							if(idx == 0 || idx == 8) {
								printf("\nOffset 0x%02x: ",
										gMhlScratchPadAccessPacket.offset + idx);
							}
							printf("  0x%02x", gMhlScratchPadAccessPacket.data[idx]);
						}
						printf("\n");

						gControlFlags &= ~CF_SCRATCH_PAD_READ;
						retryCount = 0;

					} else if(errno == EAGAIN) {

						retryCount++;
						if(retryCount < 4) {
							printf("Retrying Scratch Pad register read\n");

						} else {
							printf("Scratch Pad register read failed, "\
								   "retries exhausted\n");
							retryCount = 0;
						}

					} else {
						printf("Scratch Pad register read failed, status: %d\n",
								errno);
						gControlFlags &= ~CF_SCRATCH_PAD_READ;
						retryCount = 0;
					}

				} else if(gControlFlags & CF_SCRATCH_PAD_WRITE) {

					// Perform requested write of the Scratch Pad register(s).
					status = ioctl(gMhlDrv,SII_MHL_WRITE_SCRATCH_PAD, &gMhlScratchPadAccessPacket);

					if(status == 0) {

						printf("Scratch Pad register write successful\n");

						gControlFlags &= ~CF_SCRATCH_PAD_WRITE;
						retryCount = 0;

					} else if(errno == EAGAIN) {

						retryCount++;
						if(retryCount < 4) {
							printf("Retrying Scratch Pad register write\n");

						} else {
							printf("Scratch Pad register write failed, "\
								   "retries exhausted\n");
							gControlFlags &= ~CF_SCRATCH_PAD_WRITE;
							retryCount = 0;
						}

					} else {
						printf("Scratch Pad register write failed, status: %d\n",
								errno);
						gControlFlags &= ~CF_SCRATCH_PAD_WRITE;
						retryCount = 0;
					}
				}
			}

			pthread_mutex_unlock(&cmdLock);
		}
	}

	return NULL;
}
