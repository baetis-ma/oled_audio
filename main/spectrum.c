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

double PI;
typedef double complex cplx;

void _fft(cplx buf[], cplx out[], int n, int step)
{
	if (step < n) {
		_fft(out, buf, n, step * 2);
		_fft(out + step, buf + step, n, step * 2);

		for (int i = 0; i < n; i += 2 * step) {
			cplx t = cexp(-I * PI * i / n) * out[i + step];
			buf[i / 2]     = out[i] + t;
			buf[(i + n)/2] = out[i] - t;
		}
	}
}

void fft(cplx buf[], int n)
{
	cplx out[n];
	for (int i = 0; i < n; i++) out[i] = buf[i];
	_fft(buf, out, n, 1);
}

void app_main()
{
   ESP_ERROR_CHECK( nvs_flash_init() );
   i2cdetect();
   ssd1305_init();
   int line = 0; int size = 1; char f0; char f1; char ft;
   char *disp_string = "4  spectrum";

   //setup and clear framebuffer;
   int len = 1024;
   int framebuf_ptr;
   uint8_t regdata[128];

   while(1){

      uint8_t *framebuffer = malloc(len);
      for(int i=0; i<len; i++){ framebuffer[i]= (uint8_t)0x00; }
      framebuf_ptr = 0;
      //prints text on oled display - contents of disp_string
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

      //read 128 bytes of data
      i2c_read(0x48, 0x00, regdata, 128);

      //calculate fft of 64 bytes of data
      cplx buf[64];
      for(int x=0;x<64;x++)buf[x] = regdata[x] + 0*I;
      fft(buf, 64);

      //calc mag of each bin - better way to do this
      int spec[64];
      for (int i = 0; i < 64; i++) {
          spec[i] = 0.2 *  sqrt (pow(creal(buf[i]),2) + pow(cimag(buf[i]),2));
          //printf("%d ", spec[i]);
          if(spec[i]<256)regdata[i]= (char)spec[i]; else regdata[i]=255;
      }
      //printf("\n");

      //write frambuffer with box for each frequency of spectrum
      uint32_t scratch;
      for(int a=0; a<64;a++){
         scratch =0;
         for(int s=0; s<32; s++) if(regdata[a/2]/8>s)scratch=scratch+(1<<(31-s));
         framebuffer[6*128 + a +30] = scratch/(1<<24);
         framebuffer[5*128 + a +30] = (scratch%(1<<24))/(1<<16);
         framebuffer[4*128 + a +30] = (scratch%(1<<16))/(1<<8);
         framebuffer[3*128 + a +30] = scratch%(1<<8);
      }

      //write frame buffer to oled display
      i2c_write_block( 0x3c, 0x40, framebuffer, len);
      free(framebuffer);

      vTaskDelay(10);
   }
}

