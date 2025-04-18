/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#pragma once

#if !defined(__LIBROMANO_VECTOR)
#define __LIBROMANO_VECTOR

#include "libromano/libromano.h"

ROMANO_CPP_ENTER

struct _Vector {
    void* data;
};

typedef struct _Vector Vector;

#define GET_VEC_PTR(ptr) ((char*)(ptr) + 3 * sizeof(size_t))

/* Creates a heap allocated vector with a given initial size, and of the given element size */
ROMANO_API Vector* vector_new(const size_t initial_capacity, const size_t element_size);

/* Initializes a vector */
ROMANO_API void vector_init(Vector* vector, const size_t initial_capacity, const size_t element_size);

/* Returns the size of the given vector */
ROMANO_FORCE_INLINE size_t vector_size(Vector* vector) { ROMANO_ASSERT(vector != NULL, "Vector has not been initialized"); return ((size_t*)vector->data)[0]; }

/* Returns the capacity of the given vector */
ROMANO_FORCE_INLINE size_t vector_capacity(Vector* vector) { ROMANO_ASSERT(vector != NULL, "Vector has not been initialized"); return ((size_t*)vector->data)[1]; }

/* Returns the element size of the given vector */
ROMANO_FORCE_INLINE size_t vector_element_size(Vector* vector) { ROMANO_ASSERT(vector != NULL, "Vector has not been initialized"); return ((size_t*)vector->data)[2]; }

/* Resizes the vector to the given capacity */
ROMANO_API void vector_resize(Vector* vector, const size_t new_capacity);

/* Adds a new element at the end of the given vector */
ROMANO_API void vector_push_back(Vector* vector, void* element);

/* Returns the address where the element should be constructed */
ROMANO_API void* _vector_emplace_back(Vector* vector);

/* Macro to build the element in place */
#define vector_emplace_back(vector, type, ctor)               \
    do { type* address = (type*)_vector_emplace_back(vector); \
         *address = ctor; } while(0);

/* Adds a new element at the beginning of the given vector */
ROMANO_API void vector_insert(Vector* vector, void* element, const size_t position);

/* Removes the element of the given vector at the given index */
ROMANO_API void vector_remove(Vector* vector, const size_t position);

/* Removes the element at the end of the vector */
ROMANO_API void vector_pop(Vector* vector);

/* Returns the address to the element at the given index */
ROMANO_API void* vector_at(Vector* vector, const size_t index);

/* Returns the address to the last element */
ROMANO_API void* vector_back(Vector* vector);

/* Fits the vector to its size (if the capacity is greater than the size) */
ROMANO_API void vector_shrink_to_fit(Vector* vector);

typedef int (*vector_sort_cmp_function)(const void*, const void*);

ROMANO_API void vector_sort(Vector* vector, vector_sort_cmp_function cmp);

ROMANO_API void vector_shuffle(Vector* vector, const uint64_t seed);

#define VECTOR_NOT_FOUND 0xFFFFFFFFFFFFFFFF

ROMANO_API size_t vector_find(Vector* vector, void* value);

/* Frees the given vector and releases it */
ROMANO_API void vector_free(Vector* vector);

#if !defined(__LIBROMANO_VECTOR_IMPL)
#undef GET_VEC_PTR
#undef GET_RAW_PTR

#undef GET_SIZE
#undef GET_CAPACITY
#undef GET_ELEMENT_SIZE
#endif /* defined(__LIBROMANO_VECTOR_IMPL) */

ROMANO_CPP_END

#endif /* !defined(__LIBROMANO_VECTOR) */
