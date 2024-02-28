/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#define __LIBROMANO_VECTOR_IMPL

#include "libromano/vector.h"

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>

#define GOLDEN_RATIO 1.61f

vector_t* vector_new(size_t initial_capacity, size_t element_size)
{
    vector_t* new_vector;

    new_vector = (vector_t*)malloc(sizeof(vector_t));

    initial_capacity = initial_capacity == 0 ? 128 : initial_capacity;
    
    new_vector->data = malloc(3 * sizeof(size_t) + initial_capacity * element_size);

    ((size_t*)new_vector->data)[0] = 0;
    ((size_t*)new_vector->data)[1] = initial_capacity;
    ((size_t*)new_vector->data)[2] = element_size;

    return new_vector;
}

void vector_init(vector_t* vector, size_t initial_capacity, size_t element_size)
{
    initial_capacity = initial_capacity == 0 ? 128 : initial_capacity;
    
    vector->data = malloc(3 * sizeof(size_t) + initial_capacity * element_size);

    ((size_t*)vector->data)[0] = 0;
    ((size_t*)vector->data)[1] = initial_capacity;
    ((size_t*)vector->data)[2] = element_size;
}

void vector_resize(vector_t* vector, size_t new_capacity)
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

void _vector_grow(vector_t* vector)
{
    size_t new_capacity;

    assert(vector != NULL);
    
    new_capacity = (size_t)round((float)vector_capacity(vector) * GOLDEN_RATIO);

    vector_resize(vector, new_capacity);
}

void vector_push_back(vector_t* vector, void* element)
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

void vector_emplace_back(vector_t* vector, void* element)
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

void vector_insert(vector_t* vector, void* element, size_t position)
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

void vector_remove(vector_t* vector, size_t position)
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

void vector_pop(vector_t* vector)
{
    size_t vec_size;

    assert(vector != NULL);

    vec_size = vector_size(vector);

    assert(vec_size > 0);

    ((size_t*)vector->data)[0] = vec_size - 1;
}

void* vector_at(vector_t* vector, size_t index)
{
    size_t element_size;
    
    assert(vector != NULL);
    
    element_size = vector_element_size(vector);
    
    return (void*)((char*)((size_t*)vector->data + 3) + index * element_size);
}

void vector_shrink_to_fit(vector_t* vector)
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

void vector_free(vector_t* vector)
{
    if(vector != NULL)
    {
        free(vector->data);
        free(vector);
    }
}
