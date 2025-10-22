/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

/* https://swtch.com/~rsc/regexp/regexp1.html */
/* https://dl.acm.org/doi/pdf/10.1145/363347.363387 */

/*
 * TODO one day: https://arxiv.org/pdf/2407.20479#:~:text=We%20present%20a%20tool%20and,on%20the%20baseline%2C%20and%20outperforms
 */

#include "libromano/regex.h"
#include "libromano/common.h"
#include "libromano/memory.h"
#include "libromano/vector.h"
#include "libromano/logger.h"
#include "libromano/error.h"

#include <stdbool.h>
#include <string.h>
#include <ctype.h>

extern ErrorCode g_current_error;

/**********/
/* Lexing */
/**********/

typedef enum RegexOperatorType {
    RegexOperatorType_Alternate,
    RegexOperatorType_Concatenate,
    RegexOperatorType_ZeroOrMore,
    RegexOperatorType_OneOrMore,
    RegexOperatorType_ZeroOrOne,
} RegexOperatorType;

const char* regex_op_type_to_string(RegexOperatorType op)
{
    switch(op)
    {
        case RegexOperatorType_Alternate: return "Alternate";
        case RegexOperatorType_Concatenate: return "Concatenate";
        case RegexOperatorType_ZeroOrMore: return "ZeroOrMore";
        case RegexOperatorType_OneOrMore: return "OneOrMore";
        case RegexOperatorType_ZeroOrOne: return "ZeroOrOne";
        default: return "Unknown";
    }
}

typedef enum RegexCharacterType {
    RegexCharacterType_Single,
    RegexCharacterType_Range,
    RegexCharacterType_Any,
} RegexCharacterType;

const char* regex_char_type_to_string(RegexCharacterType c)
{
    switch(c)
    {
        case RegexCharacterType_Single: return "Single";
        case RegexCharacterType_Range: return "Range";
        case RegexCharacterType_Any: return "Any";
        default: return "Unknown";
    }
}

typedef enum RegexTokenType {
    RegexTokenType_Character,
    RegexTokenType_CharacterRange,
    RegexTokenType_Operator,
    RegexTokenType_GroupBegin,
    RegexTokenType_GroupEnd,
    RegexTokenType_Invalid,
} RegexTokenType;

typedef struct RegexToken {
    const char* data;
    uint32_t data_sz;
    uint16_t type;
    uint16_t encoding; /* RegexOperatorType/RegexCharacterType */
} RegexToken;

RegexToken regex_token_new(const char* data, uint32_t data_sz, uint16_t type, uint16_t encoding)
{
    RegexToken token;
    token.data = data;
    token.data_sz = data_sz;
    token.type = type;
    token.encoding = encoding;

    return token;
}

void regex_token_debug(RegexToken* token)
{
    switch(token->type)
    {
        case RegexTokenType_Character:
        {
            if(token->data != NULL && token->data_sz > 0)
                logger_log_debug("CHAR(%.*s, %s)",
                                 (int)token->data_sz,
                                 token->data,
                                 regex_char_type_to_string(token->encoding));
            else
                logger_log_debug("CHAR(%s)",
                                 regex_char_type_to_string(token->encoding));

            break;
        }
        case RegexTokenType_CharacterRange:
        {
            logger_log_debug("RANGE(%.*s)", (int)token->data_sz, token->data);
            break;
        }
        case RegexTokenType_Operator:
        {
            logger_log_debug("OP(%s)", regex_op_type_to_string(token->encoding));
            break;
        }
        case RegexTokenType_GroupBegin:
        {
            logger_log_debug("GROUP_BEGIN(%d)", (int)token->encoding);
            break;
        }
        case RegexTokenType_GroupEnd:
        {
            logger_log_debug("GROUP_END(%d)", (int)token->encoding);
            break;
        }
        case RegexTokenType_Invalid:
        {
            logger_log_debug("INVALID");
            break;
        }
        default:
        {
            logger_log_debug("UNKNOWN");
            break;
        }
    }
}

/*
 * Returns NULL on failure
 */
Vector* regex_lex(const char* pattern)
{
    Vector* tokens;
    RegexToken token;
    size_t pattern_sz;
    uint32_t i = 0;
    bool needs_concat = false;

    tokens = vector_new(128, sizeof(RegexToken));
    pattern_sz = strlen(pattern);

    while(i < pattern_sz)
    {
        if(isalnum(pattern[i]) || pattern[i] == '_')
        {
            if(needs_concat)
            {
                token = regex_token_new(NULL, 0u, RegexTokenType_Operator, RegexOperatorType_Concatenate);
                vector_push_back(tokens, &token);
            }

            token = regex_token_new(pattern + i, 1u, RegexTokenType_Character, RegexCharacterType_Single);
            vector_push_back(tokens, &token);
            needs_concat = true;
        }
        else
        {
            switch(pattern[i])
            {
                case '|':
                {
                    token = regex_token_new(pattern + i , 1u, RegexTokenType_Operator, RegexOperatorType_Alternate);
                    vector_push_back(tokens, &token);
                    needs_concat = false;
                    break;
                }
                case '*':
                {
                    token = regex_token_new(pattern + i, 1u, RegexTokenType_Operator, RegexOperatorType_ZeroOrMore);
                    vector_push_back(tokens, &token);
                    break;
                }
                case '+':
                {
                    token = regex_token_new(pattern + i, 1u, RegexTokenType_Operator, RegexOperatorType_OneOrMore);
                    vector_push_back(tokens, &token);
                    break;
                }
                case '?':
                {
                    token = regex_token_new(pattern + i, 1u, RegexTokenType_Operator, RegexOperatorType_ZeroOrOne);
                    vector_push_back(tokens, &token);
                    break;
                }
                case '.':
                {
                    if(needs_concat)
                    {
                        token = regex_token_new(NULL, 0u, RegexTokenType_Operator, RegexOperatorType_Concatenate);
                        vector_push_back(tokens, &token);
                    }

                    token = regex_token_new(pattern + i, 1u, RegexTokenType_Character, RegexCharacterType_Any);
                    vector_push_back(tokens, &token);

                    needs_concat = true;

                    break;
                }
                case '(':
                {
                    token = regex_token_new(pattern + i, 1u, RegexTokenType_GroupBegin, 0);
                    vector_push_back(tokens, &token);

                    break;
                }
                case ')':
                {
                    token = regex_token_new(pattern + i, 1u, RegexTokenType_GroupEnd, 0);
                    vector_push_back(tokens, &token);

                    break;
                }
                case '[':
                {
                    if(needs_concat)
                    {
                        token = regex_token_new(NULL, 0u, RegexTokenType_Operator, RegexOperatorType_Concatenate);
                        vector_push_back(tokens, &token);
                    }

                    const char* start = pattern + ++i;

                    while(i < pattern_sz && pattern[i] != ']')
                        i++;

                    if(i >= pattern_sz)
                    {
                        g_current_error = ErrorCode_RegexInvalidCharacterRange;
                        logger_log_error("Unclosed character range in regular expression");
                        vector_free(tokens);
                        return NULL;
                    }

                    token = regex_token_new(start, (pattern + i) - start, RegexTokenType_CharacterRange, 0);
                    vector_push_back(tokens, &token);
                    needs_concat = true;

                    break;
                }
                default:
                {
                    g_current_error = ErrorCode_RegexUnexpectedCharacter;
                    logger_log_error("Unsupported character found in regular expression: %c", pattern[i]);
                    vector_free(tokens);
                    return NULL;
                }
            }
        }

        i++;
    }

    return tokens;
}

ROMANO_FORCE_INLINE int get_operator_precedence(RegexOperatorType op)
{
    switch(op)
    {
        case RegexOperatorType_Alternate: return 1;
        case RegexOperatorType_Concatenate: return 2;
        case RegexOperatorType_OneOrMore:
        case RegexOperatorType_ZeroOrMore:
        case RegexOperatorType_ZeroOrOne: return 3;
        default: return 0;
    }
}

ROMANO_FORCE_INLINE bool is_left_associative(RegexOperatorType op)
{
    switch(op)
    {
        case RegexOperatorType_Alternate:
        case RegexOperatorType_Concatenate: return true;
        case RegexOperatorType_OneOrMore:
        case RegexOperatorType_ZeroOrMore:
        case RegexOperatorType_ZeroOrOne: return false;
        default: return true;
    }
}

void regex_tokens_debug(Vector* tokens)
{
    size_t i;

    for(i = 0; i < vector_size(tokens); i++)
    {
        regex_token_debug((RegexToken*)vector_at(tokens, i));
    }
}

/***********************/
/* Bytecode Generation */
/***********************/

typedef uint8_t byte;

ROMANO_FORCE_INLINE byte as_byte(char c) { byte b; memcpy(&b, &c, sizeof(byte)); return b; }
ROMANO_FORCE_INLINE char as_char(byte b) { char c; memcpy(&c, &b, sizeof(char)); return c; }

typedef enum RegexOpCode {
    RegexOpCode_TestSingle,
    RegexOpCode_TestRange,
    RegexOpCode_TestNegatedRange,
    RegexOpCode_TestAny,
    RegexOpCode_TestDigit,
    RegexOpCode_TestLowerCase,
    RegexOpCode_TestUpperCase,
    RegexOpCode_JumpEq,     /* jumps if status_flag = true */
    RegexOpCode_JumpNeq,    /* jumps if status_flag = false */
    RegexOpCode_Accept,
    RegexOpCode_Fail,
    RegexOpCode_GroupStart,
    RegexOpCode_GroupEnd,
    RegexOpCode_IncPos,     /* ++string_pos */
    RegexOpCode_DecPos,     /* --string_pos */
    RegexOpCode_IncPosEq,   /* string_pos += (status_flag ? 1 : 0) */
    RegexOpCode_JumpPos,    /* Absolute jump in string position */
    RegexOpCode_SetFlag,    /* status_flag = static_cast<bool>(bytecode[pc + 1]) */
} RegexOpCode;

typedef enum RegexOpType {
    RegexOpType_TestOp,
    RegexOpType_UnaryOp,
    RegexOpType_BinaryOp,
    RegexOpType_GroupOp,
} RegexOpType;

const int JUMP_FAIL = INT_MAX;

typedef struct RegexJump {
    byte bytes[4];
} RegexJump;

ROMANO_FORCE_INLINE RegexJump regex_encode_jump(int jump_size)
{
    RegexJump jump;
    jump.bytes[0] = jump_size >> 24 & 0xFF;
    jump.bytes[1] = jump_size >> 16 & 0xFF;
    jump.bytes[2] = jump_size >> 8 & 0xFF;
    jump.bytes[3] = jump_size >> 0 & 0xFF;

    return jump;
}

ROMANO_FORCE_INLINE int regex_decode_jump(byte* bytecode)
{
    return ((int)(bytecode[0]) << 24) |
           ((int)(bytecode[1]) << 16) |
           ((int)(bytecode[2]) << 8) |
           ((int)(bytecode[3]) << 0);
}

ROMANO_FORCE_INLINE size_t regex_emit_jump(Vector* bytecode, RegexOpCode op)
{
    size_t i;
    size_t bytecode_sz;
    RegexJump jmp;

    vector_push_back(bytecode, &op);

    bytecode_sz = vector_size(bytecode);

    jmp = regex_encode_jump(0);

    for(i = 0; i < 4; i++)
        vector_push_back(bytecode, &jmp.bytes[i]);

    return bytecode_sz;
}

ROMANO_FORCE_INLINE void regex_patch_jump(Vector* bytecode, size_t jump_pos, int offset)
{
    RegexJump jmp;
    byte* bytes;

    jmp = regex_encode_jump(offset);
    bytes = (byte*)vector_at(bytecode, jump_pos);

    bytes[0] = jmp.bytes[0];
    bytes[1] = jmp.bytes[1];
    bytes[2] = jmp.bytes[2];
    bytes[3] = jmp.bytes[3];
}

ROMANO_FORCE_INLINE void regex_emit_range_opcodes(Vector* bytecode, char range_start, char range_end)
{
    byte b;

    if(range_start == '0' && range_end == '9')
    {
        b = as_byte(RegexOpCode_TestDigit);
        vector_push_back(bytecode, &b);

        b = as_byte(RegexOpCode_IncPosEq);
        vector_push_back(bytecode, &b);
    }
    else if(range_start == 'a' && range_end == 'z')
    {
        b = as_byte(RegexOpCode_TestLowerCase);
        vector_push_back(bytecode, &b);

        b = as_byte(RegexOpCode_IncPosEq);
        vector_push_back(bytecode, &b);
    }
    else if(range_start == 'A' && range_end == 'Z')
    {
        b = as_byte(RegexOpCode_TestUpperCase);
        vector_push_back(bytecode, &b);

        b = as_byte(RegexOpCode_IncPosEq);
        vector_push_back(bytecode, &b);
    }
    else
    {
        b = as_byte(RegexOpCode_TestRange);
        vector_push_back(bytecode, &b);

        b = as_byte(range_start);
        vector_push_back(bytecode, &b);

        b = as_byte(range_end);
        vector_push_back(bytecode, &b);

        b = as_byte(RegexOpCode_IncPosEq);
        vector_push_back(bytecode, &b);
    }
}

bool regex_emit_alternation(Vector* tokens,
                            size_t* pos,
                            uint32_t* current_group_id,
                            Vector* bytecode);

bool regex_emit_concatenation(Vector* tokens,
                              size_t* pos,
                              uint32_t* current_group_id,
                              Vector* bytecode);

bool regex_emit_quantified(Vector* tokens,
                           size_t* pos,
                           uint32_t* current_group_id,
                           Vector* bytecode);

bool regex_emit_primary(Vector* tokens,
                        size_t* pos,
                        uint32_t* current_group_id,
                        Vector* bytecode);

bool regex_emit_alternation(Vector* tokens,
                            size_t* pos,
                            uint32_t* current_group_id,
                            Vector* bytecode)
{
    RegexToken* token;
    size_t lhs_start;
    size_t rhs_start;
    size_t jump_over_rhs_pos;
    int offset;

    lhs_start = vector_size(bytecode);

    if(!regex_emit_concatenation(tokens, pos, current_group_id, bytecode))
        return false;

    while(*pos < vector_size(tokens))
    {
        token = (RegexToken*)vector_at(tokens, *pos);

        if(!(token->type == RegexTokenType_Operator && token->encoding == RegexOperatorType_Alternate))
            break;

        (*pos)++;

        jump_over_rhs_pos = regex_emit_jump(bytecode, RegexOpCode_JumpEq);

        rhs_start = vector_size(bytecode);

        if(!regex_emit_concatenation(tokens, pos, current_group_id, bytecode))
            return false;

        offset = (int)vector_size(bytecode) - (int)(jump_over_rhs_pos - 1);
        regex_patch_jump(bytecode, jump_over_rhs_pos, offset);
    }

    return true;
}

bool regex_emit_concatenation(Vector* tokens,
                              size_t* pos,
                              uint32_t* current_group_id,
                              Vector* bytecode)
{
    bool first;
    size_t jump_pos;
    RegexToken* token;

    first = true;

    while(*pos < vector_size(tokens))
    {
        token = vector_at(tokens, *pos);

        if(token->type == RegexTokenType_GroupEnd)
            break;

        // if(!(token->type == RegexTokenType_Operator &&
        //      (RegexOperatorType)token->encoding == RegexOperatorType_Alternate))
        //     break;

        if(!first)
        {
            jump_pos = regex_emit_jump(bytecode, RegexOpCode_JumpNeq);
            regex_patch_jump(bytecode, jump_pos, JUMP_FAIL);
        }

        if(!regex_emit_quantified(tokens, pos, current_group_id, bytecode))
            return false;

        first = false;
    }

    return true;
}

bool regex_emit_quantified(Vector* tokens,
                           size_t* pos,
                           uint32_t* current_group_id,
                           Vector* bytecode)
{
    RegexToken* token;
    RegexOperatorType op_type;
    size_t primary_start;
    int offset_back;
    size_t loop_start;
    size_t fail_jump_pos;
    size_t exit_jump_pos;
    size_t loop_jump_pos;
    size_t primary_sz;
    size_t i;
    byte* primary_copy;
    byte b;

    primary_start = vector_size(bytecode);

    if(!regex_emit_primary(tokens, pos, current_group_id, bytecode))
        return false;

    if(*pos >= vector_size(tokens))
        return true;

    token = vector_at(tokens, *pos);

    if(token->type == RegexTokenType_Operator)
    {
        op_type = (RegexOperatorType)token->encoding;

        switch(op_type)
        {
            case RegexOperatorType_ZeroOrMore:
            {
                (*pos)++;

                offset_back = (int)(primary_start - vector_size(bytecode));
                loop_jump_pos = regex_emit_jump(bytecode, RegexOpCode_JumpEq);
                regex_patch_jump(bytecode, loop_jump_pos, offset_back);

                b = as_byte(RegexOpCode_SetFlag);
                vector_push_back(bytecode, &b);

                b = as_byte(1);
                vector_push_back(bytecode, &b);

                break;
            }
            case RegexOperatorType_OneOrMore:
            {
                (*pos)++;

                loop_start = vector_size(bytecode);

                primary_sz = (loop_start - primary_start);
                primary_copy = mem_alloca(primary_sz * sizeof(byte));
                memcpy(primary_copy, vector_at(bytecode, primary_start), primary_sz * sizeof(byte));

                fail_jump_pos = regex_emit_jump(bytecode, RegexOpCode_JumpNeq);
                regex_patch_jump(bytecode, fail_jump_pos, JUMP_FAIL);

                for(i = 0; i < primary_sz; i++)
                    vector_push_back(bytecode, &primary_copy[i]);

                exit_jump_pos = regex_emit_jump(bytecode, RegexOpCode_JumpNeq);

                b = as_byte(RegexOpCode_SetFlag);
                vector_push_back(bytecode, &b);

                b = as_byte(1);
                vector_push_back(bytecode, &b);

                offset_back = (int)(loop_start - vector_size(bytecode));
                loop_jump_pos = regex_emit_jump(bytecode, RegexOpCode_JumpEq);
                regex_patch_jump(bytecode, loop_jump_pos, offset_back);

                regex_patch_jump(bytecode, exit_jump_pos, (int)(vector_size(bytecode) - exit_jump_pos - 1));

                break;
            }
            case RegexOperatorType_ZeroOrOne:
            {
                (*pos)++;

                b = as_byte(RegexOpCode_SetFlag);
                vector_push_back(bytecode, &b);

                b = as_byte(1);
                vector_push_back(bytecode, &b);
                break;
            }
            default:
                break;
        }
    }

    return true;
}

bool regex_emit_primary(Vector* tokens,
                        size_t* pos,
                        uint32_t* current_group_id,
                        Vector* bytecode)
{
    RegexToken* token;
    byte b;
    uint32_t group_id;

    if(*pos >= vector_size(tokens))
    {
        g_current_error = ErrorCode_RegexUnexpectedEndOfExpression;
        logger_log_error("Unexpected end of regex expression");
        return false;
    }

    token = (RegexToken*)vector_at(tokens, *pos);

    switch(token->type)
    {
        case RegexTokenType_Character:
        {
            (*pos)++;

            switch(token->encoding)
            {
                case RegexCharacterType_Single:
                {
                    b = as_byte(RegexOpCode_TestSingle);
                    vector_push_back(bytecode, &b);

                    b = as_byte(token->data[0]);
                    vector_push_back(bytecode, &b);

                    break;
                }
                case RegexCharacterType_Any:
                {
                    b = as_byte(RegexOpCode_TestAny);
                    vector_push_back(bytecode, &b);
                    break;
                }
            }

            b = as_byte(RegexOpCode_IncPosEq);
            vector_push_back(bytecode, &b);

            return true;
        }

        case RegexTokenType_CharacterRange:
        {
            (*pos)++;

            regex_emit_range_opcodes(bytecode, token->data[0], token->data[2]);

            return true;
        }

        case RegexTokenType_GroupBegin:
        {
            group_id = (*current_group_id)++;
            (*pos)++;

            b = as_byte(RegexOpCode_GroupStart);
            vector_push_back(bytecode, &b);

            b = as_byte((uint8_t)(group_id & 0xFF));
            vector_push_back(bytecode, &b);

            if(!regex_emit_alternation(tokens, pos, current_group_id, bytecode))
                return false;

            if(*pos >= vector_size(tokens) ||
                ((RegexToken*)vector_at(tokens, *pos))->type != RegexTokenType_GroupEnd)
            {
                g_current_error = ErrorCode_RegexMismatchedParentheses;
                logger_log_error("Mismatched parentheses in regular expression");
                return false;
            }

            b = as_byte(RegexOpCode_GroupEnd);
            vector_push_back(bytecode, &b);

            b = as_byte((uint8_t)(group_id & 0xFF));
            vector_push_back(bytecode, &b);

            (*pos)++;

            return true;
        }

        case RegexTokenType_Operator:
        {
            (*pos)++;

            switch(token->encoding)
            {
                case RegexOperatorType_Alternate:
                    return regex_emit_alternation(tokens, pos, current_group_id, bytecode);
                case RegexOperatorType_Concatenate:
                    return regex_emit_concatenation(tokens, pos, current_group_id, bytecode);
                default:
                {
                    g_current_error = ErrorCode_RegexInvalidOperator;
                    logger_log_error("Invalid operator type during bytecode emission");
                    return false;
                }
            }
        }

        default:
        {
            g_current_error = ErrorCode_RegexInvalidToken;
            logger_log_error("Invalid token type during bytecode emission");
            return false;
        }
    }
    return true;
}

bool regex_emit(Vector* tokens, Vector* bytecode)
{
    RegexJump jmp;
    size_t pos;
    size_t final_fail_jump;
    size_t i;
    uint32_t current_group_id;
    int offset;
    byte instr;
    byte b;

    pos = 0;
    current_group_id = 1;

    if(vector_size(tokens) == 0)
    {
        b = as_byte(RegexOpCode_Accept);
        vector_push_back(bytecode, &b);
        return true;
    }

    if(!regex_emit_alternation(tokens, &pos, &current_group_id, bytecode))
        return false;

    if(pos < vector_size(tokens))
    {
        g_current_error = ErrorCode_RegexUnexpectedTokens;
        logger_log_error("Unexpected tokens after bytecode emission");
        return false;
    }

    final_fail_jump = regex_emit_jump(bytecode, RegexOpCode_JumpNeq);
    regex_patch_jump(bytecode, final_fail_jump, JUMP_FAIL);

    b = as_byte(RegexOpCode_Accept);
    vector_push_back(bytecode, &b);

    b = as_byte(RegexOpCode_Fail);
    vector_push_back(bytecode, &b);

    i = 0;

    while(i < vector_size(bytecode))
    {
        instr = (*(byte*)vector_at(bytecode, i));

        switch(instr)
        {
            case RegexOpCode_JumpEq:
            case RegexOpCode_JumpNeq:
            {
                offset = regex_decode_jump((byte*)vector_at(bytecode, i + 1));

                if(offset == JUMP_FAIL)
                {
                    offset = (int)(vector_size(bytecode) - 1 - (i + 5));

                    jmp = regex_encode_jump(offset);

                    memcpy(vector_at(bytecode, i + 1), &(jmp.bytes[0]), 4 * sizeof(byte));
                }

                i += 5;
                break;
            }
            case RegexOpCode_Accept:
            case RegexOpCode_Fail:
            case RegexOpCode_TestAny:
            case RegexOpCode_TestDigit:
            case RegexOpCode_TestLowerCase:
            case RegexOpCode_TestUpperCase:
            case RegexOpCode_IncPos:
            case RegexOpCode_DecPos:
            case RegexOpCode_IncPosEq:
                i++;
                    break;

            case RegexOpCode_JumpPos:
                i += 5;
                break;

            case RegexOpCode_TestSingle:
            case RegexOpCode_SetFlag:
            case RegexOpCode_GroupStart:
            case RegexOpCode_GroupEnd:
                i += 2;
                break;

            case RegexOpCode_TestRange:
            case RegexOpCode_TestNegatedRange:
                i += 3;
                break;

            default:
            {
                g_current_error = ErrorCode_RegexUnknownOpCode;
                logger_log_error("Unknown opcode: %d", (int)instr);
                return false;
            }
        }
    }

    return true;
}

/**********/
/* Disasm */
/**********/

void regex_disasm(Vector* bytecode)
{
    byte* opcode;
    size_t i = 0;

    while(i < vector_size(bytecode))
    {
        opcode = (byte*)(vector_at(bytecode, i));

        switch(*opcode)
        {
            case RegexOpCode_TestSingle:
                logger_log_debug("TESTSINGLE %c",
                                 as_char(*(opcode + 1)));
                i += 2;
                break;
            case RegexOpCode_TestRange:
                logger_log_debug("TESTRANGE %c-%c",
                                 as_char(*(opcode + 1)),
                                 as_char(*(opcode + 2)));
                i += 3;
                break;
            case RegexOpCode_TestNegatedRange:
                logger_log_debug("TESTNEGRANGE %c-%c",
                                 as_char(*(opcode + 1)),
                                 as_char(*(opcode + 2)));
                i += 3;
                break;
            case RegexOpCode_TestAny:
                logger_log_debug("TESTANY");
                i++;
                break;
            case RegexOpCode_TestDigit:
                logger_log_debug("TESTDIGIT");
                i++;
                break;
            case RegexOpCode_TestLowerCase:
                logger_log_debug("TESTLOWERCASE");
                i++;
                break;
            case RegexOpCode_TestUpperCase:
                logger_log_debug("TESTUPPERCASE");
                i++;
                break;
            case RegexOpCode_JumpEq:
            case RegexOpCode_JumpNeq:
                    logger_log_debug("%s %s%d",
                                     opcode[i] == RegexOpCode_JumpEq ? "JUMPEQ" : "JUMPNEQ",
                                     regex_decode_jump(opcode + 1) >= 0 ? "+" : "",
                                     regex_decode_jump(opcode + 1));
                i += 5;
                break;
            case RegexOpCode_Accept:
                logger_log_debug("ACCEPT");
                i++;
                break;
            case RegexOpCode_Fail:
                logger_log_debug("FAIL");
                i++;
                break;
            case RegexOpCode_GroupStart:
                logger_log_debug("GROUPSTART %u", (uint32_t)*(opcode + 1));
                i += 2;
                break;
            case RegexOpCode_GroupEnd:
                logger_log_debug("GROUPEND %u", (uint32_t)*(opcode + 1));
                i += 2;
                break;
            case RegexOpCode_IncPos:
                logger_log_debug("INCPOS");
                i++;
                break;
            case RegexOpCode_DecPos:
                logger_log_debug("DECPOS");
                i++;
                break;
            case RegexOpCode_IncPosEq:
                logger_log_debug("INCPOSEQ");
                i++;
                break;
            case RegexOpCode_JumpPos:
                logger_log_debug("JUMPPOS %s%d",
                                 regex_decode_jump(opcode + 1) >= 0 ? "+" : "",
                                 regex_decode_jump(opcode + 1));
                i += 5;
                break;
            case RegexOpCode_SetFlag:
                logger_log_debug("SETFLAG %u",
                                 (uint32_t)*(opcode + 1));
                i += 2;
                break;
            default:
                logger_log_debug("UNKNOWN %u", (uint32_t)*opcode);
                i++;
                break;
        }
    }
}

/***************/
/* Compilation */
/***************/

struct Regex
{
    Vector bytecode;
};

Regex* regex_compile(const char* pattern, RegexFlags flags)
{
    Vector* tokens;

    Regex* regex = malloc(sizeof(Regex));

    if(regex == NULL)
    {
        g_current_error = ErrorCode_MemAllocError;
        free(regex);
        return NULL;
    }

    vector_init(&regex->bytecode, 128, sizeof(byte));

    if(flags & RegexFlags_DebugCompilation)
        logger_log_debug("Compiling regex: %s", pattern);

    tokens = regex_lex(pattern);

    if(tokens == NULL)
    {
        logger_log_error("Failed to lex regex: %s", pattern);
        regex_free(regex);
        return NULL;
    }

    if(flags & RegexFlags_DebugCompilation)
    {
        logger_log_debug("********");
        logger_log_debug("Regex tokens");
        regex_tokens_debug(tokens);
    }

    if(!regex_emit(tokens, &(regex->bytecode)))
    {
        logger_log_error("Failed to emit bytecode for regex: %s", pattern);
        vector_free(tokens);
        regex_free(regex);
        return NULL;
    }

    if(flags & RegexFlags_DebugCompilation)
    {
        logger_log_debug("********");
        logger_log_debug("Regex disasm:");
        regex_disasm(&(regex->bytecode));
    }

    vector_free(tokens);

    return regex;
}

/************/
/* Matching */
/************/

typedef struct RegexVM {
    const char* string;
    size_t string_sz;
    Vector* bytecode;
    size_t pc;
    size_t sp;
    bool status_flag;
} RegexVM;

bool regex_exec(RegexVM* vm)
{
    byte* opcode;
    int jmp;

    while(vm->pc < vector_size(vm->bytecode) && vm->sp < vm->string_sz)
    {
        opcode = (byte*)vector_at(vm->bytecode, vm->pc);

        switch(*opcode)
        {
            case RegexOpCode_TestSingle:
                vm->status_flag = vm->string[vm->sp] == as_char(opcode[1]);
                vm->pc += 2;
                break;
            case RegexOpCode_TestRange:
                vm->status_flag = vm->string[vm->sp] >= as_char(opcode[1]) &&
                                  vm->string[vm->sp] <= as_char(opcode[2]);
                vm->pc += 3;
                break;
            case RegexOpCode_TestNegatedRange:
                vm->status_flag = vm->string[vm->sp] < as_char(opcode[1]) &&
                                  vm->string[vm->sp] > as_char(opcode[2]);
                vm->pc += 3;
                break;
            case RegexOpCode_TestAny:
                vm->status_flag = true;
                vm->pc += 1;
                break;
            case RegexOpCode_TestDigit:
                vm->status_flag = isdigit(vm->string[vm->sp]);
                vm->pc += 1;
                break;
            case RegexOpCode_TestLowerCase:
                vm->status_flag = islower(vm->string[vm->sp]);
                vm->pc += 1;
                break;
            case RegexOpCode_TestUpperCase:
                vm->status_flag = isupper(vm->string[vm->sp]);
                vm->pc += 1;
                break;
            case RegexOpCode_JumpEq:
                if(vm->status_flag)
                {
                    jmp = regex_decode_jump(opcode + 1);

                    if(jmp > 0)
                    {
                        vm->pc += 5;
                    }

                    vm->pc += jmp;
                }
                else
                {
                    vm->pc += 5;
                }

                break;
            case RegexOpCode_JumpNeq:
                if(!vm->status_flag)
                {
                    jmp = regex_decode_jump(opcode + 1);

                    if(jmp > 0)
                    {
                        vm->pc += 5;
                    }

                    vm->pc += jmp;
                }
                else
                {
                    vm->pc += 5;
                }

                break;
            case RegexOpCode_Accept:
                return true;
            case RegexOpCode_Fail:
                return false;
            case RegexOpCode_GroupStart:
                vm->pc += 2;
                break;
            case RegexOpCode_GroupEnd:
                vm->pc += 2;
                break;
            case RegexOpCode_IncPos:
                vm->sp++;
                vm->pc++;
                break;
            case RegexOpCode_DecPos:
                vm->sp--;
                vm->pc++;
                break;
            case RegexOpCode_IncPosEq:
                vm->sp += (size_t)vm->status_flag;
                vm->pc++;
                break;
            case RegexOpCode_JumpPos:
                vm->sp += regex_decode_jump(opcode + 1);
                vm->pc += 5;
                break;
            case RegexOpCode_SetFlag:
                vm->status_flag = (bool)(*(opcode + 1));
                vm->pc += 2;
                break;
            default:
                logger_log_error("Unknown instruction in regex vm: %u (pc %zu)",
                                 (uint32_t)(*opcode),
                                 vm->pc);
                return false;
        }
    }

    return (bool)vm->status_flag;
}

bool regex_match(Regex* regex, const char* string)
{
    RegexVM vm;

    if(vector_size(&(regex->bytecode)) == 0)
        return false;

    vm.string = string;
    vm.string_sz = strlen(string);
    vm.bytecode = &regex->bytecode;
    vm.pc = 0;
    vm.sp = 0;
    vm.status_flag = false;

    return regex_exec(&vm);
}

/********/
/* Free */
/********/

void regex_free(Regex* regex)
{
    if(regex != NULL)
    {
        vector_release(&(regex->bytecode));

        free(regex);
    }
}
