/***************************************************
//Web: http://www.buydisplay.com
EastRising Technology Co.,LTD
Examples for ER-TFT3.95-1 with ER-PCBA5981-1 Capacitive touch screen test
Display is Hardward SPI 4-Wire SPI Interface and 5V Power Supply,CTP is I2C interface.
Tested and worked with:
Works with Arduino 2.3.4 IDE  
NOTE: test OK:ESP32-WROOM-32 
****************************************************/

/*
  ==TFT Hardware SPI to ESP32  ==
    TFT   =>    ESP32
  *1,2. GND       ->    GND
  *3,4. VCC       ->    5V    3.3V OR 5V is optional, depending on the voltage of the module purchased 
  *5. CS        ->       5
  *6. MISO        ->    19  
  *7. MOSI       ->    23  
  *8. SCK       ->    18  
  *9. BL        ->     3.3V  (HIGH:ON  LOW:OFF)
  *11. RES       ->    16   
  *12. CTPINT     ->    17
  *13. CTPRST     ->    15
  *14. CTPSDA    ->    21
  *15. CTPSCL    ->    22 

  *16. CS        ->    14 
  *18. SDI       ->    12 
  *19. SCK       ->    13 
*/
#include <SPI.h>
#include <Wire.h>
#include "LCD.h"
#include "LCD_init.h"
#include <stdint.h>
#include "Arduino.h"
#include "Print.h"

uint8_t addr  = 0x5d;  //CTP IIC ADDRESS

#define GT911_RST 15
#define GT911_INT   17  

#define BACKLIGHT_PIN 26

unsigned char  GTP_CFG_DATA[] =
{

0x4F,0xD0,0x02,0xD0,0x02,0x05,0x35,0x00,0x01,0xC8,
0x28,0x0F,0x50,0x3C,0x03,0x05,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x18,0x1A,0x1E,0x14,0x85,0x25,0x0A,
0xEA,0xEC,0xB5,0x06,0x00,0x00,0x00,0x21,0x21,0x10,
0x00,0x01,0x00,0x0F,0x00,0x2A,0x00,0x00,0x01,0x50,
0x32,0xDC,0xFA,0x94,0xD0,0x02,0x08,0x00,0x00,0x04,
0x80,0xDE,0x00,0x80,0xE4,0x00,0x80,0xEA,0x00,0x7F,
0xF0,0x00,0x7F,0xF6,0x00,0x7F,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x14,0x12,0x10,0x0E,0x0C,0x0A,0x08,0x06,
0x04,0x02,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
0xFF,0xFF,0x21,0x20,0x1F,0x1E,0x1D,0x00,0x02,0x04,
0x06,0x08,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
0xFF,0xFF,0xFF,0xFF,0xC7,0x01


};

struct TouchLocation
{
  uint16_t x;
  uint16_t y;
};
TouchLocation touchLocations[5];


void inttostr(uint16_t value,uint8_t *str);
uint8_t gt911_Send_Cfg(uint8_t * buf,uint16_t cfg_len);
void writeGT911TouchRegister( uint16_t regAddr,uint8_t *val, uint16_t cnt);
uint8_t readGT911TouchAddr( uint16_t regAddr, uint8_t * pBuf, uint8_t len );
uint8_t readGT911TouchLocation( TouchLocation * pLoc, uint8_t num );
uint32_t dist(const TouchLocation & loc);
uint32_t dist(const TouchLocation & loc1, const TouchLocation & loc2);
bool sameLoc( const TouchLocation & loc, const TouchLocation & loc2 );

uint8_t buf[30];


uint8_t gt911_Send_Cfg(uint8_t * buf,uint16_t cfg_len)
{
	//uint8_t ret=0;
	uint8_t retry=0;
	for(retry=0;retry<5;retry++)
	{
		writeGT911TouchRegister(0x8047,buf,cfg_len);
		//if(ret==0)break;
		delay(10);	 
	}
	//return ret;
}


void writeGT911TouchRegister( uint16_t regAddr,uint8_t *val, uint16_t cnt)
{	uint16_t i=0;
  Wire.beginTransmission(addr);
   Wire.write( regAddr>>8 );  // register 0
  Wire.write( regAddr);  // register 0 
	for(i=0;i<cnt;i++,val++)//data
	{		
          Wire.write( *val );  // value
	}
  uint8_t retVal = Wire.endTransmission(); 
}



uint8_t readGT911TouchAddr( uint16_t regAddr, uint8_t * pBuf, uint8_t len )
{
  Wire.beginTransmission(addr);
  Wire.write( regAddr>>8 );  // register 0
  Wire.write( regAddr);  // register 0  
  uint8_t retVal = Wire.endTransmission();
  
  uint8_t returned = Wire.requestFrom(addr, len);    // request 1 bytes from slave device #2
  
  uint8_t i;
  for (i = 0; (i < len) && Wire.available(); i++)
  
  {
    pBuf[i] = Wire.read();
  }
  
  return i;
}

uint8_t readGT911TouchLocation( TouchLocation * pLoc, uint8_t num )
{
  uint8_t retVal;
  uint8_t i;
  uint8_t k;
  uint8_t  ss[1];
  do
  {  
    
    if (!pLoc) break; // must have a buffer
    if (!num)  break; // must be able to take at least one
     ss[0]=0;
      readGT911TouchAddr( 0x814e, ss, 1);
      uint8_t status=ss[0];

    if ((status & 0x0f) == 0) break; // no points detected
    uint8_t hitPoints = status & 0x0f;
    
    Serial.print("number of hit points = ");
    Serial.println( hitPoints );
    
     uint8_t tbuf[32]; uint8_t tbuf1[32];uint8_t tbuf2[16];  
    readGT911TouchAddr( 0x8150, tbuf, 32);
    readGT911TouchAddr( 0x8150+32, tbuf1, 32);
    
      if(hitPoints<=4)
            {   
              for (k=0,i = 0; (i <  4*8)&&(k < num); k++, i += 8)
              {
                pLoc[k].x = tbuf[i+1] << 8 | tbuf[i+0];
                pLoc[k].y = tbuf[i+3] << 8 | tbuf[i+2];
              }   
            }
        if(hitPoints>4)   
            {  
               for (k=0,i = 0; (i <  4*8)&&(k < num); k++, i += 8)
              {
                pLoc[k].x = tbuf[i+1] << 8 | tbuf[i+0];
                pLoc[k].y = tbuf[i+3] << 8 | tbuf[i+2];
              }               
              
              for (k=4,i = 0; (i <  4*8)&&(k < num); k++, i += 8)
              {
                pLoc[k].x = tbuf1[i+1] << 8 | tbuf1[i+0];
                pLoc[k].y = tbuf1[i+3] << 8 | tbuf1[i+2];
              }   
            } 
            
                
            
    
    retVal = hitPoints;
    
  } while (0);
  
    ss[0]=0;
    writeGT911TouchRegister( 0x814e,ss,1); 
  
  return retVal;
}

void inttostr(uint16_t value,uint8_t *str)
{
	str[0]=value/1000+0x30;
	str[1]=value%1000/100+0x30;
	str[2]=value%1000%100/10+0x30;
	str[3]=value%1000%100%10+0x30;

}



void setup() {uint8_t ss[4];
  Serial.begin(9600);  //DUE
 Wire.setClock(400000);
  Wire.begin();        // join i2c bus (address optional for master)   


   delay(100);
  ER_TFT.Parallel_Init();
  ER_TFT.HW_Reset();
  ER_TFT.System_Check_Temp();
  delay(100);
  while(ER_TFT.LCD_StatusRead()&0x02);
  ER_TFT.initial();
  nv3052_Initial(); 
  ER_TFT.Display_ON();

  // BL on
  pinMode(BACKLIGHT_PIN, OUTPUT);
  digitalWrite(BACKLIGHT_PIN, HIGH);

  ER_TFT.Select_Main_Window_16bpp();
  ER_TFT.Main_Image_Start_Address(layer1_start_addr);        
  ER_TFT.Main_Image_Width(LCD_XSIZE_TFT);
  ER_TFT.Main_Window_Start_XY(0,0);
  ER_TFT.Canvas_Image_Start_address(0);
  ER_TFT.Canvas_image_width(LCD_XSIZE_TFT);
  ER_TFT.Active_Window_XY(0,0);
  ER_TFT.Active_Window_WH(LCD_XSIZE_TFT,LCD_YSIZE_TFT); 
  
  ER_TFT.Foreground_color_65k(Black);
  ER_TFT.Line_Start_XY(0,0);
  ER_TFT.Line_End_XY(LCD_XSIZE_TFT-1,LCD_YSIZE_TFT-1);
  ER_TFT.Start_Square_Fill(); 
  
  ER_TFT.Foreground_color_65k(Red);
  ER_TFT.Line_Start_XY(0,0);
  ER_TFT.Line_End_XY(LCD_XSIZE_TFT-1,LCD_YSIZE_TFT-1);
  ER_TFT.Start_Square();  
  
  
   ER_TFT.Foreground_color_65k(White);
  ER_TFT.Background_color_65k(Black);
  ER_TFT.CGROM_Select_Internal_CGROM();  
  ER_TFT.Font_Select_8x16_16x16();
  ER_TFT.Goto_Text_XY(10,10); 
  ER_TFT.Show_String( "www.buydisplay.com");   
  ER_TFT.Goto_Text_XY(10,26); 
  ER_TFT.Show_String( "Capacitive touch screen tese"); 
 		ER_TFT.Goto_Text_XY(LCD_XSIZE_TFT-48,LCD_YSIZE_TFT-20);
		ER_TFT.Show_String("Clear");   
		ER_TFT.Goto_Text_XY(15,LCD_YSIZE_TFT-20);
		ER_TFT.Show_String("Exit");  



 
    pinMode(GT911_RST, OUTPUT); 
    pinMode     (GT911_INT, OUTPUT);
    digitalWrite(GT911_RST, LOW);
    digitalWrite(GT911_INT, LOW);
    delay(20);
     digitalWrite(GT911_RST, HIGH);
     delay(50);  
    pinMode     (GT911_INT, INPUT);
     delay(100);    
    uint8_t re=gt911_Send_Cfg((uint8_t*)GTP_CFG_DATA,sizeof(GTP_CFG_DATA));
 
   
     readGT911TouchAddr(0x8047,ss,5);
	if((ss[2]==0x03)&&(ss[3]==0xe0)){ ER_TFT.Goto_Text_XY(2,40);ER_TFT.Show_String("send CFG OK");}
	 else  {  ER_TFT.Goto_Text_XY(2,40);ER_TFT.Show_String("send CFG NOK");     
               pinMode(GT911_RST, OUTPUT); 
              pinMode     (GT911_INT, OUTPUT);
              digitalWrite(GT911_RST, LOW);
              digitalWrite(GT911_INT, LOW);
              delay(20);
               digitalWrite(GT911_RST, HIGH);
               delay(50);  
              pinMode     (GT911_INT, INPUT);
               delay(100);   
               re=gt911_Send_Cfg((uint8_t*)GTP_CFG_DATA,sizeof(GTP_CFG_DATA));   
              } 
  


}

  uint8_t flag = 1;
void loop() {
   static uint16_t w = LCD_XSIZE_TFT;
  static uint16_t h = LCD_YSIZE_TFT; 
   unsigned int i;
   double float_data;  

  while(flag) 
  {    pinMode     (GT911_INT, INPUT);
       uint8_t st=digitalRead(GT911_INT);       
      if(!st)    //Hardware touch interrupt
    {  
      Serial.println("Touch: ");
      
      uint8_t count = readGT911TouchLocation( touchLocations, 5);
                
        for (i = 0; i < count; i++)
        {
     
   	   if ((touchLocations[0].x<85) &&(touchLocations[0].y>680)) flag=0;     
            if (((touchLocations[0].x)>630) &&((touchLocations[0].y)>680))
              {    ER_TFT.Foreground_color_65k(Black);
                  ER_TFT.Line_Start_XY(0,0);
                  ER_TFT.Line_End_XY(LCD_XSIZE_TFT-1,LCD_YSIZE_TFT-1);
                  ER_TFT.Start_Square_Fill(); 
                  
                   ER_TFT.Foreground_color_65k(White);
                  ER_TFT.Background_color_65k(Black);
                  ER_TFT.CGROM_Select_Internal_CGROM();  
                  ER_TFT.Font_Select_8x16_16x16();
                  ER_TFT.Goto_Text_XY(0,0); 
                  ER_TFT.Show_String( "www.buydisplay.com");   
                  ER_TFT.Goto_Text_XY(0,16); 
                  ER_TFT.Show_String( "Capacitive touch screen test"); 
		ER_TFT.Goto_Text_XY(LCD_XSIZE_TFT-48,LCD_YSIZE_TFT-20);
		ER_TFT.Show_String("Clear");   
		ER_TFT.Goto_Text_XY(15,LCD_YSIZE_TFT-20);
		ER_TFT.Show_String("Exit");                  
              }  
              
          else{                                       
              snprintf((char*)buf,sizeof(buf),"(%3d,%3d)",touchLocations[i].x,touchLocations[i].y); 
            const  char *str=(const char *)buf;
             ER_TFT.Foreground_color_65k(Red);  
            ER_TFT.Text_Mode();
            ER_TFT.Goto_Text_XY(50,80+16*i);
            ER_TFT.LCD_CmdWrite(0x04);
            while(*str != '\0')
            {
            ER_TFT.LCD_DataWrite(*str);
            ER_TFT.Check_Mem_WR_FIFO_not_Full();      
            ++str; 
            } 
            ER_TFT.Check_2D_Busy();
            ER_TFT.Graphic_Mode(); //back to graphic mode;
                   
           
         if(i==0)  ER_TFT.DrawCircle_Fill(touchLocations[i].x,touchLocations[i].y, 3, Red);  
        else if(i==1)  ER_TFT.DrawCircle_Fill(touchLocations[i].x,touchLocations[i].y, 3, Green); 
        else if(i==2)  ER_TFT.DrawCircle_Fill(touchLocations[i].x,touchLocations[i].y, 3, Blue);        
        else if(i==3)  ER_TFT.DrawCircle_Fill(touchLocations[i].x,touchLocations[i].y, 3, Cyan); 
        else if(i==4)  ER_TFT.DrawCircle_Fill(touchLocations[i].x,touchLocations[i].y, 3, Yellow);   
          }
        }
     } 
    
  }
  
  
  
  
  
 ER_TFT.Select_Main_Window_16bpp();
  ER_TFT.Main_Image_Start_Address(layer1_start_addr);        
  ER_TFT.Main_Image_Width(LCD_XSIZE_TFT);
  ER_TFT.Main_Window_Start_XY(0,0);
  ER_TFT.Canvas_Image_Start_address(0);
  ER_TFT.Canvas_image_width(LCD_XSIZE_TFT);
  ER_TFT.Active_Window_XY(0,0);
  ER_TFT.Active_Window_WH(LCD_XSIZE_TFT,LCD_YSIZE_TFT); 
  
  ER_TFT.DrawSquare_Fill(0,0,LCD_XSIZE_TFT,LCD_YSIZE_TFT,Red);
  delay(1000);
  ER_TFT.DrawSquare_Fill(0,0,LCD_XSIZE_TFT,LCD_YSIZE_TFT,Green);
  delay(1000);
  ER_TFT.DrawSquare_Fill(0,0,LCD_XSIZE_TFT,LCD_YSIZE_TFT,Blue);
  delay(1000);

  ER_TFT.DrawSquare_Fill(0,0,LCD_XSIZE_TFT,LCD_YSIZE_TFT,Cyan);
  delay(1000);
  ER_TFT.DrawSquare_Fill(0,0,LCD_XSIZE_TFT,LCD_YSIZE_TFT,Yellow);
  delay(1000); 
  ER_TFT.DrawSquare_Fill(0,0,LCD_XSIZE_TFT,LCD_YSIZE_TFT,Purple);
  delay(1000);   
 
  ER_TFT.DrawSquare_Fill(0,0,LCD_XSIZE_TFT,LCD_YSIZE_TFT,Black);
  delay(1000); 
  ER_TFT.DrawSquare_Fill(0,0,LCD_XSIZE_TFT,LCD_YSIZE_TFT,White);
  delay(1000);


   
  ////////Drawing
  ER_TFT.Select_Main_Window_16bpp();
  ER_TFT.Main_Image_Start_Address(layer1_start_addr);				
  ER_TFT.Main_Image_Width(LCD_XSIZE_TFT);
  ER_TFT.Main_Window_Start_XY(0,0);
  ER_TFT.Canvas_Image_Start_address(layer1_start_addr);
  ER_TFT.Canvas_image_width(LCD_XSIZE_TFT);
  ER_TFT.Active_Window_XY(0,0);
  ER_TFT.Active_Window_WH(LCD_XSIZE_TFT,LCD_YSIZE_TFT);	

  ER_TFT.Foreground_color_65k(Black);
  ER_TFT.Line_Start_XY(0,0);
  ER_TFT.Line_End_XY(LCD_XSIZE_TFT-1,LCD_YSIZE_TFT-1);
  ER_TFT.Start_Square_Fill();

  for(i=0;i<=LCD_XSIZE_TFT/2-10;i+=8)
  {ER_TFT.Foreground_color_65k(Red);
  ER_TFT.Line_Start_XY(0+i,0+i);
  ER_TFT.Line_End_XY(LCD_XSIZE_TFT-1-i,LCD_YSIZE_TFT-1-i);
  ER_TFT.Start_Square();
  delay(10);
  }

  for(i=0;i<=LCD_XSIZE_TFT/2-10;i+=8)
  {ER_TFT.Foreground_color_65k(Black);
  ER_TFT.Line_Start_XY(0+i,0+i);
  ER_TFT.Line_End_XY(LCD_XSIZE_TFT-1-i,LCD_YSIZE_TFT-1-i);
  ER_TFT.Start_Square();
  delay(10);
  }
 delay(100);
///////////////////////////Square Of Circle
  ER_TFT.Foreground_color_65k(Black);
  ER_TFT.Line_Start_XY(0,0);
  ER_TFT.Line_End_XY(LCD_XSIZE_TFT-1,LCD_YSIZE_TFT-1);
  ER_TFT.Start_Square_Fill();

  for(i=0;i<=LCD_XSIZE_TFT/2-10;i+=8)
  {ER_TFT.Foreground_color_65k(Green);
  ER_TFT.Line_Start_XY(0+i,0+i);
  ER_TFT.Line_End_XY(LCD_XSIZE_TFT-1-i,LCD_YSIZE_TFT-1-i);
  ER_TFT.Circle_Square_Radius_RxRy(10,10);
  ER_TFT.Start_Circle_Square();
  delay(10);
  }

  for(i=0;i<=LCD_XSIZE_TFT/2-10;i+=8)
  {ER_TFT.Foreground_color_65k(Black);
  ER_TFT.Line_Start_XY(0+i,0+i);
  ER_TFT.Line_End_XY(LCD_XSIZE_TFT-1-i,LCD_YSIZE_TFT-1-i);
  ER_TFT.Circle_Square_Radius_RxRy(10,10);
  ER_TFT.Start_Circle_Square();
  delay(10);
  }
  delay(100);

///////////////////////////Circle
  ER_TFT.Foreground_color_65k(Black);
  ER_TFT.Line_Start_XY(0,0);
  ER_TFT.Line_End_XY(LCD_XSIZE_TFT-1,LCD_YSIZE_TFT-1);
  ER_TFT.Start_Square_Fill();

  for(i=0;i<=LCD_XSIZE_TFT/2-10;i+=8)
  {ER_TFT.Foreground_color_65k(Blue);
  ER_TFT.Circle_Center_XY(LCD_XSIZE_TFT/2,LCD_YSIZE_TFT/2);
  ER_TFT.Circle_Radius_R(i);
  ER_TFT.Start_Circle_or_Ellipse();
  delay(10);
  }

  for(i=0;i<=LCD_XSIZE_TFT/2-10;i+=8)
  {ER_TFT.Foreground_color_65k(Black);
  ER_TFT.Circle_Center_XY(LCD_XSIZE_TFT/2,LCD_YSIZE_TFT/2);
  ER_TFT.Circle_Radius_R(i);
  ER_TFT.Start_Circle_or_Ellipse();
  delay(10);
  }
  delay(100);

///////////////////////////Ellipse
  ER_TFT.Foreground_color_65k(Black);
  ER_TFT.Line_Start_XY(0,0);
  ER_TFT.Line_End_XY(LCD_XSIZE_TFT-1,LCD_YSIZE_TFT-1);
  ER_TFT.Start_Square_Fill();

  for(i=0;i<=LCD_XSIZE_TFT/2-100;i+=8)
  {ER_TFT.Foreground_color_65k(White);
  ER_TFT.Circle_Center_XY(LCD_XSIZE_TFT/2,LCD_YSIZE_TFT/2);
  ER_TFT.Ellipse_Radius_RxRy(i+100,i);
  ER_TFT.Start_Circle_or_Ellipse();
  delay(10);
  }

  for(i=0;i<=LCD_XSIZE_TFT/2-100;i+=8)
  {ER_TFT.Foreground_color_65k(Black);
  ER_TFT.Circle_Center_XY(LCD_XSIZE_TFT/2,LCD_YSIZE_TFT/2);
  ER_TFT.Ellipse_Radius_RxRy(i+100,i);
  ER_TFT.Start_Circle_or_Ellipse();
  delay(10);
  }
  delay(100);

 ////////////////////////////Triangle
  ER_TFT.Foreground_color_65k(Black);
  ER_TFT.Line_Start_XY(0,0);
  ER_TFT.Line_End_XY(LCD_XSIZE_TFT-1,LCD_YSIZE_TFT-1);
  ER_TFT.Start_Square_Fill();

  for(i=0;i<=LCD_XSIZE_TFT/2-10;i+=8)
  {ER_TFT.Foreground_color_65k(Yellow);
  ER_TFT.Triangle_Point1_XY(LCD_XSIZE_TFT/2,i);
  ER_TFT.Triangle_Point2_XY(i,LCD_YSIZE_TFT-1-i);
  ER_TFT.Triangle_Point3_XY(LCD_XSIZE_TFT-1-i,LCD_YSIZE_TFT-1-i);
  ER_TFT.Start_Triangle();
  delay(10);
  }

  for(i=0;i<=LCD_XSIZE_TFT/2-10;i+=8)
  {ER_TFT.Foreground_color_65k(Black);
  ER_TFT.Triangle_Point1_XY(LCD_XSIZE_TFT/2,i);
  ER_TFT.Triangle_Point2_XY(i,LCD_YSIZE_TFT-1-i);
  ER_TFT.Triangle_Point3_XY(LCD_XSIZE_TFT-1-i,LCD_YSIZE_TFT-1-i);
  ER_TFT.Start_Triangle();
  delay(10);
  }
  delay(100);


 ////////////////////////////line
  ER_TFT.Foreground_color_65k(Black);
  ER_TFT.Line_Start_XY(0,0);
  ER_TFT.Line_End_XY(LCD_XSIZE_TFT-1,LCD_YSIZE_TFT-1);
  ER_TFT.Start_Square_Fill();

  for(i=0;i<=LCD_XSIZE_TFT;i+=8)
  {ER_TFT.Foreground_color_65k(Red);
  ER_TFT.Line_Start_XY(i,0);
  ER_TFT.Line_End_XY(LCD_XSIZE_TFT-1-i,LCD_YSIZE_TFT-1);
  ER_TFT.Start_Line();
  delay(10);
  }
  for(i=0;i<=LCD_YSIZE_TFT;i+=8)
  {ER_TFT.Foreground_color_65k(Red);
  ER_TFT.Line_Start_XY(0,LCD_YSIZE_TFT-1-i);
  ER_TFT.Line_End_XY(LCD_XSIZE_TFT-1,i);
  ER_TFT.Start_Line();
  delay(10);
  }


  for(i=0;i<=LCD_XSIZE_TFT;i+=8)
  {ER_TFT.Foreground_color_65k(Black);
  ER_TFT.Line_Start_XY(i,0);
  ER_TFT.Line_End_XY(LCD_XSIZE_TFT-1-i,LCD_YSIZE_TFT-1);
  ER_TFT.Start_Line();
  delay(10);
  }
  for(i=0;i<=LCD_YSIZE_TFT;i+=8)
  {ER_TFT.Foreground_color_65k(Black);
  ER_TFT.Line_Start_XY(0,LCD_YSIZE_TFT-1-i);
  ER_TFT.Line_End_XY(LCD_XSIZE_TFT-1,i);
  ER_TFT.Start_Line();
  delay(10);
  }


  delay(100);  

/////////////Internal characters 
  ER_TFT.Select_Main_Window_16bpp();
  ER_TFT.Main_Image_Start_Address(layer1_start_addr);				
  ER_TFT.Main_Image_Width(LCD_XSIZE_TFT);
  ER_TFT.Main_Window_Start_XY(0,0);
  ER_TFT.Canvas_Image_Start_address(layer1_start_addr);
  ER_TFT.Canvas_image_width(LCD_XSIZE_TFT);
  ER_TFT.Active_Window_XY(0,0);
  ER_TFT.Active_Window_WH(LCD_XSIZE_TFT,LCD_YSIZE_TFT);	
 
  
  ER_TFT.Foreground_color_65k(Black);
  ER_TFT.Line_Start_XY(0,0);
  ER_TFT.Line_End_XY(LCD_XSIZE_TFT-1,LCD_YSIZE_TFT-1);
  ER_TFT.Start_Square_Fill();



  ER_TFT.Background_color_65k(Black);
  ER_TFT.Foreground_color_65k(Red);  
  ER_TFT.CGROM_Select_Internal_CGROM();
  ER_TFT.Font_Select_8x16_16x16();
  ER_TFT.Goto_Text_XY(0,10);
  ER_TFT.Show_String("buydisplay.com");
  
  ER_TFT.Foreground_color_65k(Green);
  ER_TFT.Font_Select_12x24_24x24();
  ER_TFT.Goto_Text_XY(0,26);
  ER_TFT.Show_String("buydisplay.com");

  ER_TFT.Foreground_color_65k(Blue);
  ER_TFT.Font_Select_16x32_32x32();
  ER_TFT.Goto_Text_XY(0,50);
  ER_TFT.Show_String("buydisplay.com");  
  
  ER_TFT.Foreground_color_65k(Yellow);
  ER_TFT.Font_Width_X4(); 
  ER_TFT.Font_Height_X4();
  ER_TFT.Goto_Text_XY(0,90);  
  ER_TFT.Show_String("buydisplay.com");    
  delay(2000);  
  

  
  
   ///////////////////////// DMA
  ER_TFT.Select_Main_Window_16bpp();
  ER_TFT.Main_Image_Start_Address(layer1_start_addr);				
  ER_TFT.Main_Image_Width(LCD_XSIZE_TFT);
  ER_TFT.Main_Window_Start_XY(0,0);
  ER_TFT.Canvas_Image_Start_address(layer1_start_addr);
  ER_TFT.Canvas_image_width(LCD_XSIZE_TFT);
  ER_TFT.Active_Window_XY(0,0);
  ER_TFT.Active_Window_WH(LCD_XSIZE_TFT,LCD_YSIZE_TFT);	
 
  
  ER_TFT.Foreground_color_65k(Black);
  ER_TFT.Line_Start_XY(0,0);
  ER_TFT.Line_End_XY(LCD_XSIZE_TFT-1,LCD_YSIZE_TFT-1);
  ER_TFT.Start_Square_Fill();
 

       unsigned long im;
 	 for( im=0;im<4;im++)
	 {
        ER_TFT.DMA_24bit_Block(1,0,0,0,LCD_XSIZE_TFT,LCD_YSIZE_TFT,LCD_XSIZE_TFT,im*LCD_XSIZE_TFT*LCD_YSIZE_TFT*2);
        // Select SPI : SCS��0       SCS��1
        // SPI Clock = System Clock /{(Clk+1)*2}
        // Transfer to SDRAM address:X1
         // Transfer to SDRAM address:Y1
        // DMA data width
        // DMA data height
        // Picture's width
        // Flash address	
	  delay(2000);
 	
	 }
 
}



