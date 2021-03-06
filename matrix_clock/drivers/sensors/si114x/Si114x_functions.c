//-----------------------------------------------------------------------------
// Si114x_functions.c
//-----------------------------------------------------------------------------
// Copyright 2013 Silicon Laboratories, Inc.
// http://www.silabs.com
//
// File Description:
//
// This file contains low-level routines that access the i2c controller
// When porting code to a different i2c controller API, only the code
// with PT_ prefix will need to be modified.
//
// Target:         Si114x
// Command Line:   None
//
//-----------------------------------------------------------------------------
#include "si114x_functions.h"
#include "Si114x_handler.h"
#include "../../ht1632c/ht1632c.h"
#include "../../sercom/twi.h"
#include "../../port/port.h"
#include "../../../modules/display/display.h"
#include <avr/io.h>
#include <util/delay.h>

static volatile uint16_t counter = 0;

void si114x_setup(void)
{
	//Disable power reduction for TCC1 
	PR.PRPC &= ~0x02;
	
	//TODO: Create function for this?
	TCC1.CTRLA = TC_CLKSEL_DIV1_gc;
	TCC1.PERL = 0x80;
	TCC1.PERH = 0x0C;
	TCC1.INTCTRLA = TC_OVFINTLVL_LO_gc;
	
	si114x_reset((HANDLE)SI114X_ADDR);
	_delay_ms(50);
	si114x_init((HANDLE)SI114X_ADDR);
}

void si114x_setup_ps1(void)
{
	si114x_reset((HANDLE)SI114X_ADDR);
	_delay_ms(50);
	//si114x_init_ps1((HANDLE)SI114X_ADDR);
	si114x_init_ps1_als((HANDLE)SI114X_ADDR);
	//si114x_init_als((HANDLE)SI114X_ADDR);
	btn_si114x_enable_interrupt();
}

void si114x_baseline_calibration(SI114X_IRQ_SAMPLE *sensor_data) 
{	
	si114x_setup();
	
	initial_baseline_counter = 128;
	
#ifdef SHOW_MANUAL
	display_print_scrolling_text("CALIBRATING. HANDS AWAY FROM DISPLAY",false);
#endif
	do {
		//Draw square
		display_draw_filled_rect(6,6,4,4,1);
		display_draw_filled_rect(7,7,2,2,0);
		display_refresh_screen();
		si114x_get_data(sensor_data);
#ifdef DEBUG_ON
		printf("DEBUG: PS1 Value: %d \n",sensor_data->ps1);
#endif
	} while (sensor_data->ps1 > PROXIMITY_THRESHOLD2);
		
	//Store for calibration usage
	env.ps1 = sensor_data->ps1;
		
	_delay_ms(1000);
		
	uint16_t cnt = 128;
	while (cnt--) {
		sensor_data->timestamp = counter;
		si114x_get_data(sensor_data);
		si114x_process_samples((HANDLE)SI114X_ADDR,sensor_data);
	}
}

void si114x_test_function(SI114X_IRQ_SAMPLE *sensor_data) 
{
	while(1) {
		si114x_get_data(sensor_data);
		//uint16_t distance_1 = QS_Counts_to_Distance_2(sensor_data.ps1,1);
		//uint16_t distance_2 = QS_Counts_to_Distance_2(sensor_data.ps2,2);
		//printf("Counts 1: %d , Counts 2: %d , Dist 1: %d, Dist 2: %d \r\n",sensor_data.ps1, sensor_data.ps2, distance_1, distance_2);
		
		printf("ALS: %d, IR: %d \r\n",sensor_data->vis, sensor_data->ir);
		if (sensor_data->ir > 800)
		{
			display_fade(15);
		} else {
			display_fade(0);
		}
		
		_delay_ms(500);
	}	
}

u8 si114x_get_data(SI114X_IRQ_SAMPLE *sensor_data) 
{
	u16 data_16;
	u8 data_8[2];
	
	//Timestamp
	sensor_data->timestamp = counter;
	
	//NOTE: This could be simplified as one large bulk read
	
	twi_read_packet(&TWIC,SI114X_ADDR,50,REG_PS1_DATA0,data_8,2);
	data_16 = ((u16)data_8[1] << 8) | data_8[0];
	sensor_data->ps1 = data_16;
	
	twi_read_packet(&TWIC,SI114X_ADDR,50,REG_PS2_DATA0,data_8,2);
	data_16 = ((u16)data_8[1] << 8) | data_8[0];
	sensor_data->ps2 = data_16;
	
	twi_read_packet(&TWIC,SI114X_ADDR,50,REG_PS3_DATA0,data_8,2);
	data_16 = ((u16)data_8[1] << 8) | data_8[0];
	sensor_data->ps3 = data_16;
	
	twi_read_packet(&TWIC,SI114X_ADDR,50,REG_ALS_IR_DATA0,data_8,2);
	data_16 = ((u16)data_8[1] << 8) | data_8[0];
	sensor_data->ir = data_16;
	
	twi_read_packet(&TWIC,SI114X_ADDR,50,REG_ALS_VIS_DATA0,data_8,2);
	data_16 = ((u16)data_8[1] << 8) | data_8[0];
	sensor_data->vis = data_16;
	
	return 0;
}

s16 si114x_init_ps1(HANDLE si114x_handle) 
{
	s16 retval   = 0;

	u8  code current_LED1  = 0x03;   // 0-359 mA

	u8  tasklist      = 0x01;   // PS1 only

	u8  measrate      = 0xA0;   // Samplingsrate

	u8  psrate        = 0x08;

	#ifdef GENERAL
	u8  code psrange  =  1;     // PS Range
	#endif

	#ifdef INDOORS
	u8  code psrange  =  0;     // PS Range
	#endif

	u8  code psgain   =  0;     // PS ADC Gain

	//SI114X_CAL_S xdata si114x_cal;

	// Choose IR PD Size
	u8  code ps1pdsize     = 1;      // PD Choice for PS1
	// 0 = Small, 1= Large
	u8  code irpd          = 1;      // PD Choice for IR Ambient
	// 0 = Small, 1= Large

	// Select which PD is enabled per PS measurement
	u8  code ps1ledsel     = LED1_EN;


	// Turn off RTC and reset
	retval+=Si114xWriteToRegister(si114x_handle, REG_MEAS_RATE,     0 );
	retval+=Si114xWriteToRegister(si114x_handle, REG_PS_RATE,       0 );
	retval+=Si114xWriteToRegister(si114x_handle, REG_ALS_RATE,      0 );

	retval+=si114x_reset(si114x_handle);

	// Program LED Currents
	u8 i21;
	i21 = current_LED1;
	retval+=Si114xWriteToRegister(si114x_handle, REG_PS_LED21, i21);

	retval+=Si114xParamSet(si114x_handle, PARAM_CH_LIST, tasklist);
		
	retval+=Si114xWriteToRegister(si114x_handle, REG_INT_CFG, ICG_INTOE + ICG_INTMODE);
	retval+=Si114xWriteToRegister(si114x_handle, REG_IRQ_ENABLE,IE_PS1_EXCEED_TH);
	retval+=Si114xWriteToRegister(si114x_handle, REG_IRQ_MODE1,IM1_PS1_EXCEED_TH);
		
	retval+=Si114xParamSet(si114x_handle, PARAM_PS1_ADC_MUX, 0x03*ps1pdsize);
	retval+=Si114xParamSet(si114x_handle, PARAM_IR_ADC_MUX, 0x03*irpd);
	retval+=Si114xParamSet(si114x_handle, PARAM_PS_ADC_GAIN, psgain);
	retval+=Si114xParamSet(si114x_handle, PARAM_PSLED12_SELECT, ps1ledsel);
	retval+=Si114xParamSet(si114x_handle, PARAM_PS_ADC_COUNTER, RECCNT_511);
	retval+=Si114xParamSet(si114x_handle, PARAM_PS_ADC_MISC, RANGE_EN*psrange + PS_MEAS_MODE);
		
	u16 threshold_val = PROXIMITY_THRESHOLD;
	retval += Si114xWriteToRegister(si114x_handle, REG_PS1_TH_LSB,(u8)threshold_val);
	retval += Si114xWriteToRegister(si114x_handle, REG_PS1_TH_MSB,(u8)(threshold_val >> 8));
		
	if( measrate > 0 )
	{
		retval+=Si114xWriteToRegister(si114x_handle, REG_MEAS_RATE, measrate);
		retval+=Si114xWriteToRegister(si114x_handle, REG_PS_RATE,   psrate);
		retval+=Si114xPsAuto(si114x_handle);
	}
	return retval;
}

s16 si114x_init_ps1_als(HANDLE si114x_handle)
{
	s16 retval   = 0;

	u8  code current_LED1  = 0x03;   // 0-359 mA

	u8  tasklist      = 0x31;   // IR, ALS, PS1

	u8  measrate      = 0xA0;   // Samplingsrate

	u8  psrate        = 0x08;
	u8  alsrate       = 0x08;

	#ifdef GENERAL
	u8  code psrange  =  1;     // PS Range
	#endif

	#ifdef INDOORS
	u8  code psrange  =  0;     // PS Range
	#endif

	u8  code psgain   =  0;     // PS ADC Gain

	//SI114X_CAL_S xdata si114x_cal;

	// Choose IR PD Size
	u8  code ps1pdsize     = 1;      // PD Choice for PS1
	// 0 = Small, 1= Large
	u8  code irpd          = 1;      // PD Choice for IR Ambient
	// 0 = Small, 1= Large

	// Select which PD is enabled per PS measurement
	u8  code ps1ledsel     = LED1_EN;
	
	u8  code irrange  =  1;     // IR Range
	u8  code irgain   =  0;     // IR ADC Gain

	u8  code visrange =  1;     // VIS Range
	u8  code visgain  =  0;     // VIS ADC Gain

	// Turn off RTC and reset
	retval+=Si114xWriteToRegister(si114x_handle, REG_MEAS_RATE,     0 );
	retval+=Si114xWriteToRegister(si114x_handle, REG_PS_RATE,       0 );
	retval+=Si114xWriteToRegister(si114x_handle, REG_ALS_RATE,      0 );

	retval+=si114x_reset(si114x_handle);

	// Program LED Currents
	u8 i21;
	i21 = current_LED1;
	retval+=Si114xWriteToRegister(si114x_handle, REG_PS_LED21, i21);

	retval+=Si114xParamSet(si114x_handle, PARAM_CH_LIST, tasklist);
	
	retval+=Si114xWriteToRegister(si114x_handle, REG_INT_CFG, ICG_INTOE + ICG_INTMODE);
	retval+=Si114xWriteToRegister(si114x_handle, REG_IRQ_ENABLE,IE_PS1_EXCEED_TH + 0x01);
	retval+=Si114xWriteToRegister(si114x_handle, REG_IRQ_MODE1,IM1_PS1_EXCEED_TH + IM1_ALS_IR_EXIT);
	
	retval+=Si114xParamSet(si114x_handle, PARAM_PS1_ADC_MUX, 0x03*ps1pdsize);
	retval+=Si114xParamSet(si114x_handle, PARAM_IR_ADC_MUX, 0x03*irpd);
	retval+=Si114xParamSet(si114x_handle, PARAM_PS_ADC_GAIN, psgain);
	retval+=Si114xParamSet(si114x_handle, PARAM_PSLED12_SELECT, ps1ledsel);
	retval+=Si114xParamSet(si114x_handle, PARAM_PS_ADC_COUNTER, RECCNT_511);
	retval+=Si114xParamSet(si114x_handle, PARAM_PS_ADC_MISC, RANGE_EN*psrange + PS_MEAS_MODE);
	
	retval+=Si114xParamSet(si114x_handle, PARAM_ALSIR_ADC_GAIN, irgain);
	retval+=Si114xParamSet(si114x_handle, PARAM_ALSVIS_ADC_GAIN, visgain);
	retval+=Si114xParamSet(si114x_handle, PARAM_ALSIR_ADC_COUNTER, RECCNT_511);
	retval+=Si114xParamSet(si114x_handle, PARAM_ALSVIS_ADC_COUNTER, RECCNT_511);
	retval+=Si114xParamSet(si114x_handle, PARAM_ALSIR_ADC_MISC, RANGE_EN*irrange );
	retval+=Si114xParamSet(si114x_handle, PARAM_ALSVIS_ADC_MISC,RANGE_EN*visrange);
	
	u16 threshold_val = PROXIMITY_THRESHOLD;
	retval += Si114xWriteToRegister(si114x_handle, REG_PS1_TH_LSB,(u8)threshold_val);
	retval += Si114xWriteToRegister(si114x_handle, REG_PS1_TH_MSB,(u8)(threshold_val >> 8));
	
	//ALS
	threshold_val = 350;
	retval += Si114xWriteToRegister(si114x_handle, REG_ALS_LO_TH_LSB,(u8)threshold_val);
	retval += Si114xWriteToRegister(si114x_handle, REG_ALS_LO_TH_MSB,(u8)(threshold_val >> 8));
	
	threshold_val = 2000;
	retval += Si114xWriteToRegister(si114x_handle, REG_ALS_HI_TH_LSB,(u8)threshold_val);
	retval += Si114xWriteToRegister(si114x_handle, REG_ALS_HI_TH_MSB,(u8)(threshold_val >> 8));
	//
	
	if( measrate > 0 )
	{
		retval+=Si114xWriteToRegister(si114x_handle, REG_MEAS_RATE, measrate);
		retval+=Si114xWriteToRegister(si114x_handle, REG_PS_RATE,   psrate);
		retval+=Si114xWriteToRegister(si114x_handle, REG_ALS_RATE,  alsrate);
		retval+=Si114xPsAlsAuto(si114x_handle);
	}
	return retval;
}

s16 si114x_init_als(HANDLE si114x_handle)
{
	s16 retval   = 0;

	u8  tasklist      = 0x30;   // ALS, IR

	u8  measrate      = 0x94;   // 0xa0 every 30.0 ms
	//u8  psrate        = 0x08;
	u8  alsrate       = 0x08;
	#ifdef GENERAL
	u8  code psrange  =  1;     // PS Range
	#endif

	#ifdef INDOORS
	//u8  code psrange  =  0;     // PS Range
	#endif

	//u8  code psgain   =  0;     // PS ADC Gain

	u8  code irrange  =  1;     // IR Range
	u8  code irgain   =  0;     // IR ADC Gain

	u8  code visrange =  1;     // VIS Range
	u8  code visgain  =  0;     // VIS ADC Gain

	//SI114X_CAL_S xdata si114x_cal;

	u8  code irpd          = 1;      // PD Choice for IR Ambient
	// 0 = Small, 1= Large

	// Turn off RTC and reset
	retval+=Si114xWriteToRegister(si114x_handle, REG_MEAS_RATE,     0 );
	retval+=Si114xWriteToRegister(si114x_handle, REG_PS_RATE,       0 );
	retval+=Si114xWriteToRegister(si114x_handle, REG_ALS_RATE,      0 );

	retval+=si114x_reset(si114x_handle);

	retval+=Si114xParamSet(si114x_handle, PARAM_CH_LIST, tasklist);
		
	// Set IRQ Modes and INT CFG to interrupt on every sample
	retval+=Si114xWriteToRegister(si114x_handle, REG_INT_CFG, ICG_INTOE + ICG_INTMODE);
	retval+=Si114xWriteToRegister(si114x_handle, REG_IRQ_ENABLE, 0x01);
	retval+=Si114xWriteToRegister(si114x_handle, REG_IRQ_MODE1, IM1_ALS_IR_EXIT);

	retval+=Si114xParamSet(si114x_handle, PARAM_IR_ADC_MUX,  0x03*irpd);

	retval+=Si114xParamSet(si114x_handle, PARAM_ALSIR_ADC_GAIN, irgain);
	retval+=Si114xParamSet(si114x_handle, PARAM_ALSVIS_ADC_GAIN, visgain);
		
	retval+=Si114xParamSet(si114x_handle, PARAM_ALSIR_ADC_COUNTER, RECCNT_511);
	retval+=Si114xParamSet(si114x_handle, PARAM_ALSVIS_ADC_COUNTER, RECCNT_511);

	retval+=Si114xParamSet(si114x_handle, PARAM_ALSIR_ADC_MISC, RANGE_EN*irrange );
	retval+=Si114xParamSet(si114x_handle, PARAM_ALSVIS_ADC_MISC,RANGE_EN*visrange);

	u16 threshold_val = 350;
	retval += Si114xWriteToRegister(si114x_handle, REG_ALS_LO_TH_LSB,(u8)threshold_val);
	retval += Si114xWriteToRegister(si114x_handle, REG_ALS_LO_TH_MSB,(u8)(threshold_val >> 8));
	
	u8 temp[2];
	Si114xBlockRead(si114x_handle,REG_ALS_LO_TH_LSB,2,temp);
	printf("Threshold LOW: %d \r\n", ((temp[1] << 8) | temp[0]));
	
	threshold_val = 2000;
	retval += Si114xWriteToRegister(si114x_handle, REG_ALS_HI_TH_LSB,(u8)threshold_val);
	retval += Si114xWriteToRegister(si114x_handle, REG_ALS_HI_TH_MSB,(u8)(threshold_val >> 8));
	
	Si114xBlockRead(si114x_handle,REG_ALS_HI_TH_LSB,2,temp);
	printf("Threshold LOW: %d \r\n", ((temp[1] << 8) | temp[0]));

	if( measrate > 0 )
	{
		retval+=Si114xWriteToRegister(si114x_handle, REG_MEAS_RATE, measrate);
		retval+=Si114xWriteToRegister(si114x_handle, REG_ALS_RATE,  alsrate);
		retval+=Si114xAlsAuto(si114x_handle);
	}
	return retval;
}

s16 si114x_init(HANDLE si114x_handle)
{
	s16 retval   = 0;

	u8  code current_LED1  = 0x0f;   // 359 mA
	u8  code current_LED2  = 0x0f;   // 359 mA
	u8  code current_LED3  = 0x00;   //   0 mA

	u8  tasklist      = 0x77;   // IR, PS1, PS2

	u8  measrate      = 0x94;   
	// 0xa0 every 30.0 ms
	// 0x94 every 20.0 ms
	// 0x84 every 10.4 ms
	// 0x74 every  5.2 ms
	// 0x70 every  4.2 ms
	// 0x60 every  3.2 ms
	u8  psrate        = 0x08;
	u8  alsrate       = 0x08;
	#ifdef GENERAL
	u8  code psrange  =  1;     // PS Range
	#endif

	#ifdef INDOORS
	u8  code psrange  =  0;     // PS Range
	#endif

	u8  code psgain   =  0;     // PS ADC Gain

	u8  code irrange  =  1;     // IR Range
	u8  code irgain   =  0;     // IR ADC Gain

	u8  code visrange =  1;     // VIS Range
	u8  code visgain  =  0;     // VIS ADC Gain

	//SI114X_CAL_S xdata si114x_cal;

	// Choose IR PD Size
	u8  code ps1pdsize     = 1;      // PD Choice for PS1
	// 0 = Small, 1= Large
	u8  code ps2pdsize     = 1;      // PD Choice for PS2
	// 0 = Small, 1= Large
	u8  code ps3pdsize     = 1;      // PD Choice for PS3
	// 0 = Small, 1= Large
	u8  code irpd          = 1;      // PD Choice for IR Ambient
	// 0 = Small, 1= Large

	// Select which PD is enabled per PS measurement
	u8  code ps1ledsel     = LED1_EN;
	u8  code ps2ledsel     = LED2_EN;
	u8  code ps3ledsel     = LED3_EN;


	// Turn off RTC
	retval+=Si114xWriteToRegister(si114x_handle, REG_MEAS_RATE,     0 );
	retval+=Si114xWriteToRegister(si114x_handle, REG_PS_RATE,       0 );
	retval+=Si114xWriteToRegister(si114x_handle, REG_ALS_RATE,      0 );


	// Note that the si114x_reset() actually performs the following functions:
	//     1. Pauses all prior measurements
	//     2. Clear  i2c registers that need to be cleared
	//     3. Clears irq status to make sure INT* is negated
	//     4. Delays 10 ms
	//     5. Sends HW Key
	retval+=si114x_reset(si114x_handle);

	// Program LED Currents
	{
		u8 i21, i3;

		i21 = (current_LED2<<4) + current_LED1;
		i3  = current_LED3;
		retval+=Si114xWriteToRegister(si114x_handle, REG_PS_LED21, i21);
		retval+=Si114xWriteToRegister(si114x_handle, REG_PS_LED3 , i3);
	}

	// Initialize CHLIST Parameter from caller to enable measurement
	// Valid Tasks are: ALS_VIS_TASK, ALS_IR_TASK, PS1_TASK
	//                  PS2_TASK, PS3_TASK and AUX_TASK
	// However, if we are passed a 'negative' task, we will
	// turn on ALS_IR, ALS_VIS and PS1. Otherwise, we will use the
	// task list specified by the caller.
	retval+=Si114xParamSet(si114x_handle, PARAM_CH_LIST, tasklist);
	
	// Set IRQ Modes and INT CFG to interrupt on every sample
	retval+=Si114xWriteToRegister(si114x_handle, REG_INT_CFG, ICG_INTOE);
	retval+=Si114xWriteToRegister(si114x_handle, REG_IRQ_ENABLE,
	IE_ALS_EVRYSAMPLE +
	IE_PS1_EVRYSAMPLE +
	IE_PS2_EVRYSAMPLE +
	IE_PS3_EVRYSAMPLE );

	retval+=Si114xWriteToRegister(si114x_handle, REG_IRQ_MODE1,
	IM1_ALS_EVRYSAMPLE +
	IM1_PS1_EVRYSAMPLE +
	IM1_PS2_EVRYSAMPLE );

	retval+=Si114xWriteToRegister(si114x_handle, REG_IRQ_MODE2,
	IM2_PS3_EVRYSAMPLE);
	
	retval+=Si114xParamSet(si114x_handle, PARAM_PS1_ADC_MUX, 0x03*ps1pdsize);
	retval+=Si114xParamSet(si114x_handle, PARAM_PS2_ADC_MUX, 0x03*ps2pdsize);
	retval+=Si114xParamSet(si114x_handle, PARAM_PS3_ADC_MUX, 0x03*ps3pdsize);

	retval+=Si114xParamSet(si114x_handle, PARAM_IR_ADC_MUX,  0x03*irpd);

	retval+=Si114xParamSet(si114x_handle, PARAM_PS_ADC_GAIN, psgain);
	retval+=Si114xParamSet(si114x_handle, PARAM_ALSIR_ADC_GAIN, irgain);
	retval+=Si114xParamSet(si114x_handle, PARAM_ALSVIS_ADC_GAIN, visgain);
	
	retval+=Si114xParamSet(si114x_handle, PARAM_PSLED12_SELECT, (ps2ledsel<<4)+ps1ledsel);
	retval+=Si114xParamSet(si114x_handle, PARAM_PSLED3_SELECT,  ps3ledsel);
	
	retval+=Si114xParamSet(si114x_handle, PARAM_PS_ADC_COUNTER,    RECCNT_511);
	retval+=Si114xParamSet(si114x_handle, PARAM_ALSIR_ADC_COUNTER, RECCNT_511);
	retval+=Si114xParamSet(si114x_handle, PARAM_ALSVIS_ADC_COUNTER, RECCNT_511);

	retval+=Si114xParamSet(si114x_handle, PARAM_PS_ADC_MISC,     RANGE_EN*psrange + PS_MEAS_MODE);
	retval+=Si114xParamSet(si114x_handle, PARAM_ALSIR_ADC_MISC, RANGE_EN*irrange );
	retval+=Si114xParamSet(si114x_handle, PARAM_ALSVIS_ADC_MISC,RANGE_EN*visrange);


	if( measrate > 0 )
	{
		// Set up how often the device wakes up to make measurements
		// measrate, for example can be the following values:
		//    0xa0 = Device Wakes up every ~30 ms
		//    0x94 = Device Wakes up every ~20 ms
		//    0x84 = Device Wakes up every ~10 ms
		//    0xB9 = Device Wakes up every ~100 ms
		//    0xFF = Device Wakes up every ~2 sec
		retval+=Si114xWriteToRegister(si114x_handle, REG_MEAS_RATE, measrate);

		// if 0x08, PS1, PS2 and PS3 made every time device wakes up.
		retval+=Si114xWriteToRegister(si114x_handle, REG_PS_RATE,   psrate);

		// if 0x08, VIS, IR, AUX Measurements every time device wakes up.
		retval+=Si114xWriteToRegister(si114x_handle, REG_ALS_RATE,  alsrate);

		// Enable Autonomous Operation
		retval+=Si114xPsAlsAuto(si114x_handle);
		
		// If nothing went wrong after all of this time, the value
		// returned will be 0. Otherwise, it will be some negative
		// number
	}
	return retval;
}

static s16 _waitUntilSleep(HANDLE si114x_handle)
{
	s16 retval;
	// This loops until the Si114x is known to be in its sleep state
	// or if an i2c error occurs
	while (1)
	{
		retval = Si114xReadFromRegister(si114x_handle, REG_CHIP_STAT);
		if (retval == 1) break;
		if (retval <  0) return retval;
	}
	return 0;
}


s16 si114x_reset(HANDLE si114x_handle)
{
	void delay_10ms(void);
	s16 retval = 0;

	//
	// Do not access the Si114x earlier than 25 ms from power-up.
	// Uncomment the following lines if si114x_reset() is the first
	// instruction encountered, and if your system MCU boots up too
	// quickly.
	//
	// delay_10ms();
	// delay_10ms();
	// delay_10ms();

	retval+=Si114xWriteToRegister(si114x_handle, REG_MEAS_RATE,  0x00);
	retval+=Si114xPauseAll(si114x_handle);

	// The clearing of the registers could be redundant, but it is okay.
	// This is to make sure that these registers are cleared.
	retval+=Si114xWriteToRegister(si114x_handle, REG_MEAS_RATE,  0x00);
	retval+=Si114xWriteToRegister(si114x_handle, REG_IRQ_ENABLE, 0x00);
	retval+=Si114xWriteToRegister(si114x_handle, REG_IRQ_MODE1,  0x00);
	retval+=Si114xWriteToRegister(si114x_handle, REG_IRQ_MODE2,  0x00);
	retval+=Si114xWriteToRegister(si114x_handle, REG_INT_CFG  ,  0x00);
	retval+=Si114xWriteToRegister(si114x_handle, REG_IRQ_STATUS, 0xFF);

	// Perform the Reset Command
	retval+=Si114xWriteToRegister(si114x_handle, REG_COMMAND, 1);

	// Delay for 10 ms. This delay is needed to allow the Si114x
	// to perform internal reset sequence.
	delay_10ms();

	// Write Hardware Key
	retval+=Si114xWriteToRegister(si114x_handle, REG_HW_KEY, HW_KEY_VAL0);

	return retval;
}


static s16 _sendCmd(HANDLE si114x_handle, u8 command)
{

	s16  response;
	s16  retval;

	// Get the response register contents
	if ((response=Si114xReadFromRegister(si114x_handle, REG_RESPONSE))<0)
	return response;

	// Double-check the response register is consistent
	while(1)
	{
		if((retval=_waitUntilSleep(si114x_handle)) != 0) return retval;

		if(command==0) break; // Skip if the command is NOP

		retval=Si114xReadFromRegister(si114x_handle, REG_RESPONSE);
		if(retval==response) break;
		else if(retval<0) return retval;
		else response = retval;
	}

	// Send the Command
	if ((retval=Si114xWriteToRegister(si114x_handle, REG_COMMAND, command)) !=0)
	return retval;

	// Expect a change in the response register
	while(1)
	{

		if(command==0) break; // Skip if the command is NOP

		retval= Si114xReadFromRegister(si114x_handle, REG_RESPONSE);
		if (retval != response) break;
		else if(retval<0) return retval;
	}
	return 0;
}

s16 Si114xNop       (HANDLE si114x_handle) {return _sendCmd(si114x_handle,0x00);}

s16 Si114xPsForce   (HANDLE si114x_handle) {return _sendCmd(si114x_handle,0x05);}

s16 Si114xAlsForce  (HANDLE si114x_handle) {return _sendCmd(si114x_handle,0x06);}

s16 Si114xPsAlsForce(HANDLE si114x_handle) {return _sendCmd(si114x_handle,0x07);}

s16 Si114xPsAuto (HANDLE si114x_handle) {return _sendCmd(si114x_handle,0x0D);}
	
s16 Si114xAlsAuto (HANDLE si114x_handle) {return _sendCmd(si114x_handle,0x0E);}

s16 Si114xPsAlsAuto (HANDLE si114x_handle) {return _sendCmd(si114x_handle,0x0F);}


s16 Si114xParamRead(HANDLE si114x_handle, u8 address)
{
	// returns Parameter[address]
	s16 retval;
	u8 cmd = 0x80 + (address & 0x1F);
	if((retval=_sendCmd(si114x_handle, cmd ))!=0) return retval;
	return Si114xReadFromRegister(si114x_handle, REG_PARAM_RD);
}


//-----------------------------------------------------------------------------
// Si114xParamSet writes to the PARAM_WR and CMD register
//
s16 Si114xParamSet(HANDLE si114x_handle, u8 address, u8 value)
{
	s16     retval;
	u8      buffer[2];
	s16     response;

	if((retval = _waitUntilSleep(si114x_handle))!=0) return retval;

	response = Si114xReadFromRegister(si114x_handle, REG_RESPONSE);

	buffer[0]= value;
	buffer[1]= 0xA0 + (address & 0x1F);

	retval=Si114xBlockWrite(si114x_handle, REG_PARAM_WR, 2, buffer);
	if (retval != 0) return retval;

	// Wait for command to finish
	while(( retval=Si114xReadFromRegister(si114x_handle, REG_RESPONSE)) == response ) ;

	if(retval < 0)
	return retval;
	else
	return 0;
}


//-----------------------------------------------------------------------------
// use this to pause measurements
static s16 _PsAlsPause (HANDLE si114x_handle)
{
	return _sendCmd(si114x_handle,0x0B);
}

s16 Si114xPauseAll(HANDLE si114x_handle)
{
	while (1)
	{
		// Keep sending nops until the response is zero
		while (1)
		{
			if ((Si114xReadFromRegister(si114x_handle, REG_RESPONSE))==0)
			break;
			else
			Si114xNop(si114x_handle);
		}

		// Pause the device
		_PsAlsPause(si114x_handle);

		// Wait for response
		while(1)
		{
			if ((Si114xReadFromRegister(si114x_handle, REG_RESPONSE))!=0)
			break;
		}

		// When the PsAlsPause() response is good, we expect it to be a '1'.
		if ((Si114xReadFromRegister(si114x_handle, REG_RESPONSE))==1)
		break;  // otherwise, start over.
	}
	return 0;
}


// --------------------------------------------------------------------
//
// The goal of uncompress is to arrive at a 16-bit value, when the input is a
// single byte of information.
//
// The approach taken here is to reuse the floating point concept, but apply it
// to this. Just as it is possible to store relatively large numbers using an
// IEEE 754 representation of a 32 bit value, we make use of a similar concept.
//
// In IEEE 754 representation, there consists of concept of a signed exponent,
// and a signed significand. The signed exponent allows representation of
// values between 2^127 to 2^-128. The signficand is also signed.
//
// The term 'significand' is the integer bit plus the fraction. The 'fraction'
// is the fractional part of the significand.
//
//              IEEE Single Precision Format
//
//     |  b31   |   b30 to  b23   |  bit22 to bit0  |
//     |  Sign  | Signed Exponent |   Fraction      |
//
// In what we need, we do not need signed exponents nor do we need signed
// significands. So, we use an unsigned exponent representation and an unsigned
// significand.
//
// uncompress takes an input byte and interprets the first 4 bits as an exponent,
// and the last 4 bits as a fraction, with an implicit integer bit
//
// The mathematical representation is similar to the concept for floating point
// numbers. First off, the bit field 7:4 is the Exponent, and the bit field 3:0
// is the fractional part of the significand.
//
//
//      | b7 b6 b5 b4 | b3 b2 b1 b0 |
//      |   unsigned  |             |
//      |   Exponent  |  Fraction   |
//
// The number representation is:
//
//      ( 2 ^ Exponent ) * 1.Fraction
//
// Note the 'implicit integer bit'. Normally, the hidden integer is 1. However,
// there is an exception. If the Exponent is zero, the representation
// becomes the following:
//
//      ( 2 ^ 0 ) * 0.Fraction
//
// This is the concept called the 'denormalized number' identical to the IEEE
// 754 representation of floating point numbers. Concept isn't new... this
// allows us to represent the value 0.
//
// Let's go through one example...
//
// Let's say input is 0x9A.
//
//     Exponent = 9
//     Fraction = A
//
// Since the Exponent is non-zero, the number representation is:
//
//     2 ^ 9 * (1.1010)
//
// So, we take 1.1010 and shift left by 9 positions. It is best illustrated in
// binary...
//
//    1.1010 << 9 = 1 1010 00000 = 0x340
//
// The main advantage is that it allows a very large range dynamic range
// to be represented in 8 bits. The largest number that can be represented
// is 0xFF, and this translates to:
//
//     2 ^ 15 * 1.1111
//
//     1.1111 << 15 = 1111 1000 0000 0000 = 0xF800
//
// When the exponent is less than 4, notice that the fraction bits are
// truncated. What this means is that there can be multiple ways of getting an
// output from 0 to
// the value '0x0000' to 0x000F
//
// To illustrate the case where exponents are less than 4:
//      Input     Output
//      00        0000
//      02        0000
//      08        0001
//      0A        0001
//      10        0002
//      14        0002
//      18        0003
//      1A        0003
//      20        0004
//      24        0005
//      28        0006
//      2c        0007
//      30        0008
//      32        0009
//      34        000a
//      36        000b
//      38        000c
//      3c        000e
//      3e        000f
//
// At exponent of 4 or greater, the fraction bits are no longer being thrown
// away, so, we now have linear values
//      40        0010
//      41        0011
//      42        0012
//      43        0013
//      44        0014
//
// But alas, once the exponent is greater than 4, we now stuff the lower
// fractional bits with zero, and we begin to skip numbers...
//      50        0020
//      51        0022
//      52        0024
//      53        0026
//      54        0028
//
// Well...strictly speaking, the IEEE format treats the largest possible exponent
// as 'infinity' or NAN. Let's not go there... Denorm concept is useful for us
// since it allows us to represent zero. However, infinity or NAN concepts are
// not useful for us.
//

#ifdef INCLUDE_SI114X_COMPRESS_CODE
u16 Uncompress(u8 input) // It is important for the input to be an unsigned 8-bit.
{
	u16 output   = 0 ;
	u8  exponent = 0;
	
	// Handle denorm case where exponent is zero. In this case, we are
	// evaluating the value with the integer bit is zero (0.F). So, we round up
	// if the fraction represents a value of 1/2 or greater. Since the fraction
	// is 4 bits, an input of less than 8/16 is less than half. If less than
	// half, return zero. Otherwise, we know that we will return a 1 later.
	//
	if ( input < 8 ) return 0;

	//
	// At this point, the exponent is non-zero, so, put in the implicit
	// fraction. Note that when we get the input, it comes in already shifted
	// by 4. So, we are dealing with a value already 4 times larger than the
	// actual starting point.
	//
	// Never fear... we just make an adjustment to the exponent and shift
	// left/right accordingly. The result will be the same as the floating
	// point concept described above.
	//
	
	exponent = (input & 0xF0 ) >> 4;      // extracts the exponent
	output = 0x10  | (input & 0x0F);      // extracts the fraction and adds
	// in the implicit integer

	if ( exponent >= 4 ) return ( output << (exponent-4) );
	return( output >> (4-exponent) );
}


// --------------------------------------------------------------------
// What if someone wants to do the inverse function?
//
// Let's say we want to figure out what byte value best represents the number
// of 32 KHz timer ticks for 500 ms.
//
// We start of by knowing how many 32 KHz cycles are in that given time period.
// Let's say that we want to have the RTC wake up every 500 ms.
//
//     500 ms * 32 KHz = 16000 cycles
//
// Then, we take the calculator, and find out what 64 looks like from a binary
// value viewpoint. Using a hex calculator, we see that:
//
//     16000 = 11111010000000
//
//     ... in floating point representation...
//
//           = 11111010000000.00000
//
// The next step is to normalize the value. Normalizing the value means that
// we represent the value in 1.F format. We do this by moving the decimal value left
// until we get the 1.F representation. The number of times we move the decimal
// point left is the exponent. Since we need to move the decimal point left
// before we get to the 1.F represenation...
//
//     16000 = 2^13 * 1.1111010000000
//
// The exponent is therefore 13, and the digits to the right hand side of the
// decimal point is the fraction. What we need is the the first four fractional
// bits. The first four fraction bits is 1111. We truncate the rest,
// unfortunately.
//
// Therefore, the nearest byte representation for 500 ms is 0xDF
//
// Notice that if you plugged in 0xDF into this uncompress function, you will get
// 496 ms. The reason we didn't quite get 500 ms is that we had to throw away the
// 6th fractional bit.
//
// Anyway, this leads us to the following function. This function takes in a
// 16-bit value and compresses it.

u8 Compress(u16 input) // input should be a 16-bit unsigned value
{
	u32 tmp         = 0;
	u32 exponent    = 0;
	u32 significand = 0;

	if(input==0)
	return 0;


	// handle denorm cases
	// There are multiple answers to 0x0000 and 0x0001 input due to rounding
	// error introduced throught the gradual underflow
	//     Answer for 0x0000 is from 0x00 to 0x07
	//     Answer for 0x0001 is from 0x08 to 0x0F
	// We will just 'pick one' answer.
	if (input == 0x0000) return 0x00;
	if (input == 0x0001) return 0x08;


	// Now we have the denorm cases out of the way, the exponent should be at
	// least one at this point.
	exponent = 0;
	tmp = input;
	while (1)
	{
		tmp >>= 1;  // Shift until there is only the integer in the lease significant position
		exponent += 1;
		if (tmp == 1)
		{
			break;  // the integer bit has been found. Stop.
		}
	}


	// Once exponent is found, look for the four fractional bits.
	//
	// If the exponent is between 1 to 4, we do not need to do any kind of
	// fractional rounding. Take care of those cases first

	if (exponent < 5) // shift left to align the significant and return the
	// result
	{
		significand = ( input << (4 - exponent) ) ;
		return ( (exponent << 4) | significand & 0xF);
	}

	// At this point, we need to calculate the fraction.
	//
	// Easiest way is to align the value so that we have the integer and
	// fraction bits at a known bit position.
	//
	// We then round the signficand to the nearest four fractional bits. To do
	// so, it is best that we also look at the 5th fractional bit and update
	// the 4th fractional bit as necessary. During rounding, it is possible for
	// a carry to occur. If this happens, simply add one to the exponent, and
	// shift the signficand by one to get to the same bit positioning.

	significand = input >> (exponent - 5);

	//
	// After the shift, the significand looks like this since we shift the
	// value by 5 less than the exponent. This is what we expect at this point:
	//
	// bit[15:6]     bit5  bit4  bit3  bit2  bit1  bit0
	//
	//   zeroes       1    2^-1  2^-2  2^-3  2^-4  2^-5
	//
	//                ^    ^^^^^^^^^^^^^^^^^^^^^^^^^^^^
	//               int            fraction
	//

	if (significand & 1) // Check if we need to round up
	{
		
		significand += 2;   // Increment the 4th fraction (in bit1 position)
		
		// We then check if a carry occurred due to the addition. If a carry
		// did occur, it would have bumped up the number such that bit6 would
		// be set. Bit6 is 0x0040.
		if (significand & 0x0040)         // Check for a carry
		{
			exponent += 1;                // A carry occurred. Increment the exponent
			significand >>= 1;            // shift the signficand right by one
		}
	}

	// Rounding is done... Encode value and return.
	return ( (exponent << 4) | ( (significand >> 1) & 0xF ) );
}
#endif


// --------------------------------------------------------------------
// --------------------------------------------------------------------
// --------------------------------------------------------------------
// Start of Calibration Code addition
#define FLT_TO_FX20(x)       ((x*1048576)+.5)
#define FX20_ONE             FLT_TO_FX20( 1.000000)
#define FX20_BAD_VALUE       0xffffffff


#ifndef INCLUDE_SI114X_CALIBRATIONCODE
#define INCLUDE_SI114X_CALIBRATIONCODE 0
#endif


#if (INCLUDE_SI114X_CALIBRATIONCODE == 1)
//                                                   msb   lsb   align
//                                                   i2c   i2c   ment
//                                                   addr  addr
#define SIRPD_ADCHI_IRLED    (collect(buffer, 0x23, 0x22,  0))
#define SIRPD_ADCLO_IRLED    (collect(buffer, 0x22, 0x25,  1))
#define SIRPD_ADCLO_WHLED    (collect(buffer, 0x24, 0x26,  0))
#define VISPD_ADCHI_WHLED    (collect(buffer, 0x26, 0x27,  1))
#define VISPD_ADCLO_WHLED    (collect(buffer, 0x28, 0x29,  0))
#define LIRPD_ADCHI_IRLED    (collect(buffer, 0x29, 0x2a,  1))
#define LED_DRV65                   (collect(buffer, 0x2b, 0x2c,  0))



// This is for internal Silabs debug only. Keep it as it is as
// this automatically defines away the embedded debug code
#ifdef SI114x_CAL_DEBUG
#include  "Si114x_cal_debug.c"
#else
#define DEBUG_PRINT_OUTPUT
#define DEBUG_PRINT_OUTPUT_2
#define DEBUG_UCOEF
#endif

//
// Structure Definition for calref array
//
struct cal_ref_t
{
	u32 sirpd_adchi_irled;
	u32 sirpd_adclo_irled;
	u32 sirpd_adclo_whled;
	u32 vispd_adchi_whled;
	u32 vispd_adclo_whled;
	u32 lirpd_adchi_irled;
	u32 ledi_65ma;
	u8  ucoef[4];
};

//
// Factory Calibration Reference Values
//
code struct cal_ref_t calref[2] =
{
	{
		FLT_TO_FX20( 4.021290),  // sirpd_adchi_irled
		FLT_TO_FX20(57.528500),  // sirpd_adclo_irled
		FLT_TO_FX20( 2.690010),  // sirpd_adclo_whled
		FLT_TO_FX20( 0.042903),  // vispd_adchi_whled
		FLT_TO_FX20( 0.633435),  // vispd_adclo_whled
		FLT_TO_FX20(23.902900),  // lirpd_adchi_irled
		FLT_TO_FX20(56.889300),  // ledi_65ma
		{0x7B, 0x6B, 0x01, 0x00} // default ucoef
	},
	{
		FLT_TO_FX20( 2.325484),  // sirpd_adchi_irled
		FLT_TO_FX20(33.541500),  // sirpd_adclo_irled
		FLT_TO_FX20( 1.693750),  // sirpd_adclo_whled
		FLT_TO_FX20( 0.026775),  // vispd_adchi_whled
		FLT_TO_FX20( 0.398443),  // vispd_adclo_whled
		FLT_TO_FX20(12.190900),  // lirpd_adchi_irled
		FLT_TO_FX20(56.558200),  // ledi_65ma
		{0xdb, 0x8f, 0x01, 0x00} // default ucoef
	}
};

//
// Converts the 12-bit factory test value from the Si114x and returns the
// fixed-point representation of this 12-bit factory test value.
//
static u32 decode(u32 input)
{
	s32  exponent, exponent_bias9;
	u32  mantissa;

	if (input==0) return 0.0;

	exponent_bias9 = (input & 0x0f00) >> 8;
	exponent       = exponent_bias9 - 9;

	mantissa       = input & 0x00ff; // fraction
	mantissa       |=        0x0100; // add in integer

	// representation in 12 bit integer, 20 bit fraction
	mantissa       = mantissa << (12+exponent);
	return mantissa;
}

//
// The buffer[] is assumed to point to a byte array that containst the
// factory calibration values after writing 0x12 to the command register
// This function takes the 12 bytes from the Si114x, then converts it
// to a fixed point representation, with the help of the decode() function
//
static u32 collect(u8 *buffer, u8 msb_addr, u8 lsb_addr, u8 alignment)
{
	u16 value;
	u8  msb_ind = msb_addr - 0x22;
	u8  lsb_ind = lsb_addr - 0x22;

	if (alignment == 0)
	{
		value =  buffer[msb_ind]<<4;
		value += buffer[lsb_ind]>>4;
	}
	else
	{
		value =  buffer[msb_ind]<<8;
		value += buffer[lsb_ind];
		value &= 0x0fff;
	}

	if (    ( value == 0x0fff )
	|| ( value == 0x0000 ) ) return FX20_BAD_VALUE;
	else return decode( value );
}

//
// This performs a shift_left function. For convenience, a negative
// shift value will shift the value right. Value pointed will be
// overwritten.
//
static void shift_left(u32 *value_p, s8 shift)
{
	if (shift > 0)
	*value_p = *value_p<<shift ;
	else
	*value_p = *value_p>>(-shift) ;
}

//
// Aligns the value pointed by value_p to either the LEFT or the RIGHT
// the number of shifted bits is returned. The value in value_p is
// overwritten.
//
#define ALIGN_LEFT   1
#define ALIGN_RIGHT -1
static s8 align( u32 *value_p, s8 direction )
{
	s8  local_shift, shift ;
	u32 mask;
	
	// Check invalid value_p and *value_p, return without shifting if bad.
	if( value_p  == NULL )  return 0;
	if( *value_p == 0 )     return 0;

	// Make sure direction is valid
	switch( direction )
	{
		case ALIGN_LEFT:
		local_shift =  1 ;
		mask  = 0x80000000;
		break;

		case ALIGN_RIGHT:
		local_shift = -1 ;
		mask  = 0x00000001;
		break;

		default:
		// Invalid direction, return without shifting
		return 0;
	}

	shift = 0;
	while(1)
	{
		if (*value_p & mask ) break;
		shift++;
		shift_left( value_p, local_shift );
	}
	return shift;
}


//
// The fx20_round was designed to perform rounding to however many significant
// digits. However, for the factory calibration code, rounding to 16 always is
// sufficient. So, the FORCE_ROUND_16 define is provided just in case it would
// be necessary to dynamically choose how many bits to round to.
//
#define FORCE_ROUND_16 1

//
// fx20_round Rounds the u32 value pointed by ptr, by the number
// of bits specified by round.
//
// This compile switch used only to experiment with
// various rounding precisions. The flexibility has
// a small performance price.
//
static void fx20_round
(
u32 *value_p
#if !FORCE_ROUND_16
, s8 round
#endif
)
{
	s8  shift;
	
	#if FORCE_ROUND_16
	// Use the following to force round = 16
	u32 mask1  = 0xffff8000;
	u32 mask2  = 0xffff0000;
	u32 lsb    = 0x00008000;
	#else
	// Use the following if you want to routine to be
	// capable of rounding to something other than 16.
	u32 mask1  = ((2<<(round))-1)<<(31-(round));
	u32 mask2  = ((2<<(round-1))-1)<<(31-(round-1));
	u32 lsb    = mask1-mask2;
	#endif
	
	shift = align( value_p, ALIGN_LEFT );
	if( ( (*value_p)&mask1 ) == mask1 )
	{
		*value_p = 0x80000000;
		shift -= 1;
	}
	else
	{
		*value_p += lsb;
		*value_p &= mask2;
	}

	shift_left( value_p, -shift );
}

//
// The fx20_divide and fx20_multiply uses this structure to pass
// values into it.
//
struct operand_t
{
	u32 op1;
	u32 op2;
};

//
// Returns a fixed-point (20-bit fraction) after dividing op1/op2
//
static u32 fx20_divide( struct operand_t *operand_p )
{
	s8  numerator_sh=0, denominator_sh=0;
	u32 result;
	u32 *numerator_p;
	u32 *denominator_p;

	if ( operand_p == NULL ) return FX20_BAD_VALUE;

	numerator_p   = &operand_p->op1;
	denominator_p = &operand_p->op2;

	if (   (*numerator_p   == FX20_BAD_VALUE)
	|| (*denominator_p == FX20_BAD_VALUE)
	|| (*denominator_p == 0             ) ) return FX20_BAD_VALUE;
	
	fx20_round  ( numerator_p   );
	fx20_round  ( denominator_p );
	numerator_sh   = align ( numerator_p,   ALIGN_LEFT  );
	denominator_sh = align ( denominator_p, ALIGN_RIGHT );

	result = *numerator_p / ( (u16)(*denominator_p) );
	shift_left( &result , 20-numerator_sh-denominator_sh );

	return result;
}

//
// Returns a fixed-point (20-bit fraction) after multiplying op1*op2
//
static u32 fx20_multiply( struct operand_t *operand_p )
{
	u32 result;
	s8  val1_sh, val2_sh;
	u32 *val1_p;
	u32 *val2_p;

	if( operand_p == NULL ) return FX20_BAD_VALUE;

	val1_p = &(operand_p->op1);
	val2_p = &(operand_p->op2);

	fx20_round( val1_p );
	fx20_round( val2_p );

	val1_sh = align( val1_p, ALIGN_RIGHT );
	val2_sh = align( val2_p, ALIGN_RIGHT );


	result = (u32)( ( (u32)(*val1_p) ) * ( (u32)(*val2_p) ) );
	shift_left( &result, -20+val1_sh+val2_sh );

	return result;
}

//
// Due to small differences in factory test setup, the reference calibration
// values may have slight variation. This function retrieves the calibration
// index stored in the Si114x so that it is possible to know which calibration
// reference values to use.
//
static s16 cal_index( u8 *buffer )
{
	s16 index;
	u8  size;

	// buffer[12] is the LSB, buffer[13] is the MSB
	index = ( s16 )( buffer[12] + ( (u16)( buffer[13] ) << 8 ) );

	switch( index )
	{
		case -1:
		index = 0;
		break;
		default:
		index = -(2+index) ;
	}

	size = sizeof(calref)/sizeof(calref[0]);

	if ( index < size )
	{
		return  index;
	}
	else
	{
		return -1;
	}
}

//
// Returns the calibration ratio to be applied to VIS measurements
//
static u32 vispd_correction(u8 *buffer)
{

	struct operand_t op;
	u32              result;
	s16              index = cal_index( buffer );

	if ( index < 0 ) result = FX20_ONE;

	op.op1 = calref[ index ].vispd_adclo_whled;
	op.op2 = VISPD_ADCLO_WHLED;
	result = fx20_divide( &op );

	if ( result == FX20_BAD_VALUE ) result = FX20_ONE;

	return result;
}

//
// Returns the calibration ratio to be applied to IR measurements
//
static u32 irpd_correction(u8 *buffer)
{
	struct operand_t op;
	u32              result;
	s16              index = cal_index( buffer );

	if ( index < 0 ) result = FX20_ONE;

	// op.op1 = SIRPD_ADCLO_IRLED_REF; op.op2 = SIRPD_ADCLO_IRLED;
	op.op1 = calref[ index ].sirpd_adclo_irled;
	op.op2 = SIRPD_ADCLO_IRLED;
	result = fx20_divide( &op );

	if ( result == FX20_BAD_VALUE ) result = FX20_ONE;

	return result;
}

//
// Returns the ratio to correlate between x_RANGE=0 and x_RANGE=1
// It is typically 14.5, but may have some slight component-to-component
// differences.
//
static u32 adcrange_ratio(u8 *buffer)
{
	struct operand_t op;
	u32              result;
	
	op.op1 = SIRPD_ADCLO_IRLED  ; op.op2 = SIRPD_ADCHI_IRLED  ;
	result = fx20_divide( &op );

	if ( result == FX20_BAD_VALUE ) result = FLT_TO_FX20( 14.5 );

	return result;
}

//
// Returns the ratio to correlate between measurements made from large PD
// to measurements made from small PD.
//
static u32 irsize_ratio(u8 *buffer)
{
	struct operand_t op;
	u32              result;

	op.op1 = LIRPD_ADCHI_IRLED  ; op.op2 = SIRPD_ADCHI_IRLED  ;
	result = fx20_divide( &op );

	if ( result == FX20_BAD_VALUE ) result = FLT_TO_FX20(  6.0 );

	return  result;
}

//
// Returns the ratio to adjust for differences in IRLED drive strength. Note
// that this does not help with LED irradiance variation.
//
static u32 ledi_ratio(u8 *buffer)
{

	struct operand_t op;
	u32              result;
	s16              index = cal_index( buffer );
	
	if ( index < 0 ) result = FX20_ONE;

	// op.op1 = LED_DRV65_REF; op.op2 = LED_DRV65;
	op.op1 = calref[ index ].ledi_65ma;
	op.op2 = LED_DRV65;
	result = fx20_divide( &op );

	if ( result == FX20_BAD_VALUE ) result = FX20_ONE;

	return result;
}

//
// This is a helper function called from si114x_get_calibration()
// Writes 0x11 to the Command Register, then populates buffer[12]
// and buffer[13] with the factory calibration index
//
static s16 si114x_get_cal_index( HANDLE si114x_handle, u8 *buffer )
{
	s16 retval, response;

	if ( ( si114x_handle == NULL ) || ( buffer == NULL ) )
	return -1;

	// Check to make sure that the device is ready to receive commands
	do
	{
		retval = Si114xNop( si114x_handle );
		if( retval != 0 ) return -1;

		response = Si114xReadFromRegister( si114x_handle, REG_RESPONSE );

	} while ( response != 0 );

	// Retrieve the index
	retval = Si114xWriteToRegister( si114x_handle, REG_COMMAND, 0x11 );
	_waitUntilSleep(si114x_handle);

	if( retval != 0 ) return -1;

	retval = Si114xBlockRead( si114x_handle, REG_PS1_DATA0, 2, &buffer[12] );
	if( retval != 0 ) return -1;

	return 0;
}


s16 si114x_get_calibration( HANDLE si114x_handle, SI114X_CAL_S *si114x_cal, char security)
{
	u8             buffer[14];
	s16 retval = 0, response;
	struct operand_t    op;
	u16                 cal_index;

	if ( si114x_handle == NULL ) { retval = -4; goto error_exit; }
	
	if ( si114x_cal    == NULL ) { retval = -4; goto error_exit; }

	// if requested, check to make sure the interface registers are zero
	// as an indication of a device that has not started any autonomous
	// operation
	if( security == 1 )
	{
		s8 i;

		retval = Si114xBlockRead( si114x_handle, REG_ALS_VIS_DATA0, 12, buffer );
		if( retval != 0 ) { retval = -2; goto error_exit; }

		for( i=0; i<12; i++)
		{
			if ( buffer[i] != 0 ) { retval = -1; goto error_exit; }
		}

		DEBUG_PRINT_OUTPUT;

	}

	// Check to make sure that the device is ready to receive commands
	do
	{
		retval = Si114xNop( si114x_handle );
		if( retval != 0 ) { retval = -2; goto error_exit; }

		response = Si114xReadFromRegister( si114x_handle, REG_RESPONSE );

	} while ( response != 0 );


	// Request for the calibration data
	retval = Si114xWriteToRegister( si114x_handle, REG_COMMAND, 0x12 );
	_waitUntilSleep(si114x_handle);

	if( retval != 0 ) { retval = -2; goto error_exit; }

	// Wait for the response register to increment
	do
	{
		response = Si114xReadFromRegister( si114x_handle, REG_RESPONSE );
		// If the upper nibbles are non-zero, something is wrong
		if ( response == 0x80 )
		{
			// calibration code has not been implemented on this device
			// leading to command error. So, rather than returning an
			// error, handle the error by Nop and set ratios to -1.0
			// and return normally.
			Si114xNop( si114x_handle );
			retval = -3;
			goto error_exit;
		}
		else if ( response & 0xfff0 )
		{
			// if upper nibble is anything but 0x80, exit with an error
			retval = -2;
			goto error_exit;
		}
	} while ( response != 1 );

	// Retrieve the 12 bytes from the interface registers
	retval = Si114xBlockRead( si114x_handle, REG_ALS_VIS_DATA0, 12, buffer );
	if( retval != 0 ) { retval = -2; goto error_exit; }

	DEBUG_PRINT_OUTPUT;

	if ( cal_index=si114x_get_cal_index( si114x_handle, buffer ) != 0 )
	{
		retval = -2; goto error_exit;
	}

	si114x_cal->vispd_correction = vispd_correction(buffer);
	si114x_cal->irpd_correction  = irpd_correction(buffer);
	si114x_cal->adcrange_ratio   = adcrange_ratio(buffer);
	si114x_cal->irsize_ratio     = irsize_ratio(buffer);
	si114x_cal->ledi_ratio       = ledi_ratio(buffer);
	si114x_cal->ucoef_p          = calref[cal_index].ucoef;

	DEBUG_PRINT_OUTPUT_2;

	return 0;

	error_exit:
	si114x_cal->vispd_correction = FX20_ONE;
	si114x_cal->irpd_correction  = FX20_ONE;
	si114x_cal->adcrange_ratio   = FLT_TO_FX20( 14.5 );
	si114x_cal->irsize_ratio     = FLT_TO_FX20(  6.0 );
	si114x_cal->ledi_ratio       = FX20_ONE;
	si114x_cal->ucoef_p          = NULL;
	return retval;
}

s16 si114x_set_ucoef( HANDLE si114x_handle, u8 *input_ucoef, SI114X_CAL_S *si114x_cal )
{
	s16     response;
	s16     temp;
	u32     vc=FX20_ONE, ic=FX20_ONE, long_temp;
	struct operand_t    op;
	u8     *ref_ucoef = si114x_cal->ucoef_p;
	u8     out_ucoef[4];
	
	if ( input_ucoef != NULL ) ref_ucoef = input_ucoef;
	
	if ( ref_ucoef == NULL )   return -1 ;

	// retrieve part identification
	response = Si114xReadFromRegister( si114x_handle, REG_PART_ID );
	switch( response )
	{
		case 0x32: case 0x45: case 0x46: case 0x47: temp = 1; break;
		default:                                    temp = 0; break;
	}
	if( !temp ) return -1;

	if (si114x_cal != 0)
	{
		if (si114x_cal->vispd_correction > 0) vc = si114x_cal->vispd_correction;
		if (si114x_cal->irpd_correction  > 0) ic = si114x_cal->irpd_correction;
	}

	op.op1 = ref_ucoef[0] + ((ref_ucoef[1])<<8);
	op.op2 = vc;
	long_temp   = fx20_multiply( &op );
	out_ucoef[0] = (long_temp & 0x00ff);
	out_ucoef[1] = (long_temp & 0xff00)>>8;

	op.op1 = ref_ucoef[2] + (ref_ucoef[3]<<8);
	op.op2 = ic;
	long_temp   = fx20_multiply( &op );
	out_ucoef[2] = (long_temp & 0x00ff);
	out_ucoef[3] = (long_temp & 0xff00)>>8;

	DEBUG_UCOEF

	response = Si114xBlockWrite( si114x_handle, REG_UCOEF0 , 4, out_ucoef);

	return response;
}
#else // INCLUDE_SI114X_CALIBRATION_CODE

s16 si114x_get_calibration( HANDLE si114x_handle, SI114X_CAL_S *si114x_cal, char security)
{
	// although the SI114x_CAL_S structure is not filled up properly, the set_ucoef() function will not use it.
	return 0;
}

s16 si114x_set_ucoef( HANDLE si114x_handle, u8 *input_ucoef, SI114X_CAL_S *si114x_cal )
{
	s16 response;
	u8 code ucoef[4] = { 0x7B, 0x6B, 0x01, 0x00 } ;
	
	// This will write 4 bytes starting with I2C address 0x13
	response = Si114xBlockWrite( si114x_handle, REG_UCOEF0, 4, &ucoef[0] );
	return response;
}

ISR(TCC1_OVF_vect) {
	counter++;
}

#endif // INCLUDE_SI114X_CALIBRATION_CODE
