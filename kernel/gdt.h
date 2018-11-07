#ifndef GDT_H
#define GDT_H

#include "descriptors.h"

constexpr int gdt_max_size = 4096;

extern tss tss0;

void gdt_init();

#endif /* GDT_H */
