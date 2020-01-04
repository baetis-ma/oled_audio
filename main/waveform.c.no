#include <string.h>
#include <stdio.h>

#include "math.h"
#include "complex.h"

//#include <sys/param.h>
#include "esp_system.h"
#include "esp_log.h"
#include "esp_event_loop.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "nvs_flash.h"
#include "sdkconfig.h"
    
//i2c and periferal requirements
#include "driver/i2c.h"
static uint32_t i2c_frequency = 100000;
static i2c_port_t i2c_port = I2C_NUM_0;
static gpio_num_t i2c_gpio_sda = 18;
static gpio_num_t i2c_gpio_scl = 19;
#include "../components/i2c.h"
#include "../components/ssd1306.h"

#define len   1024
uint8_t framebuffer[len];

void display_text(char *disp_string){
   int line = 0; int size = 1; char f0; char f1; char ft;
   int framebuf_ptr = 0;

   for (int n=0; n< strlen( disp_string); n++) {
      if(disp_string[n] == '|') { framebuf_ptr = 128 * ++line; }
      else if(disp_string[n]=='1' && framebuf_ptr%128 == 0){size = 1;++framebuf_ptr;}
      else if(disp_string[n]=='2' && framebuf_ptr%128 == 0){size = 2;++framebuf_ptr;}
      else if(disp_string[n]=='4' && framebuf_ptr%128 == 0){size = 4;++framebuf_ptr;}
      else if(size == 1){
         for(int a=0; a < 5; a++){
             framebuffer[framebuf_ptr++] = fonttable5x7[(a + 5*(disp_string[n]-' ' ))%1024]; } 
         framebuf_ptr++; }
      else if(size == 2 || size == 4){ //twice as wide
          for(int a=0; a < 5; a++){
             framebuffer[framebuf_ptr++] = fonttable5x7[(a + 5*(disp_string[n]-' ' ))%1024]; 
             framebuffer[framebuf_ptr++] = fonttable5x7[(a + 5*(disp_string[n]-' ' ))%1024]; 
             if(size == 4){ //twice as big
                 f0 = 0; f1 = 0; 
                 ft = fonttable5x7[a + 5*(disp_string[n]-' ')] ;
                 if((ft >> 7) & 1) f0 = 0xc0; 
                 if((ft >> 6) & 1) f0 = f0 + 0x30; 
                 if((ft >> 5) & 1) f0 = f0 + 0x0c; 
                 if((ft >> 4) & 1) f0 = f0 + 0x03; 
                 if((ft >> 3) & 1) f1 = 0xc0; 
                 if((ft >> 2) & 1) f1 = f1 + 0x30; 
                 if((ft >> 1) & 1) f1 = f1 + 0x0c; 
                 if((ft >> 0) & 1) f1 = f1 + 0x03; 
                 framebuffer[framebuf_ptr - 1] = f1; 
                 framebuffer[framebuf_ptr - 2] = f1; 
                 framebuffer[128+ (framebuf_ptr - 1)] = f0; 
                 framebuffer[128+ (framebuf_ptr - 2)] = f0; 
             }
          }
       framebuf_ptr++; }
   }
}

void app_main()
{
    ESP_ERROR_CHECK( nvs_flash_init() );
    i2cdetect();
    ssd1305_init();

    uint8_t regdata[128];
    while(1){
       //clear frame buffer and write text fields
       for(int i=0; i<len; i++){ framebuffer[i]= (uint8_t)0x00; }
       char *disp_string = "4  Waveform|||||||1   12ms full scale";
       display_text(disp_string);

       i2c_read(0x48, 0x00, regdata, 128);

       uint64_t scratch;
       for(int a=0; a<128;a++){
           
           regdata[a] = regdata[a] / 4;
           char byte = 0x00;
           if(regdata[a]%0x8 == 0x0)byte = 0x80;
           if(regdata[a]%0x8 == 0x1)byte = 0x40;
           if(regdata[a]%0x8 == 0x2)byte = 0x20;
           if(regdata[a]%0x8 == 0x3)byte = 0x10;
           if(regdata[a]%0x8 == 0x4)byte = 0x08;
           if(regdata[a]%0x8 == 0x5)byte = 0x04;
           if(regdata[a]%0x8 == 0x6)byte = 0x02;
           if(regdata[a]%0x8 == 0x7)byte = 0x01;
           framebuffer[(6-(regdata[a]/0x8)) * 128 + a] = byte;
           //framebuffer[(6-(regdata[a]/0x8)) * 128 + a] = (regdata[a]+0)%0x8;
           
           
           //framebuffer[6*128 + a] = scratch/(1<<32);
           //framebuffer[5*128 + a] = (scratch%(1<<32))/(1<<24);
           //framebuffer[4*128 + a] = (scratch%(1<<24))/(1<<16);
           //framebuffer[3*128 + a] = (scratch%(1<<16))/(1<<8);
           //framebuffer[2*128 + a] = scratch%(1<<8);
       }

       i2c_write_block( 0x3c, 0x40, framebuffer, len);
//       free(framebuffer);

       vTaskDelay(10);
   }
}

