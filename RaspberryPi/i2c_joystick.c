#include <bcm2835.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>

#define MODE_READ 0
#define MODE_WRITE 1
 
#define MAX_LEN 32
 
char wbuf[MAX_LEN];
 
typedef enum {
    NO_ACTION,
    I2C_BEGIN,
    I2C_END
} i2c_init;

// Static Values 
uint8_t  init = I2C_BEGIN;
uint16_t clk_div = 2500; // Sets clock to 100KHz
uint8_t slave_address = 72; // ADS1115 Address

// Non-static Variables
uint32_t len = 0;
uint8_t  mode = MODE_READ; // MODE_WRITE
 
char buf[MAX_LEN];
int i;
uint8_t data;


static int readJoystickX(void)
{
    memset(wbuf, 0, sizeof wbuf);
    memset(buf, 0, sizeof buf);
    for(int i = 0; i < 2; i++)
    {
      // Reads A0 
      //fprintf(stderr, "Reading A0...\n");
      len = 3;
      wbuf[0] = 0x01;
      wbuf[1] = 0xC3;
      wbuf[2] = 0xE3;
      data = bcm2835_i2c_write(wbuf, len);
    }
      //printf("Config Register Write Result = %d\n", data);

      len = 1;
      wbuf[0] = 0x00;
      mode = MODE_WRITE;
      data = bcm2835_i2c_write(wbuf, len);
      //printf("Conversion Register Write Result = %d\n", data);

      len = 2;
      wbuf[0] = 0x00;
      mode = MODE_READ;
      for (i=0; i<MAX_LEN; i++) buf[i] = 'n';
      data = bcm2835_i2c_read(buf, len);

    //printf("Conversion Register Read Result = %d\n", data);   
    uint8_t upper = NULL; 
    uint8_t lower = NULL;
    for (i=0; i<MAX_LEN; i++) 
    {
        if(buf[i] != 'n')
        {
          //printf("Read Buf[%d] = %x\n", i, buf[i]);
          //printf("Read Buf[%d] = %d\n", i, buf[i]);

          if(upper == NULL)
          {
            upper = (uint8_t)buf[i];
          }
          else if(lower == NULL)
          {
            lower = (uint8_t)buf[i];
          }
        } 
        
    }
    uint16_t temp_upper = upper;
    int result = ((uint16_t)temp_upper << 8) + lower;
    return result;
}

static int readJoystickY(void)
{
    memset(wbuf, 0, sizeof wbuf);
    memset(buf, 0, sizeof buf);
    for(int i = 0; i < 2; i++)
    {
      // Reads A0 
      //fprintf(stderr, "Reading A1...\n");
      len = 3;
      wbuf[0] = 0x01;
      wbuf[1] = 0xD3;
      wbuf[2] = 0xE3;
      data = bcm2835_i2c_write(wbuf, len);
      //printf("Config Register Write Result = %d\n", data);
    }
      len = 1;
      wbuf[0] = 0x00;
      mode = MODE_WRITE;
      data = bcm2835_i2c_write(wbuf, len);
      //printf("Conversion Register Write Result = %d\n", data);

      len = 2;
      wbuf[0] = 0x00;
      mode = MODE_READ;
      for (i=0; i<MAX_LEN; i++) buf[i] = 'n';
      data = bcm2835_i2c_read(buf, len); 
    

    //printf("Conversion Register Read Result = %d\n", data);   
    uint8_t upper = NULL; 
    uint8_t lower = NULL;
    for (i=0; i<MAX_LEN; i++) 
    {
        if(buf[i] != 'n')
        {
          //printf("Read Buf[%d] = %x\n", i, buf[i]);
          //printf("Read Buf[%d] = %d\n", i, buf[i]);

          if(upper == NULL)
          {
            upper = (uint8_t)buf[i];
          }
          else if(lower == NULL)
          {
            lower = (uint8_t)buf[i];
          }
        } 
        
    }
    uint16_t temp_upper = upper;
    int result = ((uint16_t)temp_upper << 8) + lower;
    return result;
}

int startI2C(void) 
{
 
    //printf("Running ... \n");
 
    if (!bcm2835_init())
    {
      printf("bcm2835_init failed. Are you running as root??\n");
      return 1;
    }
      
    // I2C begin if specified    
    if (init == I2C_BEGIN)
    {
      if (!bcm2835_i2c_begin())
      {
        printf("bcm2835_i2c_begin failed. Are you running as root??\n");
        return 1;
      }
    }    
 
    bcm2835_i2c_setSlaveAddress(slave_address);
    //fprintf(stderr, "Slave address set to: %d\n", slave_address);   
    bcm2835_i2c_setClockDivider(clk_div);
    //fprintf(stderr, "Clock divider set to: %d\n", clk_div);
    return 0;
}

int stopI2C(void)
{
  bcm2835_close();
  printf("... closing!\n");
}

int main(int argc, char **argv) 
{
    startI2C();

    uint16_t x[3] = {0};
    uint16_t y[3] = {0};
    while(1)
    {
      for(int i = 0; i < 3; i++)
      {
        usleep(100);
        y[i] = readJoystickY();
        usleep(100);
        x[i] = readJoystickX();
      }
      float y_avg = ((((float)y[1] + (float)y[2] + (float)y[3])/3) - 29000 + 500)/10000;
      float x_avg = ((((float)x[1] + (float)x[2] + (float)x[3])/3) - 29000 - 200)/10000;

      printf("Y: %f\n",y_avg);
      printf("X: %f\n",x_avg);
      
      system("clear");
    }

    
    return 0;
}


