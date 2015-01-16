#ifndef _RM31080A_CTRL_H_
#define _RM31080A_CTRL_H_

#define RM31080_REG_01 0x01
#define RM31080_REG_02 0x02
#define RM31080_REG_09 0x09
#define RM31080_REG_0E 0x0E
#define RM31080_REG_10 0x10
#define RM31080_REG_11 0x11
#define RM31080_REG_1F 0x1F
#define RM31080_REG_2F 0x2F
#define RM31080_REG_40 0x40
#define RM31080_REG_41 0x41
#define RM31080_REG_42 0x42
#define RM31080_REG_43 0x43
#define RM31080_REG_80 0x80
#define RM31080_REG_F2 0xF2
#define RM31080_REG_7E 0x7E
#define RM31080_REG_7F 0x7F

#define RM31080B1_REG_BANK0_00H		0x00
#define RM31080B1_REG_BANK0_01H		0x01
#define RM31080B1_REG_BANK0_02H		0x02
#define RM31080B1_REG_BANK0_03H		0x03
#define RM31080B1_REG_BANK0_0BH		0x0B
#define RM31080B1_REG_BANK0_0EH		0x0E
#define RM31080B1_REG_BANK0_11H		0x11


/* Tchreg.h */
/* Adaptive digital filter */
#define ADFC                                       0x10
#define FILTER_THRESHOLD_MODE                      0x08
#define FILTER_NONTHRESHOLD_MODE                   0x00

#define REPEAT_5                                   0x04
#define REPEAT_4                                   0x03
#define REPEAT_3                                   0x02
#define REPEAT_2                                   0x01
#define REPEAT_1                                   0x00

struct rm31080a_ctrl_para {

	unsigned short u16DataLength;
	unsigned short bChannelNumberX;
	unsigned short bChannelNumberY;
	unsigned short bADCNumber;
	unsigned char bfNoiseMode;
	unsigned char bNoiseRepeatTimes;
	unsigned char bfNoiseDetector;

	unsigned short u16ResolutionX;
	unsigned short u16ResolutionY;

	unsigned char bICVersion;

	/* Marty added */
	unsigned char bChannelDetectorNum;	/* Noise_Detector */
	unsigned char bChannelDetectorDummy;	/* Noise_Detector */
	signed char bNoiseThresholdMax;	/* Noise_Detector */
	signed char bNoiseThresholdMin;	/* Noise_Detector */
	signed char bNoiseThresholdLowMax;	/* Noise_Detector */
	signed char bNoiseThresholdLowMin;	/* Noise_Detector */
	unsigned char bBaselineReady;	/* Noise_Detector */
	unsigned char bNoiseDetectThd;	/* Noise_Detector */
	unsigned char bNewNoiseRepeatTimes;
	unsigned char bRepeatTimes[2];	/* Noise_Detector */
	unsigned char bIdleRepeatTimes[2];
	unsigned char bSenseNumber;	/* Noise_Detector */
	unsigned char bfADFC;	/* Noise_Detector */
	unsigned char bfTHMode;	/* Noise_Detector */
	unsigned char bfAnalogFilter;	/* Noise_Detector */
	unsigned char bXChannel[4];
	unsigned char bYChannel[2];
	unsigned char bDummyChannel[4];
	unsigned char bfNoiseModeDetector;
	unsigned char bfMediumFilter;
	unsigned char bMFBlockNumber;
	unsigned char bfSuspendReset;
	unsigned char bPressureResolution;
	unsigned char bfNoisePreHold;
	unsigned char bfTouched;
	unsigned char bSTScan;
	signed char bMTTouchThreshold;
	unsigned char bfExitNoiseMode;
	unsigned char bNoisePipelineBase;
	unsigned char bTime2Idle;
	unsigned char bfPowerMode;
	unsigned char bfIdleMessage;
	unsigned char bDummyRunCycle;
	unsigned char bfIdleModeCheck;

};

extern struct rm31080a_ctrl_para g_stCtrl;

int rm31080_ctrl_clear_int(void);
int rm31080_ctrl_scan_start(void);
int rm31080_ctrl_read_raw_data(unsigned char *p);
void rm31080_ctrl_wait_for_scan_finish(void);

void rm31080_ctrl_init(void);
void rm31080_ctrl_set_baseline(void *arg);
unsigned char rm31080_ctrl_get_noise_mode(unsigned char *p);
unsigned char rm31080_ctrl_get_idle_mode(unsigned char *p);
void rm31080_ctrl_get_parameter(void *arg);
int rm31080_soft_average(signed char *pSource);
int rm_noise_detect(signed char *pSource);
int rm_noise_main(signed char *pSource);

void rm_set_repeat_times(u8 u8Times);
#endif	/* _RM31080A_CTRL_H_ */
