/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#pragma once

#if !defined(__LIBROMANO_REGEX)
#define __LIBROMANO_REGEX

#include "libromano/common.h"
#include "libromano/bit.h"

/*
 * POSIX Extended Regular Expressions compiled to a DFA
 *
 * Pipeline: pattern -> AST -> Thompson NFA -> DFA (subset construction)
 *
 * Two DFAs are built for each pattern:
 *  - a forward anchored DFA, used for full matches and to find the end of a match
 *  - a reverse unanchored DFA, used to find where matches start
 *
 * Matching is O(n) in the size of the input, never backtracks and never touches
 * the heap (regex_iterate allocates once, only for inputs bigger than
 * REGEX_ITERATE_STACK_BITSET_WORDS * 64 bytes).
 * Compilation runs entirely on the stack (see REGEX_COMPILE_USE_HEAP), with a
 * single heap allocation holding the final DFA tables.
 *
 * Supported syntax:
 *  - Literals and escaped metacharacters:          a \. \* \\ \( ...
 *  - Any character:                                .
 *  - Bracket expressions:                          [abc] [^abc] [a-z] []a] [a-] [--/]
 *  - Character classes:                            [[:alnum:]] [[:alpha:]] [[:blank:]] [[:cntrl:]]
 *                                                  [[:digit:]] [[:graph:]] [[:lower:]] [[:print:]]
 *                                                  [[:punct:]] [[:space:]] [[:upper:]] [[:xdigit:]]
 *  - Equivalence classes / collating symbols:      [[=a=]] [[.a.]] (single characters only)
 *  - Grouping:                                     (...)
 *  - Alternation:                                  |
 *  - Quantifiers:                                  * + ? {n} {n,} {n,m} (n, m <= REGEX_DUP_MAX)
 *  - Anchors:                                      ^ $
 *  - Extensions (outside of bracket expressions):  \d \D \w \W \s \S \t \n \r \f \v
 *
 * As in POSIX, a backslash inside a bracket expression is a literal character.
 * Matching follows the POSIX leftmost-longest rule, in the "C" locale, over bytes.
 *
 * Not supported, as they cannot be expressed with a pure DFA:
 * capture groups (parentheses only group), backreferences, lazy quantifiers,
 * lookarounds and word boundaries.
 */

/*
 * Compile-time limits. Override them when building libromano (e.g. -DREGEX_MAX_DFA_STATES=4096).
 * They size the scratch memory used during compilation, which lives on the stack,
 * unless REGEX_COMPILE_USE_HEAP is defined. With the defaults, compilation uses
 * about 210kb of stack.
 */

/* Maximum number of nodes in the parsed expression tree */
#if !defined(REGEX_MAX_AST_NODES)
#define REGEX_MAX_AST_NODES 1024
#endif /* !defined(REGEX_MAX_AST_NODES) */

/* Maximum number of distinct character sets in a pattern */
#if !defined(REGEX_MAX_CHARSETS)
#define REGEX_MAX_CHARSETS 128
#endif /* !defined(REGEX_MAX_CHARSETS) */

/* Maximum nesting of parentheses */
#if !defined(REGEX_MAX_GROUP_DEPTH)
#define REGEX_MAX_GROUP_DEPTH 64
#endif /* !defined(REGEX_MAX_GROUP_DEPTH) */

/* Maximum value of a bound in {n,m} (RE_DUP_MAX in POSIX) */
#if !defined(REGEX_DUP_MAX)
#define REGEX_DUP_MAX 255
#endif /* !defined(REGEX_DUP_MAX) */

/* Maximum number of NFA nodes (counted repetitions duplicate their operand) */
#if !defined(REGEX_MAX_NFA_NODES)
#define REGEX_MAX_NFA_NODES 2048
#endif /* !defined(REGEX_MAX_NFA_NODES) */

/* Maximum number of states for each DFA */
#if !defined(REGEX_MAX_DFA_STATES)
#define REGEX_MAX_DFA_STATES 1024
#endif /* !defined(REGEX_MAX_DFA_STATES) */

/* Total number of NFA node ids stored across all the states of a DFA */
#if !defined(REGEX_MAX_DFA_SET_POOL)
#define REGEX_MAX_DFA_SET_POOL 32768
#endif /* !defined(REGEX_MAX_DFA_SET_POOL) */

/* Total number of table entries (states * (byte classes + 1)) for both DFAs */
#if !defined(REGEX_MAX_DFA_TRANSITIONS)
#define REGEX_MAX_DFA_TRANSITIONS 32768
#endif /* !defined(REGEX_MAX_DFA_TRANSITIONS) */

/* Size of the stack bitset used by regex_iterate, covers inputs up to 64 * words - 1 bytes */
#if !defined(REGEX_ITERATE_STACK_BITSET_WORDS)
#define REGEX_ITERATE_STACK_BITSET_WORDS 512
#endif /* !defined(REGEX_ITERATE_STACK_BITSET_WORDS) */

/* Define REGEX_COMPILE_USE_HEAP to allocate the compilation scratch memory on the heap */

ROMANO_CPP_ENTER

typedef enum RegexFlags {
    RegexFlags_None = 0,
    /* Logs the expression tree, byte classes, NFAs and DFAs */
    RegexFlags_DebugCompilation = BIT(0),
    /* Case insensitive matching (REG_ICASE) */
    RegexFlags_IgnoreCase = BIT(1),
    /* '.' and non-matching brackets do not match '\n', ^ and $ match around '\n' (REG_NEWLINE) */
    RegexFlags_Newline = BIT(2),
} RegexFlags;

struct Regex;

typedef struct Regex Regex;

typedef struct RegexMatch {
    /* Pointer to the first character of the match in the searched string */
    const char* data;
    /* Size of the match, can be 0 for patterns matching the empty string */
    size_t data_sz;
    /* Offset of the match in the searched string */
    size_t position;
} RegexMatch;

/*
 * Called for each match found by regex_iterate, in order of appearance
 * Return false to stop the iteration
 */
typedef bool (*RegexIterateFunc)(const RegexMatch* match, void* user_data);

/*
 * Compiles a null-terminated pattern
 * Returns NULL on failure, the error can be retrieved with error_get_last()
 */
ROMANO_API Regex* regex_compile(const char* pattern,
                                RegexFlags flags);

/*
 * Returns true if the whole string matches the pattern
 */
ROMANO_API bool regex_match(const Regex* regex,
                            const char* string,
                            size_t string_sz);

/*
 * Finds the leftmost-longest match of the pattern in the string
 * Returns true and fills match if one was found
 */
ROMANO_API bool regex_search(const Regex* regex,
                             const char* string,
                             size_t string_sz,
                             RegexMatch* match);

/*
 * Calls func for each non-overlapping leftmost-longest match in the string
 * Empty matches are reported too, the search then resumes at the next character
 * Returns the number of matches passed to func
 */
ROMANO_API size_t regex_iterate(const Regex* regex,
                                const char* string,
                                size_t string_sz,
                                RegexIterateFunc func,
                                void* user_data);

/*
 * Frees a compiled regex
 */
ROMANO_API void regex_free(Regex* regex);

ROMANO_CPP_END

#endif /* !defined(__LIBROMANO_REGEX) */