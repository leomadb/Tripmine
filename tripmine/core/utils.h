// utils.h
#ifndef UTILS_H
#define UTILS_H

#include <stddef.h>

// Initialize RNG once (e.g., call at program start).
void utils_init_rng(void);

// Returns a random integer in inclusive range [min, max].
// Handles min > max by swapping. Avoids modulo bias.
int random_int_in_range(int min, int max);

// Returns a newly allocated "word-word-word" name.
// Caller must free the returned buffer with free().
char *randomName(void);

#endif // UTILS_H
