/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#pragma once

#if !defined(__LIBROMANO_VECTOR)
#define __LIBROMANO_VECTOR

#include "libromano/libromano.h"

#include <assert.h>

ROMANO_CPP_ENTER

typedef void* vector;

#define GET_VEC_PTR(ptr) ((char*)(ptr) + 3 * sizeof(size_t))
#define GET_RAW_PTR(ptr) ((char*)(ptr) - 3 * sizeof(size_t))

#define GET_SIZE(ptr) ((size_t*)GET_RAW_PTR(ptr))[0]
#define GET_CAPACITY(ptr) ((size_t*)GET_RAW_PTR(ptr))[1]
#define GET_ELEMENT_SIZE(ptr) ((size_t*)GET_RAW_PTR(ptr))[2]

/* Creates a vector with a given initial size, and of the given element size */
ROMANO_API vector vector_new(size_t initial_capacity, const size_t element_size);

/* Returns the size of the given vector */
ROMANO_STATIC_FUNCTION ROMANO_FORCE_INLINE size_t vector_size(const vector vector) { assert(vector != NULL); return GET_SIZE(vector); }

/* Returns the capacity of the given vector */
ROMANO_STATIC_FUNCTION ROMANO_FORCE_INLINE size_t vector_capacity(const vector vector) { assert(vector != NULL); return GET_CAPACITY(vector); }

/* Returns the element size of the given vector */
ROMANO_STATIC_FUNCTION ROMANO_FORCE_INLINE size_t vector_element_size(const vector vector) { assert(vector != NULL); return GET_ELEMENT_SIZE(vector); }

/* Resizes the vector to the given capacity */
ROMANO_API void vector_resize(vector* vector, const size_t new_capacity);

/* Adds a new element at the end of the given vector */
ROMANO_API void vector_push_back(vector* vector, void* element);

/* Similar to push back but moves instead of copying */
ROMANO_API void vector_emplace_back(vector* vector, void* element);

/* Adds a new element at the beginning of the given vector */
ROMANO_API void vector_insert(vector* vector, void* element, const size_t position);

/* Removes the element of the given vector at the given index */
ROMANO_API void vector_remove(vector* vector, const size_t position);

/* Removes the element at the end of the vector */
ROMANO_API void vector_pop(vector* vector);

/* Returns the address to the element at the given index */
ROMANO_API void* vector_at(const vector vector, const size_t index);

/* Fit the vector to its size (if the capacity is greater than the size) */
ROMANO_API void vector_shrink_to_fit(vector* vector);

/* Frees the given vector and releases it */
ROMANO_API void vector_free(vector vector);

#if !defined(__LIBROMANO_VECTOR_IMPL)
#undef GET_VEC_PTR
#undef GET_RAW_PTR

#undef GET_SIZE
#undef GET_CAPACITY
#undef GET_ELEMENT_SIZE
#endif /* defined(__LIBROMANO_VECTOR_IMPL) */


ROMANO_CPP_END

#endif /* !defined(__LIBROMANO_VECTOR) */
