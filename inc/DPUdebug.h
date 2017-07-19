/*
 * DPUdebug.h
 *
 *  Created on: 2015年10月3日
 *      Author: seter
 */

#ifndef INC_DPUDEBUG_H_
#define INC_DPUDEBUG_H_

#if 0
#define debug_printf(fmt,args...) printk("DUG_INFO: <%s,%d> "fmt,__func__,__LINE__,##args)
#endif

#define DEBUG_FLAG 1
#define debug_printf(fmt,args...) \
do \
{ \
	if(1==DEBUG_FLAG) \
	{ \
		printk("debugInfo: <%s,%d> "fmt,__func__,__LINE__,##args); \
	} \
} \
while(0)

#endif /* INC_DPUDEBUG_H_ */
