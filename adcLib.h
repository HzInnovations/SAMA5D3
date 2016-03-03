#include <stdlib.h> //Standard C library, without this, nothing would happen
#include <stdint.h>

#define ADC_REG_BASE				0xF8018000
#define MAP_SIZE 					4096UL
#define MAP_MASK 					(MAP_SIZE - 1)

#define WRITE_PROTECTION_OFFSET		0xE4
#define WRITE_PROTECTION_KEY		0x414443
#define ENABLE_PROTECTION			0x1
#define DISABLE_PROTECTION			0

#define ENABLE_ADC					0x2

#define ADC_CONTROL_OFFSET			0
#define ADC_MODE_OFFSET				0x4
#define ADC_DEFAULT_SPEED			0xB012000

#define ADC_TRGR_OFFSET				0xC0
#define ADC_CONTINUOUS				0x6
#define ADC_SOFT_TRGR				0
#define ADC_PERIODIC_TRGR			0x5

//TRG speed defined by a 16 bit register
//The higher the value the lower the period
//TRGPER = (16 bit value + 1) / ADCCLK

//Data is at (0x50 + 0x4*channel)
#define ADC_DATA_OFFSET				0x50

//to enable channel write (1 << (channel))
#define ADC_OFFSET_OFFSET			0x4C

//single ended only supported;
//gain supported; 1 , 2 and 4
//write 1 for 1x, 2 for 2x, and 3 for 4x
#define ADC_GAIN_OFFSET				0x48

//write 1 << Channel
#define ADC_ENABLE_OFFSET			0x10
#define ADC_DISABLE_OFFSET			0x14

#define ADC_STATUS_OFFSET			0x18


extern int initADC(int channel);

extern void closeADC(int channel);

extern int shutdownADC(void);

extern unsigned long readADC(int channel);

extern void setGain(int channel, int gain);

extern void setADCModeReg(unsigned long speed);

extern int getGain(int channel);

extern unsigned long getADCModeReg(void);

extern int isEnabled(int channel);

//use ADC_CONTINUOUS or ADC_SOFT_TRGR
extern void setMode(int option);


/*Stuff is gonna get fun here:
 * 
 * The ADC_MR register is powerful and has many functions.
 * 
 * TRANSFER in bits 28 and 29 should always be set;
 * as per the datasheet.
 * 
 * TRACKTIM in bits 24 - 27 do not touch
 * 
 * ANACH on bit 23 allows different settings on different
 * pins; will enable
 * 
 * SETTLING on bit 20 and 21 state for how long the ADC
 * settles for after it has charged its capacitor
 * 0 = 3 Clock Cycles
 * 1 = 5 Clock Cycles
 * 2 = 9 Clock Cycles
 * 3 = 17 Clock Cycles
 * 
 * This can be set by the user. Default will be 1
 * 
 * Bits 8 through 19 control the speed of the ADC
 * 16 - 19 control the startup time:
 * 
 * 
 * 	0 		SUT0 		0 periods of ADCCLK
	1 		SUT8 		8 periods of ADCCLK
	2 		SUT16 		16 periods of ADCCLK
	3 		SUT24 		24 periods of ADCCLK
	4 		SUT64 		64 periods of ADCCLK
	5 		SUT80 		80 periods of ADCCLK
	6 		SUT96 		96 periods of ADCCLK
	7		SUT112 		112 periods of ADCCLK
	8 		SUT512 		512 periods of ADCCLK
	9 		SUT576 		576 periods of ADCCLK
	10 		SUT640 		640 periods of ADCCLK
	11 		SUT704 		704 periods of ADCCLK
	12 		SUT768 		768 periods of ADCCLK
	13 		SUT832 		832 periods of ADCCLK
	14 		SUT896 		896 periods of ADCCLK
	15 		SUT960 		960 periods of ADCCLK
 * 
 * I have used a value of '1' without much issue.
 * 
 * Will leave it open to be set.
 * 
 * 
 * Bits 8-15 set the prescaler. The formula is:
 * PRESCAL = (f(peripheral clock) / (2 × f(ADCCLK) )) – 1.
 * 
 * what this means is the higher PRESCAL, the lower
 * the speed.
 * 
 * By default it is set to its lowest setting.
 * 
 * Datasheet max is ~ 1 MSPS; thats with a SUT0; 3 clock
 * settling time, continuous conversion and PRESCAL set
 * to 0. Will leave it open also; but the defaults will
 * be set for ~ 51k samples per second.
 * 
 */
