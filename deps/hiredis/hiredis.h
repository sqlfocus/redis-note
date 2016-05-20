/*
 * Copyright (c) 2009-2011, Salvatore Sanfilippo <antirez at gmail dot com>
 * Copyright (c) 2010-2011, Pieter Noordhuis <pcnoordhuis at gmail dot com>
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *   * Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of Redis nor the names of its contributors may be used
 *     to endorse or promote products derived from this software without
 *     specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef __HIREDIS_H
#define __HIREDIS_H
#include <stdio.h> /* for size_t */
#include <stdarg.h> /* for va_list */
#include <sys/time.h> /* for struct timeval */

#define HIREDIS_MAJOR 0
#define HIREDIS_MINOR 11
#define HIREDIS_PATCH 0

#define REDIS_ERR -1
#define REDIS_OK 0

/* When an error occurs, the err flag in a context is set to hold the type of
 * error that occured. REDIS_ERR_IO means there was an I/O error and you
 * should use the "errno" variable to find out what is wrong.
 * For other values, the "errstr" field will hold a description. */
#define REDIS_ERR_IO 1 /* Error in read or write */
#define REDIS_ERR_EOF 3 /* End of file */
#define REDIS_ERR_PROTOCOL 4 /* Protocol error */
#define REDIS_ERR_OOM 5 /* Out of memory */
#define REDIS_ERR_OTHER 2 /* Everything else... */

/* Connection type can be blocking or non-blocking and is set in the
 * least significant bit of the flags field in redisContext. */
#define REDIS_BLOCK 0x1

/* Connection may be disconnected before being free'd. The second bit
 * in the flags field is set when the context is connected. */
#define REDIS_CONNECTED 0x2

/* The async API might try to disconnect cleanly and flush the output
 * buffer and read all subsequent replies before disconnecting.
 * This flag means no new commands can come in and the connection
 * should be terminated once all replies have been read. */
#define REDIS_DISCONNECTING 0x4

/* Flag specific to the async API which means that the context should be clean
 * up as soon as possible. */
#define REDIS_FREEING 0x8

/* Flag that is set when an async callback is executed. */
#define REDIS_IN_CALLBACK 0x10

/* Flag that is set when the async context has one or more subscriptions. */
#define REDIS_SUBSCRIBED 0x20

/* Flag that is set when monitor mode is active */
#define REDIS_MONITORING 0x40

#define REDIS_REPLY_STRING 1
#define REDIS_REPLY_ARRAY 2
#define REDIS_REPLY_INTEGER 3
#define REDIS_REPLY_NIL 4
#define REDIS_REPLY_STATUS 5
#define REDIS_REPLY_ERROR 6

#define REDIS_READER_MAX_BUF (1024*16)  /* Default max unused reader buffer. */

#define REDIS_KEEPALIVE_INTERVAL 15 /* seconds */

#ifdef __cplusplus
extern "C" {
#endif

/* 对应服务器回应信息的对象, reply object returned by redisCommand() */
typedef struct redisReply {
    int type;           /* 记录返回类型, REDIS_REPLY_* */

    long long integer;  /* 类型REDIS_REPLY_INTEGER对应的值 */

    int len;            /* ->str的长度 */
    char *str;          /* 类型REDIS_REPLY_ERROR/REDIS_REPLY_STRING的值 */

    size_t elements;    /* ->element[]的元素个数, 由*后的数字决定 */
    struct redisReply **element;    /* 类型REDIS_REPLY_ARRAY的值, 用于
                                        构建此类型父子对象的层级关系 */
} redisReply;

typedef struct redisReadTask {
    int type;       /* 记录返回类型, REDIS_REPLY_* */

    int elements;   /* REDIS_REPLY_ARRAY类型, *n中的n值 */
    int idx;        /* index in parent (array) object */
    void *obj;      /* 顶层对象, 生成的对象 */
    struct redisReadTask *parent; /* parent task */
    void *privdata; /* === redisReader->privdata, 
                       user-settable arbitrary field */
} redisReadTask;

typedef struct redisReplyObjectFunctions {
    void *(*createString)(const redisReadTask*, char*, size_t);
    void *(*createArray)(const redisReadTask*, int);
    void *(*createInteger)(const redisReadTask*, long long);
    void *(*createNil)(const redisReadTask*);
    void (*freeObject)(void*);
} redisReplyObjectFunctions;

/* State for the protocol parser */
typedef struct redisReader {
    int err;            /* 错误标识, 0表示无错误 */
    char errstr[128];   /* 错误字符串, 方便log */

    char *buf;      /* 盛放服务器应答, 动态分配 */
    size_t pos;     /* Buffer cursor */
    size_t len;     /* buf[]应答数据长度 */
    size_t maxbuf;  /* 未用buf[]的最大长度, 默认16k */

    /* 用于解析服务器回应的信息暂存栈 */
    redisReadTask rstack[9];
    int ridx;       /* 当前解析任务的索引, rstack[]; 解析完毕后为-1 */
    void *reply;    /* 回应信息root对象指针 */

    redisReplyObjectFunctions *fn;
                    /* 构造对象的函数指针, 默认defaultFunctions[] */
    void *privdata; /* 用户自设定的私有数据, 可成为TASK之间纽带 */
} redisReader;

/* Public API for the protocol parser. */
redisReader *redisReaderCreate(void);
void redisReaderFree(redisReader *r);
int redisReaderFeed(redisReader *r, const char *buf, size_t len);
int redisReaderGetReply(redisReader *r, void **reply);

/* Backwards compatibility, can be removed on big version bump. */
#define redisReplyReaderCreate redisReaderCreate
#define redisReplyReaderFree redisReaderFree
#define redisReplyReaderFeed redisReaderFeed
#define redisReplyReaderGetReply redisReaderGetReply
#define redisReplyReaderSetPrivdata(_r, _p) (int)(((redisReader*)(_r))->privdata = (_p))
#define redisReplyReaderGetObject(_r) (((redisReader*)(_r))->reply)
#define redisReplyReaderGetError(_r) (((redisReader*)(_r))->errstr)

/* Function to free the reply objects hiredis returns by default. */
void freeReplyObject(void *reply);

/* Functions to format a command according to the protocol. */
int redisvFormatCommand(char **target, const char *format, va_list ap);
int redisFormatCommand(char **target, const char *format, ...);
int redisFormatCommandArgv(char **target, int argc, const char **argv, const size_t *argvlen);

/* Context for a connection to Redis */
typedef struct redisContext {
    int err; /* Error flags, 0 when there is no error */
    char errstr[128]; /* String representation of error when applicable */
    int fd;                 /* socket插口描述符 */
    int flags;
    char *obuf;             /* 输出缓存 */
    redisReader *reader;    /* Protocol reader */
} redisContext;

redisContext *redisConnect(const char *ip, int port);
redisContext *redisConnectWithTimeout(const char *ip, int port, const struct timeval tv);
redisContext *redisConnectNonBlock(const char *ip, int port);
redisContext *redisConnectBindNonBlock(const char *ip, int port, const char *source_addr);
redisContext *redisConnectUnix(const char *path);
redisContext *redisConnectUnixWithTimeout(const char *path, const struct timeval tv);
redisContext *redisConnectUnixNonBlock(const char *path);
redisContext *redisConnectFd(int fd);
int redisSetTimeout(redisContext *c, const struct timeval tv);
int redisEnableKeepAlive(redisContext *c);
void redisFree(redisContext *c);
int redisFreeKeepFd(redisContext *c);
int redisBufferRead(redisContext *c);
int redisBufferWrite(redisContext *c, int *done);

/* In a blocking context, this function first checks if there are unconsumed
 * replies to return and returns one if so. Otherwise, it flushes the output
 * buffer to the socket and reads until it has a reply. In a non-blocking
 * context, it will return unconsumed replies until there are no more. */
int redisGetReply(redisContext *c, void **reply);
int redisGetReplyFromReader(redisContext *c, void **reply);

/* Write a formatted command to the output buffer. Use these functions in blocking mode
 * to get a pipeline of commands. */
int redisAppendFormattedCommand(redisContext *c, const char *cmd, size_t len);

/* Write a command to the output buffer. Use these functions in blocking mode
 * to get a pipeline of commands. */
int redisvAppendCommand(redisContext *c, const char *format, va_list ap);
int redisAppendCommand(redisContext *c, const char *format, ...);
int redisAppendCommandArgv(redisContext *c, int argc, const char **argv, const size_t *argvlen);

/* Issue a command to Redis. In a blocking context, it is identical to calling
 * redisAppendCommand, followed by redisGetReply. The function will return
 * NULL if there was an error in performing the request, otherwise it will
 * return the reply. In a non-blocking context, it is identical to calling
 * only redisAppendCommand and will always return NULL. */
void *redisvCommand(redisContext *c, const char *format, va_list ap);
void *redisCommand(redisContext *c, const char *format, ...);
void *redisCommandArgv(redisContext *c, int argc, const char **argv, const size_t *argvlen);

#ifdef __cplusplus
}
#endif

#endif
