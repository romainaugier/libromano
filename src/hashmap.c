#include "libromano/hashmap.h"
#include "libromano/math/common32.h"
#include "libromano/bit.h"
#include "libromano/logger.h"

#define INTERNED_SIZE ((sizeof(size_t) - 1) + ROMANO_SIZEOF_PTR)

ROMANO_PACKED_STRUCT(struct _entry {
    char* key;
    size_t key_size;
    void* value;
    size_t value_size;
});

typedef struct _entry entry_t;

static void entry_new(entry_t* entry,
                      const char* key, 
                      const size_t key_size,
                      void* value,
                      size_t value_size)
{
    assert(entry != NULL);

    if(key_size < (INTERNED_SIZE))
    {
        memcpy(entry, key, key_size * sizeof(char));
        ((char*)entry)[key_size] = '\0';
        ((char*)entry)[INTERNED_SIZE] = (key_size & 0xFF);
    }
    else
    {
        entry->key = (char*)malloc((key_size + 1) * sizeof(char));
        memcpy(entry->key, key, key_size * sizeof(char));
        entry->key[key_size] = '\0';
#if ROMANO_BYTE_ORDER == ROMANO_BYTE_ORDER_LITTLE_ENDIAN
        entry->key_size = mem_bswapu64(key_size);
#else
        entry->key_size = key_size;
#endif /* ROMANO_BYTE_ORDER == ROMANO_BYTE_ORDER_LITTLE_ENDIAN */
    }

    if(value_size <= INTERNED_SIZE)
    {
        memcpy(&((char*)entry)[INTERNED_SIZE + 1], value, value_size);
        ((char*)entry)[INTERNED_SIZE * 2] = (value_size & 0xFF);
    }
    else
    {
        entry->value = malloc(value_size);
        memcpy(entry->value, value, value_size);
#if ROMANO_BYTE_ORDER == ROMANO_BYTE_ORDER_LITTLE_ENDIAN
        entry->value_size = mem_bswapu64(value_size);
#else
        entry->value_size = value_size;
#endif /* ROMANO_BYTE_ORDER == ROMANO_BYTE_ORDER_LITTLE_ENDIAN */
    }
}

ROMANO_FORCE_INLINE static size_t entry_get_value_size(entry_t* entry)
{
    size_t value_size;

    value_size = (size_t)(((char*)entry)[INTERNED_SIZE * 2]);

    if(value_size <= INTERNED_SIZE)
    {
        return value_size;
    }

#if ROMANO_BYTE_ORDER == ROMANO_BYTE_ORDER_LITTLE_ENDIAN
    return mem_bswapu64(entry->value_size);
#else
    return entry->value_size;
#endif /* ROMANO_BYTE_ORDER == ROMANO_BYTE_ORDER_LITTLE_ENDIAN */
}

ROMANO_FORCE_INLINE static void* entry_get_value(entry_t* entry)
{
    if(entry_get_value_size(entry) <= INTERNED_SIZE)
    {
        return &(entry->value);
    }

    return entry->value;
}

ROMANO_FORCE_INLINE static size_t entry_get_key_size(entry_t* entry)
{
    char key_size = (((char*)entry)[INTERNED_SIZE]);

    if(key_size < (INTERNED_SIZE))
    {
        return (size_t)key_size;
    }

#if ROMANO_BYTE_ORDER == ROMANO_BYTE_ORDER_LITTLE_ENDIAN
    return mem_bswapu64(entry->key_size);
#else
    return entry->key_size;
#endif /* ROMANO_BYTE_ORDER == ROMANO_BYTE_ORDER_LITTLE_ENDIAN */
}

ROMANO_FORCE_INLINE char* entry_get_key(entry_t* entry)
{
    if(entry_get_key_size(entry) < (INTERNED_SIZE))
    {
        return (char*)entry;
    }

    return entry->key;
}

ROMANO_FORCE_INLINE bool entry_is_empty(entry_t* entry)
{
    return entry_get_key_size(entry) == 0;
}

#define TOMBSTONE 0xFFFFFFFFFFFFFFFF

ROMANO_FORCE_INLINE void entry_set_tombstone(entry_t* entry)
{
    memset(entry, 0, sizeof(entry_t));
    entry->value_size = TOMBSTONE;
}

ROMANO_FORCE_INLINE bool entry_is_tombstone(entry_t* entry)
{
    return entry_get_value_size(entry) == TOMBSTONE;
}

void entry_free(entry_t* entry)
{
    assert(entry != NULL);
    
    if(!(entry_get_key_size(entry) < INTERNED_SIZE))
    {
        free(entry->key);
    }

    if(!(entry_get_value_size(entry) <= INTERNED_SIZE))
    {
        free(entry->value);
    }

    entry->key = NULL;
    entry->key_size = 0;
    entry->value = NULL;
    entry->value_size = 0;
}

#define HASHMAP_MAX_LOAD 0.9f
#define HASHMAP_GROWTH_RATE 2.0f
#define HASHMAP_INITIAL_CAPACITY 1024

struct _StringHashMap {
   entry_t* entries;
   size_t size;
   size_t capacity;
};

entry_t* entry_find_get(entry_t* __restrict entries,
                        const size_t capacity,
                        const char* key,
                        const size_t key_len)
{
    entry_t* entry;
    uint32_t index;
    size_t entry_key_len;

    assert(entries!= NULL);
    assert(key != NULL);

    entry = NULL;
    index = hash_fnv1a(key, key_len) % capacity;

    while(1)
    {
        entry = &entries[index];

        entry_key_len = entry_get_key_size(entry);

        if(entry_is_empty(entry))
        {
            if(!entry_is_tombstone(entry))
            {
                return NULL;
            }
        }
        else if(entry_key_len == key_len && memcmp(key, entry_get_key(entry), key_len) == 0)
        {
            return entry;
        }

        index = (index + 1) % capacity;
    }

    return NULL;
}

entry_t* entry_find_grow(entry_t* __restrict entries,
                         size_t capacity,
                         const char* key,
                         size_t key_len)
{
    entry_t* entry;
    uint32_t index;
    size_t entry_key_len;

    assert(entries != NULL);
    assert(key != NULL);

    entry = NULL;
    index = hash_fnv1a(key, key_len) % capacity;

    while(1)
    {
        entry = &entries[index];

        entry_key_len = entry_get_key_size(entry);

        if(entry_is_empty(entry))
        {
            return entry;
        }

        index = (index + 1) % capacity;
    }

    return NULL;
}

size_t string_hashmap_get_new_capacity(StringHashMap* hashmap)
{
    return round_u64_to_next_pow2(hashmap->capacity + 1) + 1;
}

void string_hashmap_grow(StringHashMap* hashmap,
                         size_t capacity)
{
    size_t i;
    entry_t* entries;
    entry_t* entry;
    entry_t* new_entry;

    assert(hashmap != NULL);
    
    entries = (entry_t*)calloc(capacity, sizeof(entry_t));

    hashmap->size = 0;

    if(hashmap->entries != NULL)
    {
        for(i = 0; i < hashmap->capacity; i++)
        {
            entry = &hashmap->entries[i];

            if(entry_is_empty(entry))
            {
                continue;
            }

            new_entry = entry_find_grow(entries, capacity, entry_get_key(entry), entry_get_key_size(entry));

            memmove(new_entry, entry, sizeof(entry_t));

            hashmap->size++;
        }

        free(hashmap->entries);
    }

    hashmap->entries = entries;
    hashmap->capacity = capacity;
}

entry_t* entry_find_insert(StringHashMap* hashmap,
                           const char* key,
                           size_t key_len)
{
    entry_t* entry;
    entry_t* tombstone;
    uint32_t index;
    size_t entry_key_len;
    size_t num_probes;
    size_t max_probes;

    assert(hashmap != NULL);
    assert(key != NULL);

    start:

    entry = NULL;
    tombstone = NULL;
    num_probes = 0;
    max_probes = (size_t)mathf_log((float)hashmap->capacity);
    index = hash_fnv1a(key, key_len) % hashmap->capacity;

    while(1)
    {
        entry = &hashmap->entries[index];

        entry_key_len = entry_get_key_size(entry);

        if(entry_is_empty(entry))
        {
            if(!entry_is_tombstone(entry))
            {
                return tombstone != NULL ? tombstone : entry;
            }
            else
            {
                if(tombstone == NULL) tombstone = entry;
            }
        }
        else if(entry_key_len == key_len && memcmp(key, entry_get_key(entry), key_len) == 0)
        {
            return entry;
        }

        index = (index + 1) % hashmap->capacity;

        num_probes++;

        if(num_probes == max_probes)
        {
            string_hashmap_grow(hashmap, string_hashmap_get_new_capacity(hashmap));
            goto start;
        }
    }

    return NULL;
}

StringHashMap* string_hashmap_new(void)
{
    StringHashMap* hm = (StringHashMap*)malloc(sizeof(StringHashMap));
    hm->entries = NULL;
    hm->size = 0;
    hm->capacity = 0;

    string_hashmap_grow(hm, HASHMAP_INITIAL_CAPACITY);

    return hm;
}

size_t string_hashmap_size(StringHashMap* hashmap)
{
    return hashmap->size;
}

size_t string_hashmap_capacity(StringHashMap* hashmap)
{
    return hashmap->capacity;
}

void string_hashmap_insert(StringHashMap* hashmap, 
                           const char* key, 
                           size_t key_len,
                           void* value, 
                           size_t value_size)
{
    size_t new_capacity;
    entry_t* entry;

    assert(hashmap != NULL);
    assert(key != NULL);
    assert(value != NULL);

    if((hashmap->size + 1) > (hashmap->capacity * HASHMAP_MAX_LOAD))
    {
        string_hashmap_grow(hashmap, string_hashmap_get_new_capacity(hashmap));
    }

    entry = entry_find_insert(hashmap, key, key_len);

    if(entry_is_empty(entry))
    {
        hashmap->size++;
    }
    else
    {
        entry_free(entry);
    }

    entry_new(entry, key, key_len, value, value_size);
}

void string_hashmap_update(StringHashMap* hashmap,
                           const char* key,
                           size_t key_len,
                           void* value,
                           size_t value_size)
{
    entry_t* entry;

    assert(hashmap != NULL);
    assert(key != NULL);
    assert(value != NULL);

    entry = entry_find_insert(hashmap, key, key_len);

    if(entry_is_empty(entry))
    {
        hashmap->size++;
    }
    else
    {
        entry_free(entry);
    }

    entry_new(entry, key, key_len, value, value_size);
}

void* string_hashmap_get(StringHashMap* hashmap,
                         const char* key,
                         size_t key_len,
                         size_t* value_size)
{
    entry_t* entry;

    assert(hashmap != NULL);
    assert(key != NULL);
    assert(value_size != NULL);

    if(hashmap->size == 0)
    {
        return NULL;
    }

    entry = entry_find_get(hashmap->entries, hashmap->capacity, key, key_len);

    if(entry_is_empty(entry))
    {
        return NULL;
    }

    *value_size = entry_get_value_size(entry);

    return entry_get_value(entry);
}

void string_hashmap_remove(StringHashMap* hashmap,
                           const char* key,
                           size_t key_len)
{
    entry_t* entry;

    assert(hashmap != NULL);
    assert(key != NULL);

    if(hashmap->size == 0)
    {
        return;
    }

    entry = entry_find_get(hashmap->entries, hashmap->capacity, key, key_len);

    if(entry_is_empty(entry))
    {
        return;
    }

    entry_free(entry);

    entry_set_tombstone(entry);

    hashmap->size--;
}

void string_hashmap_free(StringHashMap* hashmap)
{
    size_t i;

    assert(hashmap != NULL);

    for(i = 0; i < hashmap->size; i++)
    {
        if(!entry_is_empty(&hashmap->entries[i]))
        {
            entry_free(&hashmap->entries[i]);
        }
    }

    if(hashmap->entries != NULL)
    {
        free(hashmap->entries);
    }

    free(hashmap);
}