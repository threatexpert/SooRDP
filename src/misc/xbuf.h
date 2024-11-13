#pragma once


struct xbuf {
    int datalen;
    int datapos;
    int capacity;
    char data[1];
};
xbuf* xbuf_create(int capacity);
void xbuf_free(xbuf* x);
bool xbuf_append(xbuf* x, const char* data, int len);
void xbuf_appended(xbuf* x, int len);
int xbuf_avail(xbuf* x);
bool xbuf_ensureavail(xbuf* x, int len);
char* xbuf_data(xbuf* x);
char* xbuf_datatail(xbuf* x);
void xbuf_pos_forward(xbuf* x, int n);
void xbuf_rebase(xbuf* x);
