/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

/* https://swtch.com/~rsc/regexp/regexp1.html */
/* https://swtch.com/~rsc/regexp/regexp3.html */
/* https://dl.acm.org/doi/pdf/10.1145/363347.363387 */
/* https://pubs.opengroup.org/onlinepubs/9799919799/basedefs/V1_chap09.html */

#include "libromano/regex.h"
#include "libromano/common.h"
#include "libromano/logger.h"
#include "libromano/error.h"
#include "libromano/bit.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern ErrorCode g_current_error;

#define REGEX_INVALID_ID 0xFFFF
#define REGEX_REPEAT_INFINITE 0xFFFF

/* Charset 0 is always the full byte set, used by '.' and the unanchored prefix loop */
#define REGEX_CHARSET_ANY 0

/* Premultiplied index of the dead state, its row is all zeros */
#define REGEX_DFA_DEAD_STATE 0

#define REGEX_DFA_HASH_TABLE_SIZE (REGEX_MAX_DFA_STATES * 2)

ROMANO_STATIC_ASSERT(REGEX_MAX_AST_NODES < REGEX_INVALID_ID, regex_max_ast_nodes_too_big);
ROMANO_STATIC_ASSERT(REGEX_MAX_CHARSETS < REGEX_INVALID_ID, regex_max_charsets_too_big);
ROMANO_STATIC_ASSERT(REGEX_MAX_NFA_NODES < REGEX_INVALID_ID, regex_max_nfa_nodes_too_big);
ROMANO_STATIC_ASSERT(REGEX_MAX_DFA_STATES < REGEX_INVALID_ID, regex_max_dfa_states_too_big);
ROMANO_STATIC_ASSERT(REGEX_DUP_MAX < REGEX_REPEAT_INFINITE, regex_dup_max_too_big);

/************/
/* Charsets */
/************/

typedef struct RegexCharset {
    uint64_t bits[4];
} RegexCharset;

ROMANO_FORCE_INLINE void regex_charset_add(RegexCharset* charset, uint8_t c)
{
    charset->bits[c >> 6] |= BIT64(c & 63);
}

ROMANO_FORCE_INLINE bool regex_charset_has(const RegexCharset* charset, uint8_t c)
{
    return (charset->bits[c >> 6] >> (c & 63)) & 1;
}

void regex_charset_add_range(RegexCharset* charset, uint32_t range_start, uint32_t range_end)
{
    uint32_t c;

    for(c = range_start; c <= range_end; c++)
        regex_charset_add(charset, (uint8_t)c);
}

void regex_charset_negate(RegexCharset* charset)
{
    uint32_t i;

    for(i = 0; i < 4; i++)
        charset->bits[i] = ~charset->bits[i];
}

void regex_charset_fold_case(RegexCharset* charset)
{
    uint32_t c;

    for(c = 'a'; c <= 'z'; c++)
    {
        if(regex_charset_has(charset, (uint8_t)c) || regex_charset_has(charset, (uint8_t)(c - 32)))
        {
            regex_charset_add(charset, (uint8_t)c);
            regex_charset_add(charset, (uint8_t)(c - 32));
        }
    }
}

/* POSIX classes in the "C" locale, independent of the current locale */
bool regex_charset_add_class(RegexCharset* charset, const char* name, size_t name_sz)
{
    if(name_sz == 5 && memcmp(name, "alnum", 5) == 0)
    {
        regex_charset_add_range(charset, '0', '9');
        regex_charset_add_range(charset, 'A', 'Z');
        regex_charset_add_range(charset, 'a', 'z');
    }
    else if(name_sz == 5 && memcmp(name, "alpha", 5) == 0)
    {
        regex_charset_add_range(charset, 'A', 'Z');
        regex_charset_add_range(charset, 'a', 'z');
    }
    else if(name_sz == 5 && memcmp(name, "blank", 5) == 0)
    {
        regex_charset_add(charset, ' ');
        regex_charset_add(charset, '\t');
    }
    else if(name_sz == 5 && memcmp(name, "cntrl", 5) == 0)
    {
        regex_charset_add_range(charset, 0, 31);
        regex_charset_add(charset, 127);
    }
    else if(name_sz == 5 && memcmp(name, "digit", 5) == 0)
    {
        regex_charset_add_range(charset, '0', '9');
    }
    else if(name_sz == 5 && memcmp(name, "graph", 5) == 0)
    {
        regex_charset_add_range(charset, 33, 126);
    }
    else if(name_sz == 5 && memcmp(name, "lower", 5) == 0)
    {
        regex_charset_add_range(charset, 'a', 'z');
    }
    else if(name_sz == 5 && memcmp(name, "print", 5) == 0)
    {
        regex_charset_add_range(charset, 32, 126);
    }
    else if(name_sz == 5 && memcmp(name, "punct", 5) == 0)
    {
        regex_charset_add_range(charset, 33, 47);
        regex_charset_add_range(charset, 58, 64);
        regex_charset_add_range(charset, 91, 96);
        regex_charset_add_range(charset, 123, 126);
    }
    else if(name_sz == 5 && memcmp(name, "space", 5) == 0)
    {
        regex_charset_add(charset, ' ');
        regex_charset_add_range(charset, '\t', '\r');
    }
    else if(name_sz == 5 && memcmp(name, "upper", 5) == 0)
    {
        regex_charset_add_range(charset, 'A', 'Z');
    }
    else if(name_sz == 6 && memcmp(name, "xdigit", 6) == 0)
    {
        regex_charset_add_range(charset, '0', '9');
        regex_charset_add_range(charset, 'A', 'F');
        regex_charset_add_range(charset, 'a', 'f');
    }
    else
    {
        return false;
    }

    return true;
}

/* Writes something like "a-z0-9\x0a" in buffer, buffer must hold at least 1300 bytes */
void regex_charset_to_string(const RegexCharset* charset, char* buffer)
{
    uint32_t c;
    uint32_t range_end;
    char* ptr;

    ptr = buffer;
    c = 0;

    while(c < 256)
    {
        if(!regex_charset_has(charset, (uint8_t)c))
        {
            c++;
            continue;
        }

        range_end = c;

        while(range_end < 255 && regex_charset_has(charset, (uint8_t)(range_end + 1)))
            range_end++;

        if(c > 32 && c < 127)
            *ptr++ = (char)c;
        else
            ptr += sprintf(ptr, "\\x%02x", c);

        if(range_end > c)
        {
            if(range_end > c + 1)
                *ptr++ = '-';

            if(range_end > 32 && range_end < 127)
                *ptr++ = (char)range_end;
            else
                ptr += sprintf(ptr, "\\x%02x", range_end);
        }

        c = range_end + 1;
    }

    *ptr = '\0';
}

/**********/
/* Parser */
/**********/

typedef enum RegexNodeType {
    RegexNodeType_Empty,
    RegexNodeType_Charset,
    RegexNodeType_LineStart,
    RegexNodeType_LineEnd,
    RegexNodeType_Concatenate,
    RegexNodeType_Alternate,
    RegexNodeType_Repeat,
} RegexNodeType;

typedef struct RegexNode {
    uint16_t type;
    uint16_t charset;
    uint16_t lhs;
    uint16_t rhs;
    uint16_t min;
    uint16_t max;
} RegexNode;

typedef struct RegexParser {
    const char* pattern;
    size_t pattern_sz;
    size_t pos;
    uint32_t flags;
    uint32_t depth;
    uint32_t num_nodes;
    uint32_t num_charsets;
    RegexNode nodes[REGEX_MAX_AST_NODES];
    RegexCharset charsets[REGEX_MAX_CHARSETS];
} RegexParser;

void regex_parser_error(RegexParser* parser, ErrorCode error, const char* message)
{
    g_current_error = error;
    logger_log_error("Regex error at position %zu: %s (pattern: \"%.*s\")",
                     parser->pos,
                     message,
                     (int)parser->pattern_sz,
                     parser->pattern);
}

ROMANO_FORCE_INLINE bool regex_parser_is_at_end(RegexParser* parser)
{
    return parser->pos >= parser->pattern_sz;
}

ROMANO_FORCE_INLINE char regex_parser_peek(RegexParser* parser)
{
    return parser->pos < parser->pattern_sz ? parser->pattern[parser->pos] : '\0';
}

uint16_t regex_parser_add_node(RegexParser* parser, RegexNodeType type)
{
    RegexNode* node;

    if(parser->num_nodes >= REGEX_MAX_AST_NODES)
    {
        regex_parser_error(parser, ErrorCode_SizeOverflow, "too many nodes, increase REGEX_MAX_AST_NODES");
        return REGEX_INVALID_ID;
    }

    node = &parser->nodes[parser->num_nodes];
    node->type = (uint16_t)type;
    node->charset = 0;
    node->lhs = REGEX_INVALID_ID;
    node->rhs = REGEX_INVALID_ID;
    node->min = 0;
    node->max = 0;

    return (uint16_t)parser->num_nodes++;
}

uint16_t regex_parser_add_binary_node(RegexParser* parser, RegexNodeType type, uint16_t lhs, uint16_t rhs)
{
    uint16_t node_id;

    node_id = regex_parser_add_node(parser, type);

    if(node_id == REGEX_INVALID_ID)
        return REGEX_INVALID_ID;

    parser->nodes[node_id].lhs = lhs;
    parser->nodes[node_id].rhs = rhs;

    return node_id;
}

/* Deduplicates charsets so the byte classes and the NFA stay small */
uint16_t regex_parser_add_charset_node(RegexParser* parser, RegexCharset* charset)
{
    uint32_t i;
    uint16_t node_id;

    if(parser->flags & RegexFlags_IgnoreCase)
        regex_charset_fold_case(charset);

    for(i = 0; i < parser->num_charsets; i++)
    {
        if(memcmp(&parser->charsets[i], charset, sizeof(RegexCharset)) == 0)
            break;
    }

    if(i == parser->num_charsets)
    {
        if(parser->num_charsets >= REGEX_MAX_CHARSETS)
        {
            regex_parser_error(parser, ErrorCode_SizeOverflow, "too many charsets, increase REGEX_MAX_CHARSETS");
            return REGEX_INVALID_ID;
        }

        memcpy(&parser->charsets[parser->num_charsets++], charset, sizeof(RegexCharset));
    }

    node_id = regex_parser_add_node(parser, RegexNodeType_Charset);

    if(node_id == REGEX_INVALID_ID)
        return REGEX_INVALID_ID;

    parser->nodes[node_id].charset = (uint16_t)i;

    return node_id;
}

/* Parses a single character of a bracket expression, including [.c.] and [=c=] */
bool regex_parse_bracket_char(RegexParser* parser, uint8_t* c)
{
    char delimiter;

    if(parser->pattern[parser->pos] == '[' &&
       parser->pos + 1 < parser->pattern_sz &&
       (parser->pattern[parser->pos + 1] == '.' || parser->pattern[parser->pos + 1] == '='))
    {
        delimiter = parser->pattern[parser->pos + 1];

        if(parser->pos + 4 >= parser->pattern_sz ||
           parser->pattern[parser->pos + 3] != delimiter ||
           parser->pattern[parser->pos + 4] != ']')
        {
            regex_parser_error(parser,
                               ErrorCode_RegexInvalidCharacterRange,
                               "only single character collating symbols and equivalence classes are supported");
            return false;
        }

        *c = (uint8_t)parser->pattern[parser->pos + 2];
        parser->pos += 5;

        return true;
    }

    *c = (uint8_t)parser->pattern[parser->pos++];

    return true;
}

uint16_t regex_parse_bracket(RegexParser* parser)
{
    RegexCharset charset;
    const char* class_name;
    size_t class_end;
    bool negate;
    bool first;
    uint8_t range_start;
    uint8_t range_end;

    memset(&charset, 0, sizeof(RegexCharset));

    /* Skip '[' */
    parser->pos++;

    negate = regex_parser_peek(parser) == '^';

    if(negate)
        parser->pos++;

    first = true;

    while(true)
    {
        if(regex_parser_is_at_end(parser))
        {
            regex_parser_error(parser, ErrorCode_RegexInvalidCharacterRange, "unclosed bracket expression");
            return REGEX_INVALID_ID;
        }

        if(parser->pattern[parser->pos] == ']' && !first)
        {
            parser->pos++;
            break;
        }

        first = false;

        /* [:class:] */
        if(parser->pattern[parser->pos] == '[' &&
           parser->pos + 1 < parser->pattern_sz &&
           parser->pattern[parser->pos + 1] == ':')
        {
            class_name = parser->pattern + parser->pos + 2;
            class_end = parser->pos + 2;

            while(class_end + 1 < parser->pattern_sz &&
                  !(parser->pattern[class_end] == ':' && parser->pattern[class_end + 1] == ']'))
                class_end++;

            if(class_end + 1 >= parser->pattern_sz)
            {
                regex_parser_error(parser, ErrorCode_RegexInvalidCharacterRange, "unclosed character class");
                return REGEX_INVALID_ID;
            }

            if(!regex_charset_add_class(&charset,
                                        class_name,
                                        (size_t)((parser->pattern + class_end) - class_name)))
            {
                regex_parser_error(parser, ErrorCode_RegexInvalidCharacterRange, "unknown character class");
                return REGEX_INVALID_ID;
            }

            parser->pos = class_end + 2;

            continue;
        }

        if(!regex_parse_bracket_char(parser, &range_start))
            return REGEX_INVALID_ID;

        /* A '-' right before the closing ']' is a literal */
        if(parser->pos + 1 < parser->pattern_sz &&
           parser->pattern[parser->pos] == '-' &&
           parser->pattern[parser->pos + 1] != ']')
        {
            parser->pos++;

            if(parser->pattern[parser->pos] == '[' &&
               parser->pos + 1 < parser->pattern_sz &&
               parser->pattern[parser->pos + 1] == ':')
            {
                regex_parser_error(parser, ErrorCode_RegexInvalidCharacterRange, "character class used as a range end point");
                return REGEX_INVALID_ID;
            }

            if(!regex_parse_bracket_char(parser, &range_end))
                return REGEX_INVALID_ID;

            if(range_end < range_start)
            {
                regex_parser_error(parser, ErrorCode_RegexInvalidCharacterRange, "invalid range end point order");
                return REGEX_INVALID_ID;
            }

            regex_charset_add_range(&charset, range_start, range_end);
        }
        else
        {
            regex_charset_add(&charset, range_start);
        }
    }

    if(negate)
    {
        /* Folding before negating so [^a] excludes both 'a' and 'A' */
        if(parser->flags & RegexFlags_IgnoreCase)
            regex_charset_fold_case(&charset);

        regex_charset_negate(&charset);

        if(parser->flags & RegexFlags_Newline)
            charset.bits['\n' >> 6] &= ~BIT64('\n' & 63);
    }

    return regex_parser_add_charset_node(parser, &charset);
}

uint16_t regex_parse_escape(RegexParser* parser)
{
    RegexCharset charset;
    char c;
    bool negate;

    /* Skip '\' */
    parser->pos++;

    if(regex_parser_is_at_end(parser))
    {
        regex_parser_error(parser, ErrorCode_RegexUnexpectedEndOfExpression, "trailing backslash");
        return REGEX_INVALID_ID;
    }

    memset(&charset, 0, sizeof(RegexCharset));

    c = parser->pattern[parser->pos];
    negate = false;

    switch(c)
    {
        case 'D':
            negate = true;
            /* fallthrough */
        case 'd':
            regex_charset_add_range(&charset, '0', '9');
            break;
        case 'W':
            negate = true;
            /* fallthrough */
        case 'w':
            regex_charset_add_class(&charset, "alnum", 5);
            regex_charset_add(&charset, '_');
            break;
        case 'S':
            negate = true;
            /* fallthrough */
        case 's':
            regex_charset_add_class(&charset, "space", 5);
            break;
        case 'n':
            regex_charset_add(&charset, '\n');
            break;
        case 't':
            regex_charset_add(&charset, '\t');
            break;
        case 'r':
            regex_charset_add(&charset, '\r');
            break;
        case 'f':
            regex_charset_add(&charset, '\f');
            break;
        case 'v':
            regex_charset_add(&charset, '\v');
            break;
        default:
        {
            if((c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'))
            {
                regex_parser_error(parser, ErrorCode_RegexUnexpectedCharacter, "unsupported escape sequence");
                return REGEX_INVALID_ID;
            }

            regex_charset_add(&charset, (uint8_t)c);
            break;
        }
    }

    parser->pos++;

    if(negate)
        regex_charset_negate(&charset);

    return regex_parser_add_charset_node(parser, &charset);
}

bool regex_parse_bound_number(RegexParser* parser, uint32_t* value)
{
    size_t start;

    start = parser->pos;
    *value = 0;

    while(!regex_parser_is_at_end(parser) &&
          parser->pattern[parser->pos] >= '0' &&
          parser->pattern[parser->pos] <= '9')
    {
        *value = *value * 10 + (uint32_t)(parser->pattern[parser->pos] - '0');

        if(*value > REGEX_DUP_MAX)
        {
            regex_parser_error(parser, ErrorCode_RegexInvalidOperator, "repetition count bigger than REGEX_DUP_MAX");
            return false;
        }

        parser->pos++;
    }

    return parser->pos > start;
}

/* Parses {n}, {n,} and {n,m} */
bool regex_parse_bound(RegexParser* parser, uint16_t* min, uint16_t* max)
{
    uint32_t value;

    /* Skip '{' */
    parser->pos++;

    if(!regex_parse_bound_number(parser, &value))
    {
        if(g_current_error != ErrorCode_RegexInvalidOperator)
            regex_parser_error(parser, ErrorCode_RegexInvalidOperator, "expected a number in repetition bound");

        return false;
    }

    *min = (uint16_t)value;
    *max = (uint16_t)value;

    if(regex_parser_peek(parser) == ',')
    {
        parser->pos++;

        if(regex_parser_peek(parser) == '}')
        {
            *max = REGEX_REPEAT_INFINITE;
        }
        else
        {
            if(!regex_parse_bound_number(parser, &value))
            {
                if(g_current_error != ErrorCode_RegexInvalidOperator)
                    regex_parser_error(parser, ErrorCode_RegexInvalidOperator, "expected a number in repetition bound");

                return false;
            }

            if(value < *min)
            {
                regex_parser_error(parser, ErrorCode_RegexInvalidOperator, "invalid repetition bound order");
                return false;
            }

            *max = (uint16_t)value;
        }
    }

    if(regex_parser_peek(parser) != '}')
    {
        regex_parser_error(parser, ErrorCode_RegexInvalidOperator, "unclosed repetition bound");
        return false;
    }

    parser->pos++;

    return true;
}

uint16_t regex_parse_alternation(RegexParser* parser);

uint16_t regex_parse_atom(RegexParser* parser)
{
    RegexCharset charset;
    uint16_t node_id;
    char c;

    c = parser->pattern[parser->pos];

    switch(c)
    {
        case '(':
        {
            if(parser->depth >= REGEX_MAX_GROUP_DEPTH)
            {
                regex_parser_error(parser, ErrorCode_SizeOverflow, "groups nested too deep, increase REGEX_MAX_GROUP_DEPTH");
                return REGEX_INVALID_ID;
            }

            parser->pos++;
            parser->depth++;

            node_id = regex_parse_alternation(parser);

            if(node_id == REGEX_INVALID_ID)
                return REGEX_INVALID_ID;

            if(regex_parser_peek(parser) != ')')
            {
                regex_parser_error(parser, ErrorCode_RegexMismatchedParentheses, "missing closing parenthesis");
                return REGEX_INVALID_ID;
            }

            parser->pos++;
            parser->depth--;

            return node_id;
        }
        case '.':
        {
            memset(&charset, 0xFF, sizeof(RegexCharset));

            if(parser->flags & RegexFlags_Newline)
                charset.bits['\n' >> 6] &= ~BIT64('\n' & 63);

            parser->pos++;

            return regex_parser_add_charset_node(parser, &charset);
        }
        case '[':
            return regex_parse_bracket(parser);
        case '\\':
            return regex_parse_escape(parser);
        case '^':
            parser->pos++;
            return regex_parser_add_node(parser, RegexNodeType_LineStart);
        case '$':
            parser->pos++;
            return regex_parser_add_node(parser, RegexNodeType_LineEnd);
        case '*':
        case '+':
        case '?':
        case '{':
            regex_parser_error(parser, ErrorCode_RegexInvalidOperator, "repetition operator without operand");
            return REGEX_INVALID_ID;
        default:
        {
            memset(&charset, 0, sizeof(RegexCharset));
            regex_charset_add(&charset, (uint8_t)c);

            parser->pos++;

            return regex_parser_add_charset_node(parser, &charset);
        }
    }
}

uint16_t regex_parse_repetition(RegexParser* parser)
{
    uint16_t atom_id;
    uint16_t node_id;
    uint16_t min;
    uint16_t max;

    atom_id = regex_parse_atom(parser);

    if(atom_id == REGEX_INVALID_ID)
        return REGEX_INVALID_ID;

    while(!regex_parser_is_at_end(parser))
    {
        switch(parser->pattern[parser->pos])
        {
            case '*':
                min = 0;
                max = REGEX_REPEAT_INFINITE;
                parser->pos++;
                break;
            case '+':
                min = 1;
                max = REGEX_REPEAT_INFINITE;
                parser->pos++;
                break;
            case '?':
                min = 0;
                max = 1;
                parser->pos++;
                break;
            case '{':
                if(!regex_parse_bound(parser, &min, &max))
                    return REGEX_INVALID_ID;
                break;
            default:
                return atom_id;
        }

        node_id = regex_parser_add_node(parser, RegexNodeType_Repeat);

        if(node_id == REGEX_INVALID_ID)
            return REGEX_INVALID_ID;

        parser->nodes[node_id].lhs = atom_id;
        parser->nodes[node_id].min = min;
        parser->nodes[node_id].max = max;

        atom_id = node_id;
    }

    return atom_id;
}

uint16_t regex_parse_concatenation(RegexParser* parser)
{
    uint16_t lhs;
    uint16_t rhs;
    char c;

    lhs = REGEX_INVALID_ID;

    while(!regex_parser_is_at_end(parser))
    {
        c = parser->pattern[parser->pos];

        if(c == '|' || c == ')')
            break;

        rhs = regex_parse_repetition(parser);

        if(rhs == REGEX_INVALID_ID)
            return REGEX_INVALID_ID;

        if(lhs == REGEX_INVALID_ID)
            lhs = rhs;
        else if((lhs = regex_parser_add_binary_node(parser, RegexNodeType_Concatenate, lhs, rhs)) == REGEX_INVALID_ID)
            return REGEX_INVALID_ID;
    }

    if(lhs == REGEX_INVALID_ID)
        lhs = regex_parser_add_node(parser, RegexNodeType_Empty);

    return lhs;
}

uint16_t regex_parse_alternation(RegexParser* parser)
{
    uint16_t lhs;
    uint16_t rhs;

    lhs = regex_parse_concatenation(parser);

    if(lhs == REGEX_INVALID_ID)
        return REGEX_INVALID_ID;

    while(regex_parser_peek(parser) == '|' && !regex_parser_is_at_end(parser))
    {
        parser->pos++;

        rhs = regex_parse_concatenation(parser);

        if(rhs == REGEX_INVALID_ID)
            return REGEX_INVALID_ID;

        lhs = regex_parser_add_binary_node(parser, RegexNodeType_Alternate, lhs, rhs);

        if(lhs == REGEX_INVALID_ID)
            return REGEX_INVALID_ID;
    }

    return lhs;
}

uint16_t regex_parse(RegexParser* parser, const char* pattern, RegexFlags flags)
{
    uint16_t root;

    parser->pattern = pattern;
    parser->pattern_sz = strlen(pattern);
    parser->pos = 0;
    parser->flags = (uint32_t)flags;
    parser->depth = 0;
    parser->num_nodes = 0;

    /* REGEX_CHARSET_ANY */
    memset(&parser->charsets[REGEX_CHARSET_ANY], 0xFF, sizeof(RegexCharset));
    parser->num_charsets = 1;

    root = regex_parse_alternation(parser);

    if(root != REGEX_INVALID_ID && !regex_parser_is_at_end(parser))
    {
        regex_parser_error(parser, ErrorCode_RegexMismatchedParentheses, "unmatched closing parenthesis");
        return REGEX_INVALID_ID;
    }

    return root;
}

void regex_ast_debug(const RegexParser* parser, uint16_t node_id, uint32_t indent)
{
    const RegexNode* node;
    char charset_str[1300];

    node = &parser->nodes[node_id];

    switch(node->type)
    {
        case RegexNodeType_Empty:
            logger_log_debug("%*sEMPTY", (int)indent * 2, "");
            break;
        case RegexNodeType_Charset:
            regex_charset_to_string(&parser->charsets[node->charset], charset_str);
            logger_log_debug("%*sCHARSET(%u) [%s]", (int)indent * 2, "", (uint32_t)node->charset, charset_str);
            break;
        case RegexNodeType_LineStart:
            logger_log_debug("%*sLINE_START", (int)indent * 2, "");
            break;
        case RegexNodeType_LineEnd:
            logger_log_debug("%*sLINE_END", (int)indent * 2, "");
            break;
        case RegexNodeType_Concatenate:
            logger_log_debug("%*sCONCATENATE", (int)indent * 2, "");
            regex_ast_debug(parser, node->lhs, indent + 1);
            regex_ast_debug(parser, node->rhs, indent + 1);
            break;
        case RegexNodeType_Alternate:
            logger_log_debug("%*sALTERNATE", (int)indent * 2, "");
            regex_ast_debug(parser, node->lhs, indent + 1);
            regex_ast_debug(parser, node->rhs, indent + 1);
            break;
        case RegexNodeType_Repeat:
            if(node->max == REGEX_REPEAT_INFINITE)
                logger_log_debug("%*sREPEAT{%u,}", (int)indent * 2, "", (uint32_t)node->min);
            else
                logger_log_debug("%*sREPEAT{%u,%u}", (int)indent * 2, "", (uint32_t)node->min, (uint32_t)node->max);

            regex_ast_debug(parser, node->lhs, indent + 1);
            break;
    }
}

/****************/
/* Byte classes */
/****************/

/*
 * Bytes that no charset of the pattern can tell apart share a class, so DFA
 * rows only need one column per class instead of 256
 */

typedef struct RegexByteClasses {
    uint8_t map[256];
    uint8_t representatives[256];
    uint32_t num_classes;
} RegexByteClasses;

void regex_byte_classes_compute(RegexByteClasses* classes, const RegexParser* parser)
{
    bool boundaries[256];
    uint32_t i;
    uint32_t c;

    memset(boundaries, 0, sizeof(boundaries));

    for(i = 0; i < parser->num_charsets; i++)
    {
        for(c = 1; c < 256; c++)
        {
            if(regex_charset_has(&parser->charsets[i], (uint8_t)c) !=
               regex_charset_has(&parser->charsets[i], (uint8_t)(c - 1)))
                boundaries[c] = true;
        }
    }

    /* '\n' always has its own class, the DFA looks at it for ^ and $ */
    boundaries['\n'] = true;
    boundaries['\n' + 1] = true;

    classes->map[0] = 0;
    classes->representatives[0] = 0;
    classes->num_classes = 1;

    for(c = 1; c < 256; c++)
    {
        if(boundaries[c])
            classes->representatives[classes->num_classes++] = (uint8_t)c;

        classes->map[c] = (uint8_t)(classes->num_classes - 1);
    }
}

void regex_byte_classes_debug(const RegexByteClasses* classes)
{
    RegexCharset charset;
    char charset_str[1300];
    uint32_t i;
    uint32_t range_end;

    logger_log_debug("Byte classes: %u", classes->num_classes);

    for(i = 0; i < classes->num_classes; i++)
    {
        range_end = i + 1 < classes->num_classes ? (uint32_t)classes->representatives[i + 1] - 1 : 255;

        memset(&charset, 0, sizeof(RegexCharset));
        regex_charset_add_range(&charset, classes->representatives[i], range_end);
        regex_charset_to_string(&charset, charset_str);

        logger_log_debug("  class %u: [%s]", i, charset_str);
    }
}

/*******/
/* NFA */
/*******/

typedef enum RegexNFANodeType {
    RegexNFANodeType_Charset,   /* consumes a byte of charset, goes to out */
    RegexNFANodeType_Split,     /* goes to out and out2 */
    RegexNFANodeType_Epsilon,   /* goes to out */
    RegexNFANodeType_LineStart, /* goes to out if at the start of a line */
    RegexNFANodeType_LineEnd,   /* goes to out if at the end of a line */
    RegexNFANodeType_Match,
} RegexNFANodeType;

typedef struct RegexNFANode {
    uint16_t type;
    uint16_t charset;
    uint16_t out;
    uint16_t out2;
} RegexNFANode;

typedef struct RegexNFA {
    uint32_t num_nodes;
    uint16_t start;
    RegexNFANode nodes[REGEX_MAX_NFA_NODES];
} RegexNFA;

/* Partially built NFA, end is a node whose out is not patched yet */
typedef struct RegexFragment {
    uint16_t start;
    uint16_t end;
} RegexFragment;

uint16_t regex_nfa_add_node(RegexNFA* nfa, RegexNFANodeType type, uint16_t charset, uint16_t out, uint16_t out2)
{
    RegexNFANode* node;

    if(nfa->num_nodes >= REGEX_MAX_NFA_NODES)
    {
        g_current_error = ErrorCode_SizeOverflow;
        logger_log_error("Regex error: too many NFA nodes, increase REGEX_MAX_NFA_NODES");
        return REGEX_INVALID_ID;
    }

    node = &nfa->nodes[nfa->num_nodes];
    node->type = (uint16_t)type;
    node->charset = charset;
    node->out = out;
    node->out2 = out2;

    return (uint16_t)nfa->num_nodes++;
}

ROMANO_FORCE_INLINE void regex_fragment_append(RegexNFA* nfa, RegexFragment* fragment, RegexFragment* other, bool* has_fragment)
{
    if(!*has_fragment)
    {
        *fragment = *other;
        *has_fragment = true;
    }
    else
    {
        nfa->nodes[fragment->end].out = other->start;
        fragment->end = other->end;
    }
}

bool regex_nfa_build_node(RegexNFA* nfa,
                          const RegexParser* parser,
                          uint16_t node_id,
                          bool reversed,
                          RegexFragment* fragment);

bool regex_nfa_build_repeat(RegexNFA* nfa,
                            const RegexParser* parser,
                            const RegexNode* node,
                            bool reversed,
                            RegexFragment* fragment)
{
    RegexFragment operand;
    RegexFragment loop;
    bool has_fragment;
    uint32_t i;

    has_fragment = false;

    /* x{n,m} -> xxx...x (n times) followed by x* or by (m - n) x? */
    for(i = 0; i < node->min; i++)
    {
        if(!regex_nfa_build_node(nfa, parser, node->lhs, reversed, &operand))
            return false;

        regex_fragment_append(nfa, fragment, &operand, &has_fragment);
    }

    if(node->max == REGEX_REPEAT_INFINITE)
    {
        if(!regex_nfa_build_node(nfa, parser, node->lhs, reversed, &operand))
            return false;

        loop.end = regex_nfa_add_node(nfa, RegexNFANodeType_Epsilon, 0, REGEX_INVALID_ID, REGEX_INVALID_ID);
        loop.start = regex_nfa_add_node(nfa, RegexNFANodeType_Split, 0, operand.start, loop.end);

        if(loop.end == REGEX_INVALID_ID || loop.start == REGEX_INVALID_ID)
            return false;

        nfa->nodes[operand.end].out = loop.start;

        regex_fragment_append(nfa, fragment, &loop, &has_fragment);
    }
    else
    {
        for(i = node->min; i < node->max; i++)
        {
            if(!regex_nfa_build_node(nfa, parser, node->lhs, reversed, &operand))
                return false;

            loop.end = regex_nfa_add_node(nfa, RegexNFANodeType_Epsilon, 0, REGEX_INVALID_ID, REGEX_INVALID_ID);
            loop.start = regex_nfa_add_node(nfa, RegexNFANodeType_Split, 0, operand.start, loop.end);

            if(loop.end == REGEX_INVALID_ID || loop.start == REGEX_INVALID_ID)
                return false;

            nfa->nodes[operand.end].out = loop.end;

            regex_fragment_append(nfa, fragment, &loop, &has_fragment);
        }
    }

    if(!has_fragment)
    {
        fragment->start = regex_nfa_add_node(nfa, RegexNFANodeType_Epsilon, 0, REGEX_INVALID_ID, REGEX_INVALID_ID);
        fragment->end = fragment->start;

        return fragment->start != REGEX_INVALID_ID;
    }

    return true;
}

bool regex_nfa_build_node(RegexNFA* nfa,
                          const RegexParser* parser,
                          uint16_t node_id,
                          bool reversed,
                          RegexFragment* fragment)
{
    const RegexNode* node;
    RegexFragment lhs;
    RegexFragment rhs;
    RegexNFANodeType type;

    node = &parser->nodes[node_id];

    switch(node->type)
    {
        case RegexNodeType_Empty:
        case RegexNodeType_Charset:
        case RegexNodeType_LineStart:
        case RegexNodeType_LineEnd:
        {
            if(node->type == RegexNodeType_Empty)
                type = RegexNFANodeType_Epsilon;
            else if(node->type == RegexNodeType_Charset)
                type = RegexNFANodeType_Charset;
            else if((node->type == RegexNodeType_LineStart) != reversed)
                type = RegexNFANodeType_LineStart;
            else
                type = RegexNFANodeType_LineEnd;

            fragment->start = regex_nfa_add_node(nfa, type, node->charset, REGEX_INVALID_ID, REGEX_INVALID_ID);
            fragment->end = fragment->start;

            return fragment->start != REGEX_INVALID_ID;
        }
        case RegexNodeType_Concatenate:
        {
            if(!regex_nfa_build_node(nfa, parser, reversed ? node->rhs : node->lhs, reversed, &lhs))
                return false;

            if(!regex_nfa_build_node(nfa, parser, reversed ? node->lhs : node->rhs, reversed, &rhs))
                return false;

            nfa->nodes[lhs.end].out = rhs.start;

            fragment->start = lhs.start;
            fragment->end = rhs.end;

            return true;
        }
        case RegexNodeType_Alternate:
        {
            if(!regex_nfa_build_node(nfa, parser, node->lhs, reversed, &lhs))
                return false;

            if(!regex_nfa_build_node(nfa, parser, node->rhs, reversed, &rhs))
                return false;

            fragment->end = regex_nfa_add_node(nfa, RegexNFANodeType_Epsilon, 0, REGEX_INVALID_ID, REGEX_INVALID_ID);
            fragment->start = regex_nfa_add_node(nfa, RegexNFANodeType_Split, 0, lhs.start, rhs.start);

            if(fragment->end == REGEX_INVALID_ID || fragment->start == REGEX_INVALID_ID)
                return false;

            nfa->nodes[lhs.end].out = fragment->end;
            nfa->nodes[rhs.end].out = fragment->end;

            return true;
        }
        case RegexNodeType_Repeat:
            return regex_nfa_build_repeat(nfa, parser, node, reversed, fragment);
        default:
            g_current_error = ErrorCode_RegexInvalidToken;
            logger_log_error("Regex error: invalid node type during NFA construction");
            return false;
    }
}

/*
 * Reversed NFAs match the reversed language (concatenations are flipped, ^ and $ swapped)
 * Unanchored NFAs are prefixed with a loop consuming any byte, like .*(pattern)
 */
bool regex_nfa_build(RegexNFA* nfa, const RegexParser* parser, uint16_t root, bool reversed, bool unanchored)
{
    RegexFragment fragment;
    uint16_t match;
    uint16_t any;

    nfa->num_nodes = 0;

    if(!regex_nfa_build_node(nfa, parser, root, reversed, &fragment))
        return false;

    match = regex_nfa_add_node(nfa, RegexNFANodeType_Match, 0, REGEX_INVALID_ID, REGEX_INVALID_ID);

    if(match == REGEX_INVALID_ID)
        return false;

    nfa->nodes[fragment.end].out = match;
    nfa->start = fragment.start;

    if(unanchored)
    {
        any = regex_nfa_add_node(nfa, RegexNFANodeType_Charset, REGEX_CHARSET_ANY, REGEX_INVALID_ID, REGEX_INVALID_ID);

        if(any == REGEX_INVALID_ID)
            return false;

        nfa->start = regex_nfa_add_node(nfa, RegexNFANodeType_Split, 0, fragment.start, any);

        if(nfa->start == REGEX_INVALID_ID)
            return false;

        nfa->nodes[any].out = nfa->start;
    }

    return true;
}

void regex_nfa_debug(const RegexNFA* nfa, const RegexParser* parser)
{
    const RegexNFANode* node;
    char charset_str[1300];
    uint32_t i;

    logger_log_debug("NFA nodes: %u, start: %u", nfa->num_nodes, (uint32_t)nfa->start);

    for(i = 0; i < nfa->num_nodes; i++)
    {
        node = &nfa->nodes[i];

        switch(node->type)
        {
            case RegexNFANodeType_Charset:
                regex_charset_to_string(&parser->charsets[node->charset], charset_str);
                logger_log_debug("  %4u: CHARSET [%s] -> %u", i, charset_str, (uint32_t)node->out);
                break;
            case RegexNFANodeType_Split:
                logger_log_debug("  %4u: SPLIT -> %u, %u", i, (uint32_t)node->out, (uint32_t)node->out2);
                break;
            case RegexNFANodeType_Epsilon:
                logger_log_debug("  %4u: EPSILON -> %u", i, (uint32_t)node->out);
                break;
            case RegexNFANodeType_LineStart:
                logger_log_debug("  %4u: LINE_START -> %u", i, (uint32_t)node->out);
                break;
            case RegexNFANodeType_LineEnd:
                logger_log_debug("  %4u: LINE_END -> %u", i, (uint32_t)node->out);
                break;
            case RegexNFANodeType_Match:
                logger_log_debug("  %4u: MATCH", i);
                break;
        }
    }
}

/*******/
/* DFA */
/*******/

typedef enum RegexDFAStateFlags {
    /* Accepts before any byte */
    RegexDFAStateFlags_Accept = BIT(0),
    /* Accepts before a '\n' (RegexFlags_Newline only) */
    RegexDFAStateFlags_AcceptNewline = BIT(1),
    /* Accepts at the end of the input */
    RegexDFAStateFlags_AcceptEnd = BIT(2),
} RegexDFAStateFlags;

/*
 * A DFA state is a set of NFA nodes: charsets, matches and unresolved $ assertions.
 * Whether ^ holds is known when a state is created (it depends on the previous byte),
 * whether $ holds is not (it depends on the next byte), so $ nodes are kept in the set
 * and expanded when stepping on '\n' or when checking for acceptance at the end
 */
typedef struct RegexDFAState {
    uint32_t set_offset;
    uint32_t set_sz;
    uint32_t hash;
    uint32_t at_line_start;
} RegexDFAState;

typedef struct RegexDFA {
    uint32_t table_offset;
    uint32_t num_states;
    uint16_t start[2]; /* indexed by at_line_start */
} RegexDFA;

typedef struct RegexCompiler {
    RegexParser parser;
    RegexByteClasses classes;
    RegexNFA nfa;

    uint32_t num_states;
    uint32_t set_pool_sz;
    uint32_t table_sz;
    uint32_t mark_generation;

    RegexDFAState states[REGEX_MAX_DFA_STATES];
    uint16_t hash_table[REGEX_DFA_HASH_TABLE_SIZE];
    uint16_t set_pool[REGEX_MAX_DFA_SET_POOL];

    /* Rows of (num_classes + 1) entries, the last one holds the state flags */
    uint16_t table[REGEX_MAX_DFA_TRANSITIONS];

    /* Closure workspace */
    uint32_t marks[REGEX_MAX_NFA_NODES];
    uint16_t closure_stack[REGEX_MAX_NFA_NODES];
    uint16_t closure_set[REGEX_MAX_NFA_NODES];
    uint16_t expanded_set[REGEX_MAX_NFA_NODES];
    uint16_t roots[REGEX_MAX_NFA_NODES];
} RegexCompiler;

int regex_u16_cmp(const void* lhs, const void* rhs)
{
    return (int)*(const uint16_t*)lhs - (int)*(const uint16_t*)rhs;
}

ROMANO_FORCE_INLINE void regex_closure_push(RegexCompiler* compiler, uint16_t node_id, uint32_t* stack_sz)
{
    if(compiler->marks[node_id] != compiler->mark_generation)
    {
        compiler->marks[node_id] = compiler->mark_generation;
        compiler->closure_stack[(*stack_sz)++] = node_id;
    }
}

/* Epsilon closure of roots, writes the sorted resulting set in out and returns its size */
uint32_t regex_dfa_closure(RegexCompiler* compiler,
                           const uint16_t* roots,
                           uint32_t roots_sz,
                           bool at_line_start,
                           bool at_line_end,
                           uint16_t* out)
{
    const RegexNFANode* node;
    uint32_t stack_sz;
    uint32_t out_sz;
    uint32_t i;
    uint16_t node_id;

    if(++compiler->mark_generation == 0)
    {
        memset(compiler->marks, 0, sizeof(compiler->marks));
        compiler->mark_generation = 1;
    }

    stack_sz = 0;
    out_sz = 0;

    for(i = 0; i < roots_sz; i++)
        regex_closure_push(compiler, roots[i], &stack_sz);

    while(stack_sz > 0)
    {
        node_id = compiler->closure_stack[--stack_sz];
        node = &compiler->nfa.nodes[node_id];

        switch(node->type)
        {
            case RegexNFANodeType_Charset:
            case RegexNFANodeType_Match:
                out[out_sz++] = node_id;
                break;
            case RegexNFANodeType_Split:
                regex_closure_push(compiler, node->out2, &stack_sz);
                regex_closure_push(compiler, node->out, &stack_sz);
                break;
            case RegexNFANodeType_Epsilon:
                regex_closure_push(compiler, node->out, &stack_sz);
                break;
            case RegexNFANodeType_LineStart:
                if(at_line_start)
                    regex_closure_push(compiler, node->out, &stack_sz);
                break;
            case RegexNFANodeType_LineEnd:
                if(at_line_end)
                    regex_closure_push(compiler, node->out, &stack_sz);
                else
                    out[out_sz++] = node_id;
                break;
        }
    }

    qsort(out, out_sz, sizeof(uint16_t), regex_u16_cmp);

    return out_sz;
}

ROMANO_FORCE_INLINE bool regex_dfa_set_has_type(RegexCompiler* compiler,
                                               const uint16_t* set,
                                               uint32_t set_sz,
                                               RegexNFANodeType type)
{
    uint32_t i;

    for(i = 0; i < set_sz; i++)
    {
        if(compiler->nfa.nodes[set[i]].type == type)
            return true;
    }

    return false;
}

/* Returns the id of the state, adding it if needed, or REGEX_INVALID_ID if a limit is exceeded */
uint16_t regex_dfa_find_or_add_state(RegexCompiler* compiler,
                                     const RegexDFA* dfa,
                                     const uint16_t* set,
                                     uint32_t set_sz,
                                     bool at_line_start)
{
    RegexDFAState* state;
    uint32_t hash;
    uint32_t slot;
    uint32_t i;
    uint32_t row_sz;

    if(set_sz == 0)
        return REGEX_DFA_DEAD_STATE;

    /* ^ only matters for states with pending $ nodes, merge the others */
    if(!regex_dfa_set_has_type(compiler, set, set_sz, RegexNFANodeType_LineEnd))
        at_line_start = false;

    /* FNV-1a */
    hash = 2166136261u ^ (uint32_t)at_line_start;

    for(i = 0; i < set_sz; i++)
    {
        hash = (hash ^ (set[i] & 0xFF)) * 16777619u;
        hash = (hash ^ (set[i] >> 8)) * 16777619u;
    }

    slot = hash % REGEX_DFA_HASH_TABLE_SIZE;

    while(compiler->hash_table[slot] != REGEX_INVALID_ID)
    {
        state = &compiler->states[compiler->hash_table[slot]];

        if(state->hash == hash &&
           state->set_sz == set_sz &&
           state->at_line_start == (uint32_t)at_line_start &&
           memcmp(&compiler->set_pool[state->set_offset], set, set_sz * sizeof(uint16_t)) == 0)
            return compiler->hash_table[slot];

        slot = (slot + 1) % REGEX_DFA_HASH_TABLE_SIZE;
    }

    row_sz = compiler->classes.num_classes + 1;

    if(compiler->num_states >= REGEX_MAX_DFA_STATES)
    {
        g_current_error = ErrorCode_SizeOverflow;
        logger_log_error("Regex error: too many DFA states, increase REGEX_MAX_DFA_STATES");
        return REGEX_INVALID_ID;
    }

    if(compiler->set_pool_sz + set_sz > REGEX_MAX_DFA_SET_POOL)
    {
        g_current_error = ErrorCode_SizeOverflow;
        logger_log_error("Regex error: DFA states too big, increase REGEX_MAX_DFA_SET_POOL");
        return REGEX_INVALID_ID;
    }

    if(dfa->table_offset + (compiler->num_states + 1) * row_sz > REGEX_MAX_DFA_TRANSITIONS)
    {
        g_current_error = ErrorCode_SizeOverflow;
        logger_log_error("Regex error: DFA table too big, increase REGEX_MAX_DFA_TRANSITIONS");
        return REGEX_INVALID_ID;
    }

    state = &compiler->states[compiler->num_states];
    state->set_offset = compiler->set_pool_sz;
    state->set_sz = set_sz;
    state->hash = hash;
    state->at_line_start = (uint32_t)at_line_start;

    memcpy(&compiler->set_pool[compiler->set_pool_sz], set, set_sz * sizeof(uint16_t));
    compiler->set_pool_sz += set_sz;

    compiler->hash_table[slot] = (uint16_t)compiler->num_states;

    return (uint16_t)compiler->num_states++;
}

/* Subset construction, the DFA table is appended to compiler->table */
bool regex_dfa_build(RegexCompiler* compiler, RegexDFA* dfa)
{
    const RegexNFANode* node;
    const uint16_t* set;
    const uint16_t* source_set;
    uint16_t* row;
    RegexDFAState* state;
    uint32_t row_sz;
    uint32_t set_sz;
    uint32_t source_set_sz;
    uint32_t expanded_set_sz;
    uint32_t roots_sz;
    uint32_t closure_sz;
    uint32_t i;
    uint32_t j;
    uint32_t k;
    uint16_t flags;
    uint16_t next_state;
    uint8_t newline_class;
    bool newline;
    bool at_line_start;

    newline = (compiler->parser.flags & RegexFlags_Newline) != 0;
    newline_class = compiler->classes.map['\n'];
    row_sz = compiler->classes.num_classes + 1;

    dfa->table_offset = compiler->table_sz;

    memset(compiler->hash_table, 0xFF, sizeof(compiler->hash_table));
    compiler->set_pool_sz = 0;

    /* Dead state */
    memset(&compiler->states[0], 0, sizeof(RegexDFAState));
    compiler->num_states = 1;

    for(i = 0; i < 2; i++)
    {
        closure_sz = regex_dfa_closure(compiler, &compiler->nfa.start, 1, i == 1, false, compiler->closure_set);
        next_state = regex_dfa_find_or_add_state(compiler, dfa, compiler->closure_set, closure_sz, i == 1);

        if(next_state == REGEX_INVALID_ID)
            return false;

        dfa->start[i] = next_state;
    }

    /* compiler->num_states grows while we iterate */
    for(i = 0; i < compiler->num_states; i++)
    {
        row = &compiler->table[dfa->table_offset + i * row_sz];

        if(i == REGEX_DFA_DEAD_STATE)
        {
            memset(row, 0, row_sz * sizeof(uint16_t));
            continue;
        }

        state = &compiler->states[i];
        set = &compiler->set_pool[state->set_offset];
        set_sz = state->set_sz;

        /* Acceptance */
        flags = 0;

        if(regex_dfa_set_has_type(compiler, set, set_sz, RegexNFANodeType_Match))
            flags |= RegexDFAStateFlags_Accept;

        expanded_set_sz = regex_dfa_closure(compiler,
                                            set,
                                            set_sz,
                                            state->at_line_start != 0,
                                            true,
                                            compiler->expanded_set);

        if(regex_dfa_set_has_type(compiler, compiler->expanded_set, expanded_set_sz, RegexNFANodeType_Match))
        {
            flags |= RegexDFAStateFlags_AcceptEnd;

            if(newline)
                flags |= RegexDFAStateFlags_AcceptNewline;
        }

        row[row_sz - 1] = flags;

        /* Transitions */
        for(k = 0; k < compiler->classes.num_classes; k++)
        {
            at_line_start = newline && k == newline_class;

            /* Stepping over '\n' in newline mode, pending $ nodes hold */
            if(at_line_start)
            {
                source_set = compiler->expanded_set;
                source_set_sz = expanded_set_sz;
            }
            else
            {
                source_set = set;
                source_set_sz = set_sz;
            }

            roots_sz = 0;

            for(j = 0; j < source_set_sz; j++)
            {
                node = &compiler->nfa.nodes[source_set[j]];

                if(node->type == RegexNFANodeType_Charset &&
                   regex_charset_has(&compiler->parser.charsets[node->charset],
                                     compiler->classes.representatives[k]))
                    compiler->roots[roots_sz++] = node->out;
            }

            closure_sz = regex_dfa_closure(compiler,
                                           compiler->roots,
                                           roots_sz,
                                           at_line_start,
                                           false,
                                           compiler->closure_set);

            next_state = regex_dfa_find_or_add_state(compiler,
                                                     dfa,
                                                     compiler->closure_set,
                                                     closure_sz,
                                                     at_line_start);

            if(next_state == REGEX_INVALID_ID)
                return false;

            /* The state array may not move, but reload the set pointer for clarity */
            set = &compiler->set_pool[compiler->states[i].set_offset];
            row[k] = next_state;
        }
    }

    dfa->num_states = compiler->num_states;
    compiler->table_sz += compiler->num_states * row_sz;

    return true;
}

void regex_dfa_debug(const RegexCompiler* compiler, const RegexDFA* dfa, const char* name)
{
    const uint16_t* row;
    char line[4096];
    uint32_t row_sz;
    uint32_t i;
    uint32_t k;
    int line_sz;

    row_sz = compiler->classes.num_classes + 1;

    logger_log_debug("%s DFA states: %u, start: %u (line start: %u)",
                     name,
                     dfa->num_states,
                     (uint32_t)dfa->start[0],
                     (uint32_t)dfa->start[1]);

    for(i = 1; i < dfa->num_states; i++)
    {
        row = &compiler->table[dfa->table_offset + i * row_sz];

        line_sz = snprintf(line,
                           sizeof(line),
                           "  %4u%s%s%s:",
                           i,
                           row[row_sz - 1] & RegexDFAStateFlags_Accept ? " ACCEPT" : "",
                           row[row_sz - 1] & RegexDFAStateFlags_AcceptNewline ? " ACCEPT_NL" : "",
                           row[row_sz - 1] & RegexDFAStateFlags_AcceptEnd ? " ACCEPT_END" : "");

        for(k = 0; k < row_sz - 1 && line_sz > 0 && line_sz < (int)sizeof(line) - 32; k++)
        {
            if(row[k] != REGEX_DFA_DEAD_STATE)
                line_sz += snprintf(line + line_sz, sizeof(line) - line_sz, " c%u->%u", k, (uint32_t)row[k]);
        }

        logger_log_debug("%s", line);
    }
}

/***************/
/* Compilation */
/***************/

struct Regex
{
    /* Rows of (num_classes + 1) entries, transitions are premultiplied by the row size */
    uint32_t* forward;
    uint32_t* reverse;
    uint32_t forward_start[2];
    uint32_t reverse_start[2];
    uint32_t num_classes;
    uint32_t flags;
    uint8_t class_map[256];
};

void regex_copy_dfa_table(const RegexCompiler* compiler, const RegexDFA* dfa, uint32_t* table)
{
    const uint16_t* row;
    uint32_t row_sz;
    uint32_t i;
    uint32_t k;

    row_sz = compiler->classes.num_classes + 1;

    for(i = 0; i < dfa->num_states; i++)
    {
        row = &compiler->table[dfa->table_offset + i * row_sz];

        for(k = 0; k < row_sz - 1; k++)
            table[i * row_sz + k] = (uint32_t)row[k] * row_sz;

        table[i * row_sz + row_sz - 1] = (uint32_t)row[row_sz - 1];
    }
}

Regex* regex_compile(const char* pattern, RegexFlags flags)
{
    RegexCompiler* compiler;
    RegexDFA forward;
    RegexDFA reverse;
    Regex* regex;
    uint32_t row_sz;
    uint16_t root;

#if defined(REGEX_COMPILE_USE_HEAP)
    compiler = (RegexCompiler*)malloc(sizeof(RegexCompiler));

    if(compiler == NULL)
    {
        g_current_error = ErrorCode_MemAllocError;
        return NULL;
    }
#else
    RegexCompiler compiler_storage;
    compiler = &compiler_storage;
#endif /* defined(REGEX_COMPILE_USE_HEAP) */

    regex = NULL;
    compiler->table_sz = 0;
    compiler->mark_generation = 0;
    memset(compiler->marks, 0, sizeof(compiler->marks));

    if(flags & RegexFlags_DebugCompilation)
        logger_log_debug("Compiling regex: %s", pattern);

    root = regex_parse(&compiler->parser, pattern, flags);

    if(root == REGEX_INVALID_ID)
        goto end;

    regex_byte_classes_compute(&compiler->classes, &compiler->parser);

    if(flags & RegexFlags_DebugCompilation)
    {
        logger_log_debug("********");
        logger_log_debug("Regex tree:");
        regex_ast_debug(&compiler->parser, root, 1);
        logger_log_debug("********");
        regex_byte_classes_debug(&compiler->classes);
    }

    if(!regex_nfa_build(&compiler->nfa, &compiler->parser, root, false, false))
        goto end;

    if(flags & RegexFlags_DebugCompilation)
    {
        logger_log_debug("********");
        logger_log_debug("Forward NFA:");
        regex_nfa_debug(&compiler->nfa, &compiler->parser);
    }

    if(!regex_dfa_build(compiler, &forward))
        goto end;

    if(flags & RegexFlags_DebugCompilation)
    {
        logger_log_debug("********");
        regex_dfa_debug(compiler, &forward, "Forward");
    }

    if(!regex_nfa_build(&compiler->nfa, &compiler->parser, root, true, true))
        goto end;

    if(flags & RegexFlags_DebugCompilation)
    {
        logger_log_debug("********");
        logger_log_debug("Reverse NFA:");
        regex_nfa_debug(&compiler->nfa, &compiler->parser);
    }

    if(!regex_dfa_build(compiler, &reverse))
        goto end;

    if(flags & RegexFlags_DebugCompilation)
    {
        logger_log_debug("********");
        regex_dfa_debug(compiler, &reverse, "Reverse");
    }

    row_sz = compiler->classes.num_classes + 1;

    /* Single allocation holding the struct and both tables */
    regex = (Regex*)malloc(sizeof(Regex) + compiler->table_sz * sizeof(uint32_t));

    if(regex == NULL)
    {
        g_current_error = ErrorCode_MemAllocError;
        goto end;
    }

    regex->forward = (uint32_t*)((char*)regex + sizeof(Regex));
    regex->reverse = regex->forward + forward.num_states * row_sz;
    regex->forward_start[0] = (uint32_t)forward.start[0] * row_sz;
    regex->forward_start[1] = (uint32_t)forward.start[1] * row_sz;
    regex->reverse_start[0] = (uint32_t)reverse.start[0] * row_sz;
    regex->reverse_start[1] = (uint32_t)reverse.start[1] * row_sz;
    regex->num_classes = compiler->classes.num_classes;
    regex->flags = (uint32_t)flags;
    memcpy(regex->class_map, compiler->classes.map, sizeof(regex->class_map));

    regex_copy_dfa_table(compiler, &forward, regex->forward);
    regex_copy_dfa_table(compiler, &reverse, regex->reverse);

    if(flags & RegexFlags_DebugCompilation)
    {
        logger_log_debug("********");
        logger_log_debug("Regex compiled: %u byte classes, %u + %u DFA states, %zu bytes",
                         regex->num_classes,
                         forward.num_states,
                         reverse.num_states,
                         sizeof(Regex) + compiler->table_sz * sizeof(uint32_t));
    }

end:
    if(regex == NULL)
        logger_log_error("Failed to compile regex: %s", pattern);

#if defined(REGEX_COMPILE_USE_HEAP)
    free(compiler);
#endif /* defined(REGEX_COMPILE_USE_HEAP) */

    return regex;
}

/************/
/* Matching */
/************/

ROMANO_FORCE_INLINE bool regex_state_accepts(uint32_t state_flags, uint8_t next_char)
{
    return (state_flags & RegexDFAStateFlags_Accept) ||
           ((state_flags & RegexDFAStateFlags_AcceptNewline) && next_char == '\n');
}

ROMANO_FORCE_INLINE bool regex_is_at_line_start(const Regex* regex, const char* string, size_t pos)
{
    return pos == 0 || ((regex->flags & RegexFlags_Newline) && string[pos - 1] == '\n');
}

/* Runs the forward DFA from start, returns the end of the longest match starting there */
bool regex_forward_longest(const Regex* regex,
                           const char* string,
                           size_t string_sz,
                           size_t start,
                           size_t* end)
{
    const uint32_t* table;
    uint32_t num_classes;
    uint32_t state;
    size_t pos;
    uint8_t c;
    bool found;

    table = regex->forward;
    num_classes = regex->num_classes;
    state = regex->forward_start[regex_is_at_line_start(regex, string, start)];
    found = false;

    if(state == REGEX_DFA_DEAD_STATE)
        return false;

    for(pos = start; pos < string_sz; pos++)
    {
        c = (uint8_t)string[pos];

        if(regex_state_accepts(table[state + num_classes], c))
        {
            found = true;
            *end = pos;
        }

        state = table[state + regex->class_map[c]];

        if(state == REGEX_DFA_DEAD_STATE)
            return found;
    }

    if(table[state + num_classes] & RegexDFAStateFlags_AcceptEnd)
    {
        found = true;
        *end = string_sz;
    }

    return found;
}

bool regex_match(const Regex* regex, const char* string, size_t string_sz)
{
    const uint32_t* table;
    uint32_t state;
    size_t pos;

    table = regex->forward;
    state = regex->forward_start[1];

    for(pos = 0; pos < string_sz; pos++)
    {
        if(state == REGEX_DFA_DEAD_STATE)
            return false;

        state = table[state + regex->class_map[(uint8_t)string[pos]]];
    }

    return (table[state + regex->num_classes] & RegexDFAStateFlags_AcceptEnd) != 0;
}

/*
 * The reverse DFA reads the string backward and accepts at every position
 * where a match of the pattern starts
 */

bool regex_search(const Regex* regex,
                  const char* string,
                  size_t string_sz,
                  RegexMatch* match)
{
    const uint32_t* table;
    uint32_t num_classes;
    uint32_t state;
    size_t pos;
    size_t start;
    size_t end;
    uint8_t c;
    bool found;

    table = regex->reverse;
    num_classes = regex->num_classes;
    state = regex->reverse_start[1];
    found = false;
    start = 0;

    for(pos = string_sz; pos > 0; pos--)
    {
        c = (uint8_t)string[pos - 1];

        if(regex_state_accepts(table[state + num_classes], c))
        {
            found = true;
            start = pos;
        }

        state = table[state + regex->class_map[c]];
    }

    if(table[state + num_classes] & RegexDFAStateFlags_AcceptEnd)
    {
        found = true;
        start = 0;
    }

    if(!found || !regex_forward_longest(regex, string, string_sz, start, &end))
        return false;

    if(match != NULL)
    {
        match->data = string + start;
        match->data_sz = end - start;
        match->position = start;
    }

    return true;
}

size_t regex_iterate(const Regex* regex,
                     const char* string,
                     size_t string_sz,
                     RegexIterateFunc func,
                     void* user_data)
{
    uint64_t stack_bitset[REGEX_ITERATE_STACK_BITSET_WORDS];
    uint64_t* bitset;
    const uint32_t* table;
    RegexMatch match;
    uint32_t num_classes;
    uint32_t state;
    size_t num_words;
    size_t word_index;
    size_t pos;
    size_t start;
    size_t end;
    size_t num_matches;
    uint64_t word;
    uint8_t c;

    /* One bit per possible match start, including string_sz */
    num_words = string_sz / 64 + 1;

    if(num_words <= REGEX_ITERATE_STACK_BITSET_WORDS)
    {
        bitset = stack_bitset;
    }
    else
    {
        bitset = (uint64_t*)malloc(num_words * sizeof(uint64_t));

        if(bitset == NULL)
        {
            g_current_error = ErrorCode_MemAllocError;
            return 0;
        }
    }

    memset(bitset, 0, num_words * sizeof(uint64_t));

    /* Backward pass, marks all the positions where a match starts */
    table = regex->reverse;
    num_classes = regex->num_classes;
    state = regex->reverse_start[1];

    for(pos = string_sz; pos > 0; pos--)
    {
        c = (uint8_t)string[pos - 1];

        if(regex_state_accepts(table[state + num_classes], c))
            bitset[pos >> 6] |= BIT64(pos & 63);

        state = table[state + regex->class_map[c]];
    }

    if(table[state + num_classes] & RegexDFAStateFlags_AcceptEnd)
        bitset[0] |= BIT64(0);

    /* Forward pass, from each leftmost start find the longest match */
    num_matches = 0;
    pos = 0;

    while(pos <= string_sz)
    {
        word_index = pos >> 6;
        word = bitset[word_index] & (~0ULL << (pos & 63));

        while(word == 0)
        {
            if(++word_index >= num_words)
                goto end;

            word = bitset[word_index];
        }

        start = word_index * 64 + ctz_u64(word);

        if(!regex_forward_longest(regex, string, string_sz, start, &end))
            break;

        match.data = string + start;
        match.data_sz = end - start;
        match.position = start;

        num_matches++;

        if(!func(&match, user_data))
            break;

        pos = end > start ? end : start + 1;
    }

end:
    if(bitset != stack_bitset)
        free(bitset);

    return num_matches;
}

/********/
/* Free */
/********/

void regex_free(Regex* regex)
{
    if(regex != NULL)
        free(regex);
}