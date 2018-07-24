/*
 * test-ringbuf.c - unit tests for C ring buffer implementation.
 *
 * Written in 2011 by Drew Hess <dhess-src@bothan.net>.
 *
 * To the extent possible under law, the author(s) have dedicated all
 * copyright and related and neighboring rights to this software to
 * the public domain worldwide. This software is distributed without
 * any warranty.
 *
 * You should have received a copy of the CC0 Public Domain Dedication
 * along with this software. If not, see
 * <http://creativecommons.org/publicdomain/zero/1.0/>.
 */

#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <signal.h>
#include <assert.h>
#include "ringbuf.h"

/*
 * Fill a buffer with a test pattern.
 */
void *
fill_buffer(void *buf, size_t buf_size, const char *test_pattern)
{
    size_t pattern_size = strlen(test_pattern);
    size_t nblocks = buf_size / pattern_size;
    uint8_t *p = buf;
    size_t n;
    for (n = 0; n != nblocks; ++n, p += pattern_size)
        memcpy(p, (const void *) test_pattern, pattern_size);
    memcpy(p, (const void *) test_pattern, buf_size % pattern_size);
    
    return buf;
}

int rdfd = -1;
int wrfd = -1;
char rd_template[] = "/tmp/tmpXXXXXXringbuf";
char wr_template[] = "/tmp/tmpXXXXXXringbuf-wr";

void
cleanup()
{
    if (rdfd != -1) {
        close(rdfd);
        unlink(rd_template);
    }
    if (wrfd != -1) {
        close(wrfd);
        unlink(wr_template);
    }
}

void
sigabort(int unused)
{
    cleanup();
    exit(1);
}

#define START_NEW_TEST(test_num) \
    fprintf(stderr, "Test %d...", (++test_num));

#define END_TEST(test_num) \
    fprintf(stderr, "pass.\n");

/* Default size for these tests. */
#define RINGBUF_SIZE 4096

int
main(int argc, char **argv)
{
    if (atexit(cleanup) == -1) {
        fprintf(stderr, "Can't install atexit handler, exiting.\n");
        exit(98);
    }
    
    /*
     * catch SIGABRT when asserts fail for proper test file
     * cleanup.
     */
    struct sigaction sa, osa;
    sa.sa_handler = sigabort;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    if (sigaction(SIGABRT, &sa, &osa) == -1) {
        fprintf(stderr, "Can't install SIGABRT handler, exiting.\n");
        exit(99);
    }

    ringbuf_t rb1 = ringbufNew(RINGBUF_SIZE - 1);

    int test_num = 0;

    /*
     * N.B.: these tests check both the ringbuf_t interface *and* a
     * particular implementation. They are not black-box tests. If you
     * make changes to the ringbuf_t implementation, some of these
     * tests may break.
     */
    
    /* We use the base pointer for some implementation testing. */
       
    const uint8_t *rb1_base = ringbufHead(rb1);

    /* Initial conditions */
    START_NEW_TEST(test_num);
    assert(ringbufBufferSize(rb1) == RINGBUF_SIZE);
    assert(ringbufCapacity(rb1) == RINGBUF_SIZE - 1);
    assert(ringbufBytesFree(rb1) == ringbufCapacity(rb1));
    assert(ringbufBytesUsed(rb1) == 0);
    assert(!ringbufIsFull(rb1));
    assert(ringbufIsEmpty(rb1));
    assert(ringbufTail(rb1) == ringbufHead(rb1));
    assert(ringbufTail(rb1) == rb1_base);
    END_TEST(test_num);

    /* freeing a ring buffer sets the pointer to 0 */
    START_NEW_TEST(test_num);
    ringbufFree(&rb1);
    assert(!rb1);
    END_TEST(test_num);

    /* Different sizes */
    rb1 = ringbufNew(24);
    rb1_base = ringbufHead(rb1);

    START_NEW_TEST(test_num);
    assert(ringbufBufferSize(rb1) == 25);
    assert(ringbufCapacity(rb1) == 24);
    assert(ringbufBytesFree(rb1) == ringbufCapacity(rb1));
    assert(ringbufBytesUsed(rb1) == 0);
    assert(!ringbufIsFull(rb1));
    assert(ringbufIsEmpty(rb1));
    assert(ringbufTail(rb1) == ringbufHead(rb1));
    assert(ringbufTail(rb1) == rb1_base);
    ringbufFree(&rb1);
    assert(!rb1);
    END_TEST(test_num);

    rb1 = ringbufNew(RINGBUF_SIZE - 1);
    rb1_base = ringbufHead(rb1);

    /* ringbufReset tests */
    START_NEW_TEST(test_num);
    ringbufMemset(rb1, 1, 8);
    ringbufReset(rb1);
    assert(ringbufBufferSize(rb1) == RINGBUF_SIZE);
    assert(ringbufCapacity(rb1) == RINGBUF_SIZE - 1);
    assert(ringbufBytesFree(rb1) == ringbufCapacity(rb1));
    assert(ringbufBytesUsed(rb1) == 0);
    assert(!ringbufIsFull(rb1));
    assert(ringbufIsEmpty(rb1));
    assert(ringbufTail(rb1) == ringbufHead(rb1));
    assert(ringbufTail(rb1) == rb1_base);
    END_TEST(test_num);

    START_NEW_TEST(test_num);
    ringbufMemset(rb1, 1, ringbufBufferSize(rb1)); /* overflow */
    ringbufReset(rb1);
    assert(ringbufBufferSize(rb1) == RINGBUF_SIZE);
    assert(ringbufCapacity(rb1) == RINGBUF_SIZE - 1);
    assert(ringbufBytesFree(rb1) == ringbufCapacity(rb1));
    assert(ringbufBytesUsed(rb1) == 0);
    assert(!ringbufIsFull(rb1));
    assert(ringbufIsEmpty(rb1));
    assert(ringbufTail(rb1) == ringbufHead(rb1));
    assert(ringbufTail(rb1) == rb1_base);
    END_TEST(test_num);

    /* ringbufMemset with zero count */
    START_NEW_TEST(test_num);
    ringbufReset(rb1);
    assert(ringbufMemset(rb1, 1, 0) == 0);
    assert(ringbufCapacity(rb1) == RINGBUF_SIZE - 1);
    assert(ringbufBytesFree(rb1) == ringbufCapacity(rb1));
    assert(ringbufBytesUsed(rb1) == 0);
    assert(!ringbufIsFull(rb1));
    assert(ringbufIsEmpty(rb1));
    assert(ringbufTail(rb1) == ringbufHead(rb1));
    END_TEST(test_num);

    uint8_t *buf = malloc(RINGBUF_SIZE * 2);
    memset(buf, 57, RINGBUF_SIZE);
    memset(buf + RINGBUF_SIZE, 58, RINGBUF_SIZE);
    
    /* ringbufMemset a few bytes of data */
    START_NEW_TEST(test_num);
    ringbufReset(rb1);
    assert(ringbufMemset(rb1, 57, 7) == 7);
    assert(ringbufCapacity(rb1) == RINGBUF_SIZE - 1);
    assert(ringbufBytesFree(rb1) == ringbufCapacity(rb1) - 7);
    assert(ringbufBytesUsed(rb1) == 7);
    assert(!ringbufIsFull(rb1));
    assert(!ringbufIsEmpty(rb1));
    assert(strncmp((const char *) buf, ringbufTail(rb1), 7) == 0);
    END_TEST(test_num);

    /* ringbufMemset full capacity */
    START_NEW_TEST(test_num);
    ringbufReset(rb1);
    assert(ringbufMemset(rb1, 57, RINGBUF_SIZE - 1) == RINGBUF_SIZE - 1);
    assert(ringbufCapacity(rb1) == RINGBUF_SIZE - 1);
    assert(ringbufBytesFree(rb1) == 0);
    assert(ringbufBytesUsed(rb1) == ringbufCapacity(rb1));
    assert(ringbufIsFull(rb1));
    assert(!ringbufIsEmpty(rb1));
    assert(strncmp(ringbufTail(rb1), (const char *) buf, RINGBUF_SIZE - 1) == 0);
    END_TEST(test_num);
    
    /* ringbufMemset, twice */
    START_NEW_TEST(test_num);
    ringbufReset(rb1);
    assert(ringbufMemset(rb1, 57, 7) == 7);
    assert(ringbufMemset(rb1, 57, 15) == 15);
    assert(ringbufCapacity(rb1) == RINGBUF_SIZE - 1);
    assert(ringbufBytesUsed(rb1) == 7 + 15);
    assert(ringbufBytesFree(rb1) == ringbufCapacity(rb1) - (7 + 15));
    assert(!ringbufIsFull(rb1));
    assert(!ringbufIsEmpty(rb1));
    assert(strncmp((const char *) buf, ringbufTail(rb1), 7 + 15) == 0);
    END_TEST(test_num);

    /* ringbufMemset, twice (to full capacity) */
    START_NEW_TEST(test_num);
    ringbufReset(rb1);
    assert(ringbufMemset(rb1, 57, RINGBUF_SIZE - 2) == RINGBUF_SIZE - 2);
    assert(ringbufMemset(rb1, 57, 1) == 1);
    assert(ringbufCapacity(rb1) == RINGBUF_SIZE - 1);
    assert(ringbufBytesFree(rb1) == 0);
    assert(ringbufBytesUsed(rb1) == RINGBUF_SIZE - 1);
    assert(ringbufIsFull(rb1));
    assert(!ringbufIsEmpty(rb1));
    assert(strncmp((const char *) buf, ringbufTail(rb1), RINGBUF_SIZE - 1) == 0);
    END_TEST(test_num);

    /* ringbufMemset, overflow by 1 byte */
    START_NEW_TEST(test_num);
    ringbufReset(rb1);
    assert(ringbufMemset(rb1, 57, RINGBUF_SIZE) == RINGBUF_SIZE);
    assert(ringbufCapacity(rb1) == RINGBUF_SIZE - 1);
    assert(ringbufBytesFree(rb1) == 0);
    assert(ringbufBytesUsed(rb1) == ringbufCapacity(rb1));
    assert(ringbufIsFull(rb1));
    assert(!ringbufIsEmpty(rb1));
    /* head should point to the beginning of the buffer */
    assert(ringbufHead(rb1) == rb1_base);
    /* tail should have bumped forward by 1 byte */
    assert(ringbufTail(rb1) == rb1_base + 1);
    assert(strncmp(ringbufTail(rb1), (const char *) buf, RINGBUF_SIZE - 1) == 0);
    END_TEST(test_num);

    /* ringbufMemset, twice (overflow by 1 byte on 2nd copy) */
    START_NEW_TEST(test_num);
    ringbufReset(rb1);
    assert(ringbufMemset(rb1, 57, RINGBUF_SIZE - 1) == RINGBUF_SIZE - 1);
    assert(ringbufMemset(rb1, 57, 1) == 1);
    assert(ringbufCapacity(rb1) == RINGBUF_SIZE - 1);
    assert(ringbufBytesFree(rb1) == 0);
    assert(ringbufBytesUsed(rb1) == ringbufCapacity(rb1));
    assert(ringbufIsFull(rb1));
    assert(!ringbufIsEmpty(rb1));
    /* head should point to the beginning of the buffer */
    assert(ringbufHead(rb1) == rb1_base);
    /* tail should have bumped forward by 1 byte */
    assert(ringbufTail(rb1) == rb1_base + 1);
    assert(strncmp(ringbufTail(rb1), (const char *) buf, RINGBUF_SIZE - 1) == 0);
    END_TEST(test_num);

    /*
     * ringbufMemset, attempt to overflow by 2 bytes, but
     * ringbufMemset will stop at 1 byte overflow (length clamping,
     * see ringbufMemset documentation).
     */
    START_NEW_TEST(test_num);
    ringbufReset(rb1);
    assert(ringbufMemset(rb1, 57, RINGBUF_SIZE + 1) == RINGBUF_SIZE);
    assert(ringbufCapacity(rb1) == RINGBUF_SIZE - 1);
    assert(ringbufBytesFree(rb1) == 0);
    assert(ringbufBytesUsed(rb1) == ringbufCapacity(rb1));
    assert(ringbufIsFull(rb1));
    assert(!ringbufIsEmpty(rb1));
    assert(ringbufHead(rb1) == rb1_base);
    assert(ringbufTail(rb1) == rb1_base + 1);
    assert(strncmp(ringbufTail(rb1), (const char *) buf, RINGBUF_SIZE - 1) == 0);
    END_TEST(test_num);
    
    /*
     * ringbufMemset, twice, overflowing both times.
     */
    START_NEW_TEST(test_num);
    ringbufReset(rb1);
    assert(ringbufMemset(rb1, 57, RINGBUF_SIZE) == RINGBUF_SIZE);
    assert(ringbufMemset(rb1, 58, RINGBUF_SIZE) == RINGBUF_SIZE);
    assert(ringbufCapacity(rb1) == RINGBUF_SIZE - 1);
    assert(ringbufBytesFree(rb1) == 0);
    assert(ringbufBytesUsed(rb1) == ringbufCapacity(rb1));
    assert(ringbufIsFull(rb1));
    assert(!ringbufIsEmpty(rb1));
    assert(ringbufHead(rb1) == rb1_base);
    assert(ringbufTail(rb1) == rb1_base + 1);
    assert(strncmp(ringbufTail(rb1), (const char *) buf + RINGBUF_SIZE, RINGBUF_SIZE - 1) == 0);
    END_TEST(test_num);

    /*
     * The length of test_pattern should not fit naturally into
     * RINGBUF_SIZE, or else it won't be possible to detect proper
     * wrapping of the head pointer.
     */
    const char test_pattern[] = "abcdefghijk";
    assert((strlen(test_pattern) % RINGBUF_SIZE) != 0);
    fill_buffer(buf, RINGBUF_SIZE * 2, test_pattern);

    /* ringbufMemcpyInto with zero count */
    START_NEW_TEST(test_num);
    ringbufReset(rb1);
    ringbufMemset(rb1, 1, ringbufBufferSize(rb1));
    ringbufReset(rb1);
    assert(ringbufMemcpyInto(rb1, buf, 0) == ringbufHead(rb1));
    assert(ringbufCapacity(rb1) == RINGBUF_SIZE - 1);
    assert(ringbufBytesFree(rb1) == ringbufCapacity(rb1));
    assert(ringbufBytesUsed(rb1) == 0);
    assert(!ringbufIsFull(rb1));
    assert(ringbufIsEmpty(rb1));
    assert(ringbufTail(rb1) == ringbufHead(rb1));
    assert(*(char *)ringbufHead(rb1) == '\1');
    END_TEST(test_num);

    /* ringbufMemcpyInto a few bytes of data */
    START_NEW_TEST(test_num);
    ringbufReset(rb1);
    ringbufMemset(rb1, 1, ringbufBufferSize(rb1));
    ringbufReset(rb1);
    assert(ringbufMemcpyInto(rb1, buf, strlen(test_pattern)) == ringbufHead(rb1));
    assert(ringbufCapacity(rb1) == RINGBUF_SIZE - 1);
    assert(ringbufBytesFree(rb1) == ringbufCapacity(rb1) - strlen(test_pattern));
    assert(ringbufBytesUsed(rb1) == strlen(test_pattern));
    assert(!ringbufIsFull(rb1));
    assert(!ringbufIsEmpty(rb1));
    assert(strncmp(test_pattern, ringbufTail(rb1), strlen(test_pattern)) == 0);
    assert(*(char *)ringbufHead(rb1) == '\1');
    END_TEST(test_num);

    /* ringbufMemcpyInto full capacity */
    START_NEW_TEST(test_num);
    ringbufReset(rb1);
    ringbufMemset(rb1, 1, ringbufBufferSize(rb1));
    ringbufReset(rb1);
    assert(ringbufMemcpyInto(rb1, buf, RINGBUF_SIZE - 1) == ringbufHead(rb1));
    assert(ringbufCapacity(rb1) == RINGBUF_SIZE - 1);
    assert(ringbufBytesFree(rb1) == 0);
    assert(ringbufBytesUsed(rb1) == ringbufCapacity(rb1));
    assert(ringbufIsFull(rb1));
    assert(!ringbufIsEmpty(rb1));
    assert(strncmp(ringbufTail(rb1), (const char *) buf, RINGBUF_SIZE - 1) == 0);
    assert(*(char *)ringbufHead(rb1) == '\1');
    END_TEST(test_num);
    
    /* ringbufMemcpyInto, twice */
    START_NEW_TEST(test_num);
    ringbufReset(rb1);
    ringbufMemset(rb1, 1, ringbufBufferSize(rb1));
    ringbufReset(rb1);
    assert(ringbufMemcpyInto(rb1, buf, strlen(test_pattern)) == ringbufHead(rb1));
    assert(ringbufMemcpyInto(rb1, buf + strlen(test_pattern), strlen(test_pattern) - 1) == ringbufHead(rb1));
    assert(ringbufCapacity(rb1) == RINGBUF_SIZE - 1);
    assert(ringbufBytesFree(rb1) == ringbufCapacity(rb1) - (2 * strlen(test_pattern) - 1));
    assert(!ringbufIsFull(rb1));
    assert(!ringbufIsEmpty(rb1));
    assert(strncmp((const char *) buf, ringbufTail(rb1), 2 * strlen(test_pattern) - 1) == 0);
    assert(*(char *)ringbufHead(rb1) == '\1');
    END_TEST(test_num);

    /* ringbufMemcpyInto, twice (to full capacity) */
    START_NEW_TEST(test_num);
    ringbufReset(rb1);
    ringbufMemset(rb1, 1, ringbufBufferSize(rb1));
    ringbufReset(rb1);
    assert(ringbufMemcpyInto(rb1, buf, RINGBUF_SIZE - 2) == ringbufHead(rb1));
    assert(ringbufMemcpyInto(rb1, buf + RINGBUF_SIZE - 2, 1) == ringbufHead(rb1));
    assert(ringbufCapacity(rb1) == RINGBUF_SIZE - 1);
    assert(ringbufBytesFree(rb1) == 0);
    assert(ringbufIsFull(rb1));
    assert(!ringbufIsEmpty(rb1));
    assert(strncmp((const char *) buf, ringbufTail(rb1), RINGBUF_SIZE - 1) == 0);
    END_TEST(test_num);

    /* ringbufMemcpyInto, overflow by 1 byte */
    START_NEW_TEST(test_num);
    ringbufReset(rb1);
    ringbufMemset(rb1, 1, ringbufBufferSize(rb1));
    ringbufReset(rb1);
    assert(ringbufMemcpyInto(rb1, buf, RINGBUF_SIZE) == ringbufHead(rb1));
    assert(ringbufCapacity(rb1) == RINGBUF_SIZE - 1);
    assert(ringbufBytesFree(rb1) == 0);
    assert(ringbufBytesUsed(rb1) == ringbufCapacity(rb1));
    assert(ringbufIsFull(rb1));
    assert(!ringbufIsEmpty(rb1));
    /* head should point to the beginning of the buffer */
    assert(ringbufHead(rb1) == rb1_base);
    /* tail should have bumped forward by 1 byte */
    assert(ringbufTail(rb1) == rb1_base + 1);
    assert(strncmp(ringbufTail(rb1), (const char *) buf + 1, RINGBUF_SIZE - 1) == 0);
    END_TEST(test_num);

    /* ringbufMemcpyInto, twice (overflow by 1 byte on 2nd copy) */
    START_NEW_TEST(test_num);
    ringbufReset(rb1);
    ringbufMemset(rb1, 1, ringbufBufferSize(rb1));
    ringbufReset(rb1);
    assert(ringbufMemcpyInto(rb1, buf, RINGBUF_SIZE - 1) == ringbufHead(rb1));
    assert(ringbufMemcpyInto(rb1, buf + RINGBUF_SIZE - 1, 1) == ringbufHead(rb1));
    assert(ringbufCapacity(rb1) == RINGBUF_SIZE - 1);
    assert(ringbufBytesFree(rb1) == 0);
    assert(ringbufBytesUsed(rb1) == ringbufCapacity(rb1));
    assert(ringbufIsFull(rb1));
    assert(!ringbufIsEmpty(rb1));
    /* head should point to the beginning of the buffer */
    assert(ringbufHead(rb1) == rb1_base);
    /* tail should have bumped forward by 1 byte */
    assert(ringbufTail(rb1) == rb1_base + 1);
    assert(strncmp(ringbufTail(rb1), (const char *) buf + 1, RINGBUF_SIZE - 1) == 0);
    END_TEST(test_num);

    /* ringbufMemcpyInto, overflow by 2 bytes (will wrap) */
    START_NEW_TEST(test_num);
    ringbufReset(rb1);
    ringbufMemset(rb1, 1, ringbufBufferSize(rb1));
    ringbufReset(rb1);
    assert(ringbufMemcpyInto(rb1, buf, RINGBUF_SIZE + 1) == ringbufHead(rb1));
    assert(ringbufCapacity(rb1) == RINGBUF_SIZE - 1);
    assert(ringbufBytesFree(rb1) == 0);
    assert(ringbufBytesUsed(rb1) == ringbufCapacity(rb1));
    assert(ringbufIsFull(rb1));
    assert(!ringbufIsEmpty(rb1));
    assert(ringbufHead(rb1) == rb1_base + 1);
    assert(ringbufTail(rb1) == rb1_base + 2);
    assert(strncmp(ringbufTail(rb1), (const char *) buf + 2, RINGBUF_SIZE - 2) == 0);
    assert(strncmp((const char *) rb1_base, (const char *) buf + RINGBUF_SIZE, 1) == 0);
    END_TEST(test_num);

    rdfd = mkstemps(rd_template, strlen("ringbuf"));
    assert(rdfd != -1);
    assert(write(rdfd, buf, RINGBUF_SIZE * 2) == RINGBUF_SIZE * 2);
    
    /* ringbufRead with zero count */
    START_NEW_TEST(test_num);
    ringbufReset(rb1);
    assert(lseek(rdfd, 0, SEEK_SET) == 0);
    ringbufMemset(rb1, 1, ringbufBufferSize(rb1));
    ringbufReset(rb1);
    assert(ringbufRead(rdfd, rb1, 0) == 0);
    assert(ringbufCapacity(rb1) == RINGBUF_SIZE - 1);
    assert(ringbufBytesFree(rb1) == ringbufCapacity(rb1));
    assert(ringbufBytesUsed(rb1) == 0);
    assert(!ringbufIsFull(rb1));
    assert(ringbufIsEmpty(rb1));
    assert(ringbufTail(rb1) == ringbufHead(rb1));
    assert(*(char *)ringbufHead(rb1) == '\1');
    assert(lseek(rdfd, 0, SEEK_CUR) == 0);
    END_TEST(test_num);

    /* ringbufRead a few bytes of data */
    START_NEW_TEST(test_num);
    ringbufReset(rb1);
    assert(lseek(rdfd, 0, SEEK_SET) == 0);
    ringbufMemset(rb1, 1, ringbufBufferSize(rb1));
    ringbufReset(rb1);
    assert(ringbufRead(rdfd, rb1, strlen(test_pattern)) == strlen(test_pattern));
    assert(ringbufCapacity(rb1) == RINGBUF_SIZE - 1);
    assert(ringbufBytesFree(rb1) == ringbufCapacity(rb1) - strlen(test_pattern));
    assert(ringbufBytesUsed(rb1) == strlen(test_pattern));
    assert(!ringbufIsFull(rb1));
    assert(!ringbufIsEmpty(rb1));
    assert(strncmp(test_pattern, ringbufTail(rb1), strlen(test_pattern)) == 0);
    assert(*(char *)ringbufHead(rb1) == '\1');
    END_TEST(test_num);

    /* ringbufRead full capacity */
    START_NEW_TEST(test_num);
    ringbufReset(rb1);
    assert(lseek(rdfd, 0, SEEK_SET) == 0);
    ringbufMemset(rb1, 1, ringbufBufferSize(rb1));
    ringbufReset(rb1);
    assert(ringbufRead(rdfd, rb1, RINGBUF_SIZE - 1) == RINGBUF_SIZE - 1);
    assert(ringbufCapacity(rb1) == RINGBUF_SIZE - 1);
    assert(ringbufBytesFree(rb1) == 0);
    assert(ringbufBytesUsed(rb1) == ringbufCapacity(rb1));
    assert(ringbufIsFull(rb1));
    assert(!ringbufIsEmpty(rb1));
    assert(strncmp(ringbufTail(rb1), (const char *) buf, RINGBUF_SIZE - 1) == 0);
    assert(*(char *)ringbufHead(rb1) == '\1');
    END_TEST(test_num);
    
    /* ringbufRead, twice */
    START_NEW_TEST(test_num);
    ringbufReset(rb1);
    assert(lseek(rdfd, 0, SEEK_SET) == 0);
    ringbufMemset(rb1, 1, ringbufBufferSize(rb1));
    ringbufReset(rb1);
    assert(ringbufRead(rdfd, rb1, strlen(test_pattern)) == strlen(test_pattern));
    assert(ringbufRead(rdfd, rb1, strlen(test_pattern) - 1) == strlen(test_pattern) - 1);
    assert(ringbufCapacity(rb1) == RINGBUF_SIZE - 1);
    assert(ringbufBytesFree(rb1) == ringbufCapacity(rb1) - (2 * strlen(test_pattern) - 1));
    assert(!ringbufIsFull(rb1));
    assert(!ringbufIsEmpty(rb1));
    assert(strncmp((const char *) buf, ringbufTail(rb1), 2 * strlen(test_pattern) - 1) == 0);
    assert(*(char *)ringbufHead(rb1) == '\1');
    END_TEST(test_num);

    /* ringbufRead, twice (to full capacity) */
    START_NEW_TEST(test_num);
    ringbufReset(rb1);
    assert(lseek(rdfd, 0, SEEK_SET) == 0);
    ringbufMemset(rb1, 1, ringbufBufferSize(rb1));
    ringbufReset(rb1);
    assert(ringbufRead(rdfd, rb1, RINGBUF_SIZE - 2) == RINGBUF_SIZE - 2);
    assert(ringbufRead(rdfd, rb1, 1) == 1);
    assert(ringbufCapacity(rb1) == RINGBUF_SIZE - 1);
    assert(ringbufBytesFree(rb1) == 0);
    assert(ringbufIsFull(rb1));
    assert(!ringbufIsEmpty(rb1));
    assert(strncmp((const char *) buf, ringbufTail(rb1), RINGBUF_SIZE - 1) == 0);
    END_TEST(test_num);

    /* ringbufRead, overflow by 1 byte */
    START_NEW_TEST(test_num);
    ringbufReset(rb1);
    assert(lseek(rdfd, 0, SEEK_SET) == 0);
    ringbufMemset(rb1, 1, ringbufBufferSize(rb1));
    ringbufReset(rb1);
    assert(ringbufRead(rdfd, rb1, RINGBUF_SIZE) == RINGBUF_SIZE);
    assert(ringbufCapacity(rb1) == RINGBUF_SIZE - 1);
    assert(ringbufBytesFree(rb1) == 0);
    assert(ringbufBytesUsed(rb1) == ringbufCapacity(rb1));
    assert(ringbufIsFull(rb1));
    assert(!ringbufIsEmpty(rb1));
    /* head should point to the beginning of the buffer */
    assert(ringbufHead(rb1) == rb1_base);
    /* tail should have bumped forward by 1 byte */
    assert(ringbufTail(rb1) == rb1_base + 1);
    assert(strncmp(ringbufTail(rb1), (const char *) buf + 1, RINGBUF_SIZE - 1) == 0);
    END_TEST(test_num);

    /* ringbufRead, twice (overflow by 1 byte on 2nd copy) */
    START_NEW_TEST(test_num);
    ringbufReset(rb1);
    assert(lseek(rdfd, 0, SEEK_SET) == 0);
    ringbufMemset(rb1, 1, ringbufBufferSize(rb1));
    ringbufReset(rb1);
    assert(ringbufRead(rdfd, rb1, RINGBUF_SIZE - 1) == RINGBUF_SIZE - 1);
    assert(ringbufRead(rdfd, rb1, 1) == 1);
    assert(ringbufCapacity(rb1) == RINGBUF_SIZE - 1);
    assert(ringbufBytesFree(rb1) == 0);
    assert(ringbufBytesUsed(rb1) == ringbufCapacity(rb1));
    assert(ringbufIsFull(rb1));
    assert(!ringbufIsEmpty(rb1));
    /* head should point to the beginning of the buffer */
    assert(ringbufHead(rb1) == rb1_base);
    /* tail should have bumped forward by 1 byte */
    assert(ringbufTail(rb1) == rb1_base + 1);
    assert(strncmp(ringbufTail(rb1), (const char *) buf + 1, RINGBUF_SIZE - 1) == 0);
    END_TEST(test_num);

    /* ringbufRead, try to overflow by 2 bytes; will return a short count */
    START_NEW_TEST(test_num);
    ringbufReset(rb1);
    assert(lseek(rdfd, 0, SEEK_SET) == 0);
    ringbufMemset(rb1, 1, ringbufBufferSize(rb1));
    ringbufReset(rb1);
    assert(ringbufRead(rdfd, rb1, RINGBUF_SIZE + 1) == RINGBUF_SIZE); /* short count */
    assert(ringbufCapacity(rb1) == RINGBUF_SIZE - 1);
    assert(ringbufBytesFree(rb1) == 0);
    assert(ringbufBytesUsed(rb1) == ringbufCapacity(rb1));
    assert(ringbufIsFull(rb1));
    assert(!ringbufIsEmpty(rb1));
    /* head should point to the beginning of the buffer */
    assert(ringbufHead(rb1) == rb1_base);
    /* tail should have bumped forward by 1 byte */
    assert(ringbufTail(rb1) == rb1_base + 1);
    assert(strncmp(ringbufTail(rb1), (const char *) buf + 1, RINGBUF_SIZE - 1) == 0);
    END_TEST(test_num);

    uint8_t *dst = malloc(RINGBUF_SIZE * 2);
    
    /* ringbufMemcpyFrom with zero count, empty ring buffer */
    START_NEW_TEST(test_num);
    ringbufReset(rb1);
    ringbufMemset(rb1, 1, ringbufBufferSize(rb1));
    ringbufReset(rb1);
    fill_buffer(dst, RINGBUF_SIZE * 2, test_pattern);
    assert(ringbufMemcpyFrom(dst, rb1, 0) == ringbufTail(rb1));
    assert(ringbufCapacity(rb1) == RINGBUF_SIZE - 1);
    assert(ringbufBytesFree(rb1) == ringbufCapacity(rb1));
    assert(ringbufBytesUsed(rb1) == 0);
    assert(!ringbufIsFull(rb1));
    assert(ringbufIsEmpty(rb1));
    assert(ringbufTail(rb1) == ringbufHead(rb1));
    assert(ringbufTail(rb1) == rb1_base);
    assert(strncmp((const char *) dst, (const char *) buf, RINGBUF_SIZE * 2) == 0);
    END_TEST(test_num);

    /*
     * The length of test_pattern2 should not fit naturally into
     * RINGBUF_SIZE, or else it won't be possible to detect proper
     * wrapping of the head pointer.
     */
    const char test_pattern2[] = "0123456789A";
    assert((strlen(test_pattern2) % RINGBUF_SIZE) != 0);
    uint8_t *buf2 = malloc(RINGBUF_SIZE * 2);
    fill_buffer(buf2, RINGBUF_SIZE * 2, test_pattern2);

    /* ringbufMemcpyFrom with zero count, non-empty ring buffer */
    START_NEW_TEST(test_num);
    ringbufReset(rb1);
    ringbufMemset(rb1, 1, ringbufBufferSize(rb1));
    ringbufReset(rb1);
    fill_buffer(dst, RINGBUF_SIZE * 2, test_pattern);
    ringbufMemcpyInto(rb1, test_pattern2, strlen(test_pattern2));
    assert(ringbufMemcpyFrom(dst, rb1, 0) == ringbufTail(rb1));
    assert(ringbufCapacity(rb1) == RINGBUF_SIZE - 1);
    assert(ringbufBytesFree(rb1) == ringbufCapacity(rb1) - strlen(test_pattern2));
    assert(ringbufBytesUsed(rb1) == strlen(test_pattern2));
    assert(!ringbufIsFull(rb1));
    assert(!ringbufIsEmpty(rb1));
    assert(ringbufTail(rb1) == rb1_base);
    assert(strncmp((const char *) dst, (const char *) buf, RINGBUF_SIZE * 2) == 0);
    END_TEST(test_num);

    /* ringbufMemcpyFrom a few bytes of data */
    START_NEW_TEST(test_num);
    ringbufReset(rb1);
    ringbufMemset(rb1, 1, ringbufBufferSize(rb1));
    ringbufReset(rb1);
    fill_buffer(dst, RINGBUF_SIZE * 2, test_pattern);
    ringbufMemcpyInto(rb1, test_pattern2, strlen(test_pattern2));
    assert(ringbufMemcpyFrom(dst, rb1, 3) == ringbufTail(rb1));
    assert(ringbufCapacity(rb1) == RINGBUF_SIZE - 1);
    assert(ringbufBytesFree(rb1) == ringbufCapacity(rb1) - (strlen(test_pattern2) - 3));
    assert(ringbufBytesUsed(rb1) == strlen(test_pattern2) - 3);
    assert(!ringbufIsFull(rb1));
    assert(!ringbufIsEmpty(rb1));
    assert(ringbufTail(rb1) == rb1_base + 3);
    assert(ringbufHead(rb1) == (uint8_t *) ringbufTail(rb1) + (strlen(test_pattern2) - 3));
    assert(strncmp((const char *) dst, test_pattern2, 3) == 0);
    assert(strncmp((const char *) dst + 3, (const char *) buf + 3, RINGBUF_SIZE * 2 - 3) == 0);
    END_TEST(test_num);

    /* ringbufMemcpyFrom full capacity */
    START_NEW_TEST(test_num);
    ringbufReset(rb1);
    ringbufMemset(rb1, 1, ringbufBufferSize(rb1));
    ringbufReset(rb1);
    fill_buffer(dst, RINGBUF_SIZE * 2, test_pattern);
    ringbufMemcpyInto(rb1, buf2, RINGBUF_SIZE - 1);
    assert(ringbufMemcpyFrom(dst, rb1, RINGBUF_SIZE - 1) == ringbufTail(rb1));
    assert(ringbufCapacity(rb1) == RINGBUF_SIZE - 1);
    assert(ringbufBytesFree(rb1) == ringbufCapacity(rb1));
    assert(ringbufBytesUsed(rb1) == 0);
    assert(!ringbufIsFull(rb1));
    assert(ringbufIsEmpty(rb1));
    assert(ringbufTail(rb1) == ringbufHead(rb1));
    assert(ringbufHead(rb1) == rb1_base + RINGBUF_SIZE - 1);
    assert(strncmp((const char *) dst, (const char *) buf2, RINGBUF_SIZE - 1) == 0);
    assert(strncmp((const char *) dst + RINGBUF_SIZE - 1, (const char *) buf + RINGBUF_SIZE - 1, RINGBUF_SIZE + 1) == 0);
    END_TEST(test_num);
    
    /* ringbufMemcpyFrom, twice */
    START_NEW_TEST(test_num);
    ringbufReset(rb1);
    ringbufMemset(rb1, 1, ringbufBufferSize(rb1));
    ringbufReset(rb1);
    fill_buffer(dst, RINGBUF_SIZE * 2, test_pattern);
    ringbufMemcpyInto(rb1, buf2, 13);
    assert(ringbufMemcpyFrom(dst, rb1, 9) == ringbufTail(rb1));
    assert(ringbufMemcpyFrom((uint8_t *) dst + 9, rb1, 4) == ringbufTail(rb1));
    assert(ringbufCapacity(rb1) == RINGBUF_SIZE - 1);
    assert(ringbufBytesFree(rb1) == ringbufCapacity(rb1));
    assert(ringbufBytesUsed(rb1) == 0);
    assert(!ringbufIsFull(rb1));
    assert(ringbufIsEmpty(rb1));
    assert(ringbufTail(rb1) == ringbufHead(rb1));
    assert(ringbufTail(rb1) == rb1_base + 13);
    assert(strncmp((const char *) dst, (const char *) buf2, 13) == 0);
    assert(strncmp((const char *) dst + 13, (const char *) buf + 13, RINGBUF_SIZE * 2 - 13) == 0);
    END_TEST(test_num);

    /* ringbufMemcpyFrom, twice (full capacity) */
    START_NEW_TEST(test_num);
    ringbufReset(rb1);
    ringbufMemset(rb1, 1, ringbufBufferSize(rb1));
    ringbufReset(rb1);
    fill_buffer(dst, RINGBUF_SIZE * 2, test_pattern);
    ringbufMemcpyInto(rb1, buf2, RINGBUF_SIZE - 1);
    assert(ringbufMemcpyFrom(dst, rb1, RINGBUF_SIZE - 2) == ringbufTail(rb1));
    assert(ringbufMemcpyFrom(dst + RINGBUF_SIZE - 2, rb1, 1) == ringbufTail(rb1));
    assert(ringbufCapacity(rb1) == RINGBUF_SIZE - 1);
    assert(ringbufBytesFree(rb1) == ringbufCapacity(rb1));
    assert(ringbufBytesUsed(rb1) == 0);
    assert(!ringbufIsFull(rb1));
    assert(ringbufIsEmpty(rb1));
    assert(ringbufTail(rb1) == ringbufHead(rb1));
    assert(ringbufTail(rb1) == rb1_base + RINGBUF_SIZE - 1);
    assert(strncmp((const char *) dst, (const char *) buf2, RINGBUF_SIZE - 1) == 0);
    assert(strncmp((const char *) dst + RINGBUF_SIZE - 1, (const char *) buf + RINGBUF_SIZE - 1, RINGBUF_SIZE * 2 - (RINGBUF_SIZE -1)) == 0);
    END_TEST(test_num);

    /* ringbufMemcpyFrom, attempt to underflow */
    START_NEW_TEST(test_num);
    ringbufReset(rb1);
    ringbufMemset(rb1, 1, ringbufBufferSize(rb1));
    ringbufReset(rb1);
    fill_buffer(dst, RINGBUF_SIZE * 2, test_pattern);
    ringbufMemcpyInto(rb1, buf2, 15);
    assert(ringbufMemcpyFrom(dst, rb1, 16) == 0);
    assert(ringbufCapacity(rb1) == RINGBUF_SIZE - 1);
    assert(ringbufBytesFree(rb1) == ringbufCapacity(rb1) - 15);
    assert(ringbufBytesUsed(rb1) == 15);
    assert(!ringbufIsFull(rb1));
    assert(!ringbufIsEmpty(rb1));
    assert(ringbufTail(rb1) == rb1_base);
    assert(ringbufHead(rb1) == rb1_base + 15);
    assert(strncmp((const char *) dst, (const char *) buf, RINGBUF_SIZE * 2) == 0);
    END_TEST(test_num);
    
    /* ringbufMemcpyFrom, attempt to underflow on 2nd call */
    START_NEW_TEST(test_num);
    ringbufReset(rb1);
    ringbufMemset(rb1, 1, ringbufBufferSize(rb1));
    ringbufReset(rb1);
    fill_buffer(dst, RINGBUF_SIZE * 2, test_pattern);
    ringbufMemcpyInto(rb1, buf2, 15);
    assert(ringbufMemcpyFrom(dst, rb1, 14) == ringbufTail(rb1));
    assert(ringbufMemcpyFrom(dst + 14, rb1, 2) == 0);
    assert(ringbufCapacity(rb1) == RINGBUF_SIZE - 1);
    assert(ringbufBytesFree(rb1) == ringbufCapacity(rb1) - 1);
    assert(ringbufBytesUsed(rb1) == 1);
    assert(!ringbufIsFull(rb1));
    assert(!ringbufIsEmpty(rb1));
    assert(ringbufTail(rb1) == rb1_base + 14);
    assert(ringbufHead(rb1) == rb1_base + 15);
    assert(strncmp((const char *) dst, (const char *) buf2, 14) == 0);
    assert(strncmp((const char *) dst + 14, (const char *) buf + 14, RINGBUF_SIZE * 2 - 14) == 0);
    END_TEST(test_num);
    
    wrfd = mkstemps(wr_template, strlen("ringbuf-wr"));
    assert(wrfd != -1);

    /* ringbufWrite with zero count, empty ring buffer */
    START_NEW_TEST(test_num);
    ringbufReset(rb1);
    ringbufMemset(rb1, 1, ringbufBufferSize(rb1));
    ringbufReset(rb1);
    assert(lseek(wrfd, 0, SEEK_SET) == 0);
    assert(ringbufWrite(wrfd, rb1, 0) == 0);
    assert(ringbufCapacity(rb1) == RINGBUF_SIZE - 1);
    assert(ringbufBytesFree(rb1) == ringbufCapacity(rb1));
    assert(ringbufBytesUsed(rb1) == 0);
    assert(!ringbufIsFull(rb1));
    assert(ringbufIsEmpty(rb1));
    assert(ringbufTail(rb1) == ringbufHead(rb1));
    assert(lseek(wrfd, 0, SEEK_SET) == 0);
    /* should return 0 (EOF) */
    assert(read(wrfd, dst, 10) == 0);
    END_TEST(test_num);

    /* ringbufWrite with zero count, non-empty ring buffer */
    START_NEW_TEST(test_num);
    ringbufReset(rb1);
    ringbufMemset(rb1, 1, ringbufBufferSize(rb1));
    ringbufReset(rb1);
    assert(lseek(wrfd, 0, SEEK_SET) == 0);
    ringbufMemcpyInto(rb1, test_pattern2, strlen(test_pattern2));
    assert(ringbufWrite(wrfd, rb1, 0) == 0);
    assert(ringbufCapacity(rb1) == RINGBUF_SIZE - 1);
    assert(ringbufBytesFree(rb1) == ringbufCapacity(rb1) - strlen(test_pattern2));
    assert(ringbufBytesUsed(rb1) == strlen(test_pattern2));
    assert(!ringbufIsFull(rb1));
    assert(!ringbufIsEmpty(rb1));
    assert(ringbufTail(rb1) == rb1_base);
    assert(lseek(wrfd, 0, SEEK_SET) == 0);
    /* should return 0 (EOF) */
    assert(read(wrfd, dst, 10) == 0);
    END_TEST(test_num);

    /* ringbufWrite a few bytes of data */
    START_NEW_TEST(test_num);
    ringbufReset(rb1);
    ringbufMemset(rb1, 1, ringbufBufferSize(rb1));
    ringbufReset(rb1);
    assert(ftruncate(wrfd, 0) == 0);
    assert(lseek(wrfd, 0, SEEK_SET) == 0);
    fill_buffer(dst, RINGBUF_SIZE * 2, test_pattern);
    ringbufMemcpyInto(rb1, test_pattern2, strlen(test_pattern2));
    assert(ringbufWrite(wrfd, rb1, 3) == 3);
    assert(ringbufCapacity(rb1) == RINGBUF_SIZE - 1);
    assert(ringbufBytesFree(rb1) == ringbufCapacity(rb1) - (strlen(test_pattern2) - 3));
    assert(ringbufBytesUsed(rb1) == strlen(test_pattern2) - 3);
    assert(!ringbufIsFull(rb1));
    assert(!ringbufIsEmpty(rb1));
    assert(ringbufTail(rb1) == rb1_base + 3);
    assert(ringbufHead(rb1) == (uint8_t *) ringbufTail(rb1) + (strlen(test_pattern2) - 3));
    assert(lseek(wrfd, 0, SEEK_SET) == 0);
    assert(read(wrfd, dst, 4) == 3);
    assert(read(wrfd, dst + 3, 1) == 0);
    assert(strncmp((const char *) dst, test_pattern2, 3) == 0);
    assert(strncmp((const char *) dst + 3, (const char *) buf + 3, RINGBUF_SIZE * 2 - 3) == 0);
    END_TEST(test_num);

    /* ringbufWrite full capacity */
    START_NEW_TEST(test_num);
    ringbufReset(rb1);
    ringbufMemset(rb1, 1, ringbufBufferSize(rb1));
    ringbufReset(rb1);
    assert(ftruncate(wrfd, 0) == 0);
    assert(lseek(wrfd, 0, SEEK_SET) == 0);
    fill_buffer(dst, RINGBUF_SIZE * 2, test_pattern);
    ringbufMemcpyInto(rb1, buf2, RINGBUF_SIZE - 1);
    assert(ringbufWrite(wrfd, rb1, RINGBUF_SIZE - 1) == RINGBUF_SIZE - 1);
    assert(ringbufCapacity(rb1) == RINGBUF_SIZE - 1);
    assert(ringbufBytesFree(rb1) == ringbufCapacity(rb1));
    assert(ringbufBytesUsed(rb1) == 0);
    assert(!ringbufIsFull(rb1));
    assert(ringbufIsEmpty(rb1));
    assert(ringbufTail(rb1) == ringbufHead(rb1));
    assert(ringbufHead(rb1) == rb1_base + RINGBUF_SIZE - 1);
    assert(lseek(wrfd, 0, SEEK_SET) == 0);
    assert(read(wrfd, dst, RINGBUF_SIZE) == RINGBUF_SIZE - 1);
    assert(read(wrfd, dst + RINGBUF_SIZE - 1, 1) == 0);
    assert(strncmp((const char *) dst, (const char *) buf2, RINGBUF_SIZE - 1) == 0);
    assert(strncmp((const char *) dst + RINGBUF_SIZE - 1, (const char *) buf + RINGBUF_SIZE - 1, RINGBUF_SIZE + 1) == 0);
    END_TEST(test_num);

    /* ringbufWrite, twice */
    START_NEW_TEST(test_num);
    ringbufReset(rb1);
    ringbufMemset(rb1, 1, ringbufBufferSize(rb1));
    ringbufReset(rb1);
    assert(ftruncate(wrfd, 0) == 0);
    assert(lseek(wrfd, 0, SEEK_SET) == 0);
    fill_buffer(dst, RINGBUF_SIZE * 2, test_pattern);
    ringbufMemcpyInto(rb1, buf2, 13);
    assert(ringbufWrite(wrfd, rb1, 9) == 9);
    assert(ringbufWrite(wrfd, rb1, 4) == 4);
    assert(ringbufCapacity(rb1) == RINGBUF_SIZE - 1);
    assert(ringbufBytesFree(rb1) == ringbufCapacity(rb1));
    assert(ringbufBytesUsed(rb1) == 0);
    assert(!ringbufIsFull(rb1));
    assert(ringbufIsEmpty(rb1));
    assert(ringbufTail(rb1) == ringbufHead(rb1));
    assert(ringbufTail(rb1) == rb1_base + 13);
    assert(lseek(wrfd, 0, SEEK_SET) == 0);
    assert(read(wrfd, dst, 14) == 13);
    assert(read(wrfd, dst + 13, 1) == 0);
    assert(strncmp((const char *) dst, (const char *) buf2, 13) == 0);
    assert(strncmp((const char *) dst + 13, (const char *) buf + 13, RINGBUF_SIZE * 2 - 13) == 0);
    END_TEST(test_num);

    /* ringbufWrite, twice (full capacity) */
    START_NEW_TEST(test_num);
    ringbufReset(rb1);
    ringbufMemset(rb1, 1, ringbufBufferSize(rb1));
    ringbufReset(rb1);
    assert(ftruncate(wrfd, 0) == 0);
    assert(lseek(wrfd, 0, SEEK_SET) == 0);
    fill_buffer(dst, RINGBUF_SIZE * 2, test_pattern);
    ringbufMemcpyInto(rb1, buf2, RINGBUF_SIZE - 1);
    assert(ringbufWrite(wrfd, rb1, RINGBUF_SIZE - 2) == RINGBUF_SIZE - 2);
    assert(ringbufWrite(wrfd, rb1, 1) == 1);
    assert(ringbufCapacity(rb1) == RINGBUF_SIZE - 1);
    assert(ringbufBytesFree(rb1) == ringbufCapacity(rb1));
    assert(ringbufBytesUsed(rb1) == 0);
    assert(!ringbufIsFull(rb1));
    assert(ringbufIsEmpty(rb1));
    assert(ringbufTail(rb1) == ringbufHead(rb1));
    assert(ringbufTail(rb1) == rb1_base + RINGBUF_SIZE - 1);
    assert(lseek(wrfd, 0, SEEK_SET) == 0);
    assert(read(wrfd, dst, RINGBUF_SIZE - 1) == RINGBUF_SIZE - 1);
    assert(read(wrfd, dst + RINGBUF_SIZE - 1, 1) == 0);
    assert(strncmp((const char *) dst, (const char *) buf2, RINGBUF_SIZE - 1) == 0);
    assert(strncmp((const char *) dst + RINGBUF_SIZE - 1, (const char *) buf + RINGBUF_SIZE - 1, RINGBUF_SIZE * 2 - (RINGBUF_SIZE -1)) == 0);
    END_TEST(test_num);

    /* ringbufWrite, attempt to underflow */
    START_NEW_TEST(test_num);
    ringbufReset(rb1);
    ringbufMemset(rb1, 1, ringbufBufferSize(rb1));
    ringbufReset(rb1);
    assert(ftruncate(wrfd, 0) == 0);
    assert(lseek(wrfd, 0, SEEK_SET) == 0);
    fill_buffer(dst, RINGBUF_SIZE * 2, test_pattern);
    ringbufMemcpyInto(rb1, buf2, 15);
    assert(ringbufWrite(wrfd, rb1, 16) == 0);
    assert(ringbufCapacity(rb1) == RINGBUF_SIZE - 1);
    assert(ringbufBytesFree(rb1) == ringbufCapacity(rb1) - 15);
    assert(ringbufBytesUsed(rb1) == 15);
    assert(!ringbufIsFull(rb1));
    assert(!ringbufIsEmpty(rb1));
    assert(ringbufTail(rb1) == rb1_base);
    assert(ringbufHead(rb1) == rb1_base + 15);
    assert(lseek(wrfd, 0, SEEK_SET) == 0);
    assert(read(wrfd, dst, 1) == 0);
    assert(strncmp((const char *) dst, (const char *) buf, RINGBUF_SIZE * 2) == 0);
    END_TEST(test_num);
    
    /* ringbufWrite, attempt to underflow on 2nd call */
    START_NEW_TEST(test_num);
    ringbufReset(rb1);
    ringbufMemset(rb1, 1, ringbufBufferSize(rb1));
    ringbufReset(rb1);
    assert(ftruncate(wrfd, 0) == 0);
    assert(lseek(wrfd, 0, SEEK_SET) == 0);
    fill_buffer(dst, RINGBUF_SIZE * 2, test_pattern);
    ringbufMemcpyInto(rb1, buf2, 15);
    assert(ringbufWrite(wrfd, rb1, 14) == 14);
    assert(ringbufWrite(wrfd, rb1, 2) == 0);
    assert(ringbufCapacity(rb1) == RINGBUF_SIZE - 1);
    assert(ringbufBytesFree(rb1) == ringbufCapacity(rb1) - 1);
    assert(ringbufBytesUsed(rb1) == 1);
    assert(!ringbufIsFull(rb1));
    assert(!ringbufIsEmpty(rb1));
    assert(ringbufTail(rb1) == rb1_base + 14);
    assert(ringbufHead(rb1) == rb1_base + 15);
    assert(lseek(wrfd, 0, SEEK_SET) == 0);
    assert(read(wrfd, dst, 15) == 14);
    assert(read(wrfd, dst, 1) == 0);
    assert(strncmp((const char *) dst, (const char *) buf2, 1) == 0);
    assert(strncmp((const char *) dst + 14, (const char *) buf + 14, RINGBUF_SIZE * 2 - 14) == 0);
    END_TEST(test_num);
    
    /* ringbufRead followed by ringbufWrite */
    START_NEW_TEST(test_num);
    ringbufReset(rb1);
    ringbufMemset(rb1, 1, ringbufBufferSize(rb1));
    ringbufReset(rb1);
    fill_buffer(dst, RINGBUF_SIZE * 2, test_pattern2);
    assert(ftruncate(wrfd, 0) == 0);
    assert(lseek(wrfd, 0, SEEK_SET) == 0);
    assert(lseek(rdfd, 0, SEEK_SET) == 0);
    assert(ringbufRead(rdfd, rb1, 11) == 11);
    assert(ringbufWrite(wrfd, rb1, 11) == 11);
    assert(ringbufCapacity(rb1) == RINGBUF_SIZE - 1);
    assert(ringbufBytesFree(rb1) == ringbufCapacity(rb1));
    assert(ringbufBytesUsed(rb1) == 0);
    assert(!ringbufIsFull(rb1));
    assert(ringbufIsEmpty(rb1));
    assert(lseek(wrfd, 0, SEEK_SET) == 0);
    assert(read(wrfd, dst, 11) == 11);
    assert(read(wrfd, dst, 1) == 0);
    assert(strncmp((const char *) dst, (const char *) buf, 11) == 0);
    assert(strncmp((const char *) dst + 11, (const char *) buf2 + 11, RINGBUF_SIZE * 2 - 11) == 0);
    assert(*(char *)ringbufHead(rb1) == '\1');
    END_TEST(test_num);

    /* ringbufRead followed by partial ringbufWrite */
    START_NEW_TEST(test_num);
    ringbufReset(rb1);
    ringbufMemset(rb1, 1, ringbufBufferSize(rb1));
    ringbufReset(rb1);
    fill_buffer(dst, RINGBUF_SIZE * 2, test_pattern2);
    assert(ftruncate(wrfd, 0) == 0);
    assert(lseek(wrfd, 0, SEEK_SET) == 0);
    assert(lseek(rdfd, 0, SEEK_SET) == 0);
    ringbufReset(rb1);
    assert(ringbufRead(rdfd, rb1, 11) == 11);
    assert(ringbufWrite(wrfd, rb1, 7) == 7);
    assert(ringbufCapacity(rb1) == RINGBUF_SIZE - 1);
    assert(ringbufBytesFree(rb1) == ringbufCapacity(rb1) - 4);
    assert(ringbufBytesUsed(rb1) == 4);
    assert(!ringbufIsFull(rb1));
    assert(!ringbufIsEmpty(rb1));
    assert(lseek(wrfd, 0, SEEK_SET) == 0);
    assert(read(wrfd, dst, 11) == 7);
    assert(read(wrfd, dst, 1) == 0);
    assert(strncmp((const char *) dst, (const char *) buf, 7) == 0);
    assert(strncmp((const char *) dst + 7, (const char *) buf2 + 7, RINGBUF_SIZE * 2 - 7) == 0);
    assert(ringbufTail(rb1) == rb1_base + 7);
    assert(ringbufHead(rb1) == rb1_base + 11);
    assert(*(char *)ringbufHead(rb1) == '\1');
    END_TEST(test_num);

    /*
     * ringbufRead, ringbufWrite, then ringbufRead to just before
     * the end of contiguous buffer, but don't wrap
     */
    START_NEW_TEST(test_num);
    ringbufReset(rb1);
    ringbufMemset(rb1, 1, ringbufBufferSize(rb1));
    ringbufReset(rb1);
    fill_buffer(dst, RINGBUF_SIZE * 2, test_pattern2);
    assert(ftruncate(wrfd, 0) == 0);
    assert(lseek(wrfd, 0, SEEK_SET) == 0);
    assert(lseek(rdfd, 0, SEEK_SET) == 0);
    assert(ringbufRead(rdfd, rb1, 11) == 11);
    assert(ringbufWrite(wrfd, rb1, 11) == 11);
    assert(ringbufRead(rdfd, rb1, RINGBUF_SIZE - 11 - 1) == RINGBUF_SIZE - 11 - 1);
    assert(ringbufCapacity(rb1) == RINGBUF_SIZE - 1);
    assert(ringbufBytesFree(rb1) == 11);
    assert(ringbufBytesUsed(rb1) == RINGBUF_SIZE - 11 - 1);
    assert(!ringbufIsFull(rb1));
    assert(!ringbufIsEmpty(rb1));
    assert(ringbufTail(rb1) == rb1_base + 11);
    assert(ringbufHead(rb1) == rb1_base + RINGBUF_SIZE - 1);
    assert(lseek(wrfd, 0, SEEK_SET) == 0);
    assert(read(wrfd, dst, 11) == 11);
    assert(read(wrfd, dst, 1) == 0);
    assert(strncmp((const char *) dst, (const char *) buf, 11) == 0);
    assert(strncmp((const char *) dst + 11, (const char *) buf2 + 11, RINGBUF_SIZE * 2 - 11) == 0);
    END_TEST(test_num);

    /*
     * ringbufRead, ringbufWrite, then ringbufRead to the end of
     * the contiguous buffer, which should cause the head pointer to
     * wrap.
     */
    START_NEW_TEST(test_num);
    ringbufReset(rb1);
    ringbufMemset(rb1, 1, ringbufBufferSize(rb1));
    ringbufReset(rb1);
    fill_buffer(dst, RINGBUF_SIZE * 2, test_pattern2);
    assert(ftruncate(wrfd, 0) == 0);
    assert(lseek(wrfd, 0, SEEK_SET) == 0);
    assert(lseek(rdfd, 0, SEEK_SET) == 0);
    assert(ringbufRead(rdfd, rb1, 11) == 11);
    assert(ringbufWrite(wrfd, rb1, 11) == 11);
    assert(ringbufRead(rdfd, rb1, RINGBUF_SIZE - 11) == RINGBUF_SIZE - 11);
    assert(ringbufCapacity(rb1) == RINGBUF_SIZE - 1);
    assert(ringbufBytesFree(rb1) == 10);
    assert(ringbufBytesUsed(rb1) == RINGBUF_SIZE - 11);
    assert(!ringbufIsFull(rb1));
    assert(!ringbufIsEmpty(rb1));
    assert(ringbufTail(rb1) == rb1_base + 11);
    assert(ringbufHead(rb1) == rb1_base);
    assert(lseek(wrfd, 0, SEEK_SET) == 0);
    assert(read(wrfd, dst, 11) == 11);
    assert(read(wrfd, dst, 1) == 0);
    assert(strncmp((const char *) dst, (const char *) buf, 11) == 0);
    assert(strncmp((const char *) dst + 11, (const char *) buf2 + 11, RINGBUF_SIZE * 2 - 11) == 0);
    END_TEST(test_num);

    /*
     * Same as previous test, except the 2nd ringbufRead attempts to
     * read 1 beyond the end of the contiguous buffer. Because
     * ringbufRead only calls read(2) at most once, it should return
     * a short count (short by one byte), since wrapping around and
     * continuing to fill the ring buffer would require 2 read(2)
     * calls. For good measure, follow it up with a ringbufWrite that
     * causes the tail pointer to stop just short of wrapping, which
     * tests behavior when the ring buffer's head pointer is less than
     * its tail pointer.
     */
    START_NEW_TEST(test_num);
    ringbufReset(rb1);
    ringbufMemset(rb1, 1, ringbufBufferSize(rb1));
    ringbufReset(rb1);
    fill_buffer(dst, RINGBUF_SIZE * 2, test_pattern2);
    assert(ftruncate(wrfd, 0) == 0);
    assert(lseek(wrfd, 0, SEEK_SET) == 0);
    assert(lseek(rdfd, 0, SEEK_SET) == 0);
    assert(ringbufRead(rdfd, rb1, 11) == 11);
    assert(ringbufWrite(wrfd, rb1, 11) == 11);
    /* should return a short count! */
    assert(ringbufRead(rdfd, rb1, RINGBUF_SIZE - 11 + 1) == RINGBUF_SIZE - 11);
    assert(ringbufWrite(wrfd, rb1, RINGBUF_SIZE - 12) == RINGBUF_SIZE - 12);
    assert(ringbufCapacity(rb1) == RINGBUF_SIZE - 1);
    assert(ringbufBytesFree(rb1) == RINGBUF_SIZE - 2);
    assert(ringbufBytesUsed(rb1) == 1);
    assert(!ringbufIsFull(rb1));
    assert(!ringbufIsEmpty(rb1));
    assert(ringbufTail(rb1) == rb1_base + RINGBUF_SIZE - 1);
    assert(ringbufHead(rb1) == rb1_base);
    assert(lseek(wrfd, 0, SEEK_SET) == 0);
    assert(read(wrfd, dst, RINGBUF_SIZE - 1) == RINGBUF_SIZE - 1);
    assert(read(wrfd, dst, 1) == 0);
    assert(strncmp((const char *) dst, (const char *) buf, RINGBUF_SIZE - 1) == 0);
    assert(strncmp((const char *) dst + RINGBUF_SIZE - 1, (const char *) buf2 + RINGBUF_SIZE - 1, RINGBUF_SIZE * 2 - (RINGBUF_SIZE - 1)) == 0);
    END_TEST(test_num);

    /*
     * Same as previous test, except when the 2nd ringbufRead returns
     * a short count, do another.
     */
    START_NEW_TEST(test_num);
    ringbufReset(rb1);
    ringbufMemset(rb1, 1, ringbufBufferSize(rb1));
    ringbufReset(rb1);
    fill_buffer(dst, RINGBUF_SIZE * 2, test_pattern2);
    assert(ftruncate(wrfd, 0) == 0);
    assert(lseek(wrfd, 0, SEEK_SET) == 0);
    assert(lseek(rdfd, 0, SEEK_SET) == 0);
    assert(ringbufRead(rdfd, rb1, 11) == 11);
    assert(ringbufWrite(wrfd, rb1, 11) == 11);
    /* should return a short count! */
    assert(ringbufRead(rdfd, rb1, RINGBUF_SIZE - 11 + 1) == RINGBUF_SIZE - 11);
    assert(ringbufRead(rdfd, rb1, 1) == 1);
    assert(ringbufWrite(wrfd, rb1, RINGBUF_SIZE - 12) == RINGBUF_SIZE - 12);
    assert(ringbufCapacity(rb1) == RINGBUF_SIZE - 1);
    assert(ringbufBytesFree(rb1) == RINGBUF_SIZE - 3);
    assert(ringbufBytesUsed(rb1) == 2);
    assert(!ringbufIsFull(rb1));
    assert(!ringbufIsEmpty(rb1));
    assert(ringbufTail(rb1) == rb1_base + RINGBUF_SIZE - 1);
    assert(ringbufHead(rb1) == rb1_base + 1);
    assert(lseek(wrfd, 0, SEEK_SET) == 0);
    assert(read(wrfd, dst, RINGBUF_SIZE - 1) == RINGBUF_SIZE - 1);
    assert(read(wrfd, dst, 1) == 0);
    assert(strncmp((const char *) dst, (const char *) buf, RINGBUF_SIZE - 1) == 0);
    assert(strncmp((const char *) dst + RINGBUF_SIZE - 1, (const char *) buf2 + RINGBUF_SIZE - 1, RINGBUF_SIZE * 2 - (RINGBUF_SIZE - 1)) == 0);
    END_TEST(test_num);

    /*
     * Same as previous test, except the 2nd ringbufWrite causes the
     * tail pointer to wrap (just).
     */
    START_NEW_TEST(test_num);
    ringbufReset(rb1);
    ringbufMemset(rb1, 1, ringbufBufferSize(rb1));
    ringbufReset(rb1);
    fill_buffer(dst, RINGBUF_SIZE * 2, test_pattern2);
    assert(ftruncate(wrfd, 0) == 0);
    assert(lseek(wrfd, 0, SEEK_SET) == 0);
    assert(lseek(rdfd, 0, SEEK_SET) == 0);
    assert(ringbufRead(rdfd, rb1, 11) == 11);
    assert(ringbufWrite(wrfd, rb1, 11) == 11);
    /* should return a short count! */
    assert(ringbufRead(rdfd, rb1, RINGBUF_SIZE - 11 + 1) == RINGBUF_SIZE - 11);
    assert(ringbufRead(rdfd, rb1, 1) == 1);
    assert(ringbufWrite(wrfd, rb1, RINGBUF_SIZE - 11) == RINGBUF_SIZE - 11);
    assert(ringbufCapacity(rb1) == RINGBUF_SIZE - 1);
    assert(ringbufBytesFree(rb1) == RINGBUF_SIZE - 2);
    assert(ringbufBytesUsed(rb1) == 1);
    assert(!ringbufIsFull(rb1));
    assert(!ringbufIsEmpty(rb1));
    assert(ringbufTail(rb1) == rb1_base);
    assert(ringbufHead(rb1) == rb1_base + 1);
    assert(lseek(wrfd, 0, SEEK_SET) == 0);
    assert(read(wrfd, dst, RINGBUF_SIZE) == RINGBUF_SIZE);
    assert(read(wrfd, dst, 1) == 0);
    assert(strncmp((const char *) dst, (const char *) buf, RINGBUF_SIZE) == 0);
    assert(strncmp((const char *) dst + RINGBUF_SIZE, (const char *) buf2 + RINGBUF_SIZE, RINGBUF_SIZE) == 0);
    END_TEST(test_num);

    /*
     * Same as previous test, except the 2nd ringbufWrite returns a
     * short count.
     */
    START_NEW_TEST(test_num);
    ringbufReset(rb1);
    ringbufMemset(rb1, 1, ringbufBufferSize(rb1));
    ringbufReset(rb1);
    fill_buffer(dst, RINGBUF_SIZE * 2, test_pattern2);
    assert(ftruncate(wrfd, 0) == 0);
    assert(lseek(wrfd, 0, SEEK_SET) == 0);
    assert(lseek(rdfd, 0, SEEK_SET) == 0);
    assert(ringbufRead(rdfd, rb1, 11) == 11);
    assert(ringbufWrite(wrfd, rb1, 11) == 11);
    /* should return a short count! */
    assert(ringbufRead(rdfd, rb1, RINGBUF_SIZE - 11 + 1) == RINGBUF_SIZE - 11);
    assert(ringbufRead(rdfd, rb1, 1) == 1);
    /* should return a short count! */
    assert(ringbufWrite(wrfd, rb1, RINGBUF_SIZE - 10) == RINGBUF_SIZE - 11);
    assert(ringbufCapacity(rb1) == RINGBUF_SIZE - 1);
    assert(ringbufBytesFree(rb1) == RINGBUF_SIZE - 2);
    assert(ringbufBytesUsed(rb1) == 1);
    assert(!ringbufIsFull(rb1));
    assert(!ringbufIsEmpty(rb1));
    assert(ringbufTail(rb1) == rb1_base);
    assert(ringbufHead(rb1) == rb1_base + 1);
    assert(lseek(wrfd, 0, SEEK_SET) == 0);
    assert(read(wrfd, dst, RINGBUF_SIZE) == RINGBUF_SIZE);
    assert(read(wrfd, dst, 1) == 0);
    assert(strncmp((const char *) dst, (const char *) buf, RINGBUF_SIZE) == 0);
    assert(strncmp((const char *) dst + RINGBUF_SIZE, (const char *) buf2 + RINGBUF_SIZE, RINGBUF_SIZE) == 0);
    END_TEST(test_num);

    /*
     * Same as previous test, except do a 3rd ringbufWrite after the
     * 2nd returns the short count.
     */
    START_NEW_TEST(test_num);
    ringbufReset(rb1);
    ringbufMemset(rb1, 1, ringbufBufferSize(rb1));
    ringbufReset(rb1);
    fill_buffer(dst, RINGBUF_SIZE * 2, test_pattern2);
    assert(ftruncate(wrfd, 0) == 0);
    assert(lseek(wrfd, 0, SEEK_SET) == 0);
    assert(lseek(rdfd, 0, SEEK_SET) == 0);
    assert(ringbufRead(rdfd, rb1, 11) == 11);
    assert(ringbufWrite(wrfd, rb1, 11) == 11);
    /* should return a short count! */
    assert(ringbufRead(rdfd, rb1, RINGBUF_SIZE - 11 + 1) == RINGBUF_SIZE - 11);
    assert(ringbufRead(rdfd, rb1, 1) == 1);
    /* should return a short count! */
    assert(ringbufWrite(wrfd, rb1, RINGBUF_SIZE - 10) == RINGBUF_SIZE - 11);
    assert(ringbufWrite(wrfd, rb1, 1) == 1);
    assert(ringbufCapacity(rb1) == RINGBUF_SIZE - 1);
    assert(ringbufBytesFree(rb1) == RINGBUF_SIZE - 1);
    assert(ringbufBytesUsed(rb1) == 0);
    assert(!ringbufIsFull(rb1));
    assert(ringbufIsEmpty(rb1));
    assert(ringbufTail(rb1) == rb1_base + 1);
    assert(ringbufHead(rb1) == rb1_base + 1);
    assert(lseek(wrfd, 0, SEEK_SET) == 0);
    assert(read(wrfd, dst, RINGBUF_SIZE + 1) == RINGBUF_SIZE + 1);
    assert(read(wrfd, dst, 1) == 0);
    assert(strncmp((const char *) dst, (const char *) buf, RINGBUF_SIZE + 1) == 0);
    assert(strncmp((const char *) dst + RINGBUF_SIZE + 1, (const char *) buf2 + RINGBUF_SIZE + 1, RINGBUF_SIZE - 1) == 0);
    END_TEST(test_num);

    /* ringbufMemcpyInto followed by ringbufMemcpyFrom */
    START_NEW_TEST(test_num);
    ringbufReset(rb1);
    ringbufMemset(rb1, 1, ringbufBufferSize(rb1));
    ringbufReset(rb1);
    fill_buffer(dst, RINGBUF_SIZE * 2, test_pattern2);
    assert(ringbufMemcpyInto(rb1, buf, 11) == ringbufHead(rb1));
    assert(ringbufMemcpyFrom(dst, rb1, 11) == ringbufTail(rb1));
    assert(ringbufCapacity(rb1) == RINGBUF_SIZE - 1);
    assert(ringbufBytesFree(rb1) == ringbufCapacity(rb1));
    assert(ringbufBytesUsed(rb1) == 0);
    assert(!ringbufIsFull(rb1));
    assert(ringbufIsEmpty(rb1));
    assert(strncmp((const char *) dst, (const char *) buf, 11) == 0);
    assert(strncmp((const char *) dst + 11, (const char *) buf2 + 11, RINGBUF_SIZE * 2 - 11) == 0);
    assert(*(char *)ringbufHead(rb1) == '\1');
    END_TEST(test_num);

    /* ringbufMemcpyInto followed by partial ringbufMemcpyFrom */
    START_NEW_TEST(test_num);
    ringbufReset(rb1);
    ringbufMemset(rb1, 1, ringbufBufferSize(rb1));
    ringbufReset(rb1);
    fill_buffer(dst, RINGBUF_SIZE * 2, test_pattern2);
    assert(ringbufMemcpyInto(rb1, buf, 11) == ringbufHead(rb1));
    assert(ringbufMemcpyFrom(dst, rb1, 7) == ringbufTail(rb1));
    assert(ringbufCapacity(rb1) == RINGBUF_SIZE - 1);
    assert(ringbufBytesFree(rb1) == ringbufCapacity(rb1) - 4);
    assert(ringbufBytesUsed(rb1) == 4);
    assert(!ringbufIsFull(rb1));
    assert(!ringbufIsEmpty(rb1));
    assert(strncmp((const char *) dst, (const char *) buf, 7) == 0);
    assert(strncmp((const char *) dst + 7, (const char *) buf2 + 7, RINGBUF_SIZE * 2 - 7) == 0);
    assert(ringbufTail(rb1) == rb1_base + 7);
    assert(ringbufHead(rb1) == rb1_base + 11);
    assert(*(char *)ringbufHead(rb1) == '\1');
    END_TEST(test_num);

    /*
     * ringbufMemcpyInto, ringbufMemcpyFrom, then
     * ringbufMemcpyInto to just before the end of contiguous
     * buffer, but don't wrap
     */
    START_NEW_TEST(test_num);
    ringbufReset(rb1);
    ringbufMemset(rb1, 1, ringbufBufferSize(rb1));
    ringbufReset(rb1);
    fill_buffer(dst, RINGBUF_SIZE * 2, test_pattern2);
    assert(ringbufMemcpyInto(rb1, buf, 11) == ringbufHead(rb1));
    assert(ringbufMemcpyFrom(dst, rb1, 11) == ringbufTail(rb1));
    assert(ringbufMemcpyInto(rb1, buf + 11, RINGBUF_SIZE - 11 - 1) == ringbufHead(rb1));
    assert(ringbufCapacity(rb1) == RINGBUF_SIZE - 1);
    assert(ringbufBytesFree(rb1) == 11);
    assert(ringbufBytesUsed(rb1) == RINGBUF_SIZE - 11 - 1);
    assert(!ringbufIsFull(rb1));
    assert(!ringbufIsEmpty(rb1));
    assert(ringbufTail(rb1) == rb1_base + 11);
    assert(ringbufHead(rb1) == rb1_base + RINGBUF_SIZE - 1);
    assert(strncmp((const char *) dst, (const char *) buf, 11) == 0);
    assert(strncmp((const char *) dst + 11, (const char *) buf2 + 11, RINGBUF_SIZE * 2 - 11) == 0);
    END_TEST(test_num);

    /*
     * ringbufMemcpyInto, ringbufMemcpyFrom, then
     * ringbufMemcpyInto to the end of the contiguous buffer, which
     * should cause the head pointer to wrap.
     */
    START_NEW_TEST(test_num);
    ringbufReset(rb1);
    ringbufMemset(rb1, 1, ringbufBufferSize(rb1));
    ringbufReset(rb1);
    fill_buffer(dst, RINGBUF_SIZE * 2, test_pattern2);
    assert(ringbufMemcpyInto(rb1, buf, 11) == ringbufHead(rb1));
    assert(ringbufMemcpyFrom(dst, rb1, 11) == ringbufTail(rb1));
    assert(ringbufMemcpyInto(rb1, buf + 11, RINGBUF_SIZE - 11) == ringbufHead(rb1));
    assert(ringbufCapacity(rb1) == RINGBUF_SIZE - 1);
    assert(ringbufBytesFree(rb1) == 10);
    assert(ringbufBytesUsed(rb1) == RINGBUF_SIZE - 11);
    assert(!ringbufIsFull(rb1));
    assert(!ringbufIsEmpty(rb1));
    assert(ringbufTail(rb1) == rb1_base + 11);
    assert(ringbufHead(rb1) == rb1_base);
    assert(strncmp((const char *) dst, (const char *) buf, 11) == 0);
    assert(strncmp((const char *) dst + 11, (const char *) buf2 + 11, RINGBUF_SIZE * 2 - 11) == 0);
    END_TEST(test_num);

    /*
     * Same as previous test, except the 2nd ringbufMemcpyInto reads
     * 1 beyond the end of the contiguous buffer, which causes it to
     * wrap and do a 2nd memcpy from the start of the contiguous
     * buffer. For good measure, follow it up with a ringbufWrite
     * that causes the tail pointer to stop just short of wrapping,
     * which tests behavior when the ring buffer's head pointer is
     * less than its tail pointer.
     */
    START_NEW_TEST(test_num);
    ringbufReset(rb1);
    ringbufMemset(rb1, 1, ringbufBufferSize(rb1));
    ringbufReset(rb1);
    fill_buffer(dst, RINGBUF_SIZE * 2, test_pattern2);
    assert(ringbufMemcpyInto(rb1, buf, 11) == ringbufHead(rb1));
    assert(ringbufMemcpyFrom(dst, rb1, 11) == ringbufTail(rb1));
    assert(ringbufMemcpyInto(rb1, buf + 11, RINGBUF_SIZE - 11 + 1) == ringbufHead(rb1));
    assert(ringbufMemcpyFrom(dst + 11, rb1, RINGBUF_SIZE - 12) == ringbufTail(rb1));
    assert(ringbufCapacity(rb1) == RINGBUF_SIZE - 1);
    assert(ringbufBytesFree(rb1) == RINGBUF_SIZE - 3);
    assert(ringbufBytesUsed(rb1) == 2);
    assert(!ringbufIsFull(rb1));
    assert(!ringbufIsEmpty(rb1));
    assert(ringbufTail(rb1) == rb1_base + RINGBUF_SIZE - 1);
    assert(ringbufHead(rb1) == rb1_base + 1);
    assert(strncmp((const char *) dst, (const char *) buf, RINGBUF_SIZE - 1) == 0);
    assert(strncmp((const char *) dst + RINGBUF_SIZE - 1, (const char *) buf2 + RINGBUF_SIZE - 1, RINGBUF_SIZE * 2 - (RINGBUF_SIZE - 1)) == 0);
    END_TEST(test_num);

    /*
     * Same as previous test, except the 2nd ringbufWrite causes the
     * tail pointer to wrap (just).
     */
    START_NEW_TEST(test_num);
    ringbufReset(rb1);
    ringbufMemset(rb1, 1, ringbufBufferSize(rb1));
    ringbufReset(rb1);
    fill_buffer(dst, RINGBUF_SIZE * 2, test_pattern2);
    assert(ringbufMemcpyInto(rb1, buf, 11) == ringbufHead(rb1));
    assert(ringbufMemcpyFrom(dst, rb1, 11) == ringbufTail(rb1));
    assert(ringbufMemcpyInto(rb1, buf + 11, RINGBUF_SIZE - 11 + 1) == ringbufHead(rb1));
    assert(ringbufMemcpyFrom(dst + 11, rb1, RINGBUF_SIZE - 11) == ringbufTail(rb1));
    assert(ringbufCapacity(rb1) == RINGBUF_SIZE - 1);
    assert(ringbufBytesFree(rb1) == RINGBUF_SIZE - 2);
    assert(ringbufBytesUsed(rb1) == 1);
    assert(!ringbufIsFull(rb1));
    assert(!ringbufIsEmpty(rb1));
    assert(ringbufTail(rb1) == rb1_base);
    assert(ringbufHead(rb1) == rb1_base + 1);
    assert(strncmp((const char *) dst, (const char *) buf, RINGBUF_SIZE) == 0);
    assert(strncmp((const char *) dst + RINGBUF_SIZE, (const char *) buf2 + RINGBUF_SIZE, RINGBUF_SIZE) == 0);
    END_TEST(test_num);

    /*
     * Same as previous test, except the 2nd ringbufWrite performs 2
     * memcpy's, the 2nd of which starts from the beginning of the
     * contiguous buffer after the wrap.
     */
    START_NEW_TEST(test_num);
    ringbufReset(rb1);
    ringbufMemset(rb1, 1, ringbufBufferSize(rb1));
    ringbufReset(rb1);
    fill_buffer(dst, RINGBUF_SIZE * 2, test_pattern2);
    assert(ringbufMemcpyInto(rb1, buf, 11) == ringbufHead(rb1));
    assert(ringbufMemcpyFrom(dst, rb1, 11) == ringbufTail(rb1));
    assert(ringbufMemcpyInto(rb1, buf + 11, RINGBUF_SIZE - 11 + 1) == ringbufHead(rb1));
    assert(ringbufMemcpyFrom(dst + 11, rb1, RINGBUF_SIZE - 11 + 1) == ringbufTail(rb1));
    assert(ringbufCapacity(rb1) == RINGBUF_SIZE - 1);
    assert(ringbufBytesFree(rb1) == RINGBUF_SIZE - 1);
    assert(ringbufBytesUsed(rb1) == 0);
    assert(!ringbufIsFull(rb1));
    assert(ringbufIsEmpty(rb1));
    assert(ringbufTail(rb1) == rb1_base + 1);
    assert(ringbufHead(rb1) == rb1_base + 1);
    assert(strncmp((const char *) dst, (const char *) buf, RINGBUF_SIZE + 1) == 0);
    assert(strncmp((const char *) dst + RINGBUF_SIZE + 1, (const char *) buf2 + RINGBUF_SIZE + 1, RINGBUF_SIZE - 1) == 0);
    END_TEST(test_num);

    /*
     * Overflow with ringbufRead when tail pointer is > head
     * pointer. Should bump tail pointer to head + 1.
     */
    START_NEW_TEST(test_num);
    ringbufReset(rb1);
    ringbufMemset(rb1, 1, ringbufBufferSize(rb1));
    fill_buffer(dst, RINGBUF_SIZE * 2, test_pattern2);
    assert(ftruncate(wrfd, 0) == 0);
    assert(lseek(wrfd, 0, SEEK_SET) == 0);
    assert(lseek(rdfd, 0, SEEK_SET) == 0);
    assert(ringbufRead(rdfd, rb1, 11) == 11);
    assert(ringbufWrite(wrfd, rb1, 11) == 11);
    /* wrap head */
    assert(ringbufRead(rdfd, rb1, RINGBUF_SIZE - 11) == RINGBUF_SIZE - 11);
    /* overflow */
    assert(ringbufRead(rdfd, rb1, 11) == 11);
    assert(ringbufCapacity(rb1) == RINGBUF_SIZE - 1);
    assert(ringbufBytesFree(rb1) == 0);
    assert(ringbufBytesUsed(rb1) == RINGBUF_SIZE - 1);
    assert(ringbufIsFull(rb1));
    assert(!ringbufIsEmpty(rb1));
    assert(ringbufHead(rb1) == rb1_base + 11);
    assert(ringbufTail(rb1) == rb1_base + 12);
    END_TEST(test_num);

    /*
     * Overflow with ringbufRead when tail pointer is > head pointer,
     * and tail pointer is at the end of the contiguous buffer. Should
     * wrap tail pointer to beginning of contiguous buffer.
     */
    START_NEW_TEST(test_num);
    ringbufReset(rb1);
    ringbufMemset(rb1, 1, ringbufBufferSize(rb1));
    ringbufReset(rb1);
    fill_buffer(dst, RINGBUF_SIZE * 2, test_pattern2);
    assert(ftruncate(wrfd, 0) == 0);
    assert(lseek(wrfd, 0, SEEK_SET) == 0);
    assert(lseek(rdfd, 0, SEEK_SET) == 0);
    assert(ringbufRead(rdfd, rb1, 11) == 11);
    assert(ringbufWrite(wrfd, rb1, 11) == 11);
    /* wrap head */
    assert(ringbufRead(rdfd, rb1, RINGBUF_SIZE - 11) == RINGBUF_SIZE - 11);
    /* write until tail points to end of contiguous buffer */
    assert(ringbufWrite(wrfd, rb1, RINGBUF_SIZE - 12) == RINGBUF_SIZE - 12);
    assert(ringbufTail(rb1) == rb1_base + RINGBUF_SIZE - 1);
    /* overflow */
    assert(ringbufRead(rdfd, rb1, RINGBUF_SIZE - 1) == RINGBUF_SIZE - 1);
    assert(ringbufCapacity(rb1) == RINGBUF_SIZE - 1);
    assert(ringbufBytesFree(rb1) == 0);
    assert(ringbufBytesUsed(rb1) == RINGBUF_SIZE - 1);
    assert(ringbufIsFull(rb1));
    assert(!ringbufIsEmpty(rb1));
    assert(ringbufHead(rb1) == rb1_base + RINGBUF_SIZE - 1);
    assert(ringbufTail(rb1) == rb1_base);
    END_TEST(test_num);

    /*
     * Overflow with ringbufMemcpyInto when tail pointer is > head
     * pointer. Should bump tail pointer to head + 1.
     */
    START_NEW_TEST(test_num);
    ringbufReset(rb1);
    ringbufMemset(rb1, 1, ringbufBufferSize(rb1));
    ringbufReset(rb1);
    fill_buffer(dst, RINGBUF_SIZE * 2, test_pattern2);
    assert(ringbufMemcpyInto(rb1, buf, 11) == ringbufHead(rb1));
    assert(ringbufMemcpyFrom(dst, rb1, 11) == ringbufTail(rb1));
    /* wrap head */
    assert(ringbufMemcpyInto(rb1, buf + 11, RINGBUF_SIZE - 11) == ringbufHead(rb1));
    /* overflow */
    assert(ringbufMemcpyInto(rb1, buf + RINGBUF_SIZE, 11) == ringbufHead(rb1));
    assert(ringbufCapacity(rb1) == RINGBUF_SIZE - 1);
    assert(ringbufBytesFree(rb1) == 0);
    assert(ringbufBytesUsed(rb1) == RINGBUF_SIZE - 1);
    assert(ringbufIsFull(rb1));
    assert(!ringbufIsEmpty(rb1));
    assert(ringbufHead(rb1) == rb1_base + 11);
    assert(ringbufTail(rb1) == rb1_base + 12);
    END_TEST(test_num);

    /*
     * Overflow with ringbufMemcpyInto when tail pointer is > head
     * pointer, and tail pointer is at the end of the contiguous
     * buffer. Should wrap tail pointer to beginning of contiguous
     * buffer.
     */
    START_NEW_TEST(test_num);
    ringbufReset(rb1);
    ringbufMemset(rb1, 1, ringbufBufferSize(rb1));
    ringbufReset(rb1);
    fill_buffer(dst, RINGBUF_SIZE * 2, test_pattern2);
    assert(ringbufMemcpyInto(rb1, buf, 11) == ringbufHead(rb1));
    assert(ringbufMemcpyFrom(dst, rb1, 11) == ringbufTail(rb1));
    /* wrap head */
    assert(ringbufMemcpyInto(rb1, buf + 11, RINGBUF_SIZE - 11) == ringbufHead(rb1));
    /* copy from until tail points to end of contiguous buffer */
    assert(ringbufMemcpyFrom(dst + 11, rb1, RINGBUF_SIZE - 12) == ringbufTail(rb1));
    assert(ringbufTail(rb1) == rb1_base + RINGBUF_SIZE - 1);
    /* overflow */
    assert(ringbufMemcpyInto(rb1, buf + RINGBUF_SIZE, RINGBUF_SIZE - 1) == ringbufHead(rb1));
    assert(ringbufCapacity(rb1) == RINGBUF_SIZE - 1);
    assert(ringbufBytesFree(rb1) == 0);
    assert(ringbufBytesUsed(rb1) == RINGBUF_SIZE - 1);
    assert(ringbufIsFull(rb1));
    assert(!ringbufIsEmpty(rb1));
    assert(ringbufHead(rb1) == rb1_base + RINGBUF_SIZE - 1);
    assert(ringbufTail(rb1) == rb1_base);
    END_TEST(test_num);

    ringbuf_t rb2 = ringbufNew(RINGBUF_SIZE - 1);
    const uint8_t *rb2_base = ringbufHead(rb2);
    
    /* ringbufCopy with zero count, empty buffers */
    START_NEW_TEST(test_num);
    ringbufReset(rb1);
    ringbufReset(rb2);
    ringbufMemset(rb1, 1, ringbufBufferSize(rb1));
    ringbufReset(rb1);
    ringbufMemset(rb2, 2, ringbufBufferSize(rb2));
    ringbufReset(rb2);
    assert(ringbufCopy(rb1, rb2, 0) == ringbufHead(rb1));
    assert(ringbufCapacity(rb1) == RINGBUF_SIZE - 1);
    assert(ringbufCapacity(rb2) == RINGBUF_SIZE - 1);
    assert(ringbufBytesFree(rb1) == ringbufCapacity(rb1));
    assert(ringbufBytesFree(rb2) == ringbufCapacity(rb2));
    assert(ringbufBytesUsed(rb1) == 0);
    assert(ringbufBytesUsed(rb2) == 0);
    assert(!ringbufIsFull(rb1));
    assert(!ringbufIsFull(rb2));
    assert(ringbufIsEmpty(rb1));
    assert(ringbufIsEmpty(rb2));
    assert(ringbufTail(rb1) == ringbufHead(rb1));
    assert(ringbufTail(rb2) == ringbufHead(rb2));
    assert(ringbufHead(rb1) == rb1_base);
    assert(ringbufHead(rb2) == rb2_base);
    END_TEST(test_num);

    /* ringbufCopy with zero count, empty src */
    START_NEW_TEST(test_num);
    ringbufReset(rb1);
    ringbufReset(rb2);
    ringbufMemset(rb1, 1, ringbufBufferSize(rb1));
    ringbufReset(rb1);
    ringbufMemset(rb2, 2, ringbufBufferSize(rb2));
    ringbufReset(rb2);
    assert(ringbufMemcpyInto(rb1, buf, 2) == ringbufHead(rb1));
    assert(ringbufCopy(rb1, rb2, 0) == ringbufHead(rb1));
    assert(ringbufCapacity(rb1) == RINGBUF_SIZE - 1);
    assert(ringbufCapacity(rb2) == RINGBUF_SIZE - 1);
    assert(ringbufBytesFree(rb1) == ringbufCapacity(rb1) - 2);
    assert(ringbufBytesFree(rb2) == ringbufCapacity(rb2));
    assert(ringbufBytesUsed(rb1) == 2);
    assert(ringbufBytesUsed(rb2) == 0);
    assert(!ringbufIsFull(rb1));
    assert(!ringbufIsFull(rb2));
    assert(!ringbufIsEmpty(rb1));
    assert(ringbufIsEmpty(rb2));
    assert(ringbufTail(rb1) == rb1_base);
    assert(ringbufTail(rb2) == ringbufHead(rb2));
    assert(ringbufHead(rb1) == rb1_base + 2);
    assert(ringbufHead(rb2) == rb2_base);
    END_TEST(test_num);

    /* ringbufCopy with zero count, empty dst */
    START_NEW_TEST(test_num);
    ringbufReset(rb1);
    ringbufReset(rb2);
    ringbufMemset(rb1, 1, ringbufBufferSize(rb1));
    ringbufReset(rb1);
    ringbufMemset(rb2, 2, ringbufBufferSize(rb2));
    ringbufReset(rb2);
    assert(ringbufMemcpyInto(rb2, buf, 2) == ringbufHead(rb2));
    assert(ringbufCopy(rb1, rb2, 0) == ringbufHead(rb1));
    assert(ringbufCapacity(rb1) == RINGBUF_SIZE - 1);
    assert(ringbufCapacity(rb2) == RINGBUF_SIZE - 1);
    assert(ringbufBytesFree(rb1) == ringbufCapacity(rb1));
    assert(ringbufBytesFree(rb2) == ringbufCapacity(rb2) - 2);
    assert(ringbufBytesUsed(rb1) == 0);
    assert(ringbufBytesUsed(rb2) == 2);
    assert(!ringbufIsFull(rb1));
    assert(!ringbufIsFull(rb2));
    assert(ringbufIsEmpty(rb1));
    assert(!ringbufIsEmpty(rb2));
    assert(ringbufTail(rb1) == ringbufHead(rb1));
    assert(ringbufTail(rb2) == rb2_base);
    assert(ringbufHead(rb1) == rb1_base);
    assert(ringbufHead(rb2) == rb2_base + 2);
    END_TEST(test_num);

    /* ringbufCopy with zero count */
    START_NEW_TEST(test_num);
    ringbufReset(rb1);
    ringbufReset(rb2);
    ringbufMemset(rb1, 1, ringbufBufferSize(rb1));
    ringbufReset(rb1);
    ringbufMemset(rb2, 2, ringbufBufferSize(rb2));
    ringbufReset(rb2);
    assert(ringbufMemcpyInto(rb1, buf, 2) == ringbufHead(rb1));
    assert(ringbufMemcpyInto(rb2, buf2, 2) == ringbufHead(rb2));
    assert(ringbufCopy(rb1, rb2, 0) == ringbufHead(rb1));
    assert(ringbufCapacity(rb1) == RINGBUF_SIZE - 1);
    assert(ringbufCapacity(rb2) == RINGBUF_SIZE - 1);
    assert(ringbufBytesFree(rb1) == ringbufCapacity(rb1) - 2);
    assert(ringbufBytesFree(rb2) == ringbufCapacity(rb2) - 2);
    assert(ringbufBytesUsed(rb1) == 2);
    assert(ringbufBytesUsed(rb2) == 2);
    assert(!ringbufIsFull(rb1));
    assert(!ringbufIsFull(rb2));
    assert(!ringbufIsEmpty(rb1));
    assert(!ringbufIsEmpty(rb2));
    assert(ringbufTail(rb1) == rb1_base);
    assert(ringbufTail(rb2) == rb2_base);
    assert(ringbufHead(rb1) == rb1_base + 2);
    assert(ringbufHead(rb2) == rb2_base + 2);
    END_TEST(test_num);

    /* ringbufCopy full contents of rb2 into rb1 (initially empty) */
    START_NEW_TEST(test_num);
    ringbufReset(rb1);
    ringbufReset(rb2);
    ringbufMemset(rb1, 1, ringbufBufferSize(rb1));
    ringbufReset(rb1);
    ringbufMemset(rb2, 2, ringbufBufferSize(rb2));
    ringbufReset(rb2);
    assert(ringbufMemcpyInto(rb2, buf2, 2) == ringbufHead(rb2));
    assert(ringbufCopy(rb1, rb2, 2) == ringbufHead(rb1));
    assert(ringbufCapacity(rb1) == RINGBUF_SIZE - 1);
    assert(ringbufCapacity(rb2) == RINGBUF_SIZE - 1);
    assert(ringbufBytesFree(rb1) == ringbufCapacity(rb1) - 2);
    assert(ringbufBytesFree(rb2) == ringbufCapacity(rb2));
    assert(ringbufBytesUsed(rb1) == 2);
    assert(ringbufBytesUsed(rb2) == 0);
    assert(!ringbufIsFull(rb1));
    assert(!ringbufIsFull(rb2));
    assert(!ringbufIsEmpty(rb1));
    assert(ringbufIsEmpty(rb2));
    assert(ringbufTail(rb1) == rb1_base);
    assert(ringbufTail(rb2) == rb2_base + 2);
    assert(ringbufHead(rb1) == rb1_base + 2);
    assert(ringbufHead(rb2) == rb2_base + 2);
    assert(strncmp(ringbufTail(rb1), (const char *) buf2, 2) == 0);
    END_TEST(test_num);

    /* ringbufCopy full contents of rb2 into rb1 (latter initially
     * has 3 bytes) */
    START_NEW_TEST(test_num);
    ringbufReset(rb1);
    ringbufReset(rb2);
    ringbufMemset(rb1, 1, ringbufBufferSize(rb1));
    ringbufReset(rb1);
    ringbufMemset(rb2, 2, ringbufBufferSize(rb2));
    ringbufReset(rb2);
    assert(ringbufMemcpyInto(rb1, buf, 3) == ringbufHead(rb1));
    assert(ringbufMemcpyInto(rb2, buf2, 2) == ringbufHead(rb2));
    assert(ringbufCopy(rb1, rb2, 2) == ringbufHead(rb1));
    assert(ringbufCapacity(rb1) == RINGBUF_SIZE - 1);
    assert(ringbufCapacity(rb2) == RINGBUF_SIZE - 1);
    assert(ringbufBytesFree(rb1) == ringbufCapacity(rb1) - 5);
    assert(ringbufBytesFree(rb2) == ringbufCapacity(rb2));
    assert(ringbufBytesUsed(rb1) == 5);
    assert(ringbufBytesUsed(rb2) == 0);
    assert(!ringbufIsFull(rb1));
    assert(!ringbufIsFull(rb2));
    assert(!ringbufIsEmpty(rb1));
    assert(ringbufIsEmpty(rb2));
    assert(ringbufTail(rb1) == rb1_base);
    assert(ringbufTail(rb2) == rb2_base + 2);
    assert(ringbufHead(rb1) == rb1_base + 5);
    assert(ringbufHead(rb2) == rb2_base + 2);
    assert(strncmp(ringbufTail(rb1), (const char *) buf, 3) == 0);
    assert(strncmp((const char *) ringbufTail(rb1) + 3, (const char *) buf2, 2) == 0);
    END_TEST(test_num);

    /* ringbufCopy, wrap head of dst */
    START_NEW_TEST(test_num);
    ringbufReset(rb1);
    ringbufReset(rb2);
    ringbufMemset(rb1, 1, ringbufBufferSize(rb1));
    ringbufReset(rb1);
    ringbufMemset(rb2, 2, ringbufBufferSize(rb2));
    ringbufReset(rb2);
    assert(ringbufMemcpyInto(rb1, buf, RINGBUF_SIZE - 1) == ringbufHead(rb1));
    assert(ringbufHead(rb1) == rb1_base + RINGBUF_SIZE - 1);
    /* make sure rb1 doesn't overflow on later ringbufCopy */
    assert(ringbufMemcpyFrom(dst, rb1, 1) == ringbufTail(rb1));
    assert(ringbufMemcpyInto(rb2, buf2, 1) == ringbufHead(rb2));
    assert(ringbufCopy(rb1, rb2, 1) == ringbufHead(rb1));
    assert(ringbufCapacity(rb1) == RINGBUF_SIZE - 1);
    assert(ringbufCapacity(rb2) == RINGBUF_SIZE - 1);
    assert(ringbufBytesFree(rb1) == 0);
    assert(ringbufBytesFree(rb2) == ringbufCapacity(rb2));
    assert(ringbufBytesUsed(rb1) == RINGBUF_SIZE - 1);
    assert(ringbufBytesUsed(rb2) == 0);
    assert(ringbufIsFull(rb1));
    assert(!ringbufIsFull(rb2));
    assert(!ringbufIsEmpty(rb1));
    assert(ringbufIsEmpty(rb2));
    assert(ringbufTail(rb1) == rb1_base + 1);
    assert(ringbufTail(rb2) == rb2_base + 1);
    assert(ringbufHead(rb1) == rb1_base);
    assert(ringbufHead(rb2) == rb2_base + 1);
    assert(strncmp(ringbufTail(rb1), (const char *) buf + 1, RINGBUF_SIZE - 2) == 0);
    assert(strncmp((const char *) ringbufTail(rb1) + RINGBUF_SIZE - 2, (const char *) buf2, 1) == 0);
    END_TEST(test_num);

    /* ringbufCopy, wrap head of dst and continue copying into start
     * of contiguous buffer */
    START_NEW_TEST(test_num);
    ringbufReset(rb1);
    ringbufReset(rb2);
    ringbufMemset(rb1, 1, ringbufBufferSize(rb1));
    ringbufReset(rb1);
    ringbufMemset(rb2, 2, ringbufBufferSize(rb2));
    ringbufReset(rb2);
    assert(ringbufMemcpyInto(rb1, buf, RINGBUF_SIZE - 1) == ringbufHead(rb1));
    assert(ringbufHead(rb1) == rb1_base + RINGBUF_SIZE - 1);
    /* make sure rb1 doesn't overflow on later ringbufCopy */
    assert(ringbufMemcpyFrom(dst, rb1, 2) == ringbufTail(rb1));
    assert(ringbufMemcpyInto(rb2, buf2, 2) == ringbufHead(rb2));
    assert(ringbufCopy(rb1, rb2, 2) == ringbufHead(rb1));
    assert(ringbufCapacity(rb1) == RINGBUF_SIZE - 1);
    assert(ringbufCapacity(rb2) == RINGBUF_SIZE - 1);
    assert(ringbufBytesFree(rb1) == 0);
    assert(ringbufBytesFree(rb2) == ringbufCapacity(rb2));
    assert(ringbufBytesUsed(rb1) == RINGBUF_SIZE - 1);
    assert(ringbufBytesUsed(rb2) == 0);
    assert(ringbufIsFull(rb1));
    assert(!ringbufIsFull(rb2));
    assert(!ringbufIsEmpty(rb1));
    assert(ringbufIsEmpty(rb2));
    assert(ringbufTail(rb1) == rb1_base + 2);
    assert(ringbufTail(rb2) == rb2_base + 2);
    assert(ringbufHead(rb1) == rb1_base + 1);
    assert(ringbufHead(rb2) == rb2_base + 2);
    assert(strncmp(ringbufTail(rb1), (const char *) buf + 2, RINGBUF_SIZE - 3) == 0);
    /* last position in contiguous buffer */
    assert(strncmp((const char *) ringbufTail(rb1) + RINGBUF_SIZE - 3, (const char *) buf2, 1) == 0);
    /* start of contiguous buffer (from copy wrap) */
    assert(strncmp((const char *) rb1_base, (const char *) buf2 + 1, 1) == 0);
    END_TEST(test_num);

    /* ringbufCopy, wrap tail of src */
    START_NEW_TEST(test_num);
    ringbufReset(rb1);
    ringbufReset(rb2);
    ringbufMemset(rb1, 1, ringbufBufferSize(rb1));
    ringbufReset(rb1);
    ringbufMemset(rb2, 2, ringbufBufferSize(rb2));
    ringbufReset(rb2);
    assert(ringbufMemcpyInto(rb2, buf2, RINGBUF_SIZE - 1) == ringbufHead(rb2));
    assert(ringbufHead(rb2) == rb2_base + RINGBUF_SIZE - 1);
    assert(ringbufMemcpyFrom(dst, rb2, RINGBUF_SIZE - 3) == ringbufTail(rb2));
    assert(ringbufTail(rb2) == rb2_base + RINGBUF_SIZE - 3);
    assert(ringbufMemcpyInto(rb2, (uint8_t *) buf2 + RINGBUF_SIZE - 1, 2) == ringbufHead(rb2));
    assert(ringbufCopy(rb1, rb2, 3) == ringbufHead(rb1));
    assert(ringbufCapacity(rb1) == RINGBUF_SIZE - 1);
    assert(ringbufCapacity(rb2) == RINGBUF_SIZE - 1);
    assert(ringbufBytesFree(rb1) == RINGBUF_SIZE - 1 - 3);
    assert(ringbufBytesFree(rb2) == RINGBUF_SIZE - 1 - 1);
    assert(ringbufBytesUsed(rb1) == 3);
    assert(ringbufBytesUsed(rb2) == 1);
    assert(!ringbufIsFull(rb1));
    assert(!ringbufIsFull(rb2));
    assert(!ringbufIsEmpty(rb1));
    assert(!ringbufIsEmpty(rb2));
    assert(ringbufTail(rb1) == rb1_base);
    assert(ringbufTail(rb2) == rb2_base);
    assert(ringbufHead(rb1) == rb1_base + 3);
    assert(ringbufHead(rb2) == rb2_base + 1);
    assert(strncmp(ringbufTail(rb1), (const char *) buf2 + RINGBUF_SIZE - 3, 3) == 0);
    assert(*(char *)ringbufHead(rb1) == '\1');
    END_TEST(test_num);

    /* ringbufCopy, wrap tail of src and continue copying from start
     * of contiguous buffer */
    START_NEW_TEST(test_num);
    ringbufReset(rb1);
    ringbufReset(rb2);
    ringbufMemset(rb1, 1, ringbufBufferSize(rb1));
    ringbufReset(rb1);
    ringbufMemset(rb2, 2, ringbufBufferSize(rb2));
    ringbufReset(rb2);
    assert(ringbufMemcpyInto(rb2, buf2, RINGBUF_SIZE - 1) == ringbufHead(rb2));
    assert(ringbufHead(rb2) == rb2_base + RINGBUF_SIZE - 1);
    assert(ringbufMemcpyFrom(dst, rb2, RINGBUF_SIZE - 3) == ringbufTail(rb2));
    assert(ringbufTail(rb2) == rb2_base + RINGBUF_SIZE - 3);
    assert(ringbufMemcpyInto(rb2, buf2 + RINGBUF_SIZE - 1, 2) == ringbufHead(rb2));
    assert(ringbufCopy(rb1, rb2, 4) == ringbufHead(rb1));
    assert(ringbufCapacity(rb1) == RINGBUF_SIZE - 1);
    assert(ringbufCapacity(rb2) == RINGBUF_SIZE - 1);
    assert(ringbufBytesFree(rb1) == RINGBUF_SIZE - 1 - 4);
    assert(ringbufBytesFree(rb2) == RINGBUF_SIZE - 1);
    assert(ringbufBytesUsed(rb1) == 4);
    assert(ringbufBytesUsed(rb2) == 0);
    assert(!ringbufIsFull(rb1));
    assert(!ringbufIsFull(rb2));
    assert(!ringbufIsEmpty(rb1));
    assert(ringbufIsEmpty(rb2));
    assert(ringbufTail(rb1) == rb1_base);
    assert(ringbufTail(rb2) == rb2_base + 1);
    assert(ringbufHead(rb1) == rb1_base + 4);
    assert(ringbufHead(rb2) == rb2_base + 1);
    assert(strncmp(ringbufTail(rb1), (const char *) buf2 + RINGBUF_SIZE - 3, 4) == 0);
    assert(*(char *)ringbufHead(rb1) == '\1');
    END_TEST(test_num);

    /* ringbufCopy, wrap tail of src and head of dst simultaneously,
     * then continue copying from start of contiguous buffer */
    START_NEW_TEST(test_num);
    ringbufReset(rb1);
    ringbufReset(rb2);
    ringbufMemset(rb1, 1, ringbufBufferSize(rb1));
    ringbufReset(rb1);
    ringbufMemset(rb2, 2, ringbufBufferSize(rb2));
    ringbufReset(rb2);
    assert(ringbufMemcpyInto(rb2, buf2, RINGBUF_SIZE - 1) == ringbufHead(rb2));
    assert(ringbufHead(rb2) == rb2_base + RINGBUF_SIZE - 1);
    assert(ringbufMemcpyFrom(dst, rb2, RINGBUF_SIZE - 3) == ringbufTail(rb2));
    assert(ringbufTail(rb2) == rb2_base + RINGBUF_SIZE - 3);
    assert(ringbufMemcpyInto(rb2, buf2 + RINGBUF_SIZE - 1, 2) == ringbufHead(rb2));
    assert(ringbufMemcpyInto(rb1, buf, RINGBUF_SIZE - 3) == ringbufHead(rb1));
    assert(ringbufHead(rb1) == rb1_base + RINGBUF_SIZE - 3);
    assert(ringbufMemcpyFrom(dst, rb1, RINGBUF_SIZE - 3) == ringbufTail(rb1));
    assert(ringbufTail(rb1) == rb1_base + RINGBUF_SIZE - 3);
    assert(ringbufCopy(rb1, rb2, 4) == ringbufHead(rb1));
    assert(ringbufCapacity(rb1) == RINGBUF_SIZE - 1);
    assert(ringbufCapacity(rb2) == RINGBUF_SIZE - 1);
    assert(ringbufBytesFree(rb1) == RINGBUF_SIZE - 1 - 4);
    assert(ringbufBytesFree(rb2) == RINGBUF_SIZE - 1);
    assert(ringbufBytesUsed(rb1) == 4);
    assert(ringbufBytesUsed(rb2) == 0);
    assert(!ringbufIsFull(rb1));
    assert(!ringbufIsFull(rb2));
    assert(!ringbufIsEmpty(rb1));
    assert(ringbufIsEmpty(rb2));
    assert(ringbufTail(rb1) == rb1_base + RINGBUF_SIZE - 3);
    assert(ringbufTail(rb2) == rb2_base + 1);
    assert(ringbufHead(rb1) == rb1_base + 1);
    assert(ringbufHead(rb2) == rb2_base + 1);
    assert(strncmp(ringbufTail(rb1), (const char *) buf2 + RINGBUF_SIZE - 3, 3) == 0);
    assert(strncmp((const char *) rb1_base, (const char *) buf2 + RINGBUF_SIZE, 1) == 0);
    END_TEST(test_num);

    /* ringbufCopy, force 3 separate memcpy's: up to end of src, then
     * up to end of dst, then copy remaining bytes. */
    START_NEW_TEST(test_num);
    ringbufReset(rb1);
    ringbufReset(rb2);
    ringbufMemset(rb1, 1, ringbufBufferSize(rb1));
    ringbufReset(rb1);
    ringbufMemset(rb2, 2, ringbufBufferSize(rb2));
    ringbufReset(rb2);
    assert(ringbufMemcpyInto(rb2, buf2, RINGBUF_SIZE - 1) == ringbufHead(rb2));
    assert(ringbufHead(rb2) == rb2_base + RINGBUF_SIZE - 1);
    assert(ringbufMemcpyFrom(dst, rb2, RINGBUF_SIZE - 2) == ringbufTail(rb2));
    assert(ringbufTail(rb2) == rb2_base + RINGBUF_SIZE - 2);
    assert(ringbufMemcpyInto(rb2, buf2 + RINGBUF_SIZE - 1, 5) == ringbufHead(rb2));
    assert(ringbufMemcpyInto(rb1, buf, RINGBUF_SIZE - 3) == ringbufHead(rb1));
    assert(ringbufHead(rb1) == rb1_base + RINGBUF_SIZE - 3);
    assert(ringbufMemcpyFrom(dst, rb1, RINGBUF_SIZE - 4) == ringbufTail(rb1));
    assert(ringbufTail(rb1) == rb1_base + RINGBUF_SIZE - 4);
    assert(ringbufCopy(rb1, rb2, 5) == ringbufHead(rb1));
    assert(ringbufCapacity(rb1) == RINGBUF_SIZE - 1);
    assert(ringbufCapacity(rb2) == RINGBUF_SIZE - 1);
    assert(ringbufBytesFree(rb1) == RINGBUF_SIZE - 1 - 6);
    assert(ringbufBytesFree(rb2) == RINGBUF_SIZE - 1 - 1);
    assert(ringbufBytesUsed(rb1) == 6);
    assert(ringbufBytesUsed(rb2) == 1);
    assert(!ringbufIsFull(rb1));
    assert(!ringbufIsFull(rb2));
    assert(!ringbufIsEmpty(rb1));
    assert(!ringbufIsEmpty(rb2));
    assert(ringbufTail(rb1) == rb1_base + RINGBUF_SIZE - 4);
    assert(ringbufTail(rb2) == rb2_base + 3);
    assert(ringbufHead(rb1) == rb1_base + 2);
    assert(ringbufHead(rb2) == rb2_base + 4);
    /* one byte from buf */
    assert(strncmp(ringbufTail(rb1), (const char *) buf + RINGBUF_SIZE - 4, 1) == 0);
    /* 5 bytes from buf2, 3 at end of contiguous buffer and 2 after the wrap */
    assert(strncmp((const char *) ringbufTail(rb1) + 1, (const char *) buf2 + RINGBUF_SIZE - 2, 3) == 0);
    assert(strncmp((const char *) rb1_base, (const char *) buf2 + RINGBUF_SIZE - 2 + 3, 2) == 0);
    END_TEST(test_num);

    /* ringbufCopy overflow */
    START_NEW_TEST(test_num);
    ringbufReset(rb1);
    ringbufReset(rb2);
    ringbufMemset(rb1, 1, ringbufBufferSize(rb1));
    ringbufReset(rb1);
    ringbufMemset(rb2, 2, ringbufBufferSize(rb2));
    ringbufReset(rb2);
    assert(ringbufMemcpyInto(rb1, buf, RINGBUF_SIZE - 1) == ringbufHead(rb1));
    assert(ringbufHead(rb1) == rb1_base + RINGBUF_SIZE - 1);
    assert(ringbufTail(rb1) == rb1_base);
    assert(ringbufMemcpyInto(rb2, buf2, 2) == ringbufHead(rb2));
    assert(ringbufCopy(rb1, rb2, 2) == ringbufHead(rb1));
    assert(ringbufCapacity(rb1) == RINGBUF_SIZE - 1);
    assert(ringbufCapacity(rb2) == RINGBUF_SIZE - 1);
    assert(ringbufBytesFree(rb1) == 0);
    assert(ringbufBytesFree(rb2) == ringbufCapacity(rb2));
    assert(ringbufBytesUsed(rb1) == RINGBUF_SIZE - 1);
    assert(ringbufBytesUsed(rb2) == 0);
    assert(ringbufIsFull(rb1));
    assert(!ringbufIsFull(rb2));
    assert(!ringbufIsEmpty(rb1));
    assert(ringbufIsEmpty(rb2));
    assert(ringbufTail(rb1) == rb1_base + 2);
    assert(ringbufTail(rb2) == rb2_base + 2);
    assert(ringbufHead(rb1) == rb1_base + 1);
    assert(ringbufHead(rb2) == rb2_base + 2);
    assert(strncmp(ringbufTail(rb1), (const char *) buf + 2, RINGBUF_SIZE - 1 - 2) == 0);
    assert(strncmp((const char *) ringbufTail(rb1) + RINGBUF_SIZE - 1 - 2, (const char *) buf2, 1) == 0);
    assert(strncmp((const char *) rb1_base, (const char *) buf2 + 1, 1) == 0);
    END_TEST(test_num);

    /* ringbufCopy attempted underflow */
    START_NEW_TEST(test_num);
    ringbufReset(rb1);
    ringbufReset(rb2);
    ringbufMemset(rb1, 1, ringbufBufferSize(rb1));
    ringbufReset(rb1);
    ringbufMemset(rb2, 2, ringbufBufferSize(rb2));
    ringbufReset(rb2);
    assert(ringbufMemcpyInto(rb1, buf, 2) == ringbufHead(rb1));
    assert(ringbufMemcpyInto(rb2, buf2, 2) == ringbufHead(rb2));
    assert(ringbufCopy(rb1, rb2, 3) == 0);
    assert(ringbufCapacity(rb1) == RINGBUF_SIZE - 1);
    assert(ringbufCapacity(rb2) == RINGBUF_SIZE - 1);
    assert(ringbufBytesFree(rb1) == ringbufCapacity(rb1) - 2);
    assert(ringbufBytesFree(rb2) == ringbufCapacity(rb2) - 2);
    assert(ringbufBytesUsed(rb1) == 2);
    assert(ringbufBytesUsed(rb2) == 2);
    assert(!ringbufIsFull(rb1));
    assert(!ringbufIsFull(rb2));
    assert(!ringbufIsEmpty(rb1));
    assert(!ringbufIsEmpty(rb2));
    assert(ringbufTail(rb1) == rb1_base);
    assert(ringbufTail(rb2) == rb2_base);
    assert(ringbufHead(rb1) == rb1_base + 2);
    assert(ringbufHead(rb2) == rb2_base + 2);
    END_TEST(test_num);

    /* ringbufCopy, different capacities, overflow 2nd */
    START_NEW_TEST(test_num);
    ringbuf_t rb3 = ringbufNew(8);
    const uint8_t *rb3_base = ringbufHead(rb3);
    ringbufMemset(rb1, 1, ringbufBufferSize(rb1));
    ringbufReset(rb1);
    ringbufMemset(rb3, 3, ringbufBufferSize(rb3));
    ringbufReset(rb3);
    assert(ringbufMemcpyInto(rb1, buf, 10) == ringbufHead(rb1));
    assert(ringbufCopy(rb3, rb1, 10) == ringbufHead(rb3));
    assert(ringbufCapacity(rb1) == RINGBUF_SIZE - 1);
    assert(ringbufCapacity(rb3) == 8);
    assert(ringbufBytesFree(rb1) == ringbufCapacity(rb1));
    assert(ringbufBytesFree(rb3) == 0);
    assert(ringbufBytesUsed(rb1) == 0);
    assert(ringbufBytesUsed(rb3) == 8);
    assert(!ringbufIsFull(rb1));
    assert(ringbufIsFull(rb3));
    assert(ringbufIsEmpty(rb1));
    assert(!ringbufIsEmpty(rb3));
    assert(ringbufTail(rb1) == rb1_base + 10);
    assert(ringbufTail(rb3) == rb3_base + 2);
    assert(ringbufHead(rb1) == rb1_base + 10);
    assert(ringbufHead(rb3) == rb3_base + 1);
    assert(strncmp(ringbufTail(rb3), (const char *) buf + 2, 7) == 0);
    assert(strncmp((const char *) rb3_base, (const char *) buf + 9, 1) == 0);
    ringbufFree(&rb3);
    END_TEST(test_num);

    /* ringbufFindchr */

    START_NEW_TEST(test_num);
    ringbufReset(rb1);
    ringbufMemset(rb1, 1, ringbufBufferSize(rb1));
    ringbufReset(rb1);
    assert(ringbufFindchr(rb1, 'a', 0) == 0);
    END_TEST(test_num);

    START_NEW_TEST(test_num);
    ringbufReset(rb1);
    ringbufMemset(rb1, 1, ringbufBufferSize(rb1));
    ringbufReset(rb1);
    assert(ringbufMemcpyInto(rb1, buf, 2) == ringbufHead(rb1));
    assert(ringbufFindchr(rb1, 'a', 0) == 0);
    END_TEST(test_num);

    START_NEW_TEST(test_num);
    ringbufReset(rb1);
    ringbufMemset(rb1, 1, ringbufBufferSize(rb1));
    ringbufReset(rb1);
    assert(ringbufMemcpyInto(rb1, buf, 2) == ringbufHead(rb1));
    assert(ringbufFindchr(rb1, 'a', 1) == ringbufBytesUsed(rb1));
    END_TEST(test_num);

    START_NEW_TEST(test_num);
    ringbufReset(rb1);
    ringbufMemset(rb1, 1, ringbufBufferSize(rb1));
    ringbufReset(rb1);
    assert(ringbufMemcpyInto(rb1, buf, 2) == ringbufHead(rb1));
    assert(ringbufFindchr(rb1, 'b', 0) == 1);
    END_TEST(test_num);

    START_NEW_TEST(test_num);
    ringbufReset(rb1);
    ringbufMemset(rb1, 1, ringbufBufferSize(rb1));
    ringbufReset(rb1);
    assert(ringbufMemcpyInto(rb1, buf, 2) == ringbufHead(rb1));
    assert(ringbufFindchr(rb1, 'b', 1) == 1);
    END_TEST(test_num);

    START_NEW_TEST(test_num);
    ringbufReset(rb1);
    ringbufMemset(rb1, 1, ringbufBufferSize(rb1));
    ringbufReset(rb1);
    assert(ringbufMemcpyInto(rb1, buf, 2) == ringbufHead(rb1));
    assert(ringbufFindchr(rb1, 'b', 2) == ringbufBytesUsed(rb1));
    END_TEST(test_num);

    START_NEW_TEST(test_num);
    ringbufReset(rb1);
    ringbufMemset(rb1, 1, ringbufBufferSize(rb1));
    ringbufReset(rb1);
    assert(ringbufMemcpyInto(rb1, buf, 2) == ringbufHead(rb1));
    assert(ringbufFindchr(rb1, 1, 0) == ringbufBytesUsed(rb1));
    END_TEST(test_num);

    START_NEW_TEST(test_num);
    ringbufReset(rb1);
    ringbufMemset(rb1, 1, ringbufBufferSize(rb1));
    ringbufReset(rb1);
    assert(ringbufMemcpyInto(rb1, buf, strlen(test_pattern) + 1) == ringbufHead(rb1));
    assert(ringbufFindchr(rb1, 'a', 1) == strlen(test_pattern));
    END_TEST(test_num);

    START_NEW_TEST(test_num);
    ringbufReset(rb1);
    ringbufMemset(rb1, 1, ringbufBufferSize(rb1));
    ringbufReset(rb1);
    assert(ringbufMemcpyInto(rb1, buf, strlen(test_pattern) + 1) == ringbufHead(rb1));
    assert(ringbufFindchr(rb1, 'a', strlen(test_pattern)) == strlen(test_pattern));
    END_TEST(test_num);

    START_NEW_TEST(test_num);
    ringbufReset(rb1);
    ringbufMemset(rb1, 1, ringbufBufferSize(rb1));
    ringbufReset(rb1);
    assert(ringbufMemcpyInto(rb1, buf, strlen(test_pattern) + 1) == ringbufHead(rb1));
    assert(ringbufFindchr(rb1, 'a', strlen(test_pattern) + 1) == ringbufBytesUsed(rb1));
    END_TEST(test_num);

    START_NEW_TEST(test_num);
    ringbufReset(rb1);
    ringbufMemset(rb1, 1, ringbufBufferSize(rb1));
    ringbufReset(rb1);
    assert(ringbufMemcpyInto(rb1, buf, (strlen(test_pattern) * 2) - 1) == ringbufHead(rb1));
    assert(ringbufFindchr(rb1, 'a', strlen(test_pattern) + 1) == ringbufBytesUsed(rb1));
    END_TEST(test_num);

    START_NEW_TEST(test_num);
    ringbufReset(rb1);
    ringbufMemset(rb1, 1, ringbufBufferSize(rb1));
    ringbufReset(rb1);
    assert(ringbufMemcpyInto(rb1, buf, 3) == ringbufHead(rb1));
    assert(ringbufMemcpyFrom(dst, rb1, 1) == ringbufTail(rb1));
    assert(ringbufFindchr(rb1, 'a', 0) == ringbufBytesUsed(rb1));
    END_TEST(test_num);

    START_NEW_TEST(test_num);
    ringbufReset(rb1);
    ringbufMemset(rb1, 1, ringbufBufferSize(rb1));
    ringbufReset(rb1);
    assert(ringbufMemcpyInto(rb1, buf, 3) == ringbufHead(rb1));
    assert(ringbufMemcpyFrom(dst, rb1, 1) == ringbufTail(rb1));
    assert(ringbufFindchr(rb1, 'b', 0) == 0);
    END_TEST(test_num);

    START_NEW_TEST(test_num);
    ringbufReset(rb1);
    ringbufMemset(rb1, 1, ringbufBufferSize(rb1));
    ringbufReset(rb1);
    assert(ringbufMemcpyInto(rb1, buf, 3) == ringbufHead(rb1));
    assert(ringbufMemcpyFrom(dst, rb1, 2) == ringbufTail(rb1));
    assert(ringbufFindchr(rb1, 'b', 0) == ringbufBytesUsed(rb1));
    END_TEST(test_num);

    /* find 'd' in last byte of contiguous buffer */
    START_NEW_TEST(test_num);
    ringbufReset(rb1);
    ringbufMemset(rb1, 1, ringbufBufferSize(rb1));
    ringbufReset(rb1);
    assert(ringbufMemcpyInto(rb1, buf, RINGBUF_SIZE - 1) == ringbufHead(rb1));
    assert(ringbufMemcpyFrom(dst, rb1, RINGBUF_SIZE - 4) == ringbufTail(rb1));
    assert(ringbufFindchr(rb1, 'd', 0) == 3);
    END_TEST(test_num);

    /*
     * Find just before wrap with offset 1.
     */
    START_NEW_TEST(test_num);
    ringbufReset(rb1);
    ringbufMemset(rb1, 1, ringbufBufferSize(rb1));
    ringbufReset(rb1);
    /* head will wrap around and overflow by 2 bytes */
    assert(ringbufMemcpyInto(rb1, buf, RINGBUF_SIZE + 1) == ringbufHead(rb1));
    assert(ringbufMemcpyFrom(dst, rb1, RINGBUF_SIZE - 4) == ringbufTail(rb1));
    assert(ringbufFindchr(rb1, 'd', 1) == 1);
    END_TEST(test_num);

    /*
     * Miss the 'd' at the end due to offset 2.
     */
    START_NEW_TEST(test_num);
    ringbufReset(rb1);
    ringbufMemset(rb1, 1, ringbufBufferSize(rb1));
    ringbufReset(rb1);
    /* head will wrap around and overflow by 2 bytes */
    assert(ringbufMemcpyInto(rb1, buf, RINGBUF_SIZE + 1) == ringbufHead(rb1));
    assert(ringbufMemcpyFrom(dst, rb1, RINGBUF_SIZE - 4) == ringbufTail(rb1));
    assert(ringbufFindchr(rb1, 'd', 2) == ringbufBytesUsed(rb1));
    END_TEST(test_num);

    /*
     * should *not* find 'a' in the first byte of the contiguous
     * buffer when head wraps.
     */
    START_NEW_TEST(test_num);
    ringbufReset(rb1);
    ringbufMemset(rb1, 1, ringbufBufferSize(rb1));
    ringbufReset(rb1);
    /* head will wrap around and overflow by 1 byte */
    assert(ringbufMemcpyInto(rb1, buf, RINGBUF_SIZE) == ringbufHead(rb1));
    assert(ringbufMemcpyFrom(dst, rb1, RINGBUF_SIZE - 4) == ringbufTail(rb1));
    assert(ringbufFindchr(rb1, 'a', 0) == ringbufBytesUsed(rb1));
    END_TEST(test_num);

    /*
     * Should find 'e' at first byte of contiguous buffer (i.e.,
     * should wrap during search).
     */
    START_NEW_TEST(test_num);
    ringbufReset(rb1);
    ringbufMemset(rb1, 1, ringbufBufferSize(rb1));
    ringbufReset(rb1);
    /* head will wrap around and overflow by 2 bytes */
    assert(ringbufMemcpyInto(rb1, buf, RINGBUF_SIZE + 1) == ringbufHead(rb1));
    assert(ringbufMemcpyFrom(dst, rb1, RINGBUF_SIZE - 4) == ringbufTail(rb1));
    assert(ringbufFindchr(rb1, 'e', 0) == 2);
    END_TEST(test_num);

    /*
     * Should find 'e' at first byte, with offset 1.
     */
    START_NEW_TEST(test_num);
    ringbufReset(rb1);
    ringbufMemset(rb1, 1, ringbufBufferSize(rb1));
    ringbufReset(rb1);
    /* head will wrap around and overflow by 2 bytes */
    assert(ringbufMemcpyInto(rb1, buf, RINGBUF_SIZE + 1) == ringbufHead(rb1));
    assert(ringbufMemcpyFrom(dst, rb1, RINGBUF_SIZE - 4) == ringbufTail(rb1));
    assert(ringbufFindchr(rb1, 'e', 1) == 2);
    END_TEST(test_num);
    
    /*
     * Search begins at first byte due to offset 2, should find 'e'.
     */
    START_NEW_TEST(test_num);
    ringbufReset(rb1);
    ringbufMemset(rb1, 1, ringbufBufferSize(rb1));
    ringbufReset(rb1);
    /* head will wrap around and overflow by 2 bytes */
    assert(ringbufMemcpyInto(rb1, buf, RINGBUF_SIZE + 1) == ringbufHead(rb1));
    assert(ringbufMemcpyFrom(dst, rb1, RINGBUF_SIZE - 4) == ringbufTail(rb1));
    assert(ringbufFindchr(rb1, 'e', 2) == 2);
    END_TEST(test_num);
    
    /*
     * Miss the 'e' at first byte due to offset 3.
     */
    START_NEW_TEST(test_num);
    ringbufReset(rb1);
    ringbufMemset(rb1, 1, ringbufBufferSize(rb1));
    ringbufReset(rb1);
    /* head will wrap around and overflow by 2 bytes */
    assert(ringbufMemcpyInto(rb1, buf, RINGBUF_SIZE + 1) == ringbufHead(rb1));
    assert(ringbufMemcpyFrom(dst, rb1, RINGBUF_SIZE - 4) == ringbufTail(rb1));
    assert(ringbufFindchr(rb1, 'e', 3) == ringbufBytesUsed(rb1));
    END_TEST(test_num);
    
    /*
     * Should *not* find the 'c' left over from overwritten contents
     * (where head is currently pointing).
     */
    START_NEW_TEST(test_num);
    ringbufReset(rb1);
    ringbufMemset(rb1, 1, ringbufBufferSize(rb1));
    ringbufReset(rb1);
    /* head will wrap around and overflow by 2 bytes */
    assert(ringbufMemcpyInto(rb1, buf, RINGBUF_SIZE + 1) == ringbufHead(rb1));
    assert(ringbufMemcpyFrom(dst, rb1, RINGBUF_SIZE - 1) == ringbufTail(rb1));
    assert(ringbufFindchr(rb1, 'c', 0) == ringbufBytesUsed(rb1));
    END_TEST(test_num);
    
    /*
     * Should *not* find the 'd' left over from overwritten contents.
     */
    START_NEW_TEST(test_num);
    ringbufReset(rb1);
    ringbufMemset(rb1, 1, ringbufBufferSize(rb1));
    ringbufReset(rb1);
    /* head will wrap around and overflow by 2 bytes */
    assert(ringbufMemcpyInto(rb1, buf, RINGBUF_SIZE + 1) == ringbufHead(rb1));
    assert(ringbufMemcpyFrom(dst, rb1, RINGBUF_SIZE - 1) == ringbufTail(rb1));
    assert(ringbufFindchr(rb1, 'd', 1) == ringbufBytesUsed(rb1));
    END_TEST(test_num);
    
    ringbufFree(&rb1);
    ringbufFree(&rb2);
    free(buf);
    free(buf2);
    free(dst);
    return 0;
}
