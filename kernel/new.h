#ifndef NEW_H
#define NEW_H

#include <stddef.h>
#include "malloc.h"
#include "debug.h"

inline void *operator new(size_t, void *p)     throw() { return p; }
inline void *operator new[](size_t, void *p)   throw() { return p; }
inline void  operator delete  (void *, void *) throw() { };
inline void  operator delete[](void *, void *) throw() { };

void *operator new(size_t size);
void *operator new[](size_t size);
void operator delete(void *p) throw();
void operator delete[](void *p) throw();
void operator delete(void*, long unsigned int);
void operator delete [](void*, long unsigned int);

#endif /* NEW_H */
