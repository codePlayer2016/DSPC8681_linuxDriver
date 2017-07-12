/*
 * DPUdebug.h
 *
 *  Created on: 2015年10月3日
 *      Author: seter
 */

#ifndef INC_DPUDEBUG_H_
#define INC_DPUDEBUG_H_

#define DEBUG_MODEL
//#undef DEBUG_MODEL

#define DEBUG_MODEL_VERBOSE
//#undef DEBUG_MODEL_VERBOSE

#define VERBOSE_MODEL 0
// first one debug macro.
#ifdef DEBUG_MODEL
#define DEBUG_INFOR(fmt,args...) \
( { if(VERBOSE_MODEL) \
	{ \
		printk("DUG_INFO: <%s,%s,%d> "fmt,__FILE__,__func__,__LINE__,##args); \
	} \
	else \
	{ \
		printk(fmt,##args); \
	} \
} )
#else
#define DEBUG_INFOR(fmt,args...)
#endif

// anther one debug macro.
#ifdef DEBUG_MODEL
#ifdef DEBUG_MODEL_VERBOSE
#define debug_printf(fmt,args...) printk("DUG_INFO: <%s,%d> "fmt,__func__,__LINE__,##args)
#else
#define debug_printf(fmt,args...) printk(fmt,##args)
#endif //DEBUG_MODEL_VERBOSE
#else
#define debug_printf(fmt,args...)
#endif //DEBUG_MODEL

#endif /* INC_DPUDEBUG_H_ */
