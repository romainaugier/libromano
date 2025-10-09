# libromano

![Windows Build](https://github.com/romainaugier/libromano/actions/workflows/build-windows.yml/badge.svg)
![Linux Build](https://github.com/romainaugier/libromano/actions/workflows/build-linux.yml/badge.svg)

C99 utility library for projects and learning.

The library follows consistent naming conventions to make the API easy to read and use.

## Structs and functions

All structs have their members exposed in all header files, but for member access prefer methods when available
instead of direct member access. For some structs we use some shenanigans to optimize storage (Str for example).
Members prefixed with _ should not be accessed and are used internally, and so for functions.

## Naming

All members/variables suffixed with _sz refers to the size/length.

All structs use capitalized names so they can be easily identified when reading code:

```c
Str str;
HashMap hashmap;
Arena arena;
Json json;
```

## Object Lifecycle

Each struct type X supports two creation and destruction methods:
 - Stack-based — using x_init / x_release
 - Heap-based — using x_new / x_free

This dual approach provides flexibility while keeping the API uniform and predictable.

### Stack-based Initialization

Use these functions when working with local (stack-allocated) instances of a struct.

```c
/*
 * Initialize an existing X struct.
 * No memory allocation occurs, except possibly for internal members of X.
 */
void x_init(X*);

/*
 * Release the resources held by a stack-allocated X struct.
 * Must only be called on structs initialized with x_init.
 */
void x_release(X*);

/* Example */

X x;
x_init(&x);
/* ... */
x_release(&x);
```

### Heap-based Allocation

Use these functions when working with dynamically allocated (heap) instances of a struct.

```c
/*
 * Allocate and initialize a new X struct on the heap.
 * Equivalent to:
 *   X* x_new() {
 *       X* x = malloc(sizeof(X));
 *       x_init(x);
 *       return x;
 *   }
 */
X* x_new();

/*
 * Release and free a heap-allocated X struct.
 * Equivalent to:
 *   void x_free(X* x) {
 *       x_release(x);
 *       free(x);
 *   }
 */
void x_free(X*);

/* Example */

X* x = x_new();
/* ... */
x_free(x);
```