/*
 * Sample dynamic sized record fifo implementation
 *
 * Copyright (C) 2010 Stefani Seibold <stefani@seibold.net>
 *
 * Released under the GPL version 2 only.
 *
 */

#include "kfifo.h"
#include <stdio.h>
#include <string.h>

/*
 * This module shows how to create a variable sized record fifo.
 */

/* fifo size in elements (bytes) */
#define FIFO_SIZE 128

/*
 * define DYNAMIC in this example for a dynamically allocated fifo.
 *
 * Otherwise the fifo storage will be a part of the fifo structure.
 */
#if 0
#define DYNAMIC
#endif

/*
 * struct kfifo_rec_ptr_1 and  STRUCT_KFIFO_REC_1 can handle records of a
 * length between 0 and 255 bytes.
 *
 * struct kfifo_rec_ptr_2 and  STRUCT_KFIFO_REC_2 can handle records of a
 * length between 0 and 65535 bytes.
 */

#ifdef DYNAMIC
struct kfifo_rec_ptr_1 test;

#else
typedef STRUCT_KFIFO_REC_1(FIFO_SIZE) mytest;

static mytest test;
#endif

static const char *expected_result[] = {
    "a", "bb", "ccc", "dddd", "eeeee", "ffffff", "ggggggg", "hhhhhhhh", "iiiiiiiii", "jjjjjjjjjj",
};

static int testfunc(void) {
    char buf[100];
    unsigned int i;
    unsigned int ret;
    struct {
        unsigned char buf[6];
    } hello = {"hello"};

    printf("record fifo test start\n");

    kfifo_in(&test, &hello, sizeof(hello));

    /* show the size of the next record in the fifo */
    printf("fifo peek len: %u\n", kfifo_peek_len(&test));

    /* put in variable length data */
    for (i = 0; i < 10; i++) {
        memset(buf, 'a' + i, i + 1);
        kfifo_in(&test, buf, i + 1);
    }

    /* skip first element of the fifo */
    printf("skip 1st element\n");
    kfifo_skip(&test);

    printf("fifo len: %u\n", kfifo_len(&test));

    /* show the first record without removing from the fifo */
    ret = kfifo_out_peek(&test, buf, sizeof(buf));
    if (ret)
        printf("%.*s\n", ret, buf);

    /* check the correctness of all values in the fifo */
    i = 0;
    while (!kfifo_is_empty(&test)) {
        ret = kfifo_out(&test, buf, sizeof(buf));
        buf[ret] = '\0';
        printf("item = %.*s\n", ret, buf);
        if (strcmp(buf, expected_result[i++])) {
            printf("value mismatch: test failed\n");
            return -EIO;
        }
    }
    if (i != ARRAY_SIZE(expected_result)) {
        printf("size mismatch: test failed\n");
        return -EIO;
    }
    printf("test passed\n");

    return 0;
}

int main(void) {
#ifdef DYNAMIC
    int ret;

    ret = kfifo_alloc(&test, FIFO_SIZE, 0);
    if (ret) {
        printf("error kfifo_alloc\n");
        return ret;
    }
#else
    INIT_KFIFO(test);
#endif
    if (testfunc() < 0) {
#ifdef DYNAMIC
        kfifo_free(&test);
#endif
        return -EIO;
    }

#ifdef DYNAMIC
    kfifo_free(&test);
#endif

    return 0;
}