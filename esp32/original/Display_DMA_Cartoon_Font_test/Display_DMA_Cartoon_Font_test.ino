/***************************************************
//Web: http://www.buydisplay.com
EastRising Technology Co.,LTD
Examples for ER-TFT3.95-1 with ER-PCBA5981-1  Display test
Display is Hardward SPI 4-Wire SPI Interface and 5V Power Supply
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


  *16. CS        ->    14
  *18. SDI       ->    12 
  *19. SCK       ->    13 
*/

#include <SPI.h>
#include <Wire.h>
#include "LCD.h"
#include "LCD_init.h"


 #define Picture_1_Addr	  0
 #define Cartoon_Addr      0x003F4800 

#define FLASH_ADDR_16	  0x005AA398
#define FLASH_ADDR_24	  0x005EB798
#define FLASH_ADDR_32	  0x0067E498

#define SIZE_16_NUM	267264
#define SIZE_24_NUM     601344
#define SIZE_32_NUM     1069056

#define MEMORY_ADDR_16   layer5_start_addr
#define MEMORY_ADDR_24	(MEMORY_ADDR_16+SIZE_16_NUM)
#define MEMORY_ADDR_32	(MEMORY_ADDR_24+SIZE_24_NUM)

#define BACKLIGHT_PIN 26

unsigned char f []={0xD0,0xF1,0xC8,0xD5,0xB6,0xAB,0xB7,0xBD,0xBF,0xC6,0xBC,0xBC,0x00};  //GBK 


void setup() {
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
}



void LCD_BTE_Memory_Copy
(
 unsigned long S0_Addr     
,unsigned short S0_W       
,unsigned short XS0        
,unsigned short YS0      
,unsigned long S1_Addr    
,unsigned short S1_W       
,unsigned short XS1        
,unsigned short YS1        
,unsigned long Des_Addr    
,unsigned short Des_W     
,unsigned short XDes      
,unsigned short YDes    
,unsigned int ROP_Code    
/*ROP_Code :
   0000b		0(Blackness)
   0001b		~S0!E~S1 or ~(S0+S1)
   0010b		~S0!ES1
   0011b		~S0
   0100b		S0!E~S1
   0101b		~S1
   0110b		S0^S1
   0111b		~S0 + ~S1 or ~(S0 + S1)
   1000b		S0!ES1
   1001b		~(S0^S1)
   1010b		S1
   1011b		~S0+S1
   1100b		S0
   1101b		S0+~S1
   1110b		S0+S1
   1111b		1(whiteness)*/
,unsigned short X_W      
,unsigned short Y_H       
)
{
  ER_TFT.BTE_S0_Color_16bpp();
  ER_TFT.BTE_S0_Memory_Start_Address(S0_Addr);
  ER_TFT.BTE_S0_Image_Width(S0_W);
  ER_TFT.BTE_S0_Window_Start_XY(XS0,YS0);

  ER_TFT.BTE_S1_Color_16bpp();
  ER_TFT.BTE_S1_Memory_Start_Address(S1_Addr);
  ER_TFT.BTE_S1_Image_Width(S1_W); 
  ER_TFT.BTE_S1_Window_Start_XY(XS1,YS1);

  ER_TFT.BTE_Destination_Color_16bpp();
  ER_TFT.BTE_Destination_Memory_Start_Address(Des_Addr);
  ER_TFT.BTE_Destination_Image_Width(Des_W);
  ER_TFT.BTE_Destination_Window_Start_XY(XDes,YDes);	
   
  ER_TFT.BTE_ROP_Code(ROP_Code);	
  ER_TFT.BTE_Operation_Code(0x02); //BTE Operation: Memory copy (move) with ROP.
  ER_TFT.BTE_Window_Size(X_W,Y_H); 
  ER_TFT.BTE_Enable();
  ER_TFT.Check_BTE_Busy();
}


void LCD_Select_Outside_Font_Init
(
 unsigned char SCS           // 选择外挂的SPI   : SCS：0       SCS：1
,unsigned char Clk           // SPI时钟分频参数 : SPI Clock = System Clock /{(Clk+1)*2}
,unsigned long FlashAddr     // 源地址(Flash)
,unsigned long MemoryAddr    // 目的地址(SDRAM)
,unsigned long Num           // 字库的数据量大小
,unsigned char Size          // 设置字体大小  16：16*16     24:24*24    32:32*32
,unsigned char XxN           // 字体的宽度放大倍数：1~4
,unsigned char YxN           // 字体的高度放大倍数：1~4
,unsigned char ChromaKey     // 0：字体背景色透明    1：可以设置字体的背景色
,unsigned char Alignment     // 0：不字体不对齐      1：字体对齐
)
{
	if(Size==16)	ER_TFT.Font_Select_8x16_16x16();
	if(Size==24)	ER_TFT.Font_Select_12x24_24x24();
	if(Size==32)	ER_TFT.Font_Select_16x32_32x32();

	//(*)
	if(XxN==1)	ER_TFT.Font_Width_X1();
	if(XxN==2)	ER_TFT.Font_Width_X2();
	if(XxN==3)	ER_TFT.Font_Width_X3();
	if(XxN==4)	ER_TFT.Font_Width_X4();

	//(*)	
	if(YxN==1)	ER_TFT.Font_Height_X1();
	if(YxN==2)	ER_TFT.Font_Height_X2();
	if(YxN==3)	ER_TFT.Font_Height_X3();
	if(YxN==4)	ER_TFT.Font_Height_X4();

	//(*)
	if(ChromaKey==0)	ER_TFT.Font_Background_select_Color();	
	if(ChromaKey==1)	ER_TFT.Font_Background_select_Transparency();	

	//(*)
	if(Alignment==0)	ER_TFT.Disable_Font_Alignment();
	if(Alignment==1)	ER_TFT.Enable_Font_Alignment();	
	
	ER_TFT.DMA_24bit_Linear(SCS,Clk,FlashAddr,MemoryAddr,Num);
	ER_TFT.CGRAM_Start_address(MemoryAddr);        
}



void LCD_Print_Outside_Font_String
(
 unsigned short x               
,unsigned short y               
,unsigned long FontColor       
,unsigned long BackGroundColor 
,unsigned char *c               
)
{
	unsigned short temp_H = 0;
	unsigned short temp_L = 0;
	unsigned short temp = 0;
	unsigned int i = 0;
	
  ER_TFT.Text_Mode();
  ER_TFT.Font_Select_UserDefine_Mode();
  ER_TFT.Foreground_color_65k(FontColor);
  ER_TFT.Background_color_65k(BackGroundColor);
  ER_TFT.Goto_Text_XY(x,y);
	
	while(c[i] != '\0')
  { 
		if(c[i] < 0xa1)
		{
		  ER_TFT.CGROM_Select_Internal_CGROM();   // 内部CGROM为字符来源
		  ER_TFT.LCD_CmdWrite(0x04);
		  ER_TFT.LCD_DataWrite(c[i]);
		  ER_TFT.Check_Mem_WR_FIFO_not_Full();  
			i += 1;
		}
		else
		{
		ER_TFT.Font_Select_UserDefine_Mode();   // 自定义字库
		ER_TFT.LCD_CmdWrite(0x04);
			temp_H = ((c[i] - 0xa1) & 0x00ff) * 94;
			temp_L = c[i+1] - 0xa1;
			temp = temp_H + temp_L + 0x8000;
		ER_TFT.LCD_DataWrite((temp>>8)&0xff);
		ER_TFT.Check_Mem_WR_FIFO_not_Full();
		ER_TFT.LCD_DataWrite(temp&0xff);
		ER_TFT.Check_Mem_WR_FIFO_not_Full();
			i += 2;		
		}
	}
	
ER_TFT.Check_2D_Busy();

ER_TFT.Graphic_Mode(); //back to graphic mode;图形模式
}





void loop() {
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

   
  ////////Drawing
  unsigned int i;
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

/////////////BTE   
  unsigned int temp;
  unsigned long im=1;
  ER_TFT.Select_Main_Window_16bpp();
  ER_TFT.Main_Image_Start_Address(0);				
  ER_TFT.Main_Image_Width(LCD_XSIZE_TFT);							
  ER_TFT.Main_Window_Start_XY(0,0);

  ER_TFT.Canvas_Image_Start_address(0);//Layer 1
  ER_TFT.Canvas_image_width(LCD_XSIZE_TFT);//
  ER_TFT.Active_Window_XY(0,0);
  ER_TFT.Active_Window_WH(LCD_XSIZE_TFT,LCD_YSIZE_TFT);

  ER_TFT.Foreground_color_65k(Black);
  ER_TFT.Line_Start_XY(0,0);
  ER_TFT.Line_End_XY(LCD_XSIZE_TFT-1,LCD_YSIZE_TFT-25);
  ER_TFT.Start_Square_Fill();

  ER_TFT.Foreground_color_65k(Blue);
  ER_TFT.Line_Start_XY(0,LCD_YSIZE_TFT-24);
  ER_TFT.Line_End_XY(LCD_XSIZE_TFT-1,LCD_YSIZE_TFT-1);
  ER_TFT.Start_Square_Fill();
  ER_TFT.Foreground_color_65k(White);
  ER_TFT.Background_color_65k(Blue);
  ER_TFT.CGROM_Select_Internal_CGROM();
  ER_TFT.Font_Width_X1(); 
  ER_TFT.Font_Height_X1();
  ER_TFT.Font_Select_12x24_24x24();
  ER_TFT.Goto_Text_XY(0,LCD_YSIZE_TFT-24);
  ER_TFT.Show_String("  Demo BTE Compare");
  ER_TFT.Foreground_color_65k(Black);
  ER_TFT.Background_color_65k(White);
  ER_TFT.Font_Select_8x16_16x16();
  ER_TFT.Goto_Text_XY(0,LCD_YSIZE_TFT-48);
  ER_TFT.Show_String("Execute Logic 'OR' 0xf000");

  ER_TFT.Active_Window_XY(20,40);
  ER_TFT.Active_Window_WH(80,80);
  ER_TFT.Goto_Pixel_XY(20,40);
  ER_TFT. Show_picture(80*80,pic_80x80); 
  ER_TFT.Active_Window_XY(20+80+20,40);
  ER_TFT.Active_Window_WH(80,80);
  ER_TFT.Goto_Pixel_XY(120,40);
  ER_TFT.Show_picture(80*80,pic_80x80);
  ER_TFT.Active_Window_XY(20+80+20+80+20,40);
  ER_TFT.Active_Window_WH(80,80);
  ER_TFT.Goto_Pixel_XY(220,40);
  ER_TFT.Show_picture(80*80,pic_80x80);		 
  ER_TFT.Active_Window_XY(0,0);
  ER_TFT.Active_Window_WH(LCD_XSIZE_TFT,LCD_YSIZE_TFT);
  ER_TFT.Foreground_color_65k(Black);
  ER_TFT.Background_color_65k(White);
  ER_TFT.CGROM_Select_Internal_CGROM();
  ER_TFT.Font_Select_8x16_16x16();
  ER_TFT.Goto_Text_XY(20,130 );
  ER_TFT.Show_String("Without BTE");
  ER_TFT.Goto_Text_XY(120,130 );
  ER_TFT.Show_String("BTE Write");
  ER_TFT.Goto_Text_XY(120,150 );
  ER_TFT.Show_String("ROP");
  ER_TFT.Goto_Text_XY(220,130 );
  ER_TFT.Show_String("BTE Move");
  ER_TFT.Goto_Text_XY(220,150 );
  ER_TFT.Show_String("ROP");
  delay(1000);
  ER_TFT.Active_Window_XY(20,40);
  ER_TFT.Active_Window_WH(80,80); 
  ER_TFT. Goto_Pixel_XY(20,40);
  ER_TFT.LCD_CmdWrite(0x04);
  temp =   ER_TFT.LCD_DataRead();
  ER_TFT.Check_Mem_RD_FIFO_not_Empty();  //dummy
  for(i=0; i<80*80;i++)
  {				
  temp =   ER_TFT.LCD_DataRead();		   
  temp=temp|(  ER_TFT.LCD_DataRead()<<8);
  ER_TFT.Check_Mem_RD_FIFO_not_Empty();
  temp |= 0xf000; 
  ER_TFT.LCD_DataWrite(temp);
  ER_TFT.LCD_DataWrite(temp>>8);
  ER_TFT.Check_Mem_WR_FIFO_not_Full();
  }
  ER_TFT.Active_Window_XY(0,0);
  ER_TFT.Active_Window_WH(LCD_XSIZE_TFT,LCD_YSIZE_TFT);
  delay(1000);
   //second block, MCU write with BTE ROP 
  ER_TFT.BTE_S0_Color_16bpp();
  ER_TFT.BTE_S1_Color_16bpp();
  ER_TFT.BTE_S1_Memory_Start_Address(0);
  ER_TFT.BTE_S1_Image_Width(LCD_XSIZE_TFT);
  ER_TFT.BTE_S1_Window_Start_XY(120,40);

  ER_TFT.BTE_Destination_Color_16bpp();  
  ER_TFT.BTE_Destination_Memory_Start_Address(0);
  ER_TFT.BTE_Destination_Image_Width(LCD_XSIZE_TFT);
  ER_TFT.BTE_Destination_Window_Start_XY(120,40);  
  ER_TFT.BTE_Window_Size(80,80);

  ER_TFT.BTE_ROP_Code(14);
  ER_TFT.BTE_Operation_Code(0); //BTE write
  ER_TFT.BTE_Enable();

  ER_TFT.LCD_CmdWrite(0x04);
  for(i=0; i<80*80;i++)
  {				
  ER_TFT.LCD_DataWrite(0xf000);
  ER_TFT.LCD_DataWrite(0xf000>>8);
  ER_TFT.Check_Mem_WR_FIFO_not_Full();
  }
  ER_TFT.Check_Mem_WR_FIFO_Empty();//糶Ч浪琩
  ER_TFT.Check_BTE_Busy();

  delay(1000);   
	  //third block, BTE MOVE with ROP
  ER_TFT.Canvas_Image_Start_address(layer2_start_addr);//
  ER_TFT.Canvas_image_width(LCD_XSIZE_TFT);//
  ER_TFT.Active_Window_XY(0,0);
  ER_TFT.Active_Window_WH(LCD_XSIZE_TFT,LCD_YSIZE_TFT);

 
  ER_TFT.Foreground_color_65k(0xf000);
  ER_TFT.Background_color_65k(Black);
  ER_TFT.Line_Start_XY(0,40);
  ER_TFT.Line_End_XY(80,120);
  ER_TFT.Start_Square_Fill();  
   
   
  ER_TFT.BTE_S0_Color_16bpp();
  ER_TFT.BTE_S0_Memory_Start_Address(layer2_start_addr);
  ER_TFT.BTE_S0_Image_Width(LCD_XSIZE_TFT);
  ER_TFT.BTE_S0_Window_Start_XY(0,40);

  ER_TFT.BTE_S1_Color_16bpp();
  ER_TFT.BTE_S1_Memory_Start_Address(layer1_start_addr);
  ER_TFT.BTE_S1_Image_Width(LCD_XSIZE_TFT);
  ER_TFT.BTE_S1_Window_Start_XY(220,40);

  ER_TFT.BTE_Destination_Color_16bpp();  
  ER_TFT.BTE_Destination_Memory_Start_Address(layer1_start_addr);
  ER_TFT.BTE_Destination_Image_Width(LCD_XSIZE_TFT);
  ER_TFT.BTE_Destination_Window_Start_XY(220,40);  
  ER_TFT.BTE_Window_Size(80,80);

  ER_TFT.BTE_ROP_Code(14);
  ER_TFT.BTE_Operation_Code(2); //BTE write
  ER_TFT.BTE_Enable();
  ER_TFT.Check_BTE_Busy();
  delay(1000);  
  ER_TFT.BTE_Disable();

  
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
 
 
 
 	 for(im=0;im<4;im++)
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
 
 
 
 ///////////////////////////Cartoon_Show
 			

 	ER_TFT.Select_Main_Window_16bpp();
	ER_TFT.Main_Image_Start_Address(layer2_start_addr);		   
	ER_TFT.Main_Image_Width(LCD_XSIZE_TFT);
	ER_TFT.Main_Window_Start_XY(0,0);				                   
	ER_TFT.Canvas_Image_Start_address(layer2_start_addr);	          
	ER_TFT.Canvas_image_width(LCD_XSIZE_TFT);				           
	ER_TFT.Active_Window_XY(0,0);
	ER_TFT.Active_Window_WH(LCD_XSIZE_TFT,LCD_YSIZE_TFT);	         
	ER_TFT.DrawSquare_Fill(0,0,LCD_XSIZE_TFT,LCD_YSIZE_TFT,Blue);
	ER_TFT.DMA_24bit_Block(1,0,0,0,LCD_XSIZE_TFT,LCD_YSIZE_TFT,LCD_XSIZE_TFT,Picture_1_Addr);

	ER_TFT.Canvas_image_width(223);
	ER_TFT.Canvas_Image_Start_address(layer3_start_addr);
	ER_TFT.DMA_24bit_Block(1,0,0,0,223,134*30,223,Cartoon_Addr);
  
	
	while(1)
	{
		for(i = 0 ; i < 30 ; i++)
		{
			LCD_BTE_Memory_Copy(layer3_start_addr,223,0,134*i,
			layer3_start_addr,223,0,134*i,
			layer2_start_addr,LCD_XSIZE_TFT,10,53,0X0C,220,134);
				
			delay(40);	
		}
	}
 
    
}
