/** 
 @file list.c
 @brief PENet linked list functions
*/
#define PENET_BUILDING_LIB 1
#include "penet/penet.h"

/**
    @defgroup list PENet linked list utility functions
    @ingroup private
    @{
*/
void
penet_list_clear (PENetList * list)
{
   list -> sentinel.next = & list -> sentinel;
   list -> sentinel.previous = & list -> sentinel;
}

PENetListIterator
penet_list_insert (PENetListIterator position, void * data)
{
   PENetListIterator result = (PENetListIterator) data;

   result -> previous = position -> previous;
   result -> next = position;

   result -> previous -> next = result;
   position -> previous = result;

   return result;
}

void *
penet_list_remove (PENetListIterator position)
{
   position -> previous -> next = position -> next;
   position -> next -> previous = position -> previous;

   return position;
}

PENetListIterator
penet_list_move (PENetListIterator position, void * dataFirst, void * dataLast)
{
   PENetListIterator first = (PENetListIterator) dataFirst,
                    last = (PENetListIterator) dataLast;

   first -> previous -> next = last -> next;
   last -> next -> previous = first -> previous;

   first -> previous = position -> previous;
   last -> next = position;

   first -> previous -> next = first;
   position -> previous = last;

   return first;
}

size_t
penet_list_size (PENetList * list)
{
   size_t size = 0;
   PENetListIterator position;

   for (position = penet_list_begin (list);
        position != penet_list_end (list);
        position = penet_list_next (position))
     ++ size;

   return size;
}

/** @} */
