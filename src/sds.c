/* SDSLib 2.0 -- A C dynamic strings library
 *
 * Copyright (c) 2006-2015, Salvatore Sanfilippo <antirez at gmail dot com>
 * Copyright (c) 2015, Oran Agra
 * Copyright (c) 2015, Redis Labs, Inc
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>
#include <limits.h>
#include "sds.h"
#include "sdsalloc.h"

const char *SDS_NOINIT = "SDS_NOINIT";

static inline int sdsHdrSize(char type) {   // 字符串头大小
    switch(type&SDS_TYPE_MASK) {    // 低三位标志字符串类型，与7(00000111)按位与，判断字符串类型
        case SDS_TYPE_5:    // 长度小于2^5
            return sizeof(struct sdshdr5);
        case SDS_TYPE_8:    // 长度大于等于2^5小于2^8
            return sizeof(struct sdshdr8);
        case SDS_TYPE_16:   // 长度大于等于2^8小于2^16
            return sizeof(struct sdshdr16);
        case SDS_TYPE_32:   // 长度大于等于2^16小于2^32
            return sizeof(struct sdshdr32);
        case SDS_TYPE_64:   // 长度大于等于2^32小于2^64
            return sizeof(struct sdshdr64);
    }
    return 0;
}

static inline char sdsReqType(size_t string_size) { // 判断字符串的长度
    if (string_size < 1<<5) // 长度小于32
        return SDS_TYPE_5;
    if (string_size < 1<<8) // 长度大于等于32且小于256
        return SDS_TYPE_8;
    if (string_size < 1<<16)    // 长度大于等于256且小于65536
        return SDS_TYPE_16;
#if (LONG_MAX == LLONG_MAX) // 支持long long
    if (string_size < 1ll<<32)  // 长度大于等于65536且小于2147483648
        return SDS_TYPE_32;
    return SDS_TYPE_64;
#else
    return SDS_TYPE_32;
#endif
}

/* Create a new sds string with the content specified by the 'init' pointer
 * and 'initlen'.
 * If NULL is used for 'init' the string is initialized with zero bytes.
 * If SDS_NOINIT is used, the buffer is left uninitialized;
 *
 * The string is always null-termined (all the sds strings are, always) so
 * even if you create an sds string with:
 *
 * mystring = sdsnewlen("abc",3);
 *
 * You can print the string with printf() as there is an implicit \0 at the
 * end of the string. However the string is binary safe and can contain
 * \0 characters in the middle, as the length is stored in the sds header. */
/**
 * 创建一个新的 SDS 字符串，这个字符串的内容是由 init 指针
 * 和 initlen 初始化的
 * 如果 init 是空指针，那么字符串将被初始化为0字节
 * 如果 SDS_NOINIT 被使用，字符串是不会初始化的
 * 这个字符串会总是以空结尾的(所有的 sds 字符串总是这样)，
 * 因此，如果你创建了一个 sds 字符串使用一下方式:
 * 
 * mysring = sdsnewlen("abc", 3);
 * 
 * 你可以使用 printf() 函数打印字符串，因为字符串的结尾是隐含一个字符串 0 的。
 * 并且，这个字符串也是二进制安全的，中间可以包含字符0，
 * 因为 sds 头部是有储存字符串的长度的。
 **/
sds sdsnewlen(const void *init, size_t initlen) {   //　创建一个SDS　字符串，并初始化
    void *sh;
    sds s;
    char type = sdsReqType(initlen);    // 判断字符串类型
    /* Empty strings are usually created in order to append. Use type 8
     * since type 5 is not good at this. */
    /**
     * 为了可以在后面添加字符串，空字符串是经常被创建的。
     * 如果类型是5，应使用类型8，因为类型5是不擅长这些的
     **/
    if (type == SDS_TYPE_5 && initlen == 0) type = SDS_TYPE_8; // 长度小于32的字符串都使用类型8来存储
    int hdrlen = sdsHdrSize(type);  // 获取所使用的类型的头的长度
    unsigned char *fp; /* flags pointer. */

    sh = s_malloc(hdrlen+initlen+1);    // sds 头大小 + 字符串的长度 + '\0'
    if (init==SDS_NOINIT)   // 不需要初始化
        init = NULL;
    else if (!init) // 需要初始化
        memset(sh, 0, hdrlen+initlen+1);    // 初始化为'\0'
    if (sh == NULL) return NULL;    // 内存分配失败
    s = (char*)sh+hdrlen;
    fp = ((unsigned char*)s)-1; // fp 指向 sds 的 flags, 标志字符串类型
    switch(type) {
        case SDS_TYPE_5: {
            *fp = type | (initlen << SDS_TYPE_BITS);    // 记录字符串长度(高5位)和类型信息(低3位)
            break;
        }
        case SDS_TYPE_8: {
            SDS_HDR_VAR(8,s);   // 将指针 sh 指向结构体首地址
            sh->len = initlen;  // 设置字符串长度
            sh->alloc = initlen;    // 记录为字符串分配的大小
            *fp = type; // 设置字符串类型
            break;
        }
        case SDS_TYPE_16: {
            SDS_HDR_VAR(16,s);
            sh->len = initlen;
            sh->alloc = initlen;
            *fp = type;
            break;
        }
        case SDS_TYPE_32: {
            SDS_HDR_VAR(32,s);
            sh->len = initlen;
            sh->alloc = initlen;
            *fp = type;
            break;
        }
        case SDS_TYPE_64: {
            SDS_HDR_VAR(64,s);
            sh->len = initlen;
            sh->alloc = initlen;
            *fp = type;
            break;
        }
    }
    if (initlen && init)
        memcpy(s, init, initlen);   // 初始化字符串
    s[initlen] = '\0';  // 以 '\0' 作为结束符
    return s;
}

/* Create an empty (zero length) sds string. Even in this case the string
 * always has an implicit null term. */
/**
 * 创建一个空(长度为0)的 sds 字符串，
 * 即使在中情况下，字符串总是有一个隐式的 null 结束标志
 **/
sds sdsempty(void) {
    return sdsnewlen("",0); // 占一个字节空间大小，存储一个 '\0'
}

/* Create a new sds string starting from a null terminated C string. */
/**
 * 创建一个新的 sds 字符串，这个字符串的初始值是以 null 结束的 C 风格字符串
 * (将 C 风格字符串转换为 sds 类型的字符串)
 **/
sds sdsnew(const char *init) {
    size_t initlen = (init == NULL) ? 0 : strlen(init); // 获取 C 风格字符串的长度
    return sdsnewlen(init, initlen);    // 创建新的字符串，并将值设为为 init 指向的字符串
}

/* Duplicate an sds string. */
/**
 * 复制一个 sds 类型的字符串
 **/
sds sdsdup(const sds s) {
    return sdsnewlen(s, sdslen(s)); // s 指向字符串的首地址(不是 SDS 类型结构体的首地址)
}

/* Free an sds string. No operation is performed if 's' is NULL. */
/**
 * 释放一个 sds 类型字符串, 如果 s 为 NULL ，则没有任何操作
 **/
void sdsfree(sds s) {
    if (s == NULL) return;
    s_free((char*)s-sdsHdrSize(s[-1])); // 释放整个结构体的内存空间，s-sdsHdrSize(s[-1])为结构体的首地址
}

/* Set the sds string length to the length as obtained with strlen(), so
 * considering as content only up to the first null term character.
 *
 * This function is useful when the sds string is hacked manually in some
 * way, like in the following example:
 *
 * s = sdsnew("foobar");
 * s[2] = '\0';
 * sdsupdatelen(s);
 * printf("%d\n", sdslen(s));
 *
 * The output will be "2", but if we comment out the call to sdsupdatelen()
 * the output will be "6" as the string was modified but the logical length
 * remains 6 bytes. */
/**
 * 将 sds 类型的字符串长度设置为 strlen() 函数获得的长度
 * 因此考虑到的字符串的内容是取决于第一个 null 结束字符
 * 
 * 当 sds 字符串被某种方式手动破坏的时候，这个函数是有用的
 * 像下面这个例子:
 * s = sdsnew("foobar");
 * s[2] = '\0';
 * sdsupdatelen(s);
 * printf("%d\n", sdslen(s));
 * 
 * 输出将会是2，但是如果我们调用 sdssupdatelen() 函数，
 * 输出将会是6， 当字符串被修改，这个逻辑长度仍然是6
 **/
void sdsupdatelen(sds s) {  // 更新字符串长度
    size_t reallen = strlen(s); // 获取字符串长度
    sdssetlen(s, reallen);  // 重新设置字符串长度
}

/* Modify an sds string in-place to make it empty (zero length).
 * However all the existing buffer is not discarded but set as free space
 * so that next append operations will not require allocations up to the
 * number of bytes previously available. */
/**
 * 修改一个 sds 字符串至空(长度为0)
 * 然而所有自存在的空间并不会被销毁，但是会设置在可用空间里，
 * 这样下次再有添加操作时，如果之前分配的字节数够用，
 * 就不用再分配空间了
 **/
void sdsclear(sds s) {
    sdssetlen(s, 0);    // 设置字符串长度为0
    s[0] = '\0';    // 只包含一个 '\0'
}

/* Enlarge the free space at the end of the sds string so that the caller
 * is sure that after calling this function can overwrite up to addlen
 * bytes after the end of the string, plus one more byte for nul term.
 *
 * Note: this does not change the *length* of the sds string as returned
 * by sdslen(), but only the free buffer space we have. */
/**
 * 在 sds 字符串后面扩大空闲空间，使调用者可以确保调用该函数之后能够在字符串后面
 * 添加 addlen 长度的字符串，后面有一个空字节
 **/
sds sdsMakeRoomFor(sds s, size_t addlen) {
    void *sh, *newsh;
    size_t avail = sdsavail(s); // 获取可用的空间大小
    size_t len, newlen;
    char type, oldtype = s[-1] & SDS_TYPE_MASK; // 旧字符串 flags 标志
    int hdrlen;

    /* Return ASAP if there is enough space left. */
    // 有足够的空间
    if (avail >= addlen) return s;
    // 没有足够的空间
    len = sdslen(s);    // 旧字符串的长度
    sh = (char*)s-sdsHdrSize(oldtype);  // 指向旧字符串 sds 类型结构体的首地址
    newlen = (len+addlen);  // 新长度
    if (newlen < SDS_MAX_PREALLOC)  // 小于 1Ｍ
        newlen *= 2;    //　分配两倍大小
    else    //　大于　１Ｍ
        newlen += SDS_MAX_PREALLOC; //　多分配　１Ｍ

    type = sdsReqType(newlen);  //　获取新字符串的类型(该字符串只是分配的长度变了)

    /* Don't use type 5: the user is appending to the string and type 5 is
     * not able to remember empty space, so sdsMakeRoomFor() must be called
     * at every appending operation. */
    /**
     * 用户在添加字符串的时候不要使用类型５，因为类型５是不能记录空闲空间的，
     * sdsMakeRoomFor() 函数在每次向字符串后执行添加操作时都会被调用
     **/
    if (type == SDS_TYPE_5) type = SDS_TYPE_8;  // 如果是类型５，　将类型提升至类型８

    hdrlen = sdsHdrSize(type);  //　获取　sds 结构体长度
    if (oldtype==type) {    //　类型没有变化，
        newsh = s_realloc(sh, hdrlen+newlen+1); // 更新内存大小， sds 头的内存可能没有改变
                                                // (要看 sh 指向的已分配的内存后面没有没足够的空间)
        if (newsh == NULL) return NULL;
        s = (char*)newsh+hdrlen;
    } else {
        /* Since the header size changes, need to move the string forward,
         * and can't use realloc */
        /**
         * 因为头的大小变化，需要清楚之前的字符串，不能使用 remalloc() 函数
         **/
        newsh = s_malloc(hdrlen+newlen+1);  // 重新分配内存
        if (newsh == NULL) return NULL; // 内存分配失败，返回 NULL 
        memcpy((char*)newsh+hdrlen, s, len+1);  // 将旧字符串内容拷贝到新分配的内存中
        s_free(sh); // 释放原有内存
        s = (char*)newsh+hdrlen;    // s 指向 sds 中字符串的首地址
        s[-1] = type;   // 设置 sds 类型
        sdssetlen(s, len);  // 重新设置 sds 长度
    }
    sdssetalloc(s, newlen); // 重新设置 sds 已分配的内存长度
    return s;   // 返回 sds 中字符串的首地址
}

/* Reallocate the sds string so that it has no free space at the end. The
 * contained string remains not altered, but next concatenation operations
 * will require a reallocation.
 *
 * After the call, the passed sds string is no longer valid and all the
 * references must be substituted with the new pointer returned by the call. */
/**
 * 重新分配 sds 字符串的内存，使字符串后面没有空闲的空间，
 * 原字符串不会被修改，但是下次在其后连接字符串时，需要重新分配内存
 * 
 * 调用该函数之后，参数中传递过来的 sds 字符串不再可用，
 * 所有的引用将会被该函数返回的新指针取代
 **/
sds sdsRemoveFreeSpace(sds s) { // 回收可用空间
    void *sh, *newsh;
    char type, oldtype = s[-1] & SDS_TYPE_MASK; // 获取旧字符串的类型
    int hdrlen, oldhdrlen = sdsHdrSize(oldtype);    // 获取就字符串头的大小
    size_t len = sdslen(s); // 获取字符串的长度
    sh = (char*)s-oldhdrlen;    // 指向该字符串所在 sds 结构体的首地址

    /* Check what would be the minimum SDS header that is just good enough to
     * fit this string. */
    /**
     * 检查刚好能放下这个字符串的最小 sds 头的大小
     **/
    type = sdsReqType(len); // 根据字符串长度判断可以有的最小类型
    hdrlen = sdsHdrSize(type);  // 根据最小类型判断在该类型下头的大小

    /* If the type is the same, or at least a large enough type is still
     * required, we just realloc(), letting the allocator to do the copy
     * only if really needed. Otherwise if the change is huge, we manually
     * reallocate the string to use the different header type. */
    /**
     * 如果类型与之前字符串类型是相同的，或者足够大的类型仍然是需要的，
     * 我们只调用 realloc() 函数， 如果确实需要，让分配器做拷贝操作，
     * 否则如果改变巨大，我们需要重新为字符串分配空间，这样可以使用不同的头类型
     **/
    if (oldtype==type || type > SDS_TYPE_8) {   // 类型相同或者足够大的类型仍需要
        newsh = s_realloc(sh, oldhdrlen+len+1); //回收部分空间
        if (newsh == NULL) return NULL; 
        s = (char*)newsh+oldhdrlen; // 设置 s 值，指向真正的字符串首地址
    } else {    // 更换头类型
        newsh = s_malloc(hdrlen+len+1); // 重新分配空间 
        if (newsh == NULL) return NULL;
        memcpy((char*)newsh+hdrlen, s, len+1);  // 拷贝原字符串的值
        s_free(sh); // 释放旧字符串的全部空间
        s = (char*)newsh+hdrlen;
        s[-1] = type;   // 设置新字符串的类型(类型、 所分配的空间变了， 字符串本身的值没有改变)
        sdssetlen(s, len);  // 设置字符串的长度
    }   
    sdssetalloc(s, len);    // 设置为字符串分配的空间大小
    return s;
}

/* Return the total size of the allocation of the specified sds string,
 * including:
 * 1) The sds header before the pointer.
 * 2) The string.
 * 3) The free buffer at the end if any.
 * 4) The implicit null term.
 */
/**
 * 返回一个特定 sds 字符串分配的总内存大小
 * 包括:
 * 1) 在指针前面 sds 头的大小
 * 2) 字符串本身的大小
 * 3) 如果有，还包括空闲空间的大小
 * 4) null 结束符
 **/
size_t sdsAllocSize(sds s) {
    size_t alloc = sdsalloc(s); // 获取字符串已分配的内存大小
    return sdsHdrSize(s[-1])+alloc+1;   // 总大小
}

/* Return the pointer of the actual SDS allocation (normally SDS strings
 * are referenced by the start of the string buffer). */
/**
 * 返回指向 SDS 结构体首地址的指针
 * (通常， SDS 字符串是字符数组首地址的引用)
 **/
void *sdsAllocPtr(sds s) {
    return (void*) (s-sdsHdrSize(s[-1]));
}

/* Increment the sds length and decrements the left free space at the
 * end of the string according to 'incr'. Also set the null term
 * in the new end of the string.
 *
 * This function is used in order to fix the string length after the
 * user calls sdsMakeRoomFor(), writes something after the end of
 * the current string, and finally needs to set the new length.
 *
 * Note: it is possible to use a negative increment in order to
 * right-trim the string.
 *
 * Usage example:
 *
 * Using sdsIncrLen() and sdsMakeRoomFor() it is possible to mount the
 * following schema, to cat bytes coming from the kernel to the end of an
 * sds string without copying into an intermediate buffer:
 *
 * oldlen = sdslen(s);
 * s = sdsMakeRoomFor(s, BUFFER_SIZE);
 * nread = read(fd, s+oldlen, BUFFER_SIZE);
 * ... check for nread <= 0 and handle it ...
 * sdsIncrLen(s, nread);
 */
/**
 * 根据 'incr' 在增加字符串的长度并且减少空闲空间的长度，
 * 字符串新的结尾也设置了 null 结束符
 * 
 * 这个函数在用户调用 sdsMakeRoomFor() 函数在当前字符串后添加一些字符串，
 * 需要重新设置新的长度时使用，它可以调整字符串的长度
 * 
 * 注意: incr 为负数也是被允许的，这样可以从右边去掉部分字符串
 * 
 * 用法示例:
 * 使用 sdsIncrLen() 函数和 sdsMakeRoomFor() 函数可以解决下面的情况，
 * 在 sds 字符串后面添加一段来自内核的数字节的字符串时不要中间的缓冲区即可完成 
 * 
 * oldlen = sdslen(s);  // s 的原长度
 * s = sdsMakeRoomFor(s, BUFFER_SIZE);  // 为 s 增加 BUFFER_SIZE 长度
 * nread = read(fd, s+oldlen, BUFFER_SIZE); // 将描述符 fd 中 BUFFER_SIZE 长度的字符串读至 s 的字符串后面
 * ... check for nread <= 0 and handle it ...   // 检查读取是否读取成功， 若失败，则处理之
 * sdsIncrLen(s, nread);    // 增加 sds 记录的长度
 **/
void sdsIncrLen(sds s, ssize_t incr) {
    unsigned char flags = s[-1];    // 获取包含 sds 类型的标志
    size_t len;
    switch(flags&SDS_TYPE_MASK) {   // sds 类型
        case SDS_TYPE_5: {
            unsigned char *fp = ((unsigned char*)s)-1;  // 指向 sds 的 flags
            unsigned char oldlen = SDS_TYPE_5_LEN(flags);   // 获取 sds 长度
            // incr 为正数时要小于 32 (类型 5 最多长度为 32 )
            // incr 为负数时要大于等于现有字符串的长度 
            assert((incr > 0 && oldlen+incr < 32) || (incr < 0 && oldlen >= (unsigned int)(-incr)));  
            *fp = SDS_TYPE_5 | ((oldlen+incr) << SDS_TYPE_BITS);    // 改变 sds 中的 flags
            len = oldlen+incr;  // 更改 sds 中记录的长度
            break;
        }
        case SDS_TYPE_8: {
            SDS_HDR_VAR(8,s);   // 获取 sds 的首地址指针
            // incr 为正时小于等于剩余空间大小，为负时，大于已有字符串的长度
            assert((incr >= 0 && sh->alloc-sh->len >= incr) || (incr < 0 && sh->len >= (unsigned int)(-incr)));
            len = (sh->len += incr);    // 更改 sds 中记录的长度
            break;
        }
        case SDS_TYPE_16: {
            SDS_HDR_VAR(16,s);
            assert((incr >= 0 && sh->alloc-sh->len >= incr) || (incr < 0 && sh->len >= (unsigned int)(-incr)));
            len = (sh->len += incr);
            break;
        }
        case SDS_TYPE_32: {
            SDS_HDR_VAR(32,s);
            assert((incr >= 0 && sh->alloc-sh->len >= (unsigned int)incr) || (incr < 0 && sh->len >= (unsigned int)(-incr)));
            len = (sh->len += incr);
            break;
        }
        case SDS_TYPE_64: {
            SDS_HDR_VAR(64,s);
            assert((incr >= 0 && sh->alloc-sh->len >= (uint64_t)incr) || (incr < 0 && sh->len >= (uint64_t)(-incr)));
            len = (sh->len += incr);
            break;
        }
        default: len = 0; /* Just to avoid compilation warnings. */ //仅仅是为了避免编译时的警告
    }
    s[len] = '\0';  // null 结束标志
}

/* Grow the sds to have the specified length. Bytes that were not part of
 * the original length of the sds will be set to zero.
 *
 * if the specified length is smaller than the current length, no operation
 * is performed. */
/**
 * 增加 sds 字符串至特定的长度，
 * 新增的空间将被初始化为0
 * 
 * 如果特定的长度小于当前的长度，不做任何操作
 **/
sds sdsgrowzero(sds s, size_t len) {
    size_t curlen = sdslen(s);  // 获取当前字符串的长度

    if (len <= curlen) return s;    // 目标长度小于当前长度，不做操作
    s = sdsMakeRoomFor(s,len-curlen);   // 分配当前长度与目标长度的长度差个字节
    if (s == NULL) return NULL;

    /* Make sure added region doesn't contain garbage */
    // 确保添加的区域没有垃圾值
    memset(s+curlen,0,(len-curlen+1)); /* also set trailing \0 byte */
    sdssetlen(s, len);  // 重新设置 sds 中记录的长度
    return s;
}

/* Append the specified binary-safe string pointed by 't' of 'len' bytes to the
 * end of the specified sds string 's'.
 *
 * After the call, the passed sds string is no longer valid and all the
 * references must be substituted with the new pointer returned by the call. */
/**
 * 在字符串 s 后面添加长度为 len 的二进制安全的字符串 t
 * 
 * 调用这个函数之后，参数中传递的 sds 不再可用，
 * 所有的引用必须被函数返回的新的指针替代
 **/
sds sdscatlen(sds s, const void *t, size_t len) {
    size_t curlen = sdslen(s);  // 获取当前 sds 的长度

    s = sdsMakeRoomFor(s,len);  // 新增 len 长度 
    if (s == NULL) return NULL;
    memcpy(s+curlen, t, len);   // 在字符串 s 后追加字符串 t
    sdssetlen(s, curlen+len);   // 设置字符串长度
    s[curlen+len] = '\0';   // 终结符
    return s;
}

/* Append the specified null termianted C string to the sds string 's'.
 *
 * After the call, the passed sds string is no longer valid and all the
 * references must be substituted with the new pointer returned by the call. */
/**
 * 在 sds 类型的字符串 s 中添加一个特定的 C 风格字符串(以 '\0' 结束)
 * 
 * 调用这个函数之后，参数中传递的 sds 不再可用，
 * 所有的引用必须被函数返回的新的指针替代
 **/
sds sdscat(sds s, const char *t) {
    return sdscatlen(s, t, strlen(t));
}

/* Append the specified sds 't' to the existing sds 's'.
 *
 * After the call, the modified sds string is no longer valid and all the
 * references must be substituted with the new pointer returned by the call. */
/**
 * 在已存在的 sds 类型的字符串 s 后面添加特定的 sds 类型的字符串 t
 * 
 * 调用这个函数之后，参数中传递的 sds 不再可用，
 * 所有的引用必须被函数返回的新的指针替代
 **/
sds sdscatsds(sds s, const sds t) {
    return sdscatlen(s, t, sdslen(t));
}

/* Destructively modify the sds string 's' to hold the specified binary
 * safe string pointed by 't' of length 'len' bytes. */
/**
 * 将 sds 类型的字符串 s 修改为长度为 len 的由 t 指向的二进制安全的字符串
 **/
sds sdscpylen(sds s, const char *t, size_t len) {
    if (sdsalloc(s) < len) {    // 如果空间不足
        s = sdsMakeRoomFor(s,len-sdslen(s));    // 分配不足的空间
        if (s == NULL) return NULL;
    }
    memcpy(s, t, len);  // 将字符串 t 复制给字符串 s
    s[len] = '\0';  // 设置空结束符
    sdssetlen(s, len);  // 设置 s 的长度
    return s;
}

/* Like sdscpylen() but 't' must be a null-termined string so that the length
 * of the string is obtained with strlen(). */
/**
 * 和 sdscpylen() 函数相似，
 * 但是这个函数中，t 必须是一个空结束的字符串
 * 以致于字符串的长度有 strlen() 函数获得
 **/
sds sdscpy(sds s, const char *t) {
    return sdscpylen(s, t, strlen(t));
}

/* Helper for sdscatlonglong() doing the actual number -> string
 * conversion. 's' must point to a string with room for at least
 * SDS_LLSTR_SIZE bytes.
 *
 * The function returns the length of the null-terminated string
 * representation stored at 's'. */
/**
 * 这个函数可辅助 sdscatlonglong() 函数完成数字到字符串的转换
 * s 指向的空间至少要有 SDS_LLSTR_SIZR 个字节
 * 
 * 这个函数返回以 null 结束的字符串 s 的长度
 **/
#define SDS_LLSTR_SIZE 21
int sdsll2str(char *s, long long value) {
    char *p, aux;
    unsigned long long v;
    size_t l;

    /* Generate the string representation, this method produces
     * an reversed string. */
    /**
     *  生成一个顺序相反的字符串
     **/
    v = (value < 0) ? -value : value;   // 如果是负数，去掉 '-' 
    p = s;
    do {
        *p++ = '0'+(v%10);  // 将数字转换为字符串
        v /= 10;
    } while(v);
    if (value < 0) *p++ = '-';  // 如果是负数，增加 ‘-’

    /* Compute length and add null term. */
    /**
     * 计算长度并添加 null 结束符
     **/
    l = p-s;
    *p = '\0';

    /* Reverse the string. */
    /**
     * 将字符串顺序调正
     **/
    p--;
    while(s < p) {
        aux = *s;
        *s = *p;
        *p = aux;
        s++;
        p--;
    }
    return l;   // 字符串长度
}

/* Identical sdsll2str(), but for unsigned long long type. */
/**
 * 和 sdsll2str() 想似，但是是为了 unsigned long long 类型
 **/
int sdsull2str(char *s, unsigned long long v) {
    char *p, aux;
    size_t l;

    /* Generate the string representation, this method produces
     * an reversed string. */
    p = s;
    do {
        *p++ = '0'+(v%10);  // 将数字转换为字符串
        v /= 10;
    } while(v);

    /* Compute length and add null term. */
    /**
     * 计算字符串的长度并添加 null 结束符
     **/
    l = p-s;
    *p = '\0';

    /* Reverse the string. */
    /**
     * 将字符串翻转过来
     **/
    p--;
    while(s < p) {
        aux = *s;
        *s = *p;
        *p = aux;
        s++;
        p--;
    }
    return l;
}

/* Create an sds string from a long long value. It is much faster than:
 *
 * sdscatprintf(sdsempty(),"%lld\n", value);
 */
/**
 * 由一个 long long 类型的数字创建一个 sds 类型的字符串
 * 它比下面的方式要快很多
 * 
 * sdscatprintf(sdsempty(),"%lld\n", value);
 **/
sds sdsfromlonglong(long long value) {
    char buf[SDS_LLSTR_SIZE];
    int len = sdsll2str(buf,value); // 将 long long 类型字符串转换为字符串

    return sdsnewlen(buf,len);
}

/* Like sdscatprintf() but gets va_list instead of being variadic. */
/**
 * 
 **/
sds sdscatvprintf(sds s, const char *fmt, va_list ap) {
    va_list cpy;
    char staticbuf[1024], *buf = staticbuf, *t;
    size_t buflen = strlen(fmt)*2;

    /* We try to start using a static buffer for speed.
     * If not possible we revert to heap allocation. */
    /**
     * 我们为了速度更快，在开始时尝试使用静态数组
     * 如果不可以，我们再使用堆分配的内存
     **/
    if (buflen > sizeof(staticbuf)) {
        buf = s_malloc(buflen);
        if (buf == NULL) return NULL;
    } else {
        buflen = sizeof(staticbuf);
    }

    /* Try with buffers two times bigger every time we fail to
     * fit the string in the current buffer size. */
    /**
     * 每当我们在当前大小下没有办法将字符串放入，
     * 我们尝试将数组大小扩大到两倍
     **/
    while(1) {
        buf[buflen-2] = '\0';
        va_copy(cpy,ap);
        vsnprintf(buf, buflen, fmt, cpy);   // 将可变参数格式化到数组中
        va_end(cpy);
        if (buf[buflen-2] != '\0') {    // 数组长度不够
            if (buf != staticbuf) s_free(buf);  // 使用堆空间，释放内存
            buflen *= 2;    // 内存扩大为两倍
            buf = s_malloc(buflen); // 重新分配内存
            if (buf == NULL) return NULL;
            continue;
        }
        break;
    }

    /* Finally concat the obtained string to the SDS string and return it. */
    /**
     * 将 buf 指向的 C 风格的字符串追加到 s 后面
     */
    t = sdscat(s, buf);
    if (buf != staticbuf) s_free(buf);  // 使用堆内存，则手动释放内存
    return t;
}

/* Append to the sds string 's' a string obtained using printf-alike format
 * specifier.
 *
 * After the call, the modified sds string is no longer valid and all the
 * references must be substituted with the new pointer returned by the call.
 *
 * Example:
 *
 * s = sdsnew("Sum is: ");
 * s = sdscatprintf(s,"%d+%d = %d",a,b,a+b).
 *
 * Often you need to create a string from scratch with the printf-alike
 * format. When this is the need, just use sdsempty() as the target string:
 *
 * s = sdscatprintf(sdsempty(), "... your format ...", args);
 */
/**
 * 使用和 printf  相似的格式说明符获得一个字符串，
 * 并将这个字符串添加到 sds 字符串后面
 * 
 * 调用这个函数之后， 被修改的 sds 字符串不在有效，
 * 所有的引用都必须使用这个函数返回的新的引用替代
 * 
 * 例如:
 * s = sdsnew("Sum is: ");
 * s = sdscatprintf(s,"%d+%d = %d",a,b,a+b).
 * 
 * 你经常会使用一个类似 printf() 函数获得的字符串来初始化一个创建的字符串
 * 当这是需要的时候， 只需使用 sdsempty() 函数作为目标字符串即可:
 * 
 * s = sdscatprintf(sdsempty(), "... your format ...", args);
 **/
sds sdscatprintf(sds s, const char *fmt, ...) {
    va_list ap;
    char *t;
    va_start(ap, fmt);  // 获得 ... 的参数
    t = sdscatvprintf(s,fmt,ap);    // 将 ap 中的参数通过 fmt 格式化后的字符串添加到 sds 字符串后
    va_end(ap);
    return t;
}

/* This function is similar to sdscatprintf, but much faster as it does
 * not rely on sprintf() family functions implemented by the libc that
 * are often very slow. Moreover directly handling the sds string as
 * new data is concatenated provides a performance improvement.
 *
 * However this function only handles an incompatible subset of printf-alike
 * format specifiers:
 *
 * %s - C String
 * %S - SDS string
 * %i - signed int
 * %I - 64 bit signed integer (long long, int64_t)
 * %u - unsigned int
 * %U - 64 bit unsigned integer (unsigned long long, uint64_t)
 * %% - Verbatim "%" character.
 */
/**
 * 这个函数和 sdscatprintf() 函数是相似的，但是它更快一些，
 * 因为它不依赖 sprintf() 族函数，
 * 这些函数是由库实现的，经常是比较慢的。
 * 更直接的处理 sds 字符串来完成字符串的连接效率会有不小的提升
 * 
 * 但是这个函数只处理和类 printf() 的不相容的格式说明符
 * 
 * %s - C 风格字符串
 * %S - SDS 类型字符串
 * %i - 有符号整数
 * %I - 64 位有符号长长整数 (long long, int64_t)
 * %u - 无符号整数
 * %U - 64 位无符号长长整数 (unsigned long long, uint64_t)
 * %% - 经转义的 “%” 字符.
 **/
sds sdscatfmt(sds s, char const *fmt, ...) {
    size_t initlen = sdslen(s);
    const char *f = fmt;
    long i;
    va_list ap;

    va_start(ap,fmt);
    f = fmt;    /* Next format specifier byte to process. */    // 将要处理的下一个格式说明符字节
    i = initlen; /* Position of the next byte to write to dest str. */  // 需要写入目的字符串的下一个字节位置
    while(*f) {
        char next, *str;
        size_t l;
        long long num;
        unsigned long long unum;

        /* Make sure there is always space for at least 1 char. */
        // 确保有至少一个字节的空间
        if (sdsavail(s)==0) {   // sds 里为空
            s = sdsMakeRoomFor(s,1);    // 分配一个字节
        }

        switch(*f) {
        case '%':
            next = *(f+1);
            f++;
            switch(next) {
            case 's':   // 字符串
            case 'S':
                str = va_arg(ap,char*); // 从可变参数中取出一个字符串
                l = (next == 's') ? strlen(str) : sdslen(str);  // 字符串长度，C 风格或者 sds 类型
                if (sdsavail(s) < l) {  // SDS 类型的字符串 s 后没有足够的空间
                    s = sdsMakeRoomFor(s,l);    // 扩大内存
                }
                memcpy(s+i,str,l);  // 拷贝
                sdsinclen(s,l); // 更新记录长度的值
                i += l; // 增加 sds 中下一个可写的字节位置
                break;
            case 'i':
            case 'I':
                if (next == 'i')
                    num = va_arg(ap,int);
                else
                    num = va_arg(ap,long long);
                {
                    char buf[SDS_LLSTR_SIZE];
                    l = sdsll2str(buf,num);
                    if (sdsavail(s) < l) {
                        s = sdsMakeRoomFor(s,l);
                    }
                    memcpy(s+i,buf,l);
                    sdsinclen(s,l);
                    i += l;
                }
                break;
            case 'u':   // 有符号整数
            case 'U':
                if (next == 'u')
                    unum = va_arg(ap,unsigned int);
                else
                    unum = va_arg(ap,unsigned long long);
                {
                    char buf[SDS_LLSTR_SIZE];
                    l = sdsull2str(buf,unum);   // 将数字转换为字符串
                    if (sdsavail(s) < l) {  // 内存不够
                        s = sdsMakeRoomFor(s,l);    // 扩大内存
                    }
                    memcpy(s+i,buf,l);  // 拷贝
                    sdsinclen(s,l);
                    i += l;
                }
                break;
            default: /* Handle %% and generally %<unknown>. */  // 处理转义的 “%” 和一般的 %后是位置字符
                s[i++] = next;
                sdsinclen(s,1);
                break;
            }
            break;
        default:   // 其他非格式说明符
            s[i++] = *f;    // 直接拷贝
            sdsinclen(s,1); 
            break;
        }
        f++;
    }
    va_end(ap);

    /* Add null-term */
    s[i] = '\0';    // 添加空结束符
    return s;
}

/* Remove the part of the string from left and from right composed just of
 * contiguous characters found in 'cset', that is a null terminted C string.
 *
 * After the call, the modified sds string is no longer valid and all the
 * references must be substituted with the new pointer returned by the call.
 *
 * Example:
 *
 * s = sdsnew("AA...AA.a.aa.aHelloWorld     :::");
 * s = sdstrim(s,"Aa. :");
 * printf("%s\n", s);
 *
 * Output will be just "Hello World".
 */
/**
 * 移除 sds 类型的字符串左右边的中 cset 出现的字符，
 * cset 是一个以空结束的 C 风格字符串
 * 
 * 调用这个函数之后， 被修改的 sds 字符串不再有效，
 * 所有的引用必须被这个函数返回的新的指针替换
 * 
 * 例如:
 * 
 * s = sdsnew("AA...AA.a.aa.aHelloWorld     :::");
 * s = sdstrim(s,"Aa. :");
 * printf("%s\n", s);
 * 
 * 输出将是“Hello World”
 **/
sds sdstrim(sds s, const char *cset) {
    char *start, *end, *sp, *ep;
    size_t len;

    sp = start = s;
    ep = end = s+sdslen(s)-1;
    while(sp <= end && strchr(cset, *sp)) sp++; // 左边查找
    while(ep > sp && strchr(cset, *ep)) ep--;   // 右边查找
    len = (sp > ep) ? 0 : ((ep-sp)+1);  // 去除后字符串的长度
    if (s != sp) memmove(s, sp, len);   // 截取字符串
    s[len] = '\0';
    sdssetlen(s,len);   // 更新 SDS 中记录的字符串长度
    return s;
}

/* Turn the string into a smaller (or equal) string containing only the
 * substring specified by the 'start' and 'end' indexes.
 *
 * start and end can be negative, where -1 means the last character of the
 * string, -2 the penultimate character, and so forth.
 *
 * The interval is inclusive, so the start and end characters will be part
 * of the resulting string.
 *
 * The string is modified in-place.
 *
 * Example:
 *
 * s = sdsnew("Hello World");
 * sdsrange(s,1,-1); => "ello World"
 */
/**
 * 将给定字符串转换为有 'start' 和 ’end‘ 下标指定的字符串
 * 
 * start 和 end 可以为负数， -1 表示字符串的最后一个字符，
 * -2 表示倒数第二个字符，以此类推
 * 
 * 最终截取的字符串是包含 start 和 end 指向的字符的
 * 字符串会按照相应的位置被修改的
 * 
 * 例如:
 * 
 * s = sdsnew("Hello World");
 * sdsrange(s,1,-1); => "ello World"
 **/
void sdsrange(sds s, ssize_t start, ssize_t end) {
    size_t newlen, len = sdslen(s);

    if (len == 0) return;
    if (start < 0) {    // start 为负数时， 调整其值
        start = len+start;
        if (start < 0) start = 0;
    }
    if (end < 0) {  // end 为负数时， 调整其值
        end = len+end;
        if (end < 0) end = 0;
    }
    newlen = (start > end) ? 0 : (end-start)+1;
    if (newlen != 0) {  // 非法值， 调整 start 或 end 的值
        if (start >= (ssize_t)len) {    
            newlen = 0;
        } else if (end >= (ssize_t)len) {
            end = len-1;
            newlen = (start > end) ? 0 : (end-start)+1;
        }
    } else {
        start = 0;
    }
    if (start && newlen) memmove(s, s+start, newlen);
    s[newlen] = 0;
    sdssetlen(s,newlen);
}

/* Apply tolower() to every character of the sds string 's'. */
void sdstolower(sds s) {
    size_t len = sdslen(s), j;

    for (j = 0; j < len; j++) s[j] = tolower(s[j]);
}

/* Apply toupper() to every character of the sds string 's'. */
void sdstoupper(sds s) {
    size_t len = sdslen(s), j;

    for (j = 0; j < len; j++) s[j] = toupper(s[j]);
}

/* Compare two sds strings s1 and s2 with memcmp().
 *
 * Return value:
 *
 *     positive if s1 > s2.
 *     negative if s1 < s2.
 *     0 if s1 and s2 are exactly the same binary string.
 *
 * If two strings share exactly the same prefix, but one of the two has
 * additional characters, the longer string is considered to be greater than
 * the smaller one. */
int sdscmp(const sds s1, const sds s2) {
    size_t l1, l2, minlen;
    int cmp;

    l1 = sdslen(s1);
    l2 = sdslen(s2);
    minlen = (l1 < l2) ? l1 : l2;
    cmp = memcmp(s1,s2,minlen);
    if (cmp == 0) return l1>l2? 1: (l1<l2? -1: 0);
    return cmp;
}

/* Split 's' with separator in 'sep'. An array
 * of sds strings is returned. *count will be set
 * by reference to the number of tokens returned.
 *
 * On out of memory, zero length string, zero length
 * separator, NULL is returned.
 *
 * Note that 'sep' is able to split a string using
 * a multi-character separator. For example
 * sdssplit("foo_-_bar","_-_"); will return two
 * elements "foo" and "bar".
 *
 * This version of the function is binary-safe but
 * requires length arguments. sdssplit() is just the
 * same function but for zero-terminated strings.
 */
sds *sdssplitlen(const char *s, ssize_t len, const char *sep, int seplen, int *count) {
    int elements = 0, slots = 5;
    long start = 0, j;
    sds *tokens;

    if (seplen < 1 || len < 0) return NULL;

    tokens = s_malloc(sizeof(sds)*slots);
    if (tokens == NULL) return NULL;

    if (len == 0) {
        *count = 0;
        return tokens;
    }
    for (j = 0; j < (len-(seplen-1)); j++) {
        /* make sure there is room for the next element and the final one */
        if (slots < elements+2) {
            sds *newtokens;

            slots *= 2;
            newtokens = s_realloc(tokens,sizeof(sds)*slots);
            if (newtokens == NULL) goto cleanup;
            tokens = newtokens;
        }
        /* search the separator */
        if ((seplen == 1 && *(s+j) == sep[0]) || (memcmp(s+j,sep,seplen) == 0)) {
            tokens[elements] = sdsnewlen(s+start,j-start);
            if (tokens[elements] == NULL) goto cleanup;
            elements++;
            start = j+seplen;
            j = j+seplen-1; /* skip the separator */
        }
    }
    /* Add the final element. We are sure there is room in the tokens array. */
    tokens[elements] = sdsnewlen(s+start,len-start);
    if (tokens[elements] == NULL) goto cleanup;
    elements++;
    *count = elements;
    return tokens;

cleanup:
    {
        int i;
        for (i = 0; i < elements; i++) sdsfree(tokens[i]);
        s_free(tokens);
        *count = 0;
        return NULL;
    }
}

/* Free the result returned by sdssplitlen(), or do nothing if 'tokens' is NULL. */
void sdsfreesplitres(sds *tokens, int count) {
    if (!tokens) return;
    while(count--)
        sdsfree(tokens[count]);
    s_free(tokens);
}

/* Append to the sds string "s" an escaped string representation where
 * all the non-printable characters (tested with isprint()) are turned into
 * escapes in the form "\n\r\a...." or "\x<hex-number>".
 *
 * After the call, the modified sds string is no longer valid and all the
 * references must be substituted with the new pointer returned by the call. */
sds sdscatrepr(sds s, const char *p, size_t len) {
    s = sdscatlen(s,"\"",1);
    while(len--) {
        switch(*p) {
        case '\\':
        case '"':
            s = sdscatprintf(s,"\\%c",*p);
            break;
        case '\n': s = sdscatlen(s,"\\n",2); break;
        case '\r': s = sdscatlen(s,"\\r",2); break;
        case '\t': s = sdscatlen(s,"\\t",2); break;
        case '\a': s = sdscatlen(s,"\\a",2); break;
        case '\b': s = sdscatlen(s,"\\b",2); break;
        default:
            if (isprint(*p))
                s = sdscatprintf(s,"%c",*p);
            else
                s = sdscatprintf(s,"\\x%02x",(unsigned char)*p);
            break;
        }
        p++;
    }
    return sdscatlen(s,"\"",1);
}

/* Helper function for sdssplitargs() that returns non zero if 'c'
 * is a valid hex digit. */
int is_hex_digit(char c) {
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') ||
           (c >= 'A' && c <= 'F');
}

/* Helper function for sdssplitargs() that converts a hex digit into an
 * integer from 0 to 15 */
int hex_digit_to_int(char c) {
    switch(c) {
    case '0': return 0;
    case '1': return 1;
    case '2': return 2;
    case '3': return 3;
    case '4': return 4;
    case '5': return 5;
    case '6': return 6;
    case '7': return 7;
    case '8': return 8;
    case '9': return 9;
    case 'a': case 'A': return 10;
    case 'b': case 'B': return 11;
    case 'c': case 'C': return 12;
    case 'd': case 'D': return 13;
    case 'e': case 'E': return 14;
    case 'f': case 'F': return 15;
    default: return 0;
    }
}

/* Split a line into arguments, where every argument can be in the
 * following programming-language REPL-alike form:
 *
 * foo bar "newline are supported\n" and "\xff\x00otherstuff"
 *
 * The number of arguments is stored into *argc, and an array
 * of sds is returned.
 *
 * The caller should free the resulting array of sds strings with
 * sdsfreesplitres().
 *
 * Note that sdscatrepr() is able to convert back a string into
 * a quoted string in the same format sdssplitargs() is able to parse.
 *
 * The function returns the allocated tokens on success, even when the
 * input string is empty, or NULL if the input contains unbalanced
 * quotes or closed quotes followed by non space characters
 * as in: "foo"bar or "foo'
 */
sds *sdssplitargs(const char *line, int *argc) {
    const char *p = line;
    char *current = NULL;
    char **vector = NULL;

    *argc = 0;
    while(1) {
        /* skip blanks */
        while(*p && isspace(*p)) p++;
        if (*p) {
            /* get a token */
            int inq=0;  /* set to 1 if we are in "quotes" */
            int insq=0; /* set to 1 if we are in 'single quotes' */
            int done=0;

            if (current == NULL) current = sdsempty();
            while(!done) {
                if (inq) {
                    if (*p == '\\' && *(p+1) == 'x' &&
                                             is_hex_digit(*(p+2)) &&
                                             is_hex_digit(*(p+3)))
                    {
                        unsigned char byte;

                        byte = (hex_digit_to_int(*(p+2))*16)+
                                hex_digit_to_int(*(p+3));
                        current = sdscatlen(current,(char*)&byte,1);
                        p += 3;
                    } else if (*p == '\\' && *(p+1)) {
                        char c;

                        p++;
                        switch(*p) {
                        case 'n': c = '\n'; break;
                        case 'r': c = '\r'; break;
                        case 't': c = '\t'; break;
                        case 'b': c = '\b'; break;
                        case 'a': c = '\a'; break;
                        default: c = *p; break;
                        }
                        current = sdscatlen(current,&c,1);
                    } else if (*p == '"') {
                        /* closing quote must be followed by a space or
                         * nothing at all. */
                        if (*(p+1) && !isspace(*(p+1))) goto err;
                        done=1;
                    } else if (!*p) {
                        /* unterminated quotes */
                        goto err;
                    } else {
                        current = sdscatlen(current,p,1);
                    }
                } else if (insq) {
                    if (*p == '\\' && *(p+1) == '\'') {
                        p++;
                        current = sdscatlen(current,"'",1);
                    } else if (*p == '\'') {
                        /* closing quote must be followed by a space or
                         * nothing at all. */
                        if (*(p+1) && !isspace(*(p+1))) goto err;
                        done=1;
                    } else if (!*p) {
                        /* unterminated quotes */
                        goto err;
                    } else {
                        current = sdscatlen(current,p,1);
                    }
                } else {
                    switch(*p) {
                    case ' ':
                    case '\n':
                    case '\r':
                    case '\t':
                    case '\0':
                        done=1;
                        break;
                    case '"':
                        inq=1;
                        break;
                    case '\'':
                        insq=1;
                        break;
                    default:
                        current = sdscatlen(current,p,1);
                        break;
                    }
                }
                if (*p) p++;
            }
            /* add the token to the vector */
            vector = s_realloc(vector,((*argc)+1)*sizeof(char*));
            vector[*argc] = current;
            (*argc)++;
            current = NULL;
        } else {
            /* Even on empty input string return something not NULL. */
            if (vector == NULL) vector = s_malloc(sizeof(void*));
            return vector;
        }
    }

err:
    while((*argc)--)
        sdsfree(vector[*argc]);
    s_free(vector);
    if (current) sdsfree(current);
    *argc = 0;
    return NULL;
}

/* Modify the string substituting all the occurrences of the set of
 * characters specified in the 'from' string to the corresponding character
 * in the 'to' array.
 *
 * For instance: sdsmapchars(mystring, "ho", "01", 2)
 * will have the effect of turning the string "hello" into "0ell1".
 *
 * The function returns the sds string pointer, that is always the same
 * as the input pointer since no resize is needed. */
sds sdsmapchars(sds s, const char *from, const char *to, size_t setlen) {
    size_t j, i, l = sdslen(s);

    for (j = 0; j < l; j++) {
        for (i = 0; i < setlen; i++) {
            if (s[j] == from[i]) {
                s[j] = to[i];
                break;
            }
        }
    }
    return s;
}

/* Join an array of C strings using the specified separator (also a C string).
 * Returns the result as an sds string. */
sds sdsjoin(char **argv, int argc, char *sep) {
    sds join = sdsempty();
    int j;

    for (j = 0; j < argc; j++) {
        join = sdscat(join, argv[j]);
        if (j != argc-1) join = sdscat(join,sep);
    }
    return join;
}

/* Like sdsjoin, but joins an array of SDS strings. */
sds sdsjoinsds(sds *argv, int argc, const char *sep, size_t seplen) {
    sds join = sdsempty();
    int j;

    for (j = 0; j < argc; j++) {
        join = sdscatsds(join, argv[j]);
        if (j != argc-1) join = sdscatlen(join,sep,seplen);
    }
    return join;
}

/* Wrappers to the allocators used by SDS. Note that SDS will actually
 * just use the macros defined into sdsalloc.h in order to avoid to pay
 * the overhead of function calls. Here we define these wrappers only for
 * the programs SDS is linked to, if they want to touch the SDS internals
 * even if they use a different allocator. */
void *sds_malloc(size_t size) { return s_malloc(size); }
void *sds_realloc(void *ptr, size_t size) { return s_realloc(ptr,size); }
void sds_free(void *ptr) { s_free(ptr); }

#if defined(SDS_TEST_MAIN)
#include <stdio.h>
#include "testhelp.h"
#include "limits.h"

#define UNUSED(x) (void)(x)
int sdsTest(void) {
    {
        sds x = sdsnew("foo"), y;

        test_cond("Create a string and obtain the length",
            sdslen(x) == 3 && memcmp(x,"foo\0",4) == 0)

        sdsfree(x);
        x = sdsnewlen("foo",2);
        test_cond("Create a string with specified length",
            sdslen(x) == 2 && memcmp(x,"fo\0",3) == 0)

        x = sdscat(x,"bar");
        test_cond("Strings concatenation",
            sdslen(x) == 5 && memcmp(x,"fobar\0",6) == 0);

        x = sdscpy(x,"a");
        test_cond("sdscpy() against an originally longer string",
            sdslen(x) == 1 && memcmp(x,"a\0",2) == 0)

        x = sdscpy(x,"xyzxxxxxxxxxxyyyyyyyyyykkkkkkkkkk");
        test_cond("sdscpy() against an originally shorter string",
            sdslen(x) == 33 &&
            memcmp(x,"xyzxxxxxxxxxxyyyyyyyyyykkkkkkkkkk\0",33) == 0)

        sdsfree(x);
        x = sdscatprintf(sdsempty(),"%d",123);
        test_cond("sdscatprintf() seems working in the base case",
            sdslen(x) == 3 && memcmp(x,"123\0",4) == 0)

        sdsfree(x);
        x = sdsnew("--");
        x = sdscatfmt(x, "Hello %s World %I,%I--", "Hi!", LLONG_MIN,LLONG_MAX);
        test_cond("sdscatfmt() seems working in the base case",
            sdslen(x) == 60 &&
            memcmp(x,"--Hello Hi! World -9223372036854775808,"
                     "9223372036854775807--",60) == 0)
        printf("[%s]\n",x);

        sdsfree(x);
        x = sdsnew("--");
        x = sdscatfmt(x, "%u,%U--", UINT_MAX, ULLONG_MAX);
        test_cond("sdscatfmt() seems working with unsigned numbers",
            sdslen(x) == 35 &&
            memcmp(x,"--4294967295,18446744073709551615--",35) == 0)

        sdsfree(x);
        x = sdsnew(" x ");
        sdstrim(x," x");
        test_cond("sdstrim() works when all chars match",
            sdslen(x) == 0)

        sdsfree(x);
        x = sdsnew(" x ");
        sdstrim(x," ");
        test_cond("sdstrim() works when a single char remains",
            sdslen(x) == 1 && x[0] == 'x')

        sdsfree(x);
        x = sdsnew("xxciaoyyy");
        sdstrim(x,"xy");
        test_cond("sdstrim() correctly trims characters",
            sdslen(x) == 4 && memcmp(x,"ciao\0",5) == 0)

        y = sdsdup(x);
        sdsrange(y,1,1);
        test_cond("sdsrange(...,1,1)",
            sdslen(y) == 1 && memcmp(y,"i\0",2) == 0)

        sdsfree(y);
        y = sdsdup(x);
        sdsrange(y,1,-1);
        test_cond("sdsrange(...,1,-1)",
            sdslen(y) == 3 && memcmp(y,"iao\0",4) == 0)

        sdsfree(y);
        y = sdsdup(x);
        sdsrange(y,-2,-1);
        test_cond("sdsrange(...,-2,-1)",
            sdslen(y) == 2 && memcmp(y,"ao\0",3) == 0)

        sdsfree(y);
        y = sdsdup(x);
        sdsrange(y,2,1);
        test_cond("sdsrange(...,2,1)",
            sdslen(y) == 0 && memcmp(y,"\0",1) == 0)

        sdsfree(y);
        y = sdsdup(x);
        sdsrange(y,1,100);
        test_cond("sdsrange(...,1,100)",
            sdslen(y) == 3 && memcmp(y,"iao\0",4) == 0)

        sdsfree(y);
        y = sdsdup(x);
        sdsrange(y,100,100);
        test_cond("sdsrange(...,100,100)",
            sdslen(y) == 0 && memcmp(y,"\0",1) == 0)

        sdsfree(y);
        sdsfree(x);
        x = sdsnew("foo");
        y = sdsnew("foa");
        test_cond("sdscmp(foo,foa)", sdscmp(x,y) > 0)

        sdsfree(y);
        sdsfree(x);
        x = sdsnew("bar");
        y = sdsnew("bar");
        test_cond("sdscmp(bar,bar)", sdscmp(x,y) == 0)

        sdsfree(y);
        sdsfree(x);
        x = sdsnew("aar");
        y = sdsnew("bar");
        test_cond("sdscmp(bar,bar)", sdscmp(x,y) < 0)

        sdsfree(y);
        sdsfree(x);
        x = sdsnewlen("\a\n\0foo\r",7);
        y = sdscatrepr(sdsempty(),x,sdslen(x));
        test_cond("sdscatrepr(...data...)",
            memcmp(y,"\"\\a\\n\\x00foo\\r\"",15) == 0)

        {
            unsigned int oldfree;
            char *p;
            int step = 10, j, i;

            sdsfree(x);
            sdsfree(y);
            x = sdsnew("0");
            test_cond("sdsnew() free/len buffers", sdslen(x) == 1 && sdsavail(x) == 0);

            /* Run the test a few times in order to hit the first two
             * SDS header types. */
            for (i = 0; i < 10; i++) {
                int oldlen = sdslen(x);
                x = sdsMakeRoomFor(x,step);
                int type = x[-1]&SDS_TYPE_MASK;

                test_cond("sdsMakeRoomFor() len", sdslen(x) == oldlen);
                if (type != SDS_TYPE_5) {
                    test_cond("sdsMakeRoomFor() free", sdsavail(x) >= step);
                    oldfree = sdsavail(x);
                }
                p = x+oldlen;
                for (j = 0; j < step; j++) {
                    p[j] = 'A'+j;
                }
                sdsIncrLen(x,step);
            }
            test_cond("sdsMakeRoomFor() content",
                memcmp("0ABCDEFGHIJABCDEFGHIJABCDEFGHIJABCDEFGHIJABCDEFGHIJABCDEFGHIJABCDEFGHIJABCDEFGHIJABCDEFGHIJABCDEFGHIJ",x,101) == 0);
            test_cond("sdsMakeRoomFor() final length",sdslen(x)==101);

            sdsfree(x);
        }
    }
    test_report()
    return 0;
}
#endif

#ifdef SDS_TEST_MAIN
int main(void) {
    return sdsTest();
}
#endif
