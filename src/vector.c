/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#include "libromano/vector.h"

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <math.h>
#include <string.h>

#define GOLDEN_RATIO 1.61f

#define GET_VEC_PTR(ptr) ((char*)(ptr) + 3 * sizeof(size_t))
#define GET_RAW_PTR(ptr) ((char*)(ptr) - 3 * sizeof(size_t))

#define GET_SIZE(ptr) ((size_t*)GET_RAW_PTR(ptr))[0]
#define GET_CAPACITY(ptr) ((size_t*)GET_RAW_PTR(ptr))[1]
#define GET_ELEMENT_SIZE(ptr) ((size_t*)GET_RAW_PTR(ptr))[2]

vector vector_new(size_t initial_capacity, size_t element_size)
{
    vector new_vector;

    initial_capacity = initial_capacity == 0 ? 128 : initial_capacity;
    
    new_vector = malloc(3 * sizeof(size_t) + initial_capacity * element_size);

    ((size_t*)new_vector)[0] = 0;
    ((size_t*)new_vector)[1] = initial_capacity;
    ((size_t*)new_vector)[2] = element_size;

    return GET_VEC_PTR(new_vector);
}

size_t vector_size(vector vector)
{
    assert(vector != NULL);

    return GET_SIZE(vector);
}

size_t vector_capacity(vector vector)
{
    assert(vector != NULL);

    return GET_CAPACITY(vector);
}

size_t vector_element_size(vector vector)
{
    assert(vector != NULL);

    return GET_ELEMENT_SIZE(vector);
}

void vector_resize(vector* vector, size_t new_capacity)
{
    size_t old_capacity;
    size_t element_size;
    size_t new_size;
    void* new_address;

    assert(*vector != NULL);

    old_capacity = vector_capacity(*vector);
    element_size = vector_element_size(*vector);
    
    if(new_capacity <= old_capacity)
    {
        return;
    }

    new_size = 3 * sizeof(size_t) + new_capacity * element_size;

    new_address = realloc(GET_RAW_PTR(*vector), new_size);

    *vector = GET_VEC_PTR(new_address);

    GET_CAPACITY(*vector) = new_capacity;
}

void _vector_grow(vector* vector)
{
    size_t new_capacity;

    assert(*vector != NULL);
    
    new_capacity = (size_t)round((float)vector_capacity(*vector) * GOLDEN_RATIO);

    vector_resize(vector, new_capacity);
}

void vector_push_back(vector* vector, void* element)
{
    size_t vec_size;
    size_t vec_capacity;
    size_t elem_size;
    void* element_address;

    assert(*vector != NULL);

    vec_size = vector_size(*vector);
    vec_capacity = vector_capacity(*vector);
    elem_size = vector_element_size(*vector);

    if(vec_capacity == vec_size)
    {
        _vector_grow(vector);
    }

    element_address = vector_at(*vector, vec_size);
    memcpy(element_address, element, elem_size);

    GET_SIZE(*vector) = vec_size + 1;
}

void vector_insert(vector* vector, void* element, size_t position)
{
    size_t vec_size;
    size_t vec_capacity;
    size_t elem_size;
    void* element_address;

    assert(*vector != NULL);

    vec_size = vector_size(*vector);
    vec_capacity = vector_capacity(*vector);
    elem_size = vector_element_size(*vector);

    assert(position < vec_size);

    if(vec_capacity == vec_size)
    {
        _vector_grow(vector);
    }

    element_address = vector_at(*vector, position);
    memmove((char*)element_address + elem_size, element_address, (vec_size - position) * elem_size);
    memcpy(element_address, element, elem_size);

    GET_SIZE(*vector) = vec_size + 1;
}

void vector_remove(vector* vector, size_t position)
{
    size_t vec_size;
    size_t elem_size;
    void* element_address;

    assert(*vector != NULL);
    
    vec_size = vector_size(*vector);
    elem_size = vector_element_size(*vector);

    element_address = vector_at(*vector, position);
    memmove(element_address, (char*)element_address + elem_size, (vec_size - position) * elem_size);

    GET_SIZE(*vector) = vec_size - 1;
}

void* vector_at(vector vector, size_t index)
{
    size_t element_size;
    
    assert(vector != NULL);
    
    element_size = vector_element_size(vector);
    
    return (void*)((char*)vector + index * element_size);
}

void vector_shrink_to_fit(vector* vector)
{
    size_t vec_size;
    size_t elem_size;
    void* new_address;

    assert(*vector != NULL);
    
    vec_size = vector_size(*vector);
    elem_size = vector_element_size(*vector);

    new_address = realloc(GET_RAW_PTR(*vector), vec_size * elem_size);

    *vector = GET_VEC_PTR(new_address);
    GET_CAPACITY(*vector) = vec_size;
}

void vector_free(vector vector)
{
    if(vector != NULL)
    {
        free(GET_RAW_PTR(vector));
        vector = NULL;
    }
}