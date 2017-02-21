#ifndef HEAP_H
#define HEAP_H

#include <stdlib.h>

typedef struct bloc bloc;

// m_reserve : initialise la mémoire réserve.
extern int m_reserve();

// m_dispose : libère la mémoire réserve.
extern void m_dispose();

// m_toggle_offer : bascule entre les modes meilleure offre et première offre
//    par défaut, le mode meilleure offre est sélectionné.
extern void m_toggle_offer(void);

// m_alloc : allocates space for an object whose size is specified by
//   size and whose value is indeterminate. The m_alloc function returns
//   either a null pointer or a pointer to the allocated space.
extern void * m_alloc(size_t size);

// m_free : causes the space pointed to by ptr to be deallocated, that is,
//   made available for further allocation. If ptr is a null pointer,
//   no action occurs. Otherwise, if the argument does not match a pointer
//   earlier returned by a memory management function, or if the space has been
//   deallocated by a call to free or realloc, the behavior is undefined.
extern void m_free(void * ptr);

// m_print : affiche la réserve mémoire
extern void m_print();

#endif
