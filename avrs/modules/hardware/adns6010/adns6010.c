/*  
 *  Copyright RobOtter (2009)
 * 
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/** @file adns6010.c
  * @author JD
  *
  * Drive RobOtter ADNS6010 FPGA system.
  */

#include <aversive.h>
#include <aversive/error.h>
#include <aversive/wait.h>
#include <util/delay.h>

#include "adns6010.h"
#include "adns6010_spi.h"
#include "adns6010_spi_registers.h"

#include "adns6010_firmware.h"

void adns6010_init()
{
  // Initialize SPI
  adns6010_spi_init();

  // Default configuration
  ADNS6010_LOCK = 0;
  ADNS6010_ENABLE = 0;

  return;
}  


uint8_t adns6010_boot(adns6010_configuration_t* config)
{
  uint8_t byte,hbyte,lbyte,lpcfg;
  uint8_t it;

  // Wait OP + IN-RST for ADNS GO
  _delay_us(ADNS6010_TIMINGS_OP + ADNS6010_TIMINGS_INRST);

  DEBUG(ADNS6010_ERROR,"Performing reset of ADNS");

  // perform reset on all adns during pw-reset
  adns6010_setReset(1);
  _delay_us(ADNS6010_TIMINGS_PWRESET);
  adns6010_setReset(0);

  // t(pu-reset)
  _delay_us(ADNS6010_TIMINGS_INRST);

  // For each ADNS
  for(it=1;it<=ADNS6010_NUM;it++)
  {
    //------------------------------------------------
    // Load FIRMWARE on current ADNS
    //------------------------------------------------
    
    DEBUG(ADNS6010_ERROR,"Uploading firmware on ADNS6010 #%d",it);
    adns6010_uploadFirmware(it);

    //------------------------------------------------
    // Check firmware on ADNS
    //------------------------------------------------
    
    // Set CS to current ADNS
    adns6010_spi_cs(it);
    // Wait NCS-SCLK
    _delay_us(1);

    // Read SROMID register
    adns6010_spi_send(ADNS6010_SPIREGISTER_SROMID);
    _delay_us(ADNS6010_TIMINGS_SRAD);
    byte = adns6010_spi_recv();

    DEBUG(ADNS6010_ERROR,"ADNS6010 #%d SROM register=0x%X",it,byte);

    // Check if SROMID is the expected value
    if( byte != ADNS6010_FIRMWARE_ID )
    {
      // Set CS inactive
      adns6010_spi_cs(0);

      // Throw error
      ERROR(ADNS6010_ERROR,"ADNS6010 #%d : SROMID doesn't match firmware ID",it);
    }

    //------------------------------------------------
    // Perform ADNS firmware CRC computation
    //------------------------------------------------
    
    // delay read-write
    _delay_us(ADNS6010_TIMINGS_SRWSRR);

    // Start firmware SROM CRC test
    adns6010_spi_send(ADNS6010_SPI_WRITE|ADNS6010_SPIREGISTER_SROMENABLE);
    adns6010_spi_send(ADNS6010_SROMENABLE_CRC);

    // Wait for CRC to be completed
    _delay_us(ADNS6010_TIMINGS_CRC_DELAY);
    
    // Read CRC LO vlaue
    adns6010_spi_send(ADNS6010_SPIREGISTER_DOUTLOW);
    _delay_us(ADNS6010_TIMINGS_SRAD);
    lbyte = adns6010_spi_recv();
    
    // delay read-read
    _delay_us(ADNS6010_TIMINGS_SRWSRR);

    // Read CRC HI value
    adns6010_spi_send(ADNS6010_SPIREGISTER_DOUTHI);
    _delay_us(ADNS6010_TIMINGS_SRAD);
    hbyte = adns6010_spi_recv();
    
    DEBUG(ADNS6010_ERROR,"ADNS6010 #%d SROM CRC hbyte=0x%X lbyte=0x%X",it,hbyte,lbyte);

    // Check CRC value
    if( !((lbyte == ADNS6010_FIRMWARE_CRCLO)
       && (hbyte == ADNS6010_FIRMWARE_CRCHI) ))
    {
      // Set CS inactive
      adns6010_spi_cs(0);
      ERROR(ADNS6010_ERROR,
              "ADNS6010 #%d : firmware CRC fail, lo=0x%X hi=0x%X",
              it, lbyte, hbyte);
    }

    //------------------------------------------------
    // Load ADNS configuration
    //------------------------------------------------
    
    //-------------------------------
    // Load Configuration register

    // delay read-read
    _delay_us(ADNS6010_TIMINGS_SRWSRR);

    // Read register current value
    adns6010_spi_send(ADNS6010_SPIREGISTER_CONFIGURATION);
    _delay_us(ADNS6010_TIMINGS_SRAD);
    byte = adns6010_spi_recv();

    DEBUG(ADNS6010_ERROR,"ADNS6010 #%d configuration=0x%X",it,byte);

    // Set laser shutter
    if(config->shutter == ADNS6010_SHUTTER_ON)
      sbi(byte,ADNS6010_CONFIGURATION_BIT_SHUTTER);
    else
      cbi(byte,ADNS6010_CONFIGURATION_BIT_SHUTTER);

    // Set resolution
    switch(config->res)
    {
      case ADNS6010_RES_400:
        cbi(byte,ADNS6010_CONFIGURATION_BIT_RES1);
        cbi(byte,ADNS6010_CONFIGURATION_BIT_RES0);
        break;

      case ADNS6010_RES_800:
        sbi(byte,ADNS6010_CONFIGURATION_BIT_RES1);
        cbi(byte,ADNS6010_CONFIGURATION_BIT_RES0);
        break;

      case ADNS6010_RES_1600:
        cbi(byte,ADNS6010_CONFIGURATION_BIT_RES1);
        sbi(byte,ADNS6010_CONFIGURATION_BIT_RES0);
        break;

      case ADNS6010_RES_2000:
        sbi(byte,ADNS6010_CONFIGURATION_BIT_RES1);
        sbi(byte,ADNS6010_CONFIGURATION_BIT_RES0);
        break;

      default:
        adns6010_spi_cs(0);
        ERROR(ADNS6010_ERROR,"ADNS6010 #%d : bad resolution configuration",it);
    }
  
    // Constant bits in register
    sbi(byte,ADNS6010_CONFIGURATION2_BIT_ONE);

    // delay read-write
    _delay_us(ADNS6010_TIMINGS_SRWSRR);

    // Write value to register 
    adns6010_spi_send(ADNS6010_SPI_WRITE|ADNS6010_SPIREGISTER_CONFIGURATION);
    adns6010_spi_send(byte);

    //-------------------------------
    // Load ConfigurationII register

    DEBUG(ADNS6010_ERROR,"ADNS6010 #%d loading configurationII register",it);

    // delay write-read
    _delay_us(ADNS6010_TIMINGS_SWR);

    // Read register current value
    adns6010_spi_send(ADNS6010_SPIREGISTER_CONFIGURATION2);
    _delay_us(ADNS6010_TIMINGS_SRAD);
    byte = adns6010_spi_recv();

    // LASER_NEN functions as normal
    cbi(byte,ADNS6010_CONFIGURATION2_BIT_FORCEDISABLE);
    
    // Constant bits in register
    sbi(byte,ADNS6010_CONFIGURATION2_BIT_ONE);

    // delay read-write
    _delay_us(ADNS6010_TIMINGS_SRWSRR);

    // Write value to register 
    adns6010_spi_send(ADNS6010_SPI_WRITE|ADNS6010_SPIREGISTER_CONFIGURATION2);
    adns6010_spi_send(byte);

    //-------------------------------
    // Set LASER power / load LP_CFG0 register
 
    // bit7/Match shall be set to 0
    lpcfg = (config->power) & 0x7F;
    
    // LP_CFG1 shall be LP_CFG0 complement
    byte = ~lpcfg;

    DEBUG(ADNS6010_ERROR,"ADNS6010 #%d setting LPCFG0 register",it);

    // delay write-write
    _delay_us(ADNS6010_TIMINGS_SWW);
    // Write value to register 
    adns6010_spi_send(ADNS6010_SPI_WRITE|ADNS6010_SPIREGISTER_LPCFG0);
    adns6010_spi_send(lpcfg);

    //------------------------------
    // Set LASER power / load LP_CFG1 register

    DEBUG(ADNS6010_ERROR,"ADNS6010 #%d setting LPCFG1 register",it);

    // delay write-write
    _delay_us(ADNS6010_TIMINGS_SWW);
    // Write value to register
    adns6010_spi_send(ADNS6010_SPI_WRITE|ADNS6010_SPIREGISTER_LPCFG1);
    adns6010_spi_send(byte);

    //-------------------------------
    // Good to GO !

    // Set CS inactive
    adns6010_spi_cs(0);

  } /* for(it=1;it<=ADNS6010_NUM;it++) */

  // Wait maximum delay (a frame period) in case we've got
  // a command immediatly after this function.
  _delay_us(ADNS6010_TIMINGS_FRAME_PERIOD);
  _delay_us(ADNS6010_TIMINGS_FRAME_PERIOD);

  return ADNS6010_RV_OK;
}


uint8_t adns6010_checks()
{
  uint8_t it;
  uint8_t byte;
  
  // For each ADNS
  for(it=1;it<=ADNS6010_NUM;it++)
  {
    // Set current ADNS CS active
    adns6010_spi_cs(it);
    // Wait NCS-SCLK
    _delay_us(1);

    //-------------------------------
    // Read Motion register

    // Read register current value
    adns6010_spi_send(ADNS6010_SPIREGISTER_MOTION);
    _delay_us(ADNS6010_TIMINGS_SRAD);
    byte = adns6010_spi_recv();
   
    DEBUG(ADNS6010_ERROR,"ADNS6010 #%d motion=0x%X",it,byte);

    // Check if ADNS is Fault, if true, error.
    if( bit_is_set(byte,ADNS6010_MOTION_BIT_FAULT) )
    {
      adns6010_spi_cs(0);
      ERROR(ADNS6010_ERROR,
              "ADNS6010 #%d : ADNS is fault, motion=0x%X",
              it,byte);
    }

    // Check LP_CFG* consistency, if not, error.
    if( !bit_is_set(byte,ADNS6010_MOTION_BIT_LPVALID) )
    {
      adns6010_spi_cs(0);
      ERROR(ADNS6010_ERROR,
              "ADNS6010 #%d : LP_CFG* inconsistent, motion=0x%X",
              it,byte);
    }

    //-------------------------------
    // Read Observation register

    // delay read-write
    _delay_us(ADNS6010_TIMINGS_SRWSRR);

    // Clear observation register
    adns6010_spi_send(ADNS6010_SPI_WRITE|ADNS6010_SPIREGISTER_OBSERVATION);
    adns6010_spi_send(0x00);

    // wait for new frame 
    //(DS don't exactly specify delay here, one frame seems to be max refresh time)
    _delay_us(ADNS6010_TIMINGS_FRAME_PERIOD);
    _delay_us(ADNS6010_TIMINGS_FRAME_PERIOD);
    
    // Read register current value
    adns6010_spi_send(ADNS6010_SPIREGISTER_OBSERVATION);
    _delay_us(ADNS6010_TIMINGS_SRAD);
    byte = adns6010_spi_recv();

    DEBUG(ADNS6010_ERROR,"ADNS6010 #%d observation=0x%X",it,byte);

    // Check if ADNS is running on SROM code, if not, error.
    if( !bit_is_set(byte,ADNS6010_OBSERVATION_BIT_OB7) )
    {
      adns6010_spi_cs(0);
      ERROR(ADNS6010_ERROR,
              "ADNS6010 #%d : ADNS is not running on SROM code, observation=0x%X",
              it,byte);
    }
 
    // Check if NPD pulse was detected, if true, error.
    if( bit_is_set(byte,ADNS6010_OBSERVATION_BIT_OB5) )
    {
      adns6010_spi_cs(0);
      ERROR(ADNS6010_ERROR,
              "ADNS6010 #%d : NPD pulse detected, observation=0x%X",
              it,byte);
    }   

    //------------------------------------------------
    // Good to GO !
    adns6010_spi_cs(0);

  } /* for(it=1;it<=ADNS6010_NUM;it++) */

  return ADNS6010_RV_OK;
}


void adns6010_setMode(adns6010_behaviour_t behaviour)
{
  switch(behaviour)
  {
    default:
    case ADNS6010_BHVR_MODE_UC_DRIVEN:
        ADNS6010_ENABLE = 0;
      break;

    case ADNS6010_BHVR_MODE_AUTOMATIC:
        ADNS6010_ENABLE = 1;
      break;
  }

  DEBUG(ADNS6010_ERROR,"ADNS6010 system behaviour = 0x%X",behaviour);

  return;
}


void adns6010_setReset(uint8_t value)
{
  if(value)
    sbi(ADNS6010_SPI_CTRL,ADNS6010_SPICTRL_BIT_RESET);
  else
    cbi(ADNS6010_SPI_CTRL,ADNS6010_SPICTRL_BIT_RESET);

  return;
}


void adns6010_uploadFirmware(uint8_t adns_i)
{
  uint16_t nb;
  uint8_t byte;

  // Set CS to current ADNS
  adns6010_spi_cs(adns_i);

  // Wait NCS-SCLK
  _delay_us(1);

  // Write 0x1D to register SROMENABLE
  adns6010_spi_send(ADNS6010_SPI_WRITE|ADNS6010_SPIREGISTER_SROMENABLE);
  adns6010_spi_send(ADNS6010_SROMENABLE_LOAD1);

  // Set CS inactive
  adns6010_spi_cs(0);

  // Wait for new frame
  _delay_us(ADNS6010_TIMINGS_FRAME_PERIOD);
  _delay_us(ADNS6010_TIMINGS_FRAME_PERIOD);

  // Set CS to current ADNS
  adns6010_spi_cs(adns_i);
  // Wait NCS-SCLK
  _delay_us(1);

  // Write 0x18 to register SROMENABLE
  adns6010_spi_send(ADNS6010_SPI_WRITE|ADNS6010_SPIREGISTER_SROMENABLE);
  adns6010_spi_send(ADNS6010_SROMENABLE_LOAD2);

  // Set CS inactive
  adns6010_spi_cs(0);

  // Wait SWW
  _delay_us(ADNS6010_TIMINGS_SWW);

  // Set CS to current ADNS
  adns6010_spi_cs(adns_i);
  // Wait NCS-SCLK
  _delay_us(1);


  // Initiate PROM download burst mode
  adns6010_spi_send(ADNS6010_SPI_WRITE|ADNS6010_SPIREGISTER_SROMLOAD);
  // Wait LOAD
  _delay_us(ADNS6010_TIMINGS_LOAD);

  // For each firmware byte 
  for(nb=0;nb<sizeof(adns6010_firmwareArray);nb++)
  {
    // Read firmware from AVR FLASH
    byte = pgm_read_byte(adns6010_firmwareArray + nb);

    // Send firmware byte
    adns6010_spi_send( byte );

    // Wait LOAD
    _delay_us(ADNS6010_TIMINGS_LOAD);
  }

  // Exit burst mode by pulling CS for FIRMWEND
  adns6010_spi_cs(0);
  _delay_us(ADNS6010_TIMINGS_FIRMWEND);

  wait_ms(10);

  return;
}


uint8_t adns6010_computeFirmwareCRC()
{
  uint8_t crc = 0x00;
  uint8_t byte;
  uint16_t it;
  
  for(it=0;it<sizeof(adns6010_firmwareArray);it++)
  {
    byte = pgm_read_byte(adns6010_firmwareArray+it);
    crc = crc ^ byte;
  }
  return crc;
}


uint8_t adns6010_checkFirmware()
{
  uint8_t crc;
  
  // Compute CRC
  crc = adns6010_computeFirmwareCRC();
  
  // Check if CRC value is the correct one
  if( crc != ADNS6010_FIRMWARE_CRC )
    ERROR(ADNS6010_ERROR,"ADNS6010 : flash firmware corrupted, CRC=0x%X",crc);

  return ADNS6010_RV_OK;
}


uint8_t adns6010_checkSPI(void)
{
  uint8_t it;
  uint8_t byte_pid;
  uint8_t byte_ipid;

  for(it=1;it<=ADNS6010_NUM;it++)
  {
    // Activate ADNS SPI
    adns6010_spi_cs(it);

    // Wait NCS-SCLK
    _delay_us(1);
    
    // Read productID
    adns6010_spi_send(ADNS6010_SPIREGISTER_PRODUCTID);
    _delay_us(ADNS6010_TIMINGS_SRAD);
    byte_pid = adns6010_spi_recv();

    // delay read-read
    _delay_us(ADNS6010_TIMINGS_SRWSRR);

    // Read inverse productID
    adns6010_spi_send(ADNS6010_SPIREGISTER_INVPRODUCTID);
    _delay_us(ADNS6010_TIMINGS_SRAD);
    
    byte_ipid = adns6010_spi_recv();

    DEBUG(ADNS6010_ERROR,"ADNS6010 #%d checking SPI pid=0x%X ipid=0x%X",
                            it, byte_pid, byte_ipid);

    // Test if productID and inverse productID are consistents
    if( (uint8_t)(byte_pid ^ byte_ipid) != 0xff )  // byte_pid != ~byte_ipid
    {  
      adns6010_spi_cs(0);
      ERROR(ADNS6010_ERROR,
              "ADNS6010 #%d : SPI communication fail, pid=0x%X ipid=0x%X",
              it, byte_pid, byte_ipid);
    }
    
    adns6010_spi_cs(0);
  }
  
  return ADNS6010_RV_OK;
}

void adns6010_encoders_get_value(adns6010_encoders_t* encoders)
{
  // Latch all movement registers
  // (latch is triggered on lock rising edge)
  ADNS6010_LOCK = ADNS6010_LOCK_NONE;
  _delay_us(1);
  ADNS6010_LOCK = ADNS6010_LOCK_ALL;
  _delay_us(1);
  
  // ADNS 1
  encoders->vectors[ADNS6010_VX1] = ADNS6010_1_DELTAX;
  encoders->vectors[ADNS6010_VY1] = ADNS6010_1_DELTAY;
  encoders->squals[0] = ADNS6010_1_SQUAL;
 
  // ADNS 2
  encoders->vectors[ADNS6010_VX2] = ADNS6010_2_DELTAX;
  encoders->vectors[ADNS6010_VY2] = ADNS6010_2_DELTAY;
  encoders->squals[1] = ADNS6010_2_SQUAL; 

  // ADNS 3
  encoders->vectors[ADNS6010_VX3] = ADNS6010_3_DELTAX;
  encoders->vectors[ADNS6010_VY3] = ADNS6010_3_DELTAY;
  encoders->squals[2] = ADNS6010_3_SQUAL;

  // FAULT register
  encoders->fault = ADNS6010_FAULT;

  return;
}
