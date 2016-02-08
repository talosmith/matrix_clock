/*
 * menu.c
 *
 * Created: 5/1/2015 10:35:39 PM
 *  Author: Administrator
 */ 

#include "menu.h"
#include "../../drivers/adc/adc.h"
#include "../../drivers/rtc/rtc.h"
#include "../../drivers/port/port.h"
#include "../../drivers/ht1632c/ht1632c.h"
#include "../../drivers/sensors/si114x/Si114x_functions.h"
#include "../../drivers/sensors/si114x/slider_algorithm.h"
#include "../../drivers/eeprom/eeprom.h"
#include "../../drivers/esp8266/esp8266.h"
#include "../../4x6_font.c"

#include "../json/jsmn.h"
#include "../json/json_functions.h"
#include "../display/display.h"
#include "../time_functions/time_functions.h"
#include "../fatfs/ff.h"
#include "../fatfs/sound.h"

#include <ctype.h>

static volatile uint16_t counter = 0;
static volatile bool GET_request = false;

#define TOKEN_BUFFER_SIZE 30

void play_sound(void)
{
	FATFS FatFs;		// FatFs work area needed for each volume
	FIL Fil;			// File object needed for each open file
	BYTE Buff[512];		// Working buffer 1024
	UINT bw;
	
	if (f_open(&Fil, "newfilea2.txt", FA_WRITE | FA_CREATE_ALWAYS) == FR_OK) {	// Create a file
		
		f_write(&Fil, "It works!\r\n", 11, &bw);	// Write data to the file
		
		f_close(&Fil);								// Close the file
	}
	
	
	f_mount(&FatFs, "", 0);
	
	BYTE res;
	res = f_open(&Fil, "rath3.wav", FA_READ);
	if (!res) {
		load_wav(&Fil, "**** WAV PLAYER ****", Buff, sizeof Buff);
		f_close(&Fil);
	}
}

void start_wifi_indication(void)
{
	display_clear_screen();
	
	PR.PRPC &= ~0x01;
	
	TCC0.CNT = 0;
	TCC0.PER = 0xffff;
	TCC0.CTRLA = TC_CLKSEL_DIV1024_gc;
	
	TCC0.CCD = 0xffff;
	TCC0.INTCTRLB |= TC_CCDINTLVL_LO_gc;
	TCC0.CTRLB |= TC0_CCDEN_bm;
}

void stop_wifi_indication(void)
{
	TCC0.CTRLA = TC_CLKSEL_OFF_gc;
	display_clear_screen();
	PR.PRPC |= 0x01;
}

void start_loading(void) 
{
	display_clear_screen();
	
	PR.PRPC &= ~0x01;
	
	TCC0.CNT = 0;
	TCC0.PER = 3125;
	TCC0.CTRLA = TC_CLKSEL_DIV1024_gc;
	
	TCC0.CCA = 3125;
	TCC0.INTCTRLB |= TC_CCAINTLVL_LO_gc;
	TCC0.CTRLB |= TC0_CCAEN_bm;
}

void stop_loading(void) 
{
	TCC0.CTRLA = TC_CLKSEL_OFF_gc;
	display_clear_screen();
	PR.PRPC |= 0x01;
}

esp8266_status_t get_internet_variables(bool get_time, bool get_temperature) 
{	
	char json_string[100];
	char token_buffer[30];
	jsmntok_t tokens[10];
	jsmn_parser p;
	jsmnerr_t r;
	
	esp8266_status_t status;
	
	status = esp8266_setup();
	if (status != ESP8266_SUCCESS) {
		return status;
	}
	status = esp8266_join_ap(env.wifi_ssid,env.wifi_pswd);
	if (status != ESP8266_SUCCESS) {
		return status;
	}
	if (get_time) {
		status = esp8266_get_json(TIME_IP,TIME_ADDR,json_string, sizeof(json_string));
	} else if (get_temperature) {
		status = esp8266_get_json(WEATHER_IP,WEATHER_ADDR,json_string, sizeof(json_string));
	} else {
		return ESP8266_ERROR;
	}
	if (status != ESP8266_SUCCESS) {
		return status;
	}
	
	esp8266_off();
	
#ifdef DEBUG_ON
	puts(json_string);
#endif

	//Parse JSON
	jsmn_init(&p);
	r = jsmn_parse(&p,json_string,strlen(json_string),tokens,sizeof(tokens));
	if (r < 0) {
		return ESP8266_ERROR;
	}
	
	if (get_time) {
		int year, month, day, hour, minute, second;
		char weekday[10];
		json_get_token(tokens,json_string,token_buffer,sizeof(token_buffer),2);
#ifdef DEBUG_ON
		puts(token_buffer);
#endif
		if (sscanf(token_buffer,"%d-%d-%d %s %d:%d:%d",&year, &month, &day, weekday, &hour, &minute, &second) != 7) {
			return ESP8266_ERROR;
		} else {
			env.time.year = year;
			env.time.month = month;
			env.time.day = day;
			env.time.hours = hour + env.time.timezone + env.time.DST;
			env.time.minutes = minute;
			env.time.seconds = second;
			env.time.weekday = time_get_weekday(day,month,year);
			
			if (env.time.hours >= 24) {
				env.time.hours -= 24;
				if (env.time.weekday++ >= Sunday) {
					env.time.weekday = Monday;
				}
				if (env.time.day++ >= time_get_days_in_month(env.time.month,env.time.year)) {
					env.time.day = 1;
					if (env.time.month++ >= December) {
						env.time.month = January;
						env.time.year++;
					}
				}
			}
			env.time.week = time_get_weeknumber(env.time.day,env.time.month,env.time.year);
		}
	}
	if (get_temperature) {
		json_get_token(tokens,json_string,token_buffer,sizeof(token_buffer),6);
#ifdef DEBUG_ON
		puts(token_buffer);
#endif
		if (strlen(token_buffer) <= sizeof(env.temperature)) {
			strncpy(env.temperature,token_buffer,strlen(token_buffer));
		} else {
			return ESP8266_ERROR;
		}
		json_get_token(tokens,json_string,token_buffer,sizeof(token_buffer),8);
#ifdef DEBUG_ON
		puts(token_buffer);
#endif
		if (strlen(token_buffer) < sizeof(env.weather_info)) {
			strncpy(env.weather_info,token_buffer,strlen(token_buffer));
		} else {
			return ESP8266_ERROR;
		}
		json_get_token(tokens,json_string,token_buffer,sizeof(token_buffer),2);
#ifdef DEBUG_ON
		puts(token_buffer);
#endif
		if (strlen(token_buffer) < sizeof(env.city)) {
			strncpy(env.city,token_buffer,strlen(token_buffer));
		} else {
			return ESP8266_ERROR;
		}

	}
	return ESP8266_SUCCESS;
}

void menu_draw_temperature_frame(void) 
{
	display_draw_small_char(1,10,'T',1,1);
	display_draw_small_char(5,10,'E',1,1);
	display_draw_small_char(9,10,'M',1,1);
	display_draw_small_char(13,10,'P',1,1);
	
	if (strlen(env.temperature) < 2) {
		display_draw_small_char(5,3,env.temperature[0],1,1);
		display_draw_small_char(9,3,'C',1,1);	
	} else {
		display_draw_small_char(3,3,env.temperature[0],1,1);
		display_draw_small_char(7,3,env.temperature[1],1,1);
		display_draw_small_char(11,3,'C',1,1);	
	}
}

void menu_draw_date_frame(void) 
{
	display_draw_small_char(1,10,'D',1,1);
	display_draw_small_char(5,10,'A',1,1);
	display_draw_small_char(9,10,'T',1,1);
	display_draw_small_char(13,10,'E',1,1);
	
	char buffer[2];
	char temp;
	
	itoa_simple(buffer,env.time.day);
	
	//append zero
	if (env.time.day < 10) {
		temp = buffer[0];
		buffer[0] = '0';
		buffer[1] = temp;
	}
	
	display_draw_small_char(1,3,buffer[0],1,1);
	display_draw_small_char(4,3,buffer[1],1,1);
	display_draw_small_char(7,3,'.',1,1);
	
	itoa_simple(buffer,env.time.month);
	
	//append zero
	if (env.time.month < 10) {
		temp = buffer[0];
		buffer[0] = '0';
		buffer[1] = temp;
	}
	
	display_draw_small_char(10,3,buffer[0],1,1);
	display_draw_small_char(13,3,buffer[1],1,1);
}

void menu_draw_week_frame(void) 
{
	display_draw_small_char(1,10,'W',1,1);
	display_draw_small_char(5,10,'E',1,1);
	display_draw_small_char(9,10,'E',1,1);
	display_draw_small_char(13,10,'K',1,1);
	
	char buffer[2];
	char temp;
	
	itoa_simple(buffer,env.time.week);
	
	//append zero
	if (env.time.week < 10) {
		temp = buffer[0];
		buffer[0] = '0';
		buffer[1] = temp;
	}
	
	display_draw_small_char(5,3,buffer[0],1,1);
	display_draw_small_char(9,3,buffer[1],1,1);
}

void menu_draw_weekday_frame(void) 
{
	display_draw_small_char(1,10,'W',1,1);
	display_draw_small_char(5,10,'D',1,1);
	display_draw_small_char(9,10,'A',1,1);
	display_draw_small_char(13,10,'Y',1,1);
	
	char buffer[3];
	strncpy(buffer,time_get_day_name(env.time.weekday),3);
	
	display_draw_small_char(3,3,buffer[0],1,1);
	display_draw_small_char(7,3,buffer[1],1,1);
	display_draw_small_char(11,3,buffer[2],1,1);
}

void menu_draw_battery_frame(void) 
{	
	display_draw_small_char(1,10,'B',1,1);
	display_draw_small_char(5,10,'A',1,1);
	display_draw_small_char(9,10,'T',1,1);
	display_draw_small_char(13,10,'T',1,1);
	
	uint8_t battery = adc_get_battery_percentage();
	
	char buffer[5];
	itoa_simple(buffer,battery);
	
	display_draw_small_char(3,3,buffer[0],1,1);
	display_draw_small_char(7,3,buffer[1],1,1);
	display_draw_small_char(11,3,'%',1,1);
	
}

void menu_draw_config_frame(void) 
{
	display_draw_four_letter_word("CONF");
}

void menu_set_env_variables(void) 
{
	strncpy(env.name,CLOCK_NAME,sizeof(env.name));
	env.id = CLOCK_ID;
	env.menu_id = 0;
	env.temperature[0] = '0';
	env.brightness = 0;
	strncpy(env.wifi_pswd,PASS, strlen(PASS));
	strncpy(env.wifi_ssid,SSID, strlen(SSID));
	
	env.time.timezone = 1;
	env.time.DST = 0;
	env.time.seconds = 0;
	env.time.minutes = 0;
	env.time.hours = 0;
	env.time.day = 3;
	env.time.month = Februrary;
	env.time.year = 2016;
	env.time.weekday = time_get_weekday(env.time.day, env.time.month, env.time.year);
	env.time.week = time_get_weeknumber(env.time.day, env.time.month, env.time.year);;
	
	env.alarm.hours = 0;
	env.alarm.minutes = 0;
	
	env.runtime = 0;
}

menu_status_t menu_state_machine(SI114X_IRQ_SAMPLE *samples) 
{	
	//Menu ID select
	if (samples->gesture != NO_GESTURE) {
		if (samples->gesture == LEFT_SWIPE) {
			if (env.menu_id != NUMBER_OF_MENUS) {
				env.menu_id++;
			} else {
				env.menu_id = 0;
			}
		} else if (samples->gesture == RIGHT_SWIPE) {
			if (env.menu_id != 0) {
				env.menu_id--;
			} else {
				env.menu_id = NUMBER_OF_MENUS;
			}
		}
	}
	
	if (samples->gesture == LEFT_SWIPE) {
		rtc_disable_time_render();
		display_slide_out_to_right();
	} else if (samples->gesture == RIGHT_SWIPE) {
		rtc_disable_time_render();
		display_slide_out_to_left();
	}
	
	//Switch between menus
	if (samples->gesture != NO_GESTURE) {
		switch(env.menu_id) {
			case MENU_TIME:
				rtc_enable_time_render();
				break;
			case MENU_DATE:
				menu_draw_date_frame();
				break;
			case MENU_WEEKDAY:
				menu_draw_weekday_frame();
				break;
			case MENU_WEEK:
				menu_draw_week_frame();
				break;
			case MENU_TEMP:
				menu_draw_temperature_frame();
				break;
			case MENU_BATTERY:
				menu_draw_battery_frame();
				break;
			case MENU_CONFIG:
				menu_draw_config_frame();
				break;
			default:
				break;
		}	
	}
	
	if (samples->gesture == LEFT_SWIPE) {
		display_slide_in_from_left();
	} else if (samples->gesture == RIGHT_SWIPE) {
		display_slide_in_from_right();
	}

	if (samples->gesture == PAUSE) {
		if (env.menu_id == MENU_CONFIG) {
			Si114xPauseAll((HANDLE)SI114X_ADDR);
			display_fade_blink();
			display_slide_out_to_top();
#ifdef SHOW_MANUAL
			display_print_scrolling_text("USE BACK BUTTONS",false);
#endif
			display_draw_four_letter_word("<  >");
			display_slide_in_from_bottom();
			env.menu_id = 0;
			while(menu_configuration(samples) == MENU_SUCCESS);
			Si114xPsAlsAuto((HANDLE)SI114X_ADDR);
			env.menu_id = 0;
			rtc_enable_time_render();
			display_slide_in_from_top();
			//A little time to remove fingers from back buttons
			_delay_ms(1000);
		} else if (env.menu_id == MENU_TEMP) {
			esp8266_status_t status;
			Si114xPauseAll((HANDLE)SI114X_ADDR);
			display_fade_blink();			
			display_slide_out_to_top();
			start_loading();
			esp8266_on();
			status = get_internet_variables(false,true);
			esp8266_off();
			stop_loading();
			
			if (status == ESP8266_SUCCESS) {
				uint8_t i = 0;
				char weather_info[60];
				strcpy(weather_info, "WEATHER FOR ");
				while(env.city[i]) {
					env.city[i] = toupper(env.city[i]);
					i++;
				}
				strcat(weather_info,env.city);
				strcat(weather_info,": ");
				strcat(weather_info,env.temperature);
				strcat(weather_info,"C ");
				i = 0;
				while(env.weather_info[i]) {
					env.weather_info[i] = toupper(env.weather_info[i]);
					i++;
				}
				strcat(weather_info,env.weather_info);
#ifdef SHOW_MANUAL
				display_print_scrolling_text(weather_info, false);
#endif
			} else {
#ifdef SHOW_MANUAL
				display_print_scrolling_text("COULD NOT GET TEMPERATURE", false);
#endif
			}
			menu_draw_temperature_frame();
			display_slide_in_from_top();
			Si114xPsAlsAuto((HANDLE)SI114X_ADDR);
		} 
	}
	return MENU_SUCCESS;
}

menu_status_t menu_configuration(SI114X_IRQ_SAMPLE *samples) 
{	
	btn_status = btn_check_press();
	
	//Menu ID select
	if (btn_status != NO_BTN) {
		if (btn_status == BTN1) {
			if (env.menu_id != NUMBER_OF_CONFIG_MENUS) {
				env.menu_id++;
			} else {
				env.menu_id = 0;
			}
		} else if (btn_status == BTN4) {
			if (env.menu_id != 0) {
				env.menu_id--;
			} else {
				env.menu_id = NUMBER_OF_CONFIG_MENUS;
			}
		}
	}
	
	if (btn_status == BTN1) {
		rtc_disable_time_render();
		display_slide_out_to_right();
	} else if (btn_status == BTN4) {
		rtc_disable_time_render();
		display_slide_out_to_left();
	} else if (btn_status == BTN3) {
		display_fade_blink();
	}
	
	//Switch between menus
	if (btn_status != NO_BTN) {
		switch(env.menu_id) {
			case CONFIG_BRIGHTNESS:
				display_draw_four_letter_word("LGHT");
				break;
			case CONFIG_TIME:
				display_draw_four_letter_word("TIME");
				break;
			case CONFIG_WIFI:
				display_draw_four_letter_word("WIFI");
				break;
			case CONFIG_ALARM:
				display_draw_four_letter_word("ALRM");
				break;
			case CONFIG_CALIBRATE:
				display_draw_four_letter_word("CALI");
				break;
			case CONFIG_INFO:
				display_draw_four_letter_word("INFO");
				break;
			case CONFIG_MUSIC:
				display_draw_four_letter_word("DEMO");
				break;
			case CONFIG_EXIT:
				display_draw_four_letter_word("EXIT");
				break;
			default:
				break;
		}	
	}
	
	if (btn_status == BTN1) {
		display_slide_in_from_left();
	} else if (btn_status == BTN4) {
		display_slide_in_from_right();
	}
	
	//Menu actions
	if (btn_status == BTN3) {
		switch(env.menu_id) {
			case CONFIG_BRIGHTNESS:
				display_slide_out_to_top();
				menu_configure_brightnesss();
				ht1632c_set_brightness(MAX_BRIGHTNESS);
				break;
			case CONFIG_TIME:
				display_slide_out_to_top();
				menu_set_time();
				break;
			case CONFIG_WIFI:
				display_slide_out_to_top();
				display_print_scrolling_text("CONNECT TO AP AND ENTER THE UPCOMMING IP ADDRESS",false);
				//Feature: Should draw wifi lines on display here.
				esp8266_on();
				if (esp8266_setup_webserver(false,true) == ESP8266_SUCCESS) {
					//start_wifi_indication();
					while(esp8266_configure_ssid_and_password() != ESP8266_TIMEOUT) {
						btn_status = btn_check_press();
						if (btn_status == BTN4) {
							break;
						}
						if (wdt_triggered) {
							//stop_wifi_indication();
#ifdef SHOW_MANUAL
							display_print_scrolling_text("SOMETHING WENT WRONG",false);
#endif
						}
					}
					//stop_wifi_indication();
				} else {
#ifdef SHOW_MANUAL
					display_print_scrolling_text("SOMETHING WENT WRONG",false);
#endif					
				}
				esp8266_off();
				break;
			case CONFIG_ALARM:
				display_slide_out_to_top();
				menu_set_alarm();
				break;
			case CONFIG_CALIBRATE:
				display_slide_out_to_top();
				si114x_baseline_calibration(samples);
				break;
			case CONFIG_INFO:
				display_slide_out_to_top();
				display_print_scrolling_text("MADE BY: ERLEND HESTNES",false);
				break;
			case CONFIG_MUSIC:
				display_slide_out_to_bottom();
				display_draw_bitmap(5,4,IMG_SPEAKER_A,IMG_SPEAKER_WIDTH,IMG_SPEAKER_HEIGHT,1);
				display_slide_in_from_bottom();
				play_sound();
				break;
			case CONFIG_EXIT:
				display_slide_out_to_bottom();
				return MENU_CLOSE;
			default:
				break;
		}
		env.menu_id = 0;
		display_slide_out_to_bottom();
		display_draw_four_letter_word("<  >");
		display_slide_in_from_top();
	}	
	return MENU_SUCCESS;
}

void menu_configure_brightnesss(void) 
{	
	bool quit = false;
	
	ht1632c_set_brightness(env.brightness);
	display_draw_filled_rect(0,7,env.brightness+1,2,1);
	display_refresh_screen();
	
	while(!quit) {
		btn_status = btn_check_press();

		switch(btn_status) {
			case BTN4:
				if (env.brightness < 15) {
					display_draw_filled_rect(0,7,env.brightness+1,2,0);
					ht1632c_set_brightness(++env.brightness);
					display_draw_filled_rect(0,7,env.brightness+1,2,1);
					display_refresh_screen();
				}
				_delay_ms(100);
				break;
			case BTN1:
				if (env.brightness > 0) {
					display_draw_filled_rect(0,7,env.brightness+1,2,0);
					ht1632c_set_brightness(--env.brightness);
					display_draw_filled_rect(0,7,env.brightness+1,2,1);
					display_refresh_screen();
				}
				_delay_ms(100);
				break;
			case BTN3:
				display_slide_out_to_bottom();
#ifdef SHOW_MANUAL
				display_print_scrolling_text("BRIGHTNESS SET",false);
#endif
				quit = true;
				EEPROM_WriteEnv();
				break;
			case BTN2:
#ifdef SHOW_MANUAL
				display_print_scrolling_text("EXIT",false);
#endif
				return;
			default:
				_delay_ms(100);
				btn_status = NO_BTN;
				break;
		}
	}
}

uint8_t menu_set_time(void) 
{	
	bool next = false;
	char buffer[3];
	
	display_draw_three_letter_word("GMT");
	display_slide_in_from_right();
	_delay_ms(1000);
	display_slide_out_to_left();
	itoa_simple(buffer,env.time.timezone);
	display_draw_small_char(3,10,'G',1,1);
	display_draw_small_char(7,10,'M',1,1);
	display_draw_small_char(11,10,'T',1,1);
	if (env.time.timezone > 0) {
		display_draw_small_char(5,3,'+',1,1);
		} else if (env.time.timezone < 0) {
		display_draw_small_char(5,3,'-',1,1);
	}
	display_draw_small_char(9,3,buffer[0],1,1);
	display_slide_in_from_right();
	//ht1632c_blink(true);
	
	while(!next) {
		btn_status = btn_check_press();
		switch(btn_status) {
			case BTN4:
				if (env.time.timezone < 9) {
					display_clear_buffer();
					env.time.timezone++;
					itoa_simple(buffer,env.time.timezone);
					display_draw_small_char(3,10,'G',1,1);
					display_draw_small_char(7,10,'M',1,1);
					display_draw_small_char(11,10,'T',1,1);
					if (env.time.timezone > 0) {
						display_draw_small_char(5,3,'+',1,1);
						display_draw_small_char(9,3,buffer[0],1,1);
					} else if (env.time.timezone < 0) {
						display_draw_small_char(5,3,'-',1,1);
						display_draw_small_char(9,3,buffer[1],1,1);
					} else {
						display_draw_small_char(8,3,'0',1,1);
					}
					display_refresh_screen();
				}
				_delay_ms(250);
				break;
			case BTN1:
				if (env.time.timezone > -9) {
					display_clear_buffer();
					env.time.timezone--;
					itoa_simple(buffer,env.time.timezone);
					display_draw_small_char(3,10,'G',1,1);
					display_draw_small_char(7,10,'M',1,1);
					display_draw_small_char(11,10,'T',1,1);
					if (env.time.timezone > 0) {
						display_draw_small_char(5,3,'+',1,1);
						display_draw_small_char(9,3,buffer[0],1,1);
					} else if (env.time.timezone < 0) {
						display_draw_small_char(5,3,'-',1,1);
						display_draw_small_char(9,3,buffer[1],1,1);
					} else {
						display_draw_small_char(8,3,'0',1,1);
					}
					display_refresh_screen();
				}
				_delay_ms(250);
				break;
			case BTN3:
				next = true;
				//ht1632c_blink(false);
				_delay_ms(250);
				break;
			case BTN2:
				//ht1632c_blink(false);
				display_slide_out_to_bottom();
#ifdef SHOW_MANUAL
				display_print_scrolling_text("CANCELLED",false);
#endif
				return 0;
			default:
				btn_status = NO_BTN;
				break;
		}
	}
	next = false;
	display_slide_out_to_left();
	display_draw_three_letter_word("DST");
	display_slide_in_from_right();
	_delay_ms(1000);
	display_slide_out_to_left();
	if (env.time.DST) {
		display_draw_four_letter_word(" ON ");
	} else {
		display_draw_three_letter_word("OFF");
	}
	display_slide_in_from_right();
	//ht1632c_blink(true);
	
	//Set hours
	while(!next) {
		btn_status = btn_check_press();
		switch(btn_status) {
			case BTN4:
				display_clear_buffer();
				env.time.DST = 1;
				display_draw_four_letter_word(" ON ");
				display_refresh_screen();
				_delay_ms(250);
				break;
			case BTN1:
				display_clear_buffer();
				env.time.DST = 0;
				display_draw_three_letter_word("OFF");
				display_refresh_screen();
				_delay_ms(250);
				break;
			case BTN3:
				next = true;
				//ht1632c_blink(false);
				_delay_ms(250);
				break;
			case BTN2:
				//ht1632c_blink(false);
				display_slide_out_to_bottom();
#ifdef SHOW_MANUAL
				display_print_scrolling_text("CANCELLED",false);
#endif
				return 0;
			default:
				btn_status = NO_BTN;
				break;
		}
	}
	next = false;
	display_slide_out_to_left();
	display_draw_three_letter_word("HRS");
	display_slide_in_from_right();
	_delay_ms(1000);
	display_slide_out_to_left();
	rtc_update_display(5,env.time.hours);
	display_slide_in_from_right();
	//ht1632c_blink(true);
	
	//Set hours
	while(!next) {
		btn_status = btn_check_press();
		switch(btn_status) {
			case BTN4:
				display_clear_buffer();
				display_draw_and_increment_hour();
				_delay_ms(250);
				break;
			case BTN1:
				display_clear_buffer();
				display_draw_and_decrement_hour();
				_delay_ms(250);
				break;
			case BTN3:
				next = true;
				_delay_ms(250);
				break;
			case BTN2:
				//ht1632c_blink(false);
				display_slide_out_to_bottom();
#ifdef SHOW_MANUAL
				display_print_scrolling_text("CANCELLED",false);
#endif
				return 0;
			default:
				btn_status = NO_BTN;
				break;
		}
	}
	next = false;
	
	//ht1632c_blink(false);
	display_slide_out_to_left();
	display_draw_three_letter_word("MIN");
	display_slide_in_from_right();
	_delay_ms(1000);
	display_slide_out_to_left();
	rtc_update_display(5,env.time.minutes);
	display_slide_in_from_right();
	//ht1632c_blink(true);
	
	//Set minutes
	while(!next) {
		btn_status = btn_check_press();
		switch(btn_status) {
			case BTN4:
				display_clear_buffer();
				display_draw_and_increment_minute();
				_delay_ms(250);
				break;
			case BTN1:
				display_clear_buffer();
				display_draw_and_decrement_minute();
				_delay_ms(250);
				break;
			case BTN3:
				next = true;
				_delay_ms(250);
				break;
			case BTN2:
				//ht1632c_blink(false);
				display_slide_out_to_bottom();
#ifdef SHOW_MANUAL
				display_print_scrolling_text("CANCELLED",false);
#endif
				return 0;
			default:
				btn_status = NO_BTN;
				break;
		}
	}
	next = false;
	
	//ht1632c_blink(false);
	display_slide_out_to_left();
	display_draw_four_letter_word("YEAR");
	display_slide_in_from_right();
	_delay_ms(1000);
	display_slide_out_to_left();
	char *year = NULL;
	itoa_simple(year,env.time.year);
	display_draw_four_letter_word(year);
	display_slide_in_from_right();
	//ht1632c_blink(true);
		
	//Set year
	while(!next) {
		btn_status = btn_check_press();
		switch(btn_status) {
			case BTN4:
				display_clear_buffer();
				display_draw_and_increment_year();
				_delay_ms(250);
				break;
			case BTN1:
				display_clear_buffer();
				display_draw_and_decrement_year();
				_delay_ms(250);
				break;
			case BTN3:
				next = true;
				_delay_ms(250);
				break;
			case BTN2:
				//ht1632c_blink(false);
				display_slide_out_to_bottom();
#ifdef SHOW_MANUAL
				display_print_scrolling_text("CANCELLED",false);
#endif
				return 0;
			default:
				btn_status = NO_BTN;
				break;
		}
	}
	next = false;
	
	//ht1632c_blink(false);
	display_slide_out_to_left();
	display_draw_four_letter_word("MNTH");
	display_slide_in_from_right();
	_delay_ms(1000);
	display_slide_out_to_left();
	display_draw_three_letter_word(time_get_month_name(env.time.month));
	display_slide_in_from_right();
	//ht1632c_blink(true);
	
	//Set month
	while(!next) {
		btn_status = btn_check_press();
		switch(btn_status) {
			case BTN4:
				display_clear_buffer();
				display_draw_and_increment_month();
				_delay_ms(250);
				break;
			case BTN1:
				display_clear_buffer();
				display_draw_and_decrement_month();
				_delay_ms(250);
				break;
			case BTN3:
				next = true;
				_delay_ms(250);
				break;
			case BTN2:
				//ht1632c_blink(false);
				display_slide_out_to_bottom();
#ifdef SHOW_MANUAL
				display_print_scrolling_text("CANCELLED",false);
#endif
				return 0;
			default:
				btn_status = NO_BTN;
				break;
		}
	}
	next = false;
	
	//ht1632c_blink(false);
	display_slide_out_to_left();
	display_draw_three_letter_word("DAY");
	display_slide_in_from_right();
	_delay_ms(1000);
	display_slide_out_to_left();
	rtc_update_display(5,env.time.day);
	display_slide_in_from_right();
	//ht1632c_blink(true);
	
	//Set days
	while(!next) {
		btn_status = btn_check_press();
		switch(btn_status) {
			case BTN4:
				display_clear_buffer();
				display_draw_and_increment_day();
				_delay_ms(250);
				break;
			case BTN1:
				display_clear_buffer();
				display_draw_and_decrement_day();
				_delay_ms(250);
				break;
			case BTN3:
				next = true;
				_delay_ms(250);
				break;
			case BTN2:
				//ht1632c_blink(false);
				display_slide_out_to_bottom();
#ifdef SHOW_MANUAL
				display_print_scrolling_text("CANCELLED",false);
#endif
				return 0;
			default:
				btn_status = NO_BTN;
				break;
		}
	}
	next = false;
	
	//Calculate the rest
	env.time.weekday = time_get_weekday(env.time.day,env.time.month,env.time.year);
	env.time.week = time_get_weeknumber(env.time.day,env.time.month,env.time.year);
	
	//ht1632c_blink(false);
	EEPROM_WriteEnv();
	display_slide_out_to_bottom();
	
#ifdef SHOW_MANUAL
	display_print_scrolling_text("TIME AND DATE SET",false);
#endif
	
	return 1;
}

uint8_t menu_set_alarm(void) 
{
	bool next = false;
	
	display_draw_three_letter_word("HRS");
	display_slide_in_from_right();
	_delay_ms(1000);
	display_slide_out_to_left();
	rtc_update_display(5,env.alarm.hours);
	display_slide_in_from_right();
	//ht1632c_blink(true);
	
	//Set hours
	while(!next) {
		btn_status = btn_check_press();
		switch(btn_status) {
			case BTN4:
				display_clear_buffer();
				display_alarm_increment_hour();
				_delay_ms(150);
				break;
			case BTN1:
				display_clear_buffer();
				display_alarm_decrement_hour();
				_delay_ms(150);
				break;
			case BTN3:
				next = true;
				_delay_ms(250);
				break;
			case BTN2:
				//ht1632c_blink(false);
				display_slide_out_to_bottom();
#ifdef SHOW_MANUAL
				display_print_scrolling_text("CANCELLED",false);
#endif
				return 0;
			default:
				btn_status = NO_BTN;
				break;
		}
	}
	next = false;
	
	//ht1632c_blink(false);
	display_slide_out_to_left();
	display_draw_three_letter_word("MIN");
	display_slide_in_from_right();
	_delay_ms(1000);
	display_slide_out_to_left();
	rtc_update_display(5,env.alarm.minutes);
	display_slide_in_from_right();
	//ht1632c_blink(true);
	
	//Set minutes
	while(!next) {
		btn_status = btn_check_press();
		switch(btn_status) {
			case BTN4:
				display_clear_buffer();
				display_alarm_increment_minute();
				_delay_ms(150);
				break;
			case BTN1:
				display_clear_buffer();
				display_alarm_decrement_minute();
				_delay_ms(150);
				break;
			case BTN3:
				next = true;
				_delay_ms(250);
				break;
			case BTN2:
				//ht1632c_blink(false);
				display_slide_out_to_bottom();
#ifdef SHOW_MANUAL
				display_print_scrolling_text("CANCELLED",false);
#endif
				return 0;
			default:
				btn_status = NO_BTN;
				break;
		}
	}
	next = false;
	
	//ht1632c_blink(false);
	EEPROM_WriteEnv();
	display_slide_out_to_bottom();
	
#ifdef SHOW_MANUAL
	display_print_scrolling_text("ALARM SET",false);
#endif

	return 1;
}

ISR(TCC0_CCA_vect) 
{
	display_show_loading_square();
}

ISR(TCC0_CCD_vect)
{
	display_draw_wifi_icon();
}