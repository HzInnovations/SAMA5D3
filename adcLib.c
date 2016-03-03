#include <sys/mman.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "adcLib.h"

void *virt_addr;
int fd;
void *map_base;

int swTrigger = 0;

int initADC(int channel){
	
	
	if((fd = open("/dev/mem", O_RDWR | O_SYNC)) == -1){
		
		return 0;		

	}

	system("i2cset -y 1 0x5b 0x54 0x39");
	system("i2cset -y 1 0x5b 0x55 0xc1");

	map_base = mmap(NULL, MAP_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, ADC_REG_BASE & ~MAP_MASK);
	virt_addr = map_base + (ADC_REG_BASE & MAP_MASK);

	*((unsigned long *) (virt_addr + WRITE_PROTECTION_OFFSET)) = (WRITE_PROTECTION_KEY << 2) + DISABLE_PROTECTION;
	*((unsigned long *) (virt_addr + ADC_MODE_OFFSET)) = ADC_DEFAULT_SPEED;
	*((unsigned long *) (virt_addr + ADC_GAIN_OFFSET)) = 0x0;
	*((unsigned long *) (virt_addr + ADC_OFFSET_OFFSET)) = 0x0;
	*((unsigned long *) (virt_addr + ADC_ENABLE_OFFSET)) = 1 << channel;
	*((unsigned long *) (virt_addr + ADC_TRGR_OFFSET)) = ADC_CONTINUOUS;
	*((unsigned long *) (virt_addr + WRITE_PROTECTION_OFFSET)) = (WRITE_PROTECTION_KEY << 2) + ENABLE_PROTECTION;
	
	return 1;

	/*else if(opt == 1){

		if(munmap(map_base, MAP_SIZE) == -1){
		
			return 0;
		
		}
		close(fd);

	}
	return 1;*/
}

void closeADC(int channel){

	*((unsigned long *) (virt_addr + WRITE_PROTECTION_OFFSET)) = (WRITE_PROTECTION_KEY << 2) + DISABLE_PROTECTION;
	*((unsigned long *) (virt_addr + ADC_DISABLE_OFFSET)) = 1 << channel;
	*((unsigned long *) (virt_addr + WRITE_PROTECTION_OFFSET)) = (WRITE_PROTECTION_KEY << 2) + ENABLE_PROTECTION;

}

int shutdownADC(void){

	if(munmap(map_base, MAP_SIZE) == -1){
		
			return 0;
		
	}
	close(fd);
	return 1;

}

unsigned long readADC(int channel){

	if(swTrigger = 0){
		unsigned long read;
		read = *((unsigned long *) (virt_addr + ADC_DATA_OFFSET+ (4*channel)));
		return read;
	}
	else{
		unsigned long read;
		*((unsigned long *) (virt_addr + WRITE_PROTECTION_OFFSET)) = (WRITE_PROTECTION_KEY << 2) + DISABLE_PROTECTION;
		*((unsigned long *) (virt_addr + ADC_CONTROL_OFFSET)) = 0x2;
		read = *((unsigned long *) (virt_addr + ADC_DATA_OFFSET + (4*channel)));
		*((unsigned long *) (virt_addr + WRITE_PROTECTION_OFFSET)) = (WRITE_PROTECTION_KEY << 2) + ENABLE_PROTECTION;
		return read;
	}

}

void setGain(int channel, int gain){

	*((unsigned long *) (virt_addr + WRITE_PROTECTION_OFFSET)) = (WRITE_PROTECTION_KEY << 2) + DISABLE_PROTECTION;
	*((unsigned long *) (virt_addr + ADC_GAIN_OFFSET)) = gain << (channel*2);
	*((unsigned long *) (virt_addr + WRITE_PROTECTION_OFFSET)) = (WRITE_PROTECTION_KEY << 2) + ENABLE_PROTECTION;

}

void setADCModeReg(unsigned long speed){

	*((unsigned long *) (virt_addr + WRITE_PROTECTION_OFFSET)) = (WRITE_PROTECTION_KEY << 2) + DISABLE_PROTECTION;
	*((unsigned long *) (virt_addr + ADC_MODE_OFFSET)) = speed;
	*((unsigned long *) (virt_addr + WRITE_PROTECTION_OFFSET)) = (WRITE_PROTECTION_KEY << 2) + ENABLE_PROTECTION;

}

int getGain(int channel){

	unsigned long gain;
	gain = *((unsigned long *) (virt_addr + ADC_GAIN_OFFSET));
	gain = gain >> (2*channel);
	int re;
	re = (int)gain;
	return re;
}

unsigned long getADCModeReg(void){

	unsigned long adc;
	adc = *((unsigned long *) (virt_addr + ADC_MODE_OFFSET));
	return adc;

}

int isEnabled(int channel){

	unsigned long adc;
	adc = *((unsigned long *) (virt_addr + ADC_STATUS_OFFSET));
	adc = (adc >> channel) & 0x1;
	return adc;

}

//use ADC_CONTINUOUS or ADC_SOFT_TRGR
void setMode(int option){

	if(option == ADC_CONTINUOUS){
		swTrigger=0;
	}
	if(option == ADC_SOFT_TRGR){
		swTrigger = 1;
	}
	*((unsigned long *) (virt_addr + WRITE_PROTECTION_OFFSET)) = (WRITE_PROTECTION_KEY << 2) + DISABLE_PROTECTION;
	*((unsigned long *) (virt_addr + ADC_TRGR_OFFSET)) = option;
	*((unsigned long *) (virt_addr + WRITE_PROTECTION_OFFSET)) = (WRITE_PROTECTION_KEY << 2) + ENABLE_PROTECTION;

}
