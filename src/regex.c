/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

/* https://swtch.com/~rsc/regexp/regexp1.html */

#include "libromano/regex.h"
#include "libromano/memory.h"

#include <string.h>

/* Convert infix regexp to postfix notation with '.' as concatenation operator */
bool regex_to_postfix(const char *re_string, 
                      char* out_buffer,
                      size_t max_out_buffer_size) 
{
    int nalt, natom;
	char* dst;
    char* re;

	struct 
    {
		int nalt;
		int natom;
	} paren[100], *p;
	
	p = paren;
	dst = out_buffer;
	nalt = 0;
	natom = 0;
    re = (char*)re_string;

	if(strlen(re) >= (max_out_buffer_size / 2))
    {
		return false;
    }

	for(; *re; re++)
    {
		switch(*re)
        {
            case '(':
                if(natom > 1)
                {
                    --natom;
                    *dst++ = '.';
                }

                if(p >= paren+100)
                {
                    return false;
                }

                p->nalt = nalt;
                p->natom = natom;
                p++;
                nalt = 0;
                natom = 0;

                break;
            case '|':
                if(natom == 0)
                {
                    return false;
                }

                while(--natom > 0)
                {
                    *dst++ = '.';
                }

                nalt++;

                break;
            case ')':
                if(p == paren)
                {
                    return false;
                }

                if(natom == 0)
                {
                    return false;
                }

                while(--natom > 0)
                {
                    *dst++ = '.';
                }

                for(; nalt > 0; nalt--)
                {
                    *dst++ = '|';
                }

                --p;
                nalt = p->nalt;
                natom = p->natom;
                natom++;

                break;

            case '*':
            case '+':
            case '?':
                if(natom == 0)
                {
                    return false;
                }

                *dst++ = *re;

                break;
            default:
                if(natom > 1)
                {
                    --natom;
                    *dst++ = '.';
                }

                *dst++ = *re;
                natom++;

                break;
		}
	}

	if(p != paren)
    {
		return false;
    }

	while(--natom > 0)
    {
		*dst++ = '.';
    }

	for(; nalt > 0; nalt--)
    {
		*dst++ = '|';
    }

	*dst = 0;

	return true;
}

/* NFA state representation */
enum { Match = 256, Split = 257 };
typedef struct State State;
struct State {
    int c;
    State *out, *out1;
    int lastlist;
};

/* Allocate state and increment state count */
State* state(int c, State* out, State* out1, size_t* nstate) {
    State *s = malloc(sizeof(State));
    if (s) {
        (*nstate)++;
        s->c = c; s->out = out; s->out1 = out1; s->lastlist = 0;
    }
    return s;
}

/* Fragments and pointer lists for NFA construction */
typedef union Ptrlist Ptrlist;
union Ptrlist { Ptrlist *next; State *s; };

typedef struct Frag Frag;
struct Frag { State *start; Ptrlist *out; };

ROMANO_FORCE_INLINE Frag frag(State *start, Ptrlist *out) 
{
    Frag n = { start, out }; 
    return n;
}

/* Create and manipulate pointer lists */
Ptrlist* list1(State **outp) 
{
    Ptrlist *l = (Ptrlist*)outp;
    l->next = NULL; 

    return l;
}

void patch(Ptrlist *l, State *s) 
{
    for(Ptrlist *next; l; l = next) 
    {
        next = l->next; l->s = s;
    }
}

Ptrlist* append(Ptrlist *l1, Ptrlist *l2) 
{
    Ptrlist *oldl1 = l1;

    while(l1->next) 
    {
        l1 = l1->next;
    }

    l1->next = l2; 

    return oldl1;
}

/* Convert postfix to NFA, returning start state and matchstate */
State* postfix_to_nfa(char *postfix, size_t *nstate, State **matchstate) 
{
    Frag stack[1000], *stackp = stack;
    *nstate = 0;

    *matchstate = malloc(sizeof(State));
    if (!*matchstate) return NULL;
    (*matchstate)->c = Match;
    (*matchstate)->out = (*matchstate)->out1 = NULL;
    (*matchstate)->lastlist = 0;

    for (char *p = postfix; *p; p++) {
        switch (*p) {
            default: { // Literal
                State *s = state(*p, NULL, NULL, nstate);
                if (!s) { free(*matchstate); return NULL; }
                *stackp++ = frag(s, list1(&s->out));
                break;
            }
            case '.': // Concatenate
                if (stackp - stack < 2) { free(*matchstate); return NULL; }
                Frag e2 = *--stackp, e1 = *--stackp;
                patch(e1.out, e2.start);
                *stackp++ = frag(e1.start, e2.out);
                break;
            case '|': // Alternate
                if (stackp - stack < 2) { free(*matchstate); return NULL; }
                e2 = *--stackp; e1 = *--stackp;
                State *s = state(Split, e1.start, e2.start, nstate);
                if (!s) { free(*matchstate); return NULL; }
                *stackp++ = frag(s, append(e1.out, e2.out));
                break;
            case '?': // Zero or one
                if (stackp - stack < 1) { free(*matchstate); return NULL; }
                Frag e = *--stackp;
                s = state(Split, e.start, NULL, nstate);
                if (!s) { free(*matchstate); return NULL; }
                *stackp++ = frag(s, append(e.out, list1(&s->out1)));
                break;
            case '*': // Zero or more
                if (stackp - stack < 1) { free(*matchstate); return NULL; }
                e = *--stackp;
                s = state(Split, e.start, NULL, nstate);
                if (!s) { free(*matchstate); return NULL; }
                patch(e.out, s);
                *stackp++ = frag(s, list1(&s->out1));
                break;
            case '+': // One or more
                if (stackp - stack < 1) { free(*matchstate); return NULL; }
                e = *--stackp;
                s = state(Split, e.start, NULL, nstate);
                if (!s) { free(*matchstate); return NULL; }
                patch(e.out, s);
                *stackp++ = frag(e.start, list1(&s->out1));
                break;
        }
    }

    if (stackp != stack + 1) { free(*matchstate); return NULL; }
    Frag e = *--stackp;
    patch(e.out, *matchstate);
    return e.start;
}

/* State lists for NFA simulation */
typedef struct List List;
struct List { State **s; int n; };

void add_state(List *l, State *s, int listid)
{
    if(!s || s->lastlist == listid) 
    {
        return;
    }

    s->lastlist = listid;

    if(s->c == Split) 
    {
        add_state(l, s->out, listid);
        add_state(l, s->out1, listid);
    } 
    else 
    {
        l->s[l->n++] = s;
    }
}

List* start_list(State *start, List *l, int *listid) 
{
    (*listid)++;

    l->n = 0;

    add_state(l, start, *listid);

    return l;
}

void step(List *clist, int c, List *nlist, int *listid) 
{
    (*listid)++;

    nlist->n = 0;

    for(int i = 0; i < clist->n; i++) 
    {
        State *s = clist->s[i];

        if(s->c == c) 
        {
            add_state(nlist, s->out, *listid);
        }
    }
}

bool is_match(List *l, State *matchstate) 
{
    for(int i = 0; i < l->n; i++)
    {
        if(l->s[i] == matchstate) 
        {
            return true;
        }
    }

    return false;
}

bool match(State *start, char *s, List *l1, List *l2, State *matchstate) 
{
    int listid = 0;

    List *clist = start_list(start, l1, &listid), *nlist = l2, *t;

    for (; *s; s++) 
    {
        step(clist, *s & 0xFF, nlist, &listid);
        t = clist; clist = nlist; nlist = t;
    }

    return is_match(clist, matchstate);
}

struct _Regex
{
    State* start_state;
    State* match_state;
    size_t n_states;
};

Regex* regex_compile(const char* pattern)
{
    char postfix[8000];
    size_t postfix_size = 8000;

    if(!regex_to_postfix(pattern, postfix, postfix_size))
    {
        return NULL;
    }

    Regex* regex = malloc(sizeof(Regex));

    regex->n_states = 0;
    regex->match_state = NULL;
    regex->start_state = postfix_to_nfa(postfix, &regex->n_states, &regex->match_state);

    if(regex->start_state == NULL)
    {
        free(regex);
        return NULL;
    }

    return regex;
}

bool regex_match(Regex* regex, const char* string)
{
    List l1, l2;

    l1.s = alloca(regex->n_states * sizeof(State*));
    l2.s = alloca(regex->n_states * sizeof(State*));

    return match(regex->start_state, (char*)string, &l1, &l2, regex->match_state);
}

void regex_destroy(Regex* regex)
{
    if(regex != NULL)
    {
        /* TODO: Free regex states */

        free(regex);
    }
}