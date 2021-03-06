/*  
 *  Copyright Droids Corporation, Microb Technology, Eirbot (2005)
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
 *
 *  Revision : $Id: uart3_recv9_nowait.c,v 1.2 2007-05-01 15:35:51 zer0 Exp $
 *
 */

/* Olivier MATZ, Droids-corp 2004 - 2007 */

#include <uart.h>
#include <uart_defs.h>
#include <uart_private.h>

#if (defined UDR3) && (defined UART3_COMPILE)
DEFINE_RECV9_NOWAIT(3)
#endif
