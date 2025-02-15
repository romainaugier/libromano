/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#define __LIBROMANO_VECTOR_IMPL

#include "libromano/vector.h"
#include "libromano/random.h"
#include "libromano/memory.h"

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>

#define GOLDEN_RATIO 1.61f

Vector* vector_new(const size_t initial_capacity, const size_t element_size)
{
    Vector* new_vector;
    size_t capacity;

    new_vector = (Vector*)malloc(sizeof(Vector));

    capacity = initial_capacity == 0 ? 128 : initial_capacity;
    
    new_vector->data = malloc(3 * sizeof(size_t) + capacity * element_size);

    ((size_t*)new_vector->data)[0] = 0;
    ((size_t*)new_vector->data)[1] = capacity;
    ((size_t*)new_vector->data)[2] = element_size;

    return new_vector;
}

void vector_init(Vector* vector, const size_t initial_capacity, const size_t element_size)
{
    size_t capacity;

    capacity = initial_capacity == 0 ? 128 : initial_capacity;
    
    vector->data = malloc(3 * sizeof(size_t) + capacity * element_size);

    ((size_t*)vector->data)[0] = 0;
    ((size_t*)vector->data)[1] = capacity;
    ((size_t*)vector->data)[2] = element_size;
}

void vector_resize(Vector* vector, const size_t new_capacity)
{
    size_t old_capacity;
    size_t element_size;
    size_t new_size;
    void* new_address;

    assert(vector != NULL);

    old_capacity = vector_capacity(vector);
    element_size = vector_element_size(vector);
    
    if(new_capacity == 0 || new_capacity <= old_capacity)
    {
        return;
    }

    new_size = 3 * sizeof(size_t) + new_capacity * element_size;

    new_address = realloc(vector->data, new_size);

    vector->data = new_address;

    ((size_t*)vector->data)[1] = new_capacity;
}

void _vector_grow(Vector* vector)
{
    size_t new_capacity;

    assert(vector != NULL);
    
    new_capacity = (size_t)round((float)vector_capacity(vector) * GOLDEN_RATIO);

    vector_resize(vector, new_capacity);
}

void vector_push_back(Vector* vector, void* element)
{
    size_t vec_size;
    size_t vec_capacity;
    size_t elem_size;
    void* element_address;

    assert(vector != NULL);

    vec_size = vector_size(vector);
    vec_capacity = vector_capacity(vector);
    elem_size = vector_element_size(vector);

    if(vec_capacity == vec_size)
    {
        _vector_grow(vector);
    }

    element_address = vector_at(vector, vec_size);
    memcpy(element_address, element, elem_size);

    ((size_t*)vector->data)[0] = vec_size + 1;
}

void vector_emplace_back(Vector* vector, void* element)
{
    size_t vec_size;
    size_t vec_capacity;
    size_t elem_size;
    void* element_address;

    assert(vector != NULL);

    vec_size = vector_size(vector);
    vec_capacity = vector_capacity(vector);
    elem_size = vector_element_size(vector);

    if(vec_capacity == vec_size)
    {
        _vector_grow(vector);
    }

    element_address = vector_at(vector, vec_size);
    memmove(element_address, element, elem_size);

    ((size_t*)vector->data)[0] = vec_size + 1;
}

void vector_insert(Vector* vector, void* element, const size_t position)
{
    size_t vec_size;
    size_t vec_capacity;
    size_t elem_size;
    void* element_address;

    assert(vector != NULL);

    vec_size = vector_size(vector);
    vec_capacity = vector_capacity(vector);
    elem_size = vector_element_size(vector);

    assert(position < vec_size);

    if(vec_capacity == vec_size)
    {
        _vector_grow(vector);
    }

    element_address = vector_at(vector, position);
    memmove((char*)element_address + elem_size, element_address, (vec_size - position) * elem_size);
    memcpy(element_address, element, elem_size);

    ((size_t*)vector->data)[0] = vec_size + 1;
}

void vector_remove(Vector* vector, const size_t position)
{
    size_t vec_size;
    size_t elem_size;
    void* element_address;

    assert(vector != NULL);
    
    vec_size = vector_size(vector);
    elem_size = vector_element_size(vector);

    element_address = vector_at(vector, position);
    memmove(element_address, (char*)element_address + elem_size, (vec_size - position) * elem_size);

    ((size_t*)vector->data)[0] = vec_size - 1;
}

void vector_pop(Vector* vector)
{
    size_t vec_size;

    assert(vector != NULL);

    vec_size = vector_size(vector);

    assert(vec_size > 0);

    ((size_t*)vector->data)[0] = vec_size - 1;
}

void* vector_at(Vector* vector, const size_t index)
{
    size_t element_size;
    
    assert(vector != NULL);
    
    element_size = vector_element_size(vector);
    
    return (void*)((char*)((size_t*)vector->data + 3) + index * element_size);
}

void* vector_back(Vector* vector)
{
    assert(vector != NULL);

    return vector_at(vector, vector_size(vector) - 1);
}

void vector_shrink_to_fit(Vector* vector)
{
    size_t vec_size;
    size_t elem_size;
    void* new_address;

    assert(vector != NULL);
    
    vec_size = vector_size(vector);
    elem_size = vector_element_size(vector);

    new_address = realloc(vector->data, vec_size * elem_size);

    vector->data = new_address;
    ((size_t*)vector->data)[1] = vec_size;
}

void vector_sort(Vector* vector, vector_sort_cmp_function cmp)
{
    qsort(vector_at(vector, 0), vector_size(vector), vector_element_size(vector), cmp);
}

void vector_shuffle(Vector* vector, const uint64_t seed)
{
    size_t size;
    size_t element_size;
    size_t i;
    size_t j;
    size_t rnd;

    size = vector_size(vector);
    element_size = vector_element_size(vector);

    for (i = 0; i < (size - 1); ++i) 
    {
        rnd = murmur_64(seed + i);
        j = i + rnd / (UINT64_MAX / (size - i) + 1);

        mem_swap(vector_at(vector, i), vector_at(vector, j), element_size);
    }
}

void vector_free(Vector* vector)
{
    if(vector != NULL)
    {
        if(vector->data != NULL)
        {
            free(vector->data);
        }

        free(vector);
    }
}
