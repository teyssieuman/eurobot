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

/** \file motor_cs.c
  * \author JD
  *
  * Manage motors' control systems
  *
  */

#include "motor_cs.h"
#include "motor_cs_config.h"

#include <avr/interrupt.h>
#include <aversive/wait.h>
#include <aversive/error.h>
#include <pwm.h>

#include "settings.h"

// control system managers
struct cs csm_motor1;
struct cs csm_motor2;
struct cs csm_motor3;

// pids
struct pid_filter pid_motor1;
struct pid_filter pid_motor2;
struct pid_filter pid_motor3;

// encoders previous values
//int32_t encoder1_pvalue;
//int32_t encoder2_pvalue;
//int32_t encoder3_pvalue;

// motors bit signs
volatile uint8_t motor1_sign;
volatile uint8_t motor2_sign;
volatile uint8_t motor3_sign;

uint8_t motor_overpwm_count[3];

// PWM overflow event
ISR(SIG_OVERFLOW1)
{
  // set motor1 sign bit
  if(motor1_sign)
    sbi(MOTOR_CS_PWM1_PORT,MOTOR_CS_PWM1_PIN);
  else
    cbi(MOTOR_CS_PWM1_PORT,MOTOR_CS_PWM1_PIN);

  // set motor2 sign bit
  if(motor2_sign)
    sbi(MOTOR_CS_PWM2_PORT,MOTOR_CS_PWM2_PIN);
  else
    cbi(MOTOR_CS_PWM2_PORT,MOTOR_CS_PWM2_PIN);

  // set motor3 sign bit
  if(motor3_sign)
    sbi(MOTOR_CS_PWM3_PORT,MOTOR_CS_PWM3_PIN);
  else
    cbi(MOTOR_CS_PWM3_PORT,MOTOR_CS_PWM3_PIN);
}

void motor_cs_init()
{
  // initialize pwms
  pwm_init();

  // setup pwms dirs
  sbi(MOTOR_CS_PWM1_DDR,MOTOR_CS_PWM1_PIN);
  cbi(MOTOR_CS_PWM1_PORT,MOTOR_CS_PWM1_PIN);

  sbi(MOTOR_CS_PWM2_DDR,MOTOR_CS_PWM2_PIN);
  cbi(MOTOR_CS_PWM2_PORT,MOTOR_CS_PWM2_PIN);

  sbi(MOTOR_CS_PWM3_DDR,MOTOR_CS_PWM3_PIN);
  cbi(MOTOR_CS_PWM3_PORT,MOTOR_CS_PWM3_PIN);

  motor1_sign = 0;
  motor2_sign = 0;
  motor3_sign = 0;

  pwm_set_1A(0);
  pwm_set_1B(0);
  pwm_set_1C(0);
  
  // over pwms
  motor_overpwm_count[0] = 0;
  motor_overpwm_count[1] = 0;
  motor_overpwm_count[2] = 0;

  // setup brake
  sbi(MOTOR_CS_BREAK_DDR, MOTOR_CS_BREAK_PIN);
  cbi(MOTOR_CS_BREAK_PORT, MOTOR_CS_BREAK_PIN);

  // activate interrupts
  sbi(TIMSK,TOIE1);

	// setup PIDs
	pid_init(&pid_motor1);
	pid_init(&pid_motor2);
	pid_init(&pid_motor3);

  pid_set_gains(&pid_motor1, SETTING_PID_MOTOR1_GAIN_P,
                             SETTING_PID_MOTOR1_GAIN_I,
                             SETTING_PID_MOTOR1_GAIN_D);
  pid_set_maximums(&pid_motor1, SETTING_PID_MOTOR1_MAX_IN,
                                SETTING_PID_MOTOR1_MAX_I,
                                SETTING_PID_MOTOR1_MAX_OUT);
  pid_set_out_shift(&pid_motor1, SETTING_PID_MOTOR1_SHIFT);
 

  pid_set_gains(&pid_motor2, SETTING_PID_MOTOR2_GAIN_P,
                             SETTING_PID_MOTOR2_GAIN_I,
                             SETTING_PID_MOTOR2_GAIN_D);
  pid_set_maximums(&pid_motor2, SETTING_PID_MOTOR2_MAX_IN,
                                SETTING_PID_MOTOR2_MAX_I,
                                SETTING_PID_MOTOR2_MAX_OUT);
  pid_set_out_shift(&pid_motor2, SETTING_PID_MOTOR2_SHIFT);
 

  pid_set_gains(&pid_motor3, SETTING_PID_MOTOR3_GAIN_P,
                             SETTING_PID_MOTOR3_GAIN_I,
                             SETTING_PID_MOTOR3_GAIN_D);
  pid_set_maximums(&pid_motor3, SETTING_PID_MOTOR3_MAX_IN,
                                SETTING_PID_MOTOR3_MAX_I,
                                SETTING_PID_MOTOR3_MAX_OUT);
  pid_set_out_shift(&pid_motor3, SETTING_PID_MOTOR3_SHIFT);
 
	// setup CSMs
	cs_init(&csm_motor1);
	cs_init(&csm_motor2);
	cs_init(&csm_motor3);

	cs_set_consign_filter(&csm_motor1, NULL, NULL);
	cs_set_consign_filter(&csm_motor2, NULL, NULL);
	cs_set_consign_filter(&csm_motor3, NULL, NULL);

	cs_set_correct_filter(&csm_motor1, &pid_do_filter, &pid_motor1);
	cs_set_correct_filter(&csm_motor2, &pid_do_filter, &pid_motor2);
	cs_set_correct_filter(&csm_motor3, &pid_do_filter, &pid_motor3);

	cs_set_feedback_filter(&csm_motor1, NULL, NULL);
	cs_set_feedback_filter(&csm_motor2, NULL, NULL);
	cs_set_feedback_filter(&csm_motor3, NULL, NULL);

	cs_set_process_out( &csm_motor1, &get_encoder_motor1, NULL);
	cs_set_process_out( &csm_motor2, &get_encoder_motor2, NULL);
	cs_set_process_out( &csm_motor3, &get_encoder_motor3, NULL);

	cs_set_process_in( &csm_motor1, &set_pwm_motor1, NULL);
	cs_set_process_in( &csm_motor2, &set_pwm_motor2, NULL);
	cs_set_process_in( &csm_motor3, &set_pwm_motor3, NULL);

	return;
}


void motor_cs_update(void* dummy, int32_t m1, int32_t m2, int32_t m3)
{
	// set consigns for motors CS
	cs_set_consign(&csm_motor1, m1);
	cs_set_consign(&csm_motor2, m2);
	cs_set_consign(&csm_motor3, m3);

	// update motors CS
	cs_manage(&csm_motor1);
	cs_manage(&csm_motor2);
	cs_manage(&csm_motor3);

 	return;
}

void motor_cs_break(uint8_t state)
{
  if(state)
    cbi(MOTOR_CS_BREAK_PORT,MOTOR_CS_BREAK_PIN);
  else    
    sbi(MOTOR_CS_BREAK_PORT,MOTOR_CS_BREAK_PIN);
}

int32_t get_encoder_motor1(void* dummy)
{
  int32_t value;
  int32_t speed;
  static int32_t pvalue;

  value = _SFR_MEM32(MOTOR_CS_ENCODER1_ADDR);

  speed = value - pvalue;
  pvalue = value;

	return speed;
}

int32_t get_encoder_motor2(void* dummy)
{
  int32_t value;
  int32_t speed;
  static int32_t pvalue;

  value = _SFR_MEM32(MOTOR_CS_ENCODER2_ADDR);

  speed = value - pvalue;
  pvalue = value;

	return speed;
}

int32_t get_encoder_motor3(void* dummy)
{
  int32_t value;
  int32_t speed;
  static int32_t pvalue;

  value = _SFR_MEM32(MOTOR_CS_ENCODER3_ADDR);

  speed = value - pvalue;
  pvalue = value;

	return speed;
}

void set_pwm_motor1(void* dummy, int32_t pwm)
{
  S_MAX(pwm,4095);

  if( pwm >= SETTING_MOTORS_MAXPWM )
  {
    if(motor_overpwm_count[0] < 255)
      motor_overpwm_count[0]++;
  }
  else
    motor_overpwm_count[0] = 0;

  if(pwm>0)
  {
    pwm_set_1C(4095-pwm);
    motor1_sign = 1;
  }
  else
  {
    pwm_set_1C(-pwm);
    motor1_sign = 0;
  }

  return;
}

void set_pwm_motor2(void* dummy, int32_t pwm)
{
  S_MAX(pwm,4095);

  if( pwm >= SETTING_MOTORS_MAXPWM )
  {
    if(motor_overpwm_count[1] < 255)
      motor_overpwm_count[1]++;
  }
  else
    motor_overpwm_count[1] = 0;

  if(pwm>0)
  {
    pwm_set_1B(4095-pwm);
    motor2_sign = 1;
  }
  else
  {
    pwm_set_1B(-pwm);
    motor2_sign = 0;
  }

  return;
}

void set_pwm_motor3(void* dummy, int32_t pwm)
{
  S_MAX(pwm,4095);
  
  if( pwm >= SETTING_MOTORS_MAXPWM )
  {
    if(motor_overpwm_count[2] < 255)
      motor_overpwm_count[2]++;
  }
  else
    motor_overpwm_count[2] = 0;

	if(pwm>0)
  {
    pwm_set_1A(4095-pwm);
    motor3_sign = 1;
  }
  else
  {
    pwm_set_1A(-pwm);
    motor3_sign = 0;
  }

  return;
}
