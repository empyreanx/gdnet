/** 
 @file  time.h
 @brief PENet time constants and macros
*/
#ifndef __PENET_TIME_H__
#define __PENET_TIME_H__

#define PENET_TIME_OVERFLOW 86400000

#define PENET_TIME_LESS(a, b) ((a) - (b) >= PENET_TIME_OVERFLOW)
#define PENET_TIME_GREATER(a, b) ((b) - (a) >= PENET_TIME_OVERFLOW)
#define PENET_TIME_LESS_EQUAL(a, b) (! PENET_TIME_GREATER (a, b))
#define PENET_TIME_GREATER_EQUAL(a, b) (! PENET_TIME_LESS (a, b))

#define PENET_TIME_DIFFERENCE(a, b) ((a) - (b) >= PENET_TIME_OVERFLOW ? (b) - (a) : (a) - (b))

#endif /* __PENET_TIME_H__ */
