/*************************************************************************************
****************************OBDH main for the uG mission******************************
**************************************************************************************
		Taks:

/////////////////////SETUP/////////////////////

				***waiting for hw/sw boot***----->TBD
				*watchdog
				*clock setup
					-master clock
					-submaster clock
				*Timers setup-------------------->TBD
				*serial communication setup
					-uart Zboard
					-i2c eps
					-i2c mpu
					-spi beacon
					-spi memory
				*enable interrupts

/////////////////////MAIN LOOP////////////////////

				*Read 18 bytes from EPS
				*Read 5 bytes from beacon
				*Read 12 bytes from IMU
				*CRC
				*Concatenate the frame
				*Write the frame on memory
				*Send the frame to uZed via uart

////////////////////frame///////////////////////////

[START BYTE][EPS DATA][BEACON DATA][IMU DATA][CRC][END BYTE]

/////////////frame to save into flash///////////////

[RTC_TIME][EPS DATA][BEACON DATA][IMU DATA]


*************************************************************************************/
#include <msp430.h> 
#include "hal/obdh_engmodel1.h"
#include "modules_interfaces/mpu.h"
#include "util/uart.h"
#include "util/misc.h"
#include "util/i2c.h"
#include "util/watchdog.h"
#include "util/flash.h"



//main frames and variables
unsigned int cycle_counter = 0;
char strbuff[100];
char EPS_data_buffer[EPS_DATA_LENGTH];
char MPU_data_buffer[MPU_DATA_LENGTH];
char BEACON_data_buffer[BEACON_DATA_LENGTH];
char FSAT_frame[FSAT_FRAME_LENGTH];
char misc_info_frame[MISC_INFO_LENGHT];


//debuf frames
unsigned char Debug_FSAT_Frame[] = {"0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,"
									"0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,"
									"0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,"
									"0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00"};//TODO rm

unsigned char Debug_EPS_Data[] = {"0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,"
								  "0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00"};//TODO rm

unsigned char Debug_MPU_Data[] = {"0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,"
								  "0x00,0x00,0x00,0x00"};//TODO rm

unsigned char Debug_BEACON_Data[] = {"0x00,0x00,0x00"};//TODO rm


//main tasks
void main_setup(void);
void readEps(void);
void readImu(void);
void readBeacon(void);
void write2Flash(void);
void send2uZed(void);

//frames manipulation
void concatenate_frame(void);
void concatenate_info_frame(void);

void main(void) {

	main_setup();

    while(1) {

    	debug("Main cycle init");
    	cycle_counter++;
    	sysled_toggle();
    	uart_tx("[FSAT DEBUG] Cycle: "); uart_tx(int2char(strbuff, cycle_counter));uart_tx("/r/n");

    	readEps();
    	readImu();
    	readBeacon();
    	write2Flash();
    	send2uZed();

    	__delay_cycles(100001);

    	debug("Main cycle done.");
    }
}



void main_setup(void){
	watchdog_setup(INTERVAL,_524_3_mSEC);
	uart_setup(9600);
	debug("\n\n\r MAIN booting...\n\r"); //TODO rm
	sysled_enable();
	flash_setup(BANK1_ADDR);
	i2c_setup(EPS);
	i2c_setup(MPU);
	__enable_interrupt();
	mpu_config();
	debug("MAIN boot completed.\n\r"); //TODO rm
	wdt_reset_counter();
}

void readEps(void){
	debug("Reading EPS init");
	i2c_read_epsFrame(EPS_data_buffer,EPS_DATA_LENGTH);
	frame2string(EPS_data_buffer,Debug_EPS_Data, sizeof Debug_EPS_Data); //TODO rm
	debug(Debug_EPS_Data);
	debug("Reading EPS done");
	wdt_reset_counter();
}

void readImu(void){
	debug("Reading MPU init");
	i2c_IMU_read(MPU9150_ACCEL_XOUT_H, MPU_data_buffer, sizeof MPU_data_buffer);
	frame2string(MPU_data_buffer,Debug_MPU_Data, sizeof Debug_MPU_Data); //TODO rm
	debug(Debug_MPU_Data);
	debug("Reading MPU done");
	wdt_reset_counter();
}

void readBeacon(void){
//    	debug("Reading Beacon init");
//    	debug("Reading Beacon done");
//		wdt_reset_counter();
}

void write2Flash(void){
	debug("Writing to flash init");
	concatenate_frame();
	flash_write(FSAT_frame,FSAT_FRAME_LENGTH);
	concatenate_info_frame();
	flash_write(misc_info_frame,MISC_INFO_LENGHT);
	debug("Writing to flash done");
	wdt_reset_counter();
}

void send2uZed(void){
	debug("Sending to uZED/uG init");
	debug("Sending FSAT FRAME TO uZED...\n\r"); //TODO rm
    uart_tx(FSAT_frame);
	frame2string(FSAT_frame,Debug_FSAT_Frame, sizeof Debug_FSAT_Frame); //TODO rm
	debug(Debug_FSAT_Frame); //todo rm
	debug("Sending to uZED/uG done");
	wdt_reset_counter();
}

void concatenate_frame(void){
	unsigned int i,j = 1;
	FSAT_frame[0] = STT_BYTE;
	for(i = 0;i < EPS_DATA_LENGTH;i++)
		FSAT_frame[j++] = EPS_data_buffer[i];
	for(i = 0;i < MPU_DATA_LENGTH;i++)
		FSAT_frame[j++] = MPU_data_buffer[i];
	for(i = 0;i < BEACON_DATA_LENGTH;i++)
		FSAT_frame[j++] = BEACON_data_buffer[i];
	debug("CRC...\n\r"); //TODO rm
	FSAT_frame[FSAT_FRAME_LENGTH - 2] = CRC8(FSAT_frame, sizeof FSAT_frame);
	debug("CRC DONE.\n\r"); //TODO rm
	FSAT_frame[FSAT_FRAME_LENGTH - 1] = END_BYTE;
}

void concatenate_info_frame(void){
	misc_info_frame[0] = INFO_STT_BYTE;
	misc_info_frame[1] = (char)(cycle_counter >> 8);
	misc_info_frame[2] = (char)(cycle_counter);
	misc_info_frame[MISC_INFO_LENGHT - 1] = INFO_END_BYTE;
}


