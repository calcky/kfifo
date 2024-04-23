/*
 * A generic kernel FIFO implementation
 *
 * Copyright (C) 2009/2010 Stefani Seibold <stefani@seibold.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#include "kfifo.h"

#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#define kmalloc(size, mask) malloc(size)
#define kfree(ptr)          free(ptr)
#define EXPORT_SYMBOL(sym)
#define min(x, y)                                                                                                      \
    ({                                                                                                                 \
        typeof(x) _x = (x);                                                                                            \
        typeof(y) _y = (y);                                                                                            \
        (void)(&_x == &_y);                                                                                            \
        _x < _y ? _x : _y;                                                                                             \
    })

/*
 * internal helper to calculate the unused elements in a fifo
 */
static inline unsigned int kfifo_unused(struct __kfifo *fifo) {
    return (fifo->mask + 1) - (fifo->in - fifo->out);
}

static __inline__ int fls(int x) {
    int r = 32;

    if (!x)
        return 0;
    if (!(x & 0xffff0000u)) {
        x <<= 16;
        r -= 16;
    }
    if (!(x & 0xff000000u)) {
        x <<= 8;
        r -= 8;
    }
    if (!(x & 0xf0000000u)) {
        x <<= 4;
        r -= 4;
    }
    if (!(x & 0xc0000000u)) {
        x <<= 2;
        r -= 2;
    }
    if (!(x & 0x80000000u)) {
        x <<= 1;
        r -= 1;
    }
    return r;
}

static inline unsigned long __attribute_const__ roundup_pow_of_two(unsigned long x) {
    return (1UL << fls(x - 1));
}

static inline unsigned long __attribute_const__ rounddown_pow_of_two(unsigned long x) {
    return (1UL << (fls(x) - 1));
}

static int is_power_of_2(int x) {
    return (x > 0) && ((x & (x - 1)) == 0);
}

int __kfifo_alloc(struct __kfifo *fifo, unsigned int size, size_t esize, gfp_t gfp_mask) {
    /*
     * round down to the next power of 2, since our 'let the indices
     * wrap' technique works only in this case.
     */
    size = roundup_pow_of_two(size);

    fifo->in = 0;
    fifo->out = 0;
    fifo->esize = esize;

    if (size < 2) {
        fifo->data = NULL;
        fifo->mask = 0;
        return -EINVAL;
    }

    fifo->data = kmalloc(size * esize, gfp_mask);

    if (!fifo->data) {
        fifo->mask = 0;
        return -ENOMEM;
    }
    fifo->mask = size - 1;

    return 0;
}
EXPORT_SYMBOL(__kfifo_alloc);

void __kfifo_free(struct __kfifo *fifo) {
    kfree(fifo->data);
    fifo->in = 0;
    fifo->out = 0;
    fifo->esize = 0;
    fifo->data = NULL;
    fifo->mask = 0;
}
EXPORT_SYMBOL(__kfifo_free);

int __kfifo_init(struct __kfifo *fifo, void *buffer, unsigned int size, size_t esize) {
    size /= esize;

    if (!is_power_of_2(size))
        size = rounddown_pow_of_two(size);

    fifo->in = 0;
    fifo->out = 0;
    fifo->esize = esize;
    fifo->data = buffer;

    if (size < 2) {
        fifo->mask = 0;
        return -EINVAL;
    }
    fifo->mask = size - 1;

    return 0;
}
EXPORT_SYMBOL(__kfifo_init);

static void kfifo_copy_in(struct __kfifo *fifo, const void *src, unsigned int len, unsigned int off) {
    unsigned int size = fifo->mask + 1;
    unsigned int esize = fifo->esize;
    unsigned int l;

    off &= fifo->mask;
    if (esize != 1) {
        off *= esize;
        size *= esize;
        len *= esize;
    }
    l = min(len, size - off);

    memcpy(fifo->data + off, src, l);
    memcpy(fifo->data, src + l, len - l);
    /*
     * make sure that the data in the fifo is up to date before
     * incrementing the fifo->in index counter
     */
    smp_wmb();
}

unsigned int __kfifo_in(struct __kfifo *fifo, const void *buf, unsigned int len) {
    unsigned int l;

    l = kfifo_unused(fifo);
    if (len > l)
        len = l;

    kfifo_copy_in(fifo, buf, len, fifo->in);
    fifo->in += len;
    return len;
}
EXPORT_SYMBOL(__kfifo_in);

static void kfifo_copy_out(struct __kfifo *fifo, void *dst, unsigned int len, unsigned int off) {
    unsigned int size = fifo->mask + 1;
    unsigned int esize = fifo->esize;
    unsigned int l;

    off &= fifo->mask;
    if (esize != 1) {
        off *= esize;
        size *= esize;
        len *= esize;
    }
    l = min(len, size - off);

    memcpy(dst, fifo->data + off, l);
    memcpy(dst + l, fifo->data, len - l);
    /*
     * make sure that the data is copied before
     * incrementing the fifo->out index counter
     */
    smp_wmb();
}

unsigned int __kfifo_out_peek(struct __kfifo *fifo, void *buf, unsigned int len) {
    unsigned int l;

    l = fifo->in - fifo->out;
    if (len > l)
        len = l;

    kfifo_copy_out(fifo, buf, len, fifo->out);
    return len;
}
EXPORT_SYMBOL(__kfifo_out_peek);

unsigned int __kfifo_out(struct __kfifo *fifo, void *buf, unsigned int len) {
    len = __kfifo_out_peek(fifo, buf, len);
    fifo->out += len;
    return len;
}
EXPORT_SYMBOL(__kfifo_out);

unsigned int __kfifo_max_r(unsigned int len, size_t recsize) {
    unsigned int max = (1 << (recsize << 3)) - 1;

    if (len > max)
        return max;
    return len;
}
EXPORT_SYMBOL(__kfifo_max_r);

#define __KFIFO_PEEK(data, out, mask) ((data)[(out) & (mask)])
/*
 * __kfifo_peek_n internal helper function for determinate the length of
 * the next record in the fifo
 */
static unsigned int __kfifo_peek_n(struct __kfifo *fifo, size_t recsize) {
    unsigned int l;
    unsigned int mask = fifo->mask;
    unsigned char *data = fifo->data;

    l = __KFIFO_PEEK(data, fifo->out, mask);

    if (--recsize)
        l |= __KFIFO_PEEK(data, fifo->out + 1, mask) << 8;

    return l;
}

#define __KFIFO_POKE(data, in, mask, val) ((data)[(in) & (mask)] = (unsigned char)(val))

/*
 * __kfifo_poke_n internal helper function for storeing the length of
 * the record into the fifo
 */
static void __kfifo_poke_n(struct __kfifo *fifo, unsigned int n, size_t recsize) {
    unsigned int mask = fifo->mask;
    unsigned char *data = fifo->data;

    __KFIFO_POKE(data, fifo->in, mask, n);

    if (recsize > 1)
        __KFIFO_POKE(data, fifo->in + 1, mask, n >> 8);
}

unsigned int __kfifo_len_r(struct __kfifo *fifo, size_t recsize) {
    return __kfifo_peek_n(fifo, recsize);
}
EXPORT_SYMBOL(__kfifo_len_r);

unsigned int __kfifo_in_r(struct __kfifo *fifo, const void *buf, unsigned int len, size_t recsize) {
    if (len + recsize > kfifo_unused(fifo))
        return 0;

    __kfifo_poke_n(fifo, len, recsize);

    kfifo_copy_in(fifo, buf, len, fifo->in + recsize);
    fifo->in += len + recsize;
    return len;
}
EXPORT_SYMBOL(__kfifo_in_r);

static unsigned int kfifo_out_copy_r(struct __kfifo *fifo, void *buf, unsigned int len, size_t recsize,
                                     unsigned int *n) {
    *n = __kfifo_peek_n(fifo, recsize);

    if (len > *n)
        len = *n;

    kfifo_copy_out(fifo, buf, len, fifo->out + recsize);
    return len;
}

unsigned int __kfifo_out_peek_r(struct __kfifo *fifo, void *buf, unsigned int len, size_t recsize) {
    unsigned int n;

    if (fifo->in == fifo->out)
        return 0;

    return kfifo_out_copy_r(fifo, buf, len, recsize, &n);
}
EXPORT_SYMBOL(__kfifo_out_peek_r);

unsigned int __kfifo_out_r(struct __kfifo *fifo, void *buf, unsigned int len, size_t recsize) {
    unsigned int n;

    if (fifo->in == fifo->out)
        return 0;

    len = kfifo_out_copy_r(fifo, buf, len, recsize, &n);
    fifo->out += n + recsize;
    return len;
}
EXPORT_SYMBOL(__kfifo_out_r);

void __kfifo_skip_r(struct __kfifo *fifo, size_t recsize) {
    unsigned int n;

    n = __kfifo_peek_n(fifo, recsize);
    fifo->out += n + recsize;
}
EXPORT_SYMBOL(__kfifo_skip_r);