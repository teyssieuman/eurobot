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

/** \file stratcomm.c
  * \author JD
  */

#include <aversive/error.h>
#include <i2cs.h>
#include <string.h>
#include <time.h>

#include "stratcomm.h"
#include "stratcomm_payloads.h"
#include "stratcomm_orders.h"
#include "settings.h"
#include "led.h"
#include "actuators.h"

extern actuators_t actuators;

void stratcomm_init(stratcomm_t* sc)
{
  sc->payloadIt = 0;
  sc->returnPayloadIt = 0;
  return;
}

void stratcomm_update(stratcomm_t* sc)
{
  stratcommOrder_t order;
  uint8_t flags;

  uint8_t data_recv[I2CS_RECV_BUF_SIZE];

  // check if mailbox is full
  if(i2cs_recv_size > 0)
  {
    // ------------------------------------------------------------
    // RECEIVE

    led_on(1);

    IRQ_LOCK(flags);

    // release RX i2c
    i2cs_recv_size = 0;
    // block TX i2c
    i2cs_send_size = 0;

    // copy i2c received data to local buffer
    memcpy(data_recv, (uint8_t*)i2cs_recv_buf, I2CS_RECV_BUF_SIZE*sizeof(uint8_t));

    IRQ_UNLOCK(flags);

    // -- ORDER --
    // first byte should be ORDER
    order = (stratcommOrder_t)data_recv[0];
    
    // -- PAYLOAD SIZE  --
    // second byte should be payload size
    sc->payloadSize = data_recv[1];
  
    // if payloadSize will overflow i2c buffer size
    if(sc->payloadSize > STRATCOMM_MAX_PAYLOAD_SIZE)
    {
      WARNING(STRATCOMM_ERROR,
        "i2c message payload size too big (payloadSize=%d, I2CS_SEND_BUF_SIZE=%d\n",
        sc->payloadSize,I2CS_SEND_BUF_SIZE); 
     
      return;
    }
    
    DEBUG(0,"ORDER RCV : order=0x%2.2x psize=0x%2.2x",order,sc->payloadSize);

    // --
    stratcomm_resetPayload(sc);
    stratcomm_resetReturnPayload(sc);

    // perform ORDER
    stratcomm_doOrder(sc, order, data_recv+2);

    // ------------------------------------------------------------
    // SEND
    
    // 
    memset((uint8_t*)i2cs_send_buf, 0, I2CS_SEND_BUF_SIZE);

    // size of returned payload
    i2cs_send_buf[0] = sc->returnPayloadIt;

    // prepare returned payload
    memcpy((uint8_t*)i2cs_send_buf + 1, sc->returnPayload, sc->returnPayloadIt);

    // set i2c state machine to READY (data OK to be sent)
    i2cs_send_size = sc->returnPayloadIt + 1;

    led_off(1);
  }

  return;
}

void stratcomm_doOrder(stratcomm_t* sc,
                        stratcommOrder_t order,
                        uint8_t* payload)
{
  uint8_t b,n;
  time_h tv;

  switch(order)
  {
    //---------------------------------------------------------
    // NONE order
    case SO_NONE:
      DEBUG(0,"new order NONE received");
      break;

    //---------------------------------------------------------
    // 42 order
    case SO_FORTYTWO:
      DEBUG(0,"new order FORTYTWO received");

      b = 0x42;
      stratcomm_pushReturnPayload(sc, PACK_UINT8(b), sizeof(uint8_t));

      break;

    //---------------------------------------------------------
    // open clamp
    case SO_CLAMP_OPEN:

      // unpack payload
      n = UNPACK_UINT8(sc,payload);

      DEBUG(0,"new order CLAMP(%d) OPEN received",n);

      // perform order
      actuators_clamp_open(&actuators,n);

      break;

    //---------------------------------------------------------
    // close clamp
    case SO_CLAMP_CLOSE:

      // unpack payload
      n = UNPACK_UINT8(sc,payload);

      DEBUG(0,"new order CLAMP(%d) CLOSE received",n);

      // perform order
      actuators_clamp_close(&actuators,n);

      break;

    //---------------------------------------------------------
    // clamp is opened
    case SO_CLAMP_IS_OPENED:

      // unpack payload
      n = UNPACK_UINT8(sc,payload);

      DEBUG(0,"new order CLAMP(%d) IS OPENED received",n);

      // prepare data
      b = actuators_clamp_isOpened(&actuators,n);

      // push payload
      stratcomm_pushReturnPayload(sc, PACK_UINT8(b), sizeof(uint8_t));

      break;

    //---------------------------------------------------------
    // clamp is closed
    case SO_CLAMP_IS_CLOSED:

      // unpack payload
      n = UNPACK_UINT8(sc,payload);

      DEBUG(0,"new order CLAMP(%d) IS CLOSED received",n);

      // prepare data
      b = actuators_clamp_isClosed(&actuators,n);

      // push payload
      stratcomm_pushReturnPayload(sc, PACK_UINT8(b), sizeof(uint8_t));

      break;

    //---------------------------------------------------------
    // raise clamp
    case SO_CLAMP_RAISE:

      // unpack payload
      n = UNPACK_UINT8(sc,payload);

      DEBUG(0,"new order CLAMP(%d) RAISE received",n);

      // perform order
      actuators_clamp_raise(&actuators,n);

      break;

    //---------------------------------------------------------
    // lower clamp
    case SO_CLAMP_LOWER:

      // unpack payload
      n = UNPACK_UINT8(sc,payload);

      DEBUG(0,"new order CLAMP(%d) LOWER received",n);

      // perform order
      actuators_clamp_lower(&actuators,n);

      break;

    //---------------------------------------------------------
    // clamp is raised
    case SO_CLAMP_IS_RAISED:

      // unpack payload
      n = UNPACK_UINT8(sc,payload);

      DEBUG(0,"new order CLAMP(%d) IS RAISED received",n);

      // prepare data
      b = actuators_clamp_isRaised(&actuators,n);

      // push payload
      stratcomm_pushReturnPayload(sc, PACK_UINT8(b), sizeof(uint8_t));

      break;

    //---------------------------------------------------------
    // clamp is closed
    case SO_CLAMP_IS_LOWERED:

      // unpack payload
      n = UNPACK_UINT8(sc,payload);

      DEBUG(0,"new order CLAMP(%d) IS LOWERED received",n);

      // prepare data
      b = actuators_clamp_isLowered(&actuators,n);

      // push payload
      stratcomm_pushReturnPayload(sc, PACK_UINT8(b), sizeof(uint8_t));

      break;

    //---------------------------------------------------------
    // get system time
    case SO_GET_TIME:

      DEBUG(0,"new order GET_TIME received");

      // no payload to unpack

      // get current time
      tv = time_get_time();

      stratcomm_pushReturnPayload(sc,PACK_UINT8(tv.s),sizeof(uint32_t));
      stratcomm_pushReturnPayload(sc,PACK_UINT8(tv.us),sizeof(uint32_t));

      break;


    //---------------------------------------------------------
    default:
      WARNING(STRATCOMM_ERROR,"unrecognized order 0x%2.2x",order);
      break;
  }

  return;
}

uint8_t stratcomm_computeChecksum(uint8_t* payload, uint8_t payloadSize)
{
  uint8_t it;
  uint8_t crc = 0x00;

  for(it=0;it<payloadSize;it++)
    crc ^= payload[it]; 
  
  return crc;
}

void stratcomm_resetPayload(stratcomm_t *sc)
{
  // reset payload read pointer
  sc->payloadIt = 0;
}

uint8_t* stratcomm_popPayload(stratcomm_t* sc, uint8_t *p, uint8_t psize)
{
  uint8_t *pdata;

  if(sc->payloadIt + psize > sc->payloadSize)
    ERROR(STRATCOMM_ERROR,
      "can't pop value on payload, will overflow. (pit=%d ps=%d)",
      sc->payloadIt, psize);

  pdata = (p + sc->payloadIt);
  sc->payloadIt += psize;

  return pdata;
}

void stratcomm_resetReturnPayload(stratcomm_t *sc)
{
  // reset buffer write pointer
  sc->returnPayloadIt = 0;
}

void stratcomm_pushReturnPayload(stratcomm_t* sc, uint8_t* p, uint8_t psize )
{
  // if push will overflow return payload buffer
  if(sc->returnPayloadIt + psize > STRATCOMM_MAX_RPAYLOAD_SIZE)
    ERROR(STRATCOMM_ERROR,
      "can't push new value on return payload, will overflow. (rpit=%d psz=%d",sc->returnPayloadIt, psize);
 
  // add value to payload
  memcpy( sc->returnPayload + sc->returnPayloadIt, p, psize);

  // update payload iterator
  sc->returnPayloadIt += psize;

  return;
}
