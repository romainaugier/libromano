/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#pragma once

#if !defined(__LIBROMANO_VECTOR)
#define __LIBROMANO_VECTOR

#include "libromano/libromano.h"

#include <assert.h>

ROMANO_CPP_ENTER

struct vector {
    void* data;
};

typedef struct vector vector_t;

#define GET_VEC_PTR(ptr) ((char*)(ptr) + 3 * sizeof(size_t))

/* Creates a heap allocated vector with a given initial size, and of the given element size */
ROMANO_API vector_t* vector_new(const size_t initial_capacity, const size_t element_size);

/* Initializes a vector */
ROMANO_API void vector_init(vector_t* vector, const size_t initial_capacity, const size_t element_size);

/* Returns the size of the given vector */
ROMANO_STATIC_FUNCTION ROMANO_FORCE_INLINE size_t vector_size(vector_t* vector) { assert(vector != NULL); return ((size_t*)vector->data)[0]; }

/* Returns the capacity of the given vector */
ROMANO_STATIC_FUNCTION ROMANO_FORCE_INLINE size_t vector_capacity(vector_t* vector) { assert(vector != NULL); return ((size_t*)vector->data)[1]; }

/* Returns the element size of the given vector */
ROMANO_STATIC_FUNCTION ROMANO_FORCE_INLINE size_t vector_element_size(vector_t* vector) { assert(vector != NULL); return ((size_t*)vector->data)[2]; }

/* Resizes the vector to the given capacity */
ROMANO_API void vector_resize(vector_t* vector, const size_t new_capacity);

/* Adds a new element at the end of the given vector */
ROMANO_API void vector_push_back(vector_t* vector, void* element);

/* Similar to push back but moves instead of copying */
ROMANO_API void vector_emplace_back(vector_t* vector, void* element);

/* Adds a new element at the beginning of the given vector */
ROMANO_API void vector_insert(vector_t* vector, void* element, const size_t position);

/* Removes the element of the given vector at the given index */
ROMANO_API void vector_remove(vector_t* vector, const size_t position);

/* Removes the element at the end of the vector */
ROMANO_API void vector_pop(vector_t* vector);

/* Returns the address to the element at the given index */
ROMANO_API void* vector_at(vector_t* vector, const size_t index);

/* Fit the vector to its size (if the capacity is greater than the size) */
ROMANO_API void vector_shrink_to_fit(vector_t* vector);

/* Frees the given vector and releases it */
ROMANO_API void vector_free(vector_t* vector);

#if !defined(__LIBROMANO_VECTOR_IMPL)
#undef GET_VEC_PTR
#undef GET_RAW_PTR

#undef GET_SIZE
#undef GET_CAPACITY
#undef GET_ELEMENT_SIZE
#endif /* defined(__LIBROMANO_VECTOR_IMPL) */

ROMANO_CPP_END

#endif /* !defined(__LIBROMANO_VECTOR) */
