/*  
 *  Copyright RobOtter (2010) 
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

/** \file sys.c
  * \author JD
  */

#include <aversive.h>
#include <aversive/error.h>
#include <ax12.h>
#include <timer.h>
#include <scheduler.h>

#include "sys.h"
#include "led.h"
#include "ax12_user.h"
#include "scanner.h"
#include "actuators.h"
#include "stratcomm.h"
#include "settings.h"

// ax12s
AX12 ax12;
// actuators
actuators_t actuators;
// scanner
scanner_t scanner;
// stratcomm
extern stratcomm_t stratcomm;

// SYS cpu usage in percents
uint8_t sys_cpuUsage;

void sys_init(void)
{
  NOTICE(0,"Initializing AX12s");
  ax12_user_init();

  NOTICE(0,"Initializing actuators");
  actuators_init(&actuators);

  NOTICE(0,"Initializing scanner");
  scanner_init(&scanner);
}

void sys_update(void* dummy)
{
  uint16_t dt;
  static uint8_t led = 0;

  // some LED feedback
  if( (led+=40)>50 )
    led_on(0);
  else
    led_off(0);

  // reset TIMER3
  timer3_set(0);

  // update communications
  stratcomm_update(&stratcomm);

  // update scanner
  scanner_update(&scanner);

  // compute CPU usage
  dt = timer3_get();
  sys_cpuUsage = (100*timer3_tics_to_us(dt))
       /(SETTING_SCHED_SYS_PERIOD*SCHEDULER_UNIT);

  #ifndef SETTING_OVERRIDE_CPUUSAGE
    if(sys_cpuUsage > 95)
      ERROR(SYS_ERROR,"sys_update CPU usage go over 95% : %d",sys_cpuUsage);
  #endif
}
