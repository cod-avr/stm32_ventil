#include <stdio.h>
#include <stdlib.h>
//#include "stm32_adafruit_spi_usd.h"
//#include "stm32_adafruit_spi_lcd.h"


#define Bank1_PSRAM1_ADDR	((uint32_t)0x60000000)
#define ADC1_DR_Address  0x4001244C
#define SPI1_DR_Address  0x4001300C


#define FlashLastPageAddr    ((uint32_t)0x08007c00)	   //F103C8: 0x0801f800   //F103VG: 0x080ff800
#define FlashPageSize	  0x400

#define ModBusUartSpeed	  19200

#define BKPSize 10

ErrorStatus HSEStartUpStatus;

DMA_InitTypeDef DMA_InitStructure;

u16 ADC_DMA_buffer[2];

//--- ������������ ������� -- ����� ������������ ��������� ��� ������ �� ������� ---------
struct corr_conf_t
  {
    u8 debug_mode;		     //0  ���������� �����
    u8 start_us_measure;	 //0  ����� ������������ � ���������� ������
    s16 press_i2c;			 //1	 ��������
    s16 temper_i2c;			 //2	 �����������
    u16 i2c_sens_error; 	 //3
    u16 i2c_success_count;	 //4	 ����� ������� ���������
    u16 OurMBAddress;		 //5	 ����� �� �������
    s16 press_offset;		 //6     �������� ���� ��������
    s16 rawtemper_i2c;		 //7	����� �����������
    s16 rawpress_i2c;		 //8	����� ��������
    s16 kf_press;		     //9	����������� ��������� ��������
    s16 temper_base;		 //A ����������� ��������� �������, ������� �����������. ���� * 100
    u16 press_diap;			 //B �������� ������� �� ����������� �������� ��������� ���������
    s16 tcomp_offset;		 //C �������� ���������������� ����
    s16 tcomp_diap;			 //D �������� ���������������� ���������

    u16 ustav;               //E ������� �� ��������
    s16 error;               //F ������ ����������
    u16 power;               //10 ��������
    s16 filter;              //11 ������ �����������

    u16 PCA9534_0;               //12 18d ������� 0 ������ ������ (8 ������� ������� PCA9534) � ������� �����
    u16 PCA9534_1;               //13 19d ������� 1 (8 �������� ������� PCA9534) � ������� �����
    u16 PCA9534_3;               //14 20d ������� 3 (8 ��� ����������� PCA9534) � ������� �����

    s16 temperature;         //15  21d   �����������
    u16 rheostat;            //16  22d   ��������� ���������
//    u16 PCA9534_2;               //17  23d    ������� 2 ������ ������ (�������� ������ PCA9534) � ������� �����

    u16 WorkMode;            //17  23d   ����� ������ ������������
    u16 LightTime;            //18  24d   ����� ��������� ���������
    u16 BlowTime;            //19  25d   ����� ���������


  };

struct vent_conf_t
  {
  u16     fan_on;            //40 ��� �����������
  u16     set_select;        //41 ����� �������
  u16     con_alarm;         //42
  u16     fan_alarm;         //43
  u16     filter_alarm;	     //44
  u16     main_alarm;        //45
  u16     fan_overload;      //46
  u16     fan_power;         //47
  u16     flaw_set1;         //48 ������� 1
  u16     flaw_set2;         //49 ������� 2
  s16     cur_flaw;          //4A �������� ������ = K*sqrt(��������)  ������ ��� 1�� ������
  u16     min_pres;          //4B ����������� �������� �� �������
  u16     max_pres;          //4C ������������ �������� �� �������
  u16     FanOverloadSet;    //4D
  u16     UV_on;             //4E
  u16     LightOn;           //4F
  u16     WorkHours;         //50
  u16     WorkHoursReset;    //51
  u16     UVHours;           //52
  u16     UVHoursReset;      //53
  u16     Kfactor;           //54
  u16     Pcoef;             //55
  u16     Icoef;             //56
  u16     ZoneNumber;        //57
  u16     FVMNumber;         //58
  u16     FanSpeed;          //59
  u16     UVSecondCounter;   //5A ������� ������ ������ ��-�����
  u16     Version;           //5B
  u16     HX711_temper;      //5C
  u16     HX711_press;       //5D
  };

//--- ������������ ��������� -- ��� ������ �� ������� ---------
struct dev_conf_t
  {
	s16 devConfig[32];  //������������ ��������� ������-������ ��� ������� (������ == ���������  corr_conf_t)
  };

struct dev_conf_t dev_conf[3];

//������ ���������� (���������) ��� ������ �� ���� (����� �������� ������������ ����� � ��������� corr_conf_t)
#define CH2 (sizeof(dev_conf[0])/2)
//const uint8_t ParNumTable [] =	{5, 6, 9, 5+CH2, 6+CH2};
//const uint8_t ParNumTable [] =	{5, 6, 9, 0xa, 0xb, 6+CH2, };
const uint8_t ParNumTable [] =	{5, 6, 9, 0xa, 0xb, 6+CH2};
const uint8_t ParNumTableSize = sizeof(ParNumTable)/sizeof(ParNumTable[0]);
const uint8_t ParNumTableBKP [] =	{0x5A, 9+CH2, 0x54, 0xb+CH2, 0x52, 0x4b, 0x4c, 0x48, 0x49};
const uint8_t ParNumTableBKPSize = sizeof(ParNumTableBKP)/sizeof(ParNumTableBKP[0]);

#undef CH2

struct corr_conf_t *CorrConf[2];
struct vent_conf_t *VentConf;

//struct corr_conf_t *CorrConf2;

u8 RTCounter;	     //������� ������ �������
u8 frame[220];	    //���� ����� �����/��������
