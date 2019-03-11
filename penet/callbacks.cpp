/** 
 @file callbacks.c
 @brief PENet callback functions
*/
#define PENET_BUILDING_LIB 1
#include "penet/penet.h"

#include "core/os/memory.h"

static PENetCallbacks callbacks = { malloc, free, abort };

int
penet_initialize_with_callbacks (PENetVersion version, const PENetCallbacks * inits)
{
   if (version < PENET_VERSION_CREATE (1, 3, 0))
     return -1;

   if (inits -> malloc != NULL || inits -> free != NULL)
   {
      if (inits -> malloc == NULL || inits -> free == NULL)
        return -1;

      callbacks.malloc = inits -> malloc;
      callbacks.free = inits -> free;
   }

   if (inits -> no_memory != NULL)
     callbacks.no_memory = inits -> no_memory;

   return penet_initialize ();
}

PENetVersion
penet_linked_version (void)
{
    return PENET_VERSION;
}

void *
penet_malloc (size_t size)
{
	return memalloc(size);
   /*void * memory = callbacks.malloc (size);

   if (memory == NULL)
     callbacks.no_memory ();

   return memory;*/
}

void
penet_free (void * memory)
{
	return memfree(memory);
   //callbacks.free (memory);
}
