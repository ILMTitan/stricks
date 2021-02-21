/*
Stricks v0.1.0
Copyright (C) 2021 - Francois Alcover <francois@alcover.fr>
NO WARRANTY EXPRESSED OR IMPLIED.
*/

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdarg.h>
#include <stddef.h>
#include <string.h>
#include <assert.h>
#include <errno.h>

#include "stx.h"
#include "log.h"
#include "util.c"


#define TYPE_SHORT  8
#define TYPE_LONG   64 

#define BLOCK_SZ(T,cap) (sizeof(Head##T) + (cap) + 1)
#define HEAD(s,T) ((s) - offsetof(Head##T,data))
#define FIELD_ADR(s,htype,name) ((char*)(s) - offsetof(htype,data) + offsetof(htype,name))
#define FIELD_VAL(s,htype,name) (*FIELD_ADR((s),htype,name))
#define CHECK(s) (s && (((char*)s)[-2] == COOKIE))

#define COOKIE 147

struct Head64 {   
    size_t  cap;  
    size_t  len; 
    char cookie; 
    char type;
    char data[]; 
};

typedef struct Head64 Head64;

struct Head8 {   
    uint8_t cap;  
    uint8_t len; 
    char cookie; 
    char type;
    char data[]; 
};

typedef struct Head8 Head8;

//==== PRIVATE ================================================================

void* headof(const stx_t s)
{
    char type = s[-1];
    int off;

    if (type == TYPE_SHORT) {
        off = offsetof(Head8, data);
    } else {
        off = offsetof(Head64, data);
    }

    return s - off;
}

// duck-validate
static inline bool
check (const void* head)
{
    return head && (((char*)head)[-2] == COOKIE);
}


static bool 
resize (void **phead, const size_t newcap)
{    
    // negative value - abort
    if ((intmax_t)newcap < 0) {
        #ifdef STX_WARNINGS
            ERR("stx_resize: negative value\n");
        #endif
        return false;
    }


    void* head = *phead;
    
    if (newcap == head->cap) 
        return true;
    
    void* tmp = realloc (head, BLOCK_SZ(newcap));    

    if (!tmp) {
        ERR ("stx_resize: realloc failed\n");
        return false;
    }
    
    // truncated
    if (newcap < tmp->len) {
        #ifdef STX_WARNINGS
            LOG ("stx_resize: truncated");
        #endif
        tmp->len = newcap;
    }

    // set new cap sentinel
    tmp->data[newcap] = 0;
    // update cap
    tmp->cap = newcap;
    *phead = tmp;
    
    return true;
}


static int 
append_count (void* dst, const char* src, const size_t n) 
{
    if (!src) 
        return STX_FAIL;

    const size_t dst_len = dst->len;
    char* dst_end = dst->data + dst_len;
    const size_t inc_len = n ? strnlen(src,n) : strlen(src);

    // Would truncate - return total needed capacity
    if (inc_len > dst->cap - dst_len)
        return -(dst_len + inc_len);
    
    memcpy (dst_end, src, inc_len);
    *(dst_end + inc_len) = 0;
    dst->len += inc_len;

    return inc_len;            
} 

//==== PUBLIC ================================================================

#define INIT(head, cap, T) \
    ((Head##T*)head)->cap = cap; \
    ((Head##T*)head)->cookie = COOKIE; \
    ((Head##T*)head)->type = T; \
    ((Head##T*)head)->data[0] = 0; \
    ((Head##T*)head)->data[cap] = 0

const stx_t 
stx_new (const size_t cap)
{
    void* head;

    if (cap < 256) {
        Head8* head = STX_MALLOC (BLOCK_SZ(8,cap));
        if (!head) return NULL;
        INIT (head, cap, 8);
        return head->data;
    } else {
        Head64* head = STX_MALLOC (BLOCK_SZ(64,cap));
        if (!head) return NULL;
        INIT (head, cap, 64);
        return head->data;
    }
}


void 
stx_free (const stx_t s)
{
    if (!CHECK(s)) {
        ERR ("stx_free: invalid header\n");
        return;
    }

    char type = s[-1];
    void* head = headof(s);
    
    if (type == TYPE_SHORT) {
        *(Head8*)head = (Head8){0};
    } else {
        *(Head64*)head = (Head64){0};
    }

    STX_FREE(head);
}


bool
stx_resize (stx_t *pstx, const size_t newcap)
{
    void* head = headof(*pstx);

    if (!check(head)) 
        return false;

    bool resized = resize(&head, newcap);

    *pstx = head->data;

    return resized;
}


int 
stx_append_count (const stx_t dst, const char* src, const size_t n) 
{
    void* head = headof(dst);

    if (!check(head)) 
        return STX_FAIL;

    return append_count (head, src, n);            
}

size_t 
stx_cap (const stx_t s)
{
    size_t ret;
    char type = s[-1];
    
    if (type == TYPE_SHORT) {
        ret = *((uint8_t*)(s - offsetof(Head8,data) + offsetof(Head8,cap)));
    } else {
        ret = *((size_t*)(s - offsetof(Head64,data) + offsetof(Head64,cap)));
    }

    return ret;
}

size_t 
stx_len (const stx_t s)
{
    size_t ret;
    char type = s[-1];

    if (type == TYPE_SHORT) {
        ret = *((uint8_t*)(s - offsetof(Head8,data) + offsetof(Head8,len)));
    } else {
        ret = *((size_t*)(s - offsetof(Head64,data) + offsetof(Head64,len)));
    }

    return ret;
}


void 
stx_show (const stx_t s)
{
    const Head64* head = HEAD(s);

    if (!check(head)) {
        ERR ("stx_show: invalid header\n");
        return;
    }

    printf ("cap:%zu len:%zu data:'%s'\n",
        (size_t)head->cap, (size_t)head->len, head->data);

    fflush(stdout);
}