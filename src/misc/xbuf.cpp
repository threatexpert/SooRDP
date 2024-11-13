#include "pch.h"
#include <stdlib.h>
#include <string.h>
#include "xbuf.h"


xbuf* xbuf_create(int capacity)
{
    xbuf* p = (xbuf*)malloc(sizeof(xbuf) + capacity);
    memset(p, 0, sizeof(xbuf) + capacity);
    p->capacity = capacity;
    return p;
}

void xbuf_free(xbuf* x)
{
    free(x);
}

bool xbuf_append(xbuf* x, const char* data, int len)
{
    int bufavail = x->capacity - x->datalen;
    if (bufavail < len) {
        return false;
    }
    bufavail = x->capacity - x->datapos - x->datalen;
    if (bufavail < len) {
        memmove(x->data, x->data + x->datapos, x->datalen);
        x->datapos = 0;
        bufavail = x->capacity - x->datalen;
    }

    memcpy(x->data + x->datapos + x->datalen, data, len);
    x->datalen += len;
    return true;
}

void xbuf_appended(xbuf* x, int len)
{
    x->datalen += len;
}

int xbuf_avail(xbuf* x)
{
    return x->capacity - x->datapos - x->datalen;
}

bool xbuf_ensureavail(xbuf* x, int len)
{
    if (x->capacity - x->datapos - x->datalen >= len)
        return true;
    else if (x->capacity - x->datalen >= len) {
        xbuf_rebase(x);
        return true;
    }else
        return false;
}

char* xbuf_data(xbuf* x)
{
    return x->data + x->datapos;
}

char* xbuf_datatail(xbuf* x)
{
    return x->data + x->datapos + x->datalen;
}

void xbuf_pos_forward(xbuf* x, int n)
{
    x->datapos += n;
    x->datalen -= n;
    if (x->datalen == 0)
        x->datapos = 0;
}

void xbuf_rebase(xbuf* x)
{
    if (x->datapos) {
        memmove(x->data, x->data + x->datapos, x->datalen);
        x->datapos = 0;
    }
}
