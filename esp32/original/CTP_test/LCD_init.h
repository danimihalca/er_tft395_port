#ifndef LCD_init_h
#define LCD_init_h
#include "Arduino.h"
#include <SPI.h>

#define  SPI_CS     14
#define  SPI_CLK    13  
#define  SPI_DI     12  


#define  SPI_CS_RES   digitalWrite(SPI_CS, LOW)
#define  SPI_CS_SET   digitalWrite(SPI_CS, HIGH)
#define  SPI_CLK_RES   digitalWrite(SPI_CLK, LOW)
#define  SPI_CLK_SET   digitalWrite(SPI_CLK, HIGH)
#define  SPI_DI_RES   digitalWrite(SPI_DI, LOW)
#define  SPI_DI_SET   digitalWrite(SPI_DI, HIGH)



void HW_SPI_Send(unsigned char i)
{ 
  SPI.transfer( i);
}




//RGB+9b_SPI(rise)
 void SW_SPI_Send(unsigned char i)
{  
   unsigned char n;
   
   for(n=0; n<8; n++)			
   {       
			SPI_CLK_RES;
			if(i&0x80)SPI_DI_SET ;
                        else SPI_DI_RES ;

			SPI_CLK_SET;

			i<<=1;
	  
   }
}


void SPI_WriteComm(unsigned char i)
{
    SPI_CS_RES;
    SPI_DI_RES;//DI=0 COMMAND
 
    SPI_CLK_RES;
    SPI_CLK_SET;
  //  HW_SPI_Send(i);
    SW_SPI_Send(i);	
    SPI_CS_SET;	
}



void SPI_WriteData(unsigned char i)
{ 
    SPI_CS_RES;
    SPI_DI_SET;//DI=1 DATA
    SPI_CLK_RES;
    SPI_CLK_SET;
  //  HW_SPI_Send(i);   
    SW_SPI_Send(i);
    SPI_CS_SET;  		
} 





void nv3052_Initial(void)
{  
     pinMode(SPI_CS,   OUTPUT);
    digitalWrite(SPI_CS, HIGH);
    pinMode(SPI_CLK,   OUTPUT);
    digitalWrite(SPI_CLK, HIGH);
    pinMode(SPI_DI,   OUTPUT);
    digitalWrite(SPI_DI, HIGH); 
     
  
	// 720X720 BOE3.95 6G （B3 QV040YNQ-N80）
	SPI_WriteComm(0xFF);SPI_WriteData(0x30);
	SPI_WriteComm(0xFF);SPI_WriteData(0x52);
	SPI_WriteComm(0xFF);SPI_WriteData(0x01);
	SPI_WriteComm(0xE3);SPI_WriteData(0x00);
	SPI_WriteComm(0x0A);SPI_WriteData(0x00);
	SPI_WriteComm(0x23);SPI_WriteData(0x20);//a2
	SPI_WriteComm(0x24);SPI_WriteData(0x0f);
	SPI_WriteComm(0x25);SPI_WriteData(0x14);
	SPI_WriteComm(0x26);SPI_WriteData(0x2E);
	SPI_WriteComm(0x27);SPI_WriteData(0x2E);
	SPI_WriteComm(0x29);SPI_WriteData(0x02);
	SPI_WriteComm(0x2A);SPI_WriteData(0xCF);
	SPI_WriteComm(0x32);SPI_WriteData(0x34);
	SPI_WriteComm(0x38);SPI_WriteData(0x9C);
	SPI_WriteComm(0x39);SPI_WriteData(0xA7);
	SPI_WriteComm(0x3A);SPI_WriteData(0x77);   
	SPI_WriteComm(0x3B);SPI_WriteData(0x94);
	SPI_WriteComm(0x40);SPI_WriteData(0x07);
	SPI_WriteComm(0x42);SPI_WriteData(0x6D);
	SPI_WriteComm(0x43);SPI_WriteData(0x83);
	SPI_WriteComm(0x81);SPI_WriteData(0x00);
	SPI_WriteComm(0x91);SPI_WriteData(0x57);
	SPI_WriteComm(0x92);SPI_WriteData(0x57);
	SPI_WriteComm(0xA0);SPI_WriteData(0x52);
	SPI_WriteComm(0xA1);SPI_WriteData(0x50);
	SPI_WriteComm(0xA4);SPI_WriteData(0x9C);
	SPI_WriteComm(0xA7);SPI_WriteData(0x02);
	SPI_WriteComm(0xA8);SPI_WriteData(0x02);
	SPI_WriteComm(0xA9);SPI_WriteData(0x02);
	SPI_WriteComm(0xAA);SPI_WriteData(0xA8);
	SPI_WriteComm(0xAB);SPI_WriteData(0x28);
	SPI_WriteComm(0xAE);SPI_WriteData(0xD2);
	SPI_WriteComm(0xAF);SPI_WriteData(0x02);
	SPI_WriteComm(0xB0);SPI_WriteData(0xD2);
	SPI_WriteComm(0xB2);SPI_WriteData(0x26);
	SPI_WriteComm(0xB3);SPI_WriteData(0x26);
	
	SPI_WriteComm(0xFF);SPI_WriteData(0x30);
	SPI_WriteComm(0xFF);SPI_WriteData(0x52);
	SPI_WriteComm(0xFF);SPI_WriteData(0x02);
	
	SPI_WriteComm(0xB0);SPI_WriteData(0x02);
	SPI_WriteComm(0xB1);SPI_WriteData(0x0E);
	SPI_WriteComm(0xB2);SPI_WriteData(0x08);
	SPI_WriteComm(0xB3);SPI_WriteData(0x29);
	SPI_WriteComm(0xB4);SPI_WriteData(0x28);
	SPI_WriteComm(0xB5);SPI_WriteData(0x37);
	SPI_WriteComm(0xB6);SPI_WriteData(0x12);
	SPI_WriteComm(0xB7);SPI_WriteData(0x32);
	SPI_WriteComm(0xB8);SPI_WriteData(0x0B);
	SPI_WriteComm(0xB9);SPI_WriteData(0x03);
	SPI_WriteComm(0xBA);SPI_WriteData(0x0E);
	SPI_WriteComm(0xBB);SPI_WriteData(0x0D);
	SPI_WriteComm(0xBC);SPI_WriteData(0x10);
	SPI_WriteComm(0xBD);SPI_WriteData(0x13);
	SPI_WriteComm(0xBE);SPI_WriteData(0x18);
	SPI_WriteComm(0xBF);SPI_WriteData(0x0F);
	SPI_WriteComm(0xC0);SPI_WriteData(0x16);
	SPI_WriteComm(0xC1);SPI_WriteData(0x08);
	
	SPI_WriteComm(0xD0);SPI_WriteData(0x05);
	SPI_WriteComm(0xD1);SPI_WriteData(0x0B);
	SPI_WriteComm(0xD2);SPI_WriteData(0x03);
	SPI_WriteComm(0xD3);SPI_WriteData(0x33);
	SPI_WriteComm(0xD4);SPI_WriteData(0x32);
	SPI_WriteComm(0xD5);SPI_WriteData(0x32);
	SPI_WriteComm(0xD6);SPI_WriteData(0x0F);
	SPI_WriteComm(0xD7);SPI_WriteData(0x39);
	SPI_WriteComm(0xD8);SPI_WriteData(0x0B);
	SPI_WriteComm(0xD9);SPI_WriteData(0x02);
	SPI_WriteComm(0xDA);SPI_WriteData(0x10);
	SPI_WriteComm(0xDB);SPI_WriteData(0x0F);
	SPI_WriteComm(0xDC);SPI_WriteData(0x11);
	SPI_WriteComm(0xDD);SPI_WriteData(0x14);
	SPI_WriteComm(0xDE);SPI_WriteData(0x1A);
	SPI_WriteComm(0xDF);SPI_WriteData(0x11);
	SPI_WriteComm(0xE0);SPI_WriteData(0x18);
	SPI_WriteComm(0xE1);SPI_WriteData(0x04);
	
	SPI_WriteComm(0xFF);SPI_WriteData(0x30);
	SPI_WriteComm(0xFF);SPI_WriteData(0x52);
	SPI_WriteComm(0xFF);SPI_WriteData(0x03);
	SPI_WriteComm(0x00);SPI_WriteData(0x00);
	SPI_WriteComm(0x01);SPI_WriteData(0x00);
	SPI_WriteComm(0x02);SPI_WriteData(0x00);
	SPI_WriteComm(0x03);SPI_WriteData(0x00);
	SPI_WriteComm(0x08);SPI_WriteData(0x0D);
	SPI_WriteComm(0x09);SPI_WriteData(0x0E);
	SPI_WriteComm(0x0A);SPI_WriteData(0x0F);
	SPI_WriteComm(0x0B);SPI_WriteData(0x10);
	SPI_WriteComm(0x20);SPI_WriteData(0x00);
	SPI_WriteComm(0x21);SPI_WriteData(0x00);
	SPI_WriteComm(0x22);SPI_WriteData(0x00);
	SPI_WriteComm(0x23);SPI_WriteData(0x00);
	SPI_WriteComm(0x28);SPI_WriteData(0x22);
	SPI_WriteComm(0x2A);SPI_WriteData(0xE9);
	SPI_WriteComm(0x2B);SPI_WriteData(0xE9);
	SPI_WriteComm(0x30);SPI_WriteData(0x00);
	SPI_WriteComm(0x31);SPI_WriteData(0x00);
	SPI_WriteComm(0x32);SPI_WriteData(0x00);
	SPI_WriteComm(0x33);SPI_WriteData(0x00);
	SPI_WriteComm(0x34);SPI_WriteData(0x01);
	SPI_WriteComm(0x35);SPI_WriteData(0x00);
	SPI_WriteComm(0x36);SPI_WriteData(0x00);
	SPI_WriteComm(0x37);SPI_WriteData(0x03);
	SPI_WriteComm(0x40);SPI_WriteData(0x0A);  
	SPI_WriteComm(0x41);SPI_WriteData(0x0B);  
	SPI_WriteComm(0x42);SPI_WriteData(0x0C);  
	SPI_WriteComm(0x43);SPI_WriteData(0x0D); 
	SPI_WriteComm(0x44);SPI_WriteData(0x22);
	SPI_WriteComm(0x45);SPI_WriteData(0xE4); 
	SPI_WriteComm(0x46);SPI_WriteData(0xE5);  
	SPI_WriteComm(0x47);SPI_WriteData(0x22);
	SPI_WriteComm(0x48);SPI_WriteData(0xE6);  
	SPI_WriteComm(0x49);SPI_WriteData(0xE7);
	SPI_WriteComm(0x50);SPI_WriteData(0x0E);  
	SPI_WriteComm(0x51);SPI_WriteData(0x0F);  
	SPI_WriteComm(0x52);SPI_WriteData(0x10);  
	SPI_WriteComm(0x53);SPI_WriteData(0x11);
	
	SPI_WriteComm(0x54);SPI_WriteData(0x22);
	SPI_WriteComm(0x55);SPI_WriteData(0xE8);  
	SPI_WriteComm(0x56);SPI_WriteData(0xE9);  
	SPI_WriteComm(0x57);SPI_WriteData(0x22);
	SPI_WriteComm(0x58);SPI_WriteData(0xEA);  
	SPI_WriteComm(0x59);SPI_WriteData(0xEB);
	
	SPI_WriteComm(0x60);SPI_WriteData(0x05);  
	SPI_WriteComm(0x61);SPI_WriteData(0x05);  
	SPI_WriteComm(0x65);SPI_WriteData(0x0A);  
	SPI_WriteComm(0x66);SPI_WriteData(0x0A);  
	SPI_WriteComm(0x80);SPI_WriteData(0x05);
	SPI_WriteComm(0x81);SPI_WriteData(0x00);
	SPI_WriteComm(0x82);SPI_WriteData(0x02);
	SPI_WriteComm(0x83);SPI_WriteData(0x04);
	SPI_WriteComm(0x84);SPI_WriteData(0x00);
	SPI_WriteComm(0x85);SPI_WriteData(0x00);
	SPI_WriteComm(0x86);SPI_WriteData(0x1f);
	SPI_WriteComm(0x87);SPI_WriteData(0x1f);
	SPI_WriteComm(0x88);SPI_WriteData(0x0a);
	SPI_WriteComm(0x89);SPI_WriteData(0x0c);
	SPI_WriteComm(0x8A);SPI_WriteData(0x0e);
	SPI_WriteComm(0x8B);SPI_WriteData(0x10);
	
	SPI_WriteComm(0x96);SPI_WriteData(0x05);
	SPI_WriteComm(0x97);SPI_WriteData(0x00);
	SPI_WriteComm(0x98);SPI_WriteData(0x01); 
	SPI_WriteComm(0x99);SPI_WriteData(0x03); 
	SPI_WriteComm(0x9A);SPI_WriteData(0x00); 
	SPI_WriteComm(0x9B);SPI_WriteData(0x00); 
	SPI_WriteComm(0x9C);SPI_WriteData(0x1f); 
	SPI_WriteComm(0x9D);SPI_WriteData(0x1f); 
	SPI_WriteComm(0x9E);SPI_WriteData(0x09); 
	SPI_WriteComm(0x9F);SPI_WriteData(0x0b); 
	SPI_WriteComm(0xA0);SPI_WriteData(0x0d); 
	SPI_WriteComm(0xA1);SPI_WriteData(0x0f); 
	   
	SPI_WriteComm(0xFF);SPI_WriteData(0x30);
	SPI_WriteComm(0xFF);SPI_WriteData(0x52);
	SPI_WriteComm(0xFF);SPI_WriteData(0x00);
//	SPI_WriteComm(0x3A);SPI_WriteData(0x66);
	SPI_WriteComm(0x0C);SPI_WriteData(0x66);
	
	SPI_WriteComm(0x36);SPI_WriteData(0x0A);
												
 	SPI_WriteComm(0x11);//SPI_WriteData(0x00);
	delay(120);                        
	SPI_WriteComm(0x29);//SPI_WriteData(0x00);
	delay(20); 


}



#endif


