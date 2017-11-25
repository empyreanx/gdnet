/** 
 @file  list.h
 @brief PENet list management
*/
#ifndef __PENET_LIST_H__
#define __PENET_LIST_H__

#include <stdlib.h>

typedef struct _PENetListNode
{
   struct _PENetListNode * next;
   struct _PENetListNode * previous;
} PENetListNode;

typedef PENetListNode * PENetListIterator;

typedef struct _PENetList
{
   PENetListNode sentinel;
} PENetList;

extern void penet_list_clear (PENetList *);

extern PENetListIterator penet_list_insert (PENetListIterator, void *);
extern void * penet_list_remove (PENetListIterator);
extern PENetListIterator penet_list_move (PENetListIterator, void *, void *);

extern size_t penet_list_size (PENetList *);

#define penet_list_begin(list) ((list) -> sentinel.next)
#define penet_list_end(list) (& (list) -> sentinel)

#define penet_list_empty(list) (penet_list_begin (list) == penet_list_end (list))

#define penet_list_next(iterator) ((iterator) -> next)
#define penet_list_previous(iterator) ((iterator) -> previous)

#define penet_list_front(list) ((void *) (list) -> sentinel.next)
#define penet_list_back(list) ((void *) (list) -> sentinel.previous)

#endif /* __PENET_LIST_H__ */
