/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#pragma once

#if !defined(__LIBROMANO_VECTOR)
#define __LIBROMANO_VECTOR

#include "libromano/libromano.h"

ROMANO_CPP_ENTER

typedef void* vector;

/* Creates a vector with a given initial size, and of the given element size */
ROMANO_API vector vector_new(size_t initial_capacity, size_t element_size);

/* Returns the size of the given vector */
ROMANO_API size_t vector_size(vector vector);

/* Returns the capacity of the given vector */
ROMANO_API size_t vector_capacity(vector vector);

/* Returns the element size of the given vector */
ROMANO_API size_t vector_element_size(vector vector);

/* Resizes the vector to the given capacity */
ROMANO_API void vector_resize(vector* vector, size_t new_capacity);

/* Adds a new element at the end of the given vector */
ROMANO_API void vector_push_back(vector* vector, void* element);

/* Adds a new element at the beginning of the given vector */
ROMANO_API void vector_insert(vector* vector, void* element, size_t position);

/* Removes the element of the given vector at the given index */
ROMANO_API void vector_remove(vector* vector, size_t position);

/* Returns the address to the element at the given index */
ROMANO_API void* vector_at(vector vector, size_t index);

/* Fit the vector to its size (if the capacity is greater than the size) */
ROMANO_API void vector_shrink_to_fit(vector* vector);

/* Frees the given vector and releases it */
ROMANO_API void vector_free(vector vector);

ROMANO_CPP_END

#endif /* !defined(__LIBROMANO_VECTOR) */