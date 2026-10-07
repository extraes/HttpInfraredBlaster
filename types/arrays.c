#pragma once

#include <assert.h>
#include <pico/assert.h>
#include <pico/types.h>
#include <stdint.h>
#include <pico/stdlib.h>

typedef struct array_byte {
    int length;
    uint8_t* data;
} array_byte;

static inline uint8_t get_byte(array_byte* array, int index) {
    hard_assert(0 <= index && index < array->length);

    return array->data[index];
}

static inline void set_byte(array_byte* array, int index, uint8_t item) {
    hard_assert(0 <= index && index < array->length);

    array->data[index] = item;
}

typedef struct array_uint {
    int length;
    uint* data;
} array_uint;

static inline uint get_uint(array_uint* array, int index) {
    hard_assert(0 <= index && index < array->length);

    return array->data[index];
}

static inline void set_uint(array_uint* array, int index, uint item) {
    hard_assert(0 <= index && index < array->length);

    array->data[index] = item;
}