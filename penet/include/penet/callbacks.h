/**
 @file  callbacks.h
 @brief PENet callbacks
*/
#ifndef __PENET_CALLBACKS_H__
#define __PENET_CALLBACKS_H__

#include <stdlib.h>

typedef struct _PENetCallbacks
{
    void * (PENET_CALLBACK * malloc) (size_t size);
    void (PENET_CALLBACK * free) (void * memory);
    void (PENET_CALLBACK * no_memory) (void);
} PENetCallbacks;

/** @defgroup callbacks PENet internal callbacks
    @{
    @ingroup private
*/
extern void * penet_malloc (size_t);
extern void   penet_free (void *);

/** @} */

#endif /* __PENET_CALLBACKS_H__ */
