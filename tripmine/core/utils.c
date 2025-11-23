// utils.c
#include "utils.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

void utils_init_rng(void) {
    // Seed the RNG once per process run
    srand((unsigned)time(NULL));
}

int random_int_in_range(int min, int max) {
    if (min > max) {
        int tmp = min;
        min = max;
        max = tmp;
    }

    // Use long for intermediate to avoid overflow when max - min is large
    long span = (long)max - (long)min + 1;

    // Rejection sampling to avoid modulo bias
    long limit = RAND_MAX - (RAND_MAX % span);

    int r;
    do {
        r = rand();
    } while ((long)r >= limit);

    return min + (r % span);
}

char *randomName(void) {
    // Word lists (const: string literals live in read-only memory)
    static const char *first_word[]  = {"my", "tiny", "huge", "some"};
    static const char *second_word[] = {"blue", "green", "yellow", "red"};
    static const char *third_word[]  = {"whale", "monkey", "dog", "cat"};

    const char *w1 = first_word[random_int_in_range(0, 3)];
    const char *w2 = second_word[random_int_in_range(0, 3)];
    const char *w3 = third_word[random_int_in_range(0, 3)];

    // Measure formatted length (excluding terminator)
    int n = snprintf(NULL, 0, "%s-%s-%s", w1, w2, w3);
    if (n < 0) {
        return NULL; // formatting error
    }

    char *out = (char *)malloc((size_t)n + 1);
    if (!out) {
        return NULL; // allocation failed
    }

    // Write into allocated buffer
    snprintf(out, (size_t)n + 1, "%s-%s-%s", w1, w2, w3);
    return out; // caller must free(out)
}
