#ifndef __LINUX_PORT_H
#define __LINUX_PORT_H


#ifdef CONFIG_MIPS /* Is there a better define to use? */

/*
 * Linux include files
 */
#include <linux/types.h>
#include <asm/barrier.h>

#else

/*
 * Compatible functions or include files
 */
//#include <linux/types.h>
#include <stdint.h>
#include <stddef.h>
//#include <asm/barrier.h>
#define wmb()	do { } while(0)
#define rmb()	do { } while(0)
#define mb()	do { } while(0)

#endif


#endif
