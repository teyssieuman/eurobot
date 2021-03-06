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

/** \file htrajectory.c
 * \date 02/01/2010 
 * \author JD
 *
 * Trajectory management NG
 *
 */

#include "htrajectory.h"

#include "avoidance.h"
#include "logging.h"
#include <math.h>

// avoidance system
extern avoidance_t avoidance;

// robot system
extern hrobot_system_t system;

/* -- private functions -- */

/** \brief Compute normalized error vector to current point */
inline vect_xy_t computeNormalizedError( vect_xy_t point, vect_xy_t carrot )
{ 
  vect_xy_t error;
  double errorLength;
  double dx,dy;

  dx = point.x - carrot.x;
  dy = point.y - carrot.y;

  // compute error vector length
  errorLength = sqrt( dx*dx + dy*dy );

  if( errorLength == 0.0 )
  {
    error.x = 0.0;
    error.y = 0.0;
  }
  else
  {
    // normalize error vector
    error.x = dx/errorLength;
    error.y = dy/errorLength;
  }

  return error;
}

/** \brief Prepare point */
inline void preparePoint( htrajectory_t *htj )
{
  // compute and store normalized error vector
  htj->normalizedError = 
    computeNormalizedError(htj->path[htj->pathIndex],htj->carrot);
  
  DEBUG(0,"Going to %d (%2.2f,%2.2f) state=%d",
    htj->pathIndex,
    htj->path[htj->pathIndex].x,
    htj->path[htj->pathIndex].y,
    htj->state);
}

/** \brief Set htj->carrot angle, transfer orders to low level CSs */
inline void setCarrotAPosition( htrajectory_t *htj, double a )
{
  robot_cs_set_a_consign( htj->rcs, RCS_RAD_TO_CSUNIT*a );
}

/** \brief Set htj->carrot position, transfer orders to low level CSs */
inline void setCarrotXYPosition( htrajectory_t *htj, vect_xy_t pos )
{
  robot_cs_set_xy_consigns( htj->rcs, RCS_MM_TO_CSUNIT*pos.x,
                                      RCS_MM_TO_CSUNIT*pos.y );
}

/** \brief Copy given path into internal structure */
inline void copyPath( htrajectory_t *htj, vect_xy_t *path, uint8_t n)
{
  uint8_t it;

  if( n == 0 )
    ERROR(HTRAJECTORY_ERROR,"zero point path");

  if( n > HTRAJECTORY_MAX_POINTS )
    ERROR(HTRAJECTORY_ERROR,"%d points path over %d points limit",n,HTRAJECTORY_MAX_POINTS);

  htj->pathSize = n;

  // copy points in internal structure
  for(it=0;it<n;it++)
    htj->path[it] = path[it];
}

/** \brief Check if robot is in window of current point
  * \return 1- if target reached 0- otherwise
  */
uint8_t inWindowXY( htrajectory_t *htj )
{
  vect_xy_t robot,point;
  double dx,dy,dl;
  uint8_t inWindow;

  // get robot position
  hposition_get_xy( htj->hrp, &robot);

  // get next point position
  point = htj->path[htj->pathIndex];
  
  // XXX stop window is default window ?
  if( htj->state == STATE_PATH_MID )
    dl = htj->xySteeringWindow;
  else
    dl = htj->xyStopWindow;

  dx = point.x - robot.x;
  dy = point.y - robot.y;

  // coarse inegality to save computing time
  if( dx > dl && dy > dl )
    inWindow = 0;
  else
    // (squared inegality)
    // check if robot is in window  
    if( dx*dx + dy*dy < dl*dl )
      inWindow = 1;
    else
      inWindow = 0;

  return inWindow;
} 

/* -- public functions -- */

void htrajectory_init( htrajectory_t *htj,
                        hrobot_position_t *hrp,
                        robot_cs_t *rcs,
                        struct quadramp_filter *qramp_angle)
{
  htj->hrp = hrp;
  htj->rcs = rcs;
  htj->qramp_a = qramp_angle;


  htj->cruiseSpeed = 0.0;
  htj->cruiseAcc = 0.0;

  htj->steeringSpeed = 0.0;
  htj->steeringAcc = 0.0;

  htj->stopSpeed = 0.0;
  htj->stopAcc = 0.0;

  htj->xySteeringWindow = 1.0;
  htj->xyStopWindow = 1.0;
  htj->aStopWindow = 1.0;
 
  htj->carrot = (vect_xy_t){0.0,0.0};
  htj->carrotA = 0.0;
  htj->carrotSpeed = 0.0;

  htj->pathIndex = 0;
  htj->state = STATE_STOP;
  htj->blocked = 0;

  htj->lpos = (vect_xy_t){0.0,0.0};
  htj->la = 0.0;

  return;
}

/* -- orders -- */

void htrajectory_run( htrajectory_t *htj, vect_xy_t *path, uint8_t n )
{
  // copy points to internal structure
  copyPath( htj, path, n );

  DEBUG(0,"New path loaded (size=%d) and running",n);

  htj->pathIndex = 0;

  if(htj->pathSize == 1)
    htj->state = STATE_PATH_LAST;
  else
    htj->state = STATE_PATH_MID;

  preparePoint(htj);

  return;
}

void htrajectory_gotoA( htrajectory_t *htj, double a )
{
  double robot_a;
  double da;

  // get robot angle
  hposition_get_a( htj->hrp, &robot_a );

  // compute distance between consign and position modulo 2pi
  da = fmod( a - robot_a, 2*M_PI );
  
  // update consign
  htj->carrotA = robot_a + da;

  // set robot carrot
  setCarrotAPosition(htj, htj->carrotA );
}

void htrajectory_gotoXY( htrajectory_t *htj, double x, double y)
{
  vect_xy_t path;

  // create a one point path
  path = (vect_xy_t){x,y};
  
  // load and run path
  htrajectory_run(htj, &path,  1);
}

void htrajectory_gotoA_R( htrajectory_t *htj, double da)
{
  double ra;
  hposition_get_a( htj->hrp, &ra);
  htrajectory_gotoA( htj, ra + da);
}

void htrajectory_gotoXY_R( htrajectory_t *htj, double dx, double dy)
{
  vect_xy_t rp;
  hposition_get_xy( htj->hrp, &rp);
  htrajectory_gotoXY( htj, rp.x + dx, rp.y + dy);
}

/* -- speed management -- */

void htrajectory_setASpeed( htrajectory_t *htj, double speed, double acc )
{
  quadramp_set_1st_order_vars(htj->qramp_a, speed, speed);
  quadramp_set_2nd_order_vars(htj->qramp_a, acc, acc);
}

void htrajectory_setXYCruiseSpeed( htrajectory_t *htj, double speed, double acc )
{
  htj->cruiseSpeed = speed;
  htj->cruiseAcc = acc;
}

void htrajectory_setXYSteeringSpeed( htrajectory_t *htj, double speed, double acc )
{
  htj->steeringSpeed = speed;
  htj->steeringAcc = acc;
}

void htrajectory_setXYStopSpeed( htrajectory_t *htj, double speed, double acc )
{
  htj->stopSpeed = speed;
  htj->stopAcc = acc;
}

/* -- target window management -- */

void htrajectory_setSteeringWindow( htrajectory_t *htj, double xywin )
{
  htj->xySteeringWindow = xywin;
}

void htrajectory_setStopWindows( htrajectory_t *htj, double xywin, double awin )
{
  htj->xyStopWindow = xywin;
  htj->aStopWindow = awin;
}

/* -- status -- */

uint8_t htrajectory_doneXY( htrajectory_t *htj )
{
  if( htj->state == STATE_STOP )
    return 1;
  else
    return 0;
}

uint8_t htrajectory_doneA( htrajectory_t *htj )
{
  double robot_a;
  double da;

  hposition_get_a(htj->hrp, &robot_a);

  da = fabs(htj->carrotA - robot_a);

  if( da < htj->aStopWindow )
    return 1;
  else
    return 0;
}

uint8_t htrajectory_blocked( htrajectory_t *htj )
{
  return htj->blocked;
}

uint8_t htrajectory_doneAutoset( htrajectory_t *htj )
{
  if( (htj->state == STATE_AUTOSET_HEADING)
    || (htj->state == STATE_AUTOSET_MOVE) )
    return 1;
  else
    return 0;
}

/* -- trajectory update -- */

void htrajectory_update( htrajectory_t *htj )
{
  double sqErrorLength;
  vect_xy_t *point;
  double dx,dy,da;
  direction_t dir;
  vect_xy_t robot;
  double a;
  double dv;

  if( htj->state == STATE_STOP )
    /* nothing to do */
    return;

  if( htj->state == STATE_AUTOSET_HEADING )
  {
    // wait for robot heading OK
    if( htrajectory_doneA(htj) )
    {
      DEBUG(0,"AUTOSET HEADING done (a=%2.2f)",htj->carrotA);

      // set autoset to next step
      htj->state = STATE_AUTOSET_MOVE;

      // shutdown robot CSs
      robot_cs_activate(htj->rcs, 0);
    
      dx = SETTING_AUTOSET_SPEED*cos(htj->carrotA+1.5*M_PI);
      dy = SETTING_AUTOSET_SPEED*sin(htj->carrotA+1.5*M_PI);
      
      DEBUG(0,"AUTOSET MOTORS (dx=%2.2f dy=%2.2f)",dx,dy);

      // set course
      hrobot_set_motors(&system, dx, dy, 0.0);

      // reset autoset count
      htj->autosetCount = 0;
    }

    return;
  }

  if( htj->state == STATE_AUTOSET_MOVE )
  {
    // get robot position
    hposition_get_xy(htj->hrp,&robot);
    hposition_get_a(htj->hrp,&a);
    
    // compute speed in configuration space
    dx = robot.x - htj->lpos.x;
    dy = robot.y - htj->lpos.y;
    da = a - htj->la;
    dv = dx*dx + dy*dy + da*da;
    
    //
    if( dv < SETTING_AUTOSET_ZEROSPEED )
    {
      htj->autosetCount++;

      if( htj->autosetCount > SETTING_AUTOSET_ZEROCOUNT )
      {
        // autoset done
        
        // reactivate robot CSs damn it !
        robot_cs_activate(htj->rcs, 1);

        // set trajectory status to stop
        htj->state = STATE_STOP;
      }
    }
    else
      htj->autosetCount = 0;

    // remember last position
    htj->lpos = robot;
    htj->la = a;
  }

  // is robot blocked by something ?
  dir = avoidance_check(&avoidance);

  // if blocked ...
  if( dir != DIR_NONE )
  {
    // ... and previously not blocked
    if( !(htj->blocked) )
    {

      // get robot position
      hposition_get_xy( htj->hrp, &robot);
      
      // set carrot to current position
      setCarrotXYPosition(htj, robot);

      DEBUG(0,"AVOID BLOCK(dir=%d) (%2.2f,%2.2f)", 
                dir, robot.x, robot.y);

      // reset carrot speed
      htj->carrotSpeed = 0;
      htj->blocked = 1;
    }
    
    // if robot blocked do not update anything more
    return;
  }

  // if NOT blocked and previously blocked
  if( (dir == DIR_NONE) && (htj->blocked) )
  {
    DEBUG(0,"AVOID NOBLOCK carrot (%2.2f,%2.2f)",
              htj->carrot.x, htj->carrot.y);

    hposition_get_xy( htj->hrp, &robot);

    htj->carrot = robot;

    // resume target to carrot
    setCarrotXYPosition(htj, htj->carrot);

    htj->blocked = 0;
  }

  if( htj->blocked )
    return;

  // is robot in position ?
  if( inWindowXY(htj) )
  {
    // last point reached
    if( htj->state == STATE_PATH_LAST )
    {
      DEBUG(0,"Point %d (%2.2f,%2.2f) reached, full stop",
        htj->pathIndex,
        htj->path[htj->pathIndex].x,
        htj->path[htj->pathIndex].y);

      // put trajectory management to full stop
      htj->pathIndex = 0;
      htj->state = STATE_STOP;
      
      // set htj->carrot to last position
      setCarrotXYPosition( htj, htj->path[htj->pathSize - 1] );

      return;
    }

    DEBUG(0,"Point %d (%2.2f,%2.2f) reached, goto next point",
        htj->pathIndex,
        htj->path[htj->pathIndex].x,
        htj->path[htj->pathIndex].y);

    // switch to next point 
    htj->pathIndex++;
    
    // if next point is last point
    if( htj->pathIndex >= htj->pathSize - 1 )
      htj->state = STATE_PATH_LAST;

    preparePoint(htj);    
  }
  
  point = htj->path + htj->pathIndex;


  // --- compute speed consign & ramp ---
  
  // compute distance at which constant deceleration will bring robot
  // to desired speed
  double nextSpeed = 0.0;
  double currAcc = 0.0;
  double decDistance;
  
  if( htj->state == STATE_PATH_MID )
  {
    nextSpeed = htj->steeringSpeed;
    currAcc = htj->steeringAcc;
  }
  else
    if( htj->state == STATE_PATH_LAST )
    {
      nextSpeed = htj->stopSpeed;
      currAcc = htj->stopAcc;
    }
    else
    {
      /* should not happen */
      ERROR(HTRAJECTORY_ERROR,
              "update reach an incorrect state : state=%d",htj->state);
    }
       
  // compute squared distance between carrot and target
  dx = point->x - htj->carrot.x;
  dy = point->y - htj->carrot.y;
  sqErrorLength = dx*dx + dy*dy;

  decDistance = 0.5*((htj->carrotSpeed)*(htj->carrotSpeed)
                        - nextSpeed*nextSpeed)/currAcc;
  // * deceleration phase
  if( sqErrorLength < decDistance*decDistance )
  {
    // set speed to minimum speed when acceleration will cause algorithm to overshoot
    if( htj->carrotSpeed - nextSpeed > currAcc )
      htj->carrotSpeed -= currAcc;
    else
      htj->carrotSpeed = nextSpeed;
  } 
  // * acceleration phase
  else if( (htj->carrotSpeed) < htj->cruiseSpeed )
  {
    // set speed to maximum speed when acceleration will cause algorithm to overshoot
    if( htj->cruiseSpeed - htj->carrotSpeed > htj->cruiseAcc )
      htj->carrotSpeed += htj->cruiseAcc;
    else
      htj->carrotSpeed = htj->cruiseSpeed;
  }
  // * stable phase
  else
  {
    /* nothing to do */
  }
  
  // --- compute carrot position ---

  if( (htj->carrotSpeed)*(htj->carrotSpeed) > sqErrorLength )
  {
    htj->carrot.x = point->x;
    htj->carrot.y = point->y;
  }
  else
  {
    htj->carrot.x += (htj->carrotSpeed)*(htj->normalizedError.x);
    htj->carrot.y += (htj->carrotSpeed)*(htj->normalizedError.y);
  }

  // --- update carrot position ---
  setCarrotXYPosition( htj, htj->carrot );

}

// --- AUTOSET ---

void htrajectory_autoset( htrajectory_t *htj, robotSide_t side)
{
  vect_xy_t robot;

  // reset trajectory and start autoset
  htj->pathIndex = 0;
  htj->state = STATE_AUTOSET_HEADING;

  // get robot position
  hposition_get_xy( htj->hrp, &robot);

  // set carrot position to current position
  setCarrotXYPosition( htj, robot);

  DEBUG(0,"Robot AUTOSET initiated");

  // set carrot heading
  switch(side)
  {
    case RS_0:
      htj->carrotA = 0;
      break;

    case RS_120:
      htj->carrotA = 2.0*M_PI/3;
      break;

    case RS_240:
      htj->carrotA = 4.0*M_PI/3;
      break;

    default:
      ERROR(HTRAJECTORY_ERROR,
        "AUTOSET launched with invalid side (side=%d)", side);
  }

  htj->autosetSide = side;
}


