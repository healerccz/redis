/* adlist.c - A generic doubly linked list implementation
 *
 * Copyright (c) 2006-2010, Salvatore Sanfilippo <antirez at gmail dot com>
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


#include <stdlib.h>
#include "adlist.h"
#include "zmalloc.h"

/* Create a new list. The created list can be freed with
 * AlFreeList(), but private value of every node need to be freed
 * by the user before to call AlFreeList().
 *
 * On error, NULL is returned. Otherwise the pointer to the new list. */
/**
 * 创建一个新链表，被创建的链表可以使用 AlFreeList() 函数进行释放，
 * 但是在调用 AlFreeList() 函数前，用户需要自己释放每个结点的私有值
 * 
 * 出错时，返回NULL， 否则返回指向新链表的指针
 **/
list *listCreate(void)
{
    struct list *list;  // 定义指向链表类型的指针

    if ((list = zmalloc(sizeof(*list))) == NULL)    // 为链表分配空间， zmalloc() 函数是作者对 malloc() 函数的封装，
                                                    // 包含了对 malloc() 函数的出错处理
        return NULL;
    list->head = list->tail = NULL; // 将指向链表首结点和尾结点的指针初始化为空
    list->len = 0;  // 将链表长度初始化为 0
    list->dup = NULL;   // 将链表复制函数指针初始化为 NULL
    list->free = NULL;  // 将链表释放函数指针初始化为 NULL
    list->match = NULL; // 将链表比较函数指针初始化为 NULL
    return list;   // 返回指向新创建的链表的指针
}

/* Remove all the elements from the list without destroying the list itself. */
/**
 * 清除链表的所有元素，但不销毁链表本身
 **/
void listEmpty(list *list)
{
    unsigned long len;  // 存储链表的长度
    listNode *current, *next;   // 链表的当前结点指针和后继结点指针

    current = list->head;   // 将指向当前结点的指针设置为链表的第一个结点
    len = list->len;
    while(len--) {
        next = current->next;   // 设置后继结点指针的值
        if (list->free) list->free(current->value); // 如果链表有释放函数，则调用之，释放该结点的值
                                                    // 用户在链表中存储的值需要单独释放，如结点中的值涉及动态分配内存空间，
                                                    // 就需要定义释放该结点的函数，否则会造成内存泄露
        zfree(current); // 释放该结点
        current = next; // 指针后移
    }
    list->head = list->tail = NULL; // 将指向首结点和尾结点的指针重置为NULL
    list->len = 0;  // 重置链表长度
}

/* Free the whole list.
 *
 * This function can't fail. */
/**
 * 释放整个结点
 * 
 * 这个函数不会失败
 **/
void listRelease(list *list)
{
    listEmpty(list);    // 释放链表结点
    zfree(list);    // 释放链表类型
}

/* Add a new node to the list, to head, containing the specified 'value'
 * pointer as value.
 *
 * On error, NULL is returned and no operation is performed (i.e. the
 * list remains unaltered).
 * On success the 'list' pointer you pass to the function is returned. */
/**
 * 使用头插法向链表中插入一个结点，新结点是 value 指针作为值
 * 
 * 出错时，返回 NULL 并且是没有任何操作被执行的(链表没有被修改)
 * 成功时传入函数的指向链表首地址 list 将会被返回
 **/
list *listAddNodeHead(list *list, void *value)
{
    listNode *node; 

    if ((node = zmalloc(sizeof(*node))) == NULL)    // 为新结点分配空间
        return NULL;    // 出错时返回 NULL 指针
    node->value = value;    // 设置新结点的值(存储 value 里的地址)
    if (list->len == 0) {   // 插入之前链表长度为0
        list->head = list->tail = node;
        node->prev = node->next = NULL;
    } else {    // 插入之前链表长度不为0
        node->prev = NULL;
        node->next = list->head;
        list->head->prev = node;
        list->head = node;
    }
    list->len++;    // 链表长度加1
    return list;    // 返回链表首地址
}

/* Add a new node to the list, to tail, containing the specified 'value'
 * pointer as value.
 *
 * On error, NULL is returned and no operation is performed (i.e. the
 * list remains unaltered).
 * On success the 'list' pointer you pass to the function is returned. */
/**
 * 使用尾插法向链表中插入一个结点，新结点是 value 指针作为值
 * 
 * 出错时，NULL 被返回并且没有任何操作(链表没有修改)
 * 成功时，返回传入函数参数的指向链表首地址
 **/ 
list *listAddNodeTail(list *list, void *value)
{
    listNode *node;

    if ((node = zmalloc(sizeof(*node))) == NULL)    // 为新结点分配空间
        return NULL;    // 出错时返回 NULL
    node->value = value;    // 设置新结点的值
    if (list->len == 0) {   // 插入前链表长度为0时
        list->head = list->tail = node;
        node->prev = node->next = NULL;
    } else {    // 插入前链表长度不为0
        node->prev = list->tail;
        node->next = NULL;
        list->tail->next = node;
        list->tail = node;
    }
    list->len++;    // 链表长度加1
    return list;
}

/**
 * 向链表的 old_node 前/后插入新结点
 * 当 after 为 0 时， 在 old_node 前插入新结点
 * 当 after 非 0 时， 在 old_node 后插入新结点
 **/
list *listInsertNode(list *list, listNode *old_node, void *value, int after) {
    listNode *node;

    if ((node = zmalloc(sizeof(*node))) == NULL)    // 为新结点分配空间
        return NULL;
    node->value = value;    // 设置结点的值
    if (after) {    // 在 old_node 后插入
        node->prev = old_node;
        node->next = old_node->next;
        if (list->tail == old_node) {   // 新结点是尾结点
            list->tail = node;
        }
    } else {    // 在 old_node 前插入
        node->next = old_node;
        node->prev = old_node->prev;
        if (list->head == old_node) {   // 新结点是首结点
            list->head = node;
        }
    }
    if (node->prev != NULL) {   // 新结点不是首结点
        node->prev->next = node;
    }
    if (node->next != NULL) {   // 新结点不是尾结点
        node->next->prev = node;
    }
    list->len++;    // 链表长度加1
    return list;
}

/* Remove the specified node from the specified list.
 * It's up to the caller to free the private value of the node.
 *
 * This function can't fail. */
/**
 * 从指定的链表中删除指定的结点
 * 取决于释放结点私有值的调用者
 * 
 * 这个函数是不会失败的
 **/
void listDelNode(list *list, listNode *node)
{
    if (node->prev) // 如果node 有前驱结点
        node->prev->next = node->next;  // node 的前驱结点的后继指针指向node 的后继结点
    else    // node 没有前驱结点，即node 为链表的首结点
        list->head = node->next;    // 链表的头指针指向node 的后继结点
    if (node->next) // 如果node 有后继结点
        node->next->prev = node->prev;  // node 的后继结点的前驱指针指向node 的前驱结点
    else    // node 没有后继结点，即node 为尾结点
        list->tail = node->prev;    // 尾指针前移，指向node 结点的前驱结点(新的尾结点)
    if (list->free) list->free(node->value);    // 如果定义了链表的释放函数，调用该函数，为了释放结点的值
    zfree(node);    // 释放结点空间
    list->len--;    // 链表长度减一
}

/* Returns a list iterator 'iter'. After the initialization every
 * call to listNext() will return the next element of the list.
 *
 * This function can't fail. */
/**
 * 返回一个链表的迭代器，在初始化之后，每次调用 listNext() 函数
 * 将返回指向链表下一元素的迭代器
 **/
listIter *listGetIterator(list *list, int direction)
{
    listIter *iter;

    if ((iter = zmalloc(sizeof(*iter))) == NULL) return NULL;   // 为链表迭代器分配内存，发生错误时返回NULL
    if (direction == AL_START_HEAD) // 如果迭代器为正向迭代器
        iter->next = list->head;    // 迭代器设置指向链表首结点
    else    // 迭代器为反向迭代器
        iter->next = list->tail;    // 迭代器设置指向链表尾结点
    iter->direction = direction;    // 设置迭代器类型
    return iter;    // 返回指向迭代器的指针
}

/* Release the iterator memory */
/**
 * 释放迭代器空间
 **/
void listReleaseIterator(listIter *iter) {
    zfree(iter);
}

/* Create an iterator in the list private iterator structure */
/**
 * 将迭代器li 设置成链表list 的正向迭代器
 **/
void listRewind(list *list, listIter *li) {
    li->next = list->head;  // 将迭代器中的指针指向链表的首结点
    li->direction = AL_START_HEAD;  // 将迭代器的方向设置为正向
}

/**
 * 将迭代器li 设置为链表list 的反向迭代器
 **/
void listRewindTail(list *list, listIter *li) { 
    li->next = list->tail;  // 将迭代器中的指针指向链表的尾结点
    li->direction = AL_START_TAIL;  // 将迭代器的方向设置为反方向
}

/* Return the next element of an iterator.
 * It's valid to remove the currently returned element using
 * listDelNode(), but not to remove other elements.
 *
 * The function returns a pointer to the next element of the list,
 * or NULL if there are no more elements, so the classical usage patter
 * is:
 *
 * iter = listGetIterator(list,<direction>);
 * while ((node = listNext(iter)) != NULL) {
 *     doSomethingWith(listNodeValue(node));
 * }
 *
 * */
/**
 * 返回指向下一元素的迭代器
 * 使用 listDelNode 删除当前返回的元素是有效的,但是没有删除其他元素
 * 
 * 这个函数返回一个指向链表下一元素的指针，如果没有更多的元素，则返回 NULL指针，
 * 因此典型使用模式如下:
 * 
 * iter = listGetIterator(list,<direction>);
 * while ((node = listNext(iter)) != NULL) {
 *     doSomethingWith(listNodeValue(node));
 * }
 * 
 **/
listNode *listNext(listIter *iter)
{
    listNode *current = iter->next; // current 存储迭代器中指向的结点地址

    if (current != NULL) {  // 判断当前迭代器指向的是否为空
        // 根据迭代器的类型，决定迭代器的移动方向
        if (iter->direction == AL_START_HEAD)   // 正向迭代器  
            iter->next = current->next; // 指针后移
        else    // 反向迭代器
            iter->next = current->prev; // 指针前移
    }
    return current;
}

/* Duplicate the whole list. On out of memory NULL is returned.
 * On success a copy of the original list is returned.
 *
 * The 'Dup' method set with listSetDupMethod() function is used
 * to copy the node value. Otherwise the same pointer value of
 * the original node is used as value of the copied node.
 *
 * The original list both on success or error is never modified. */
/**
 * 复制整个链表，当内存不足时返回 NULL 指针，
 * 成功时原链表的复制品首地址被返回。
 * 
 * listSetDupMethod() 函数设置了 Dup 方法，整个函数被用来复制结点值，
 * 否则原始节点的相同指针值被用作复制节点的值。
 * 
 * 无论成功还是失败，原链表都不会被修改
 **/
list *listDup(list *orig)
{
    list *copy; // 新链表
    listIter iter;  // 拷贝中使用的迭代器
    listNode *node; // 指向链表结点的指针

    if ((copy = listCreate()) == NULL)  // 创建一个空链表(实际上就是链表的头结点)，并初始化表头、尾指针等变量的值
        return NULL;    // 发生错误时，返回NULL 指针
    copy->dup = orig->dup;  // 复制链表orig 的复制函数指针
    copy->free = orig->free;    // 复制链表orig 的释放函数指针 
    copy->match = orig->match;  // 复制链表orig 的比较函数指针
    listRewind(orig, &iter);    // 将迭代器iter 绑定到链表orig 上，初始指向链表orign 的首结点
    while((node = listNext(&iter)) != NULL) {   // 当iter 没有到链表的结尾时，说明还没有复制完
        void *value;    // 中间变量，暂存结点的值的指针

        if (copy->dup) {    // 如果该结点值有复制函数，则调用之
            value = copy->dup(node->value); // 调用复制函数，复制结点node 的值，可能是深拷贝
            if (value == NULL) {    // 值指针指向空，出错
                listRelease(copy);  // 释放整个链表
                return NULL;    // 返回空指针
            }
        } else  // 没有复制函数
            value = node->value;    // 直接进行复制
        if (listAddNodeTail(copy, value) == NULL) { // 将结点以尾插法的方式插入新链表中
            listRelease(copy);  // 出错时释放整个链表
            return NULL;    // 出错返回NULL指针
        }
    }
    return copy;    // 复制完成，返回新链表的表头地址
}

/* Search the list for a node matching a given key.
 * The match is performed using the 'match' method
 * set with listSetMatchMethod(). If no 'match' method
 * is set, the 'value' pointer of every node is directly
 * compared with the 'key' pointer.
 *
 * On success the first matching node pointer is returned
 * (search starts from head). If no matching node exists
 * NULL is returned. */
/**
 * 在链表中寻找与给定值相匹配的结点
 * 匹配在执行时使用 listSetMatchMethod() 函数。
 * 如果没有匹配函数，每个结点的 value 指针将直接与 key 指针比较
 * 
 * 成功时返回第一个相匹配的结点指针
 * (查找从链表头部开始)，如果没有匹配，则返回 NULL 指针
 **/
listNode *listSearchKey(list *list, void *key)
{
    listIter iter;
    listNode *node;

    listRewind(list, &iter);    // 将迭代器iter 与list 正向绑定，初始状态指向链表list 的头结点
    while((node = listNext(&iter)) != NULL) {   // 迭代器"自增"
        if (list->match) {  // 如果有比较函数指针，则调用之
            if (list->match(node->value, key)) {    // 判断是否相等
                return node;    // 相等则返回与之相等的结点的指针
            }
        } else {    // 没有比较函数，则直接比较
            if (key == node->value) {   // 值相等
                return node;    // 返回该结点的地址
            }
        }
    }
    return NULL;    // 没有与之相等的结点，则返回NULL指针
}

/* Return the element at the specified zero-based index
 * where 0 is the head, 1 is the element next to head
 * and so on. Negative integers are used in order to count
 * from the tail, -1 is the last element, -2 the penultimate
 * and so on. If the index is out of range NULL is returned. */
/**
 * 返回几点在以0开始的下标，其中0是链表的首结点，1是下一个结点，以此类推。
 * 负数被使用是为了从链表尾部开始计数，-1是最后一个结点，-2是倒数第二个结点
 * 以此类推。如果索引值超出范围，则返回 NULL 指针
 **/
listNode *listIndex(list *list, long index) {
    listNode *n;

    if (index < 0) {    // 处理下标为负数的情况
        index = (-index)-1; // 将下标转换为整数
        n = list->tail; // 并将指针n 初始状态指向链表尾结点
        while(index-- && n) n = n->prev;    // 从尾部循环遍历链表，直到找到下标为index的结点或遍历完链表时结束
    } else {    // 下标为正数
        n = list->head; // 将指针n 初始状态指向链表首结点
        while(index-- && n) n = n->next;    // 从首部循环遍历链表，知道找到下标为index 的结点或遍历完链表时结束
    }
    return n;   // 返回查询的结果
}

/* Rotate the list removing the tail node and inserting it to the head. */
/**
 * 将链表list 的尾结点插入到头结点的后面，首结点的前面
 **/
void listRotate(list *list) {
    listNode *tail = list->tail; // 备份指向尾结点的指针

    if (listLength(list) <= 1) return;  //如果链表的长度为1，则不做任何操作

    // 将尾结点从链表中解除
    list->tail = tail->prev;    // 尾指针前移，前移至原为节点的前驱结点
    list->tail->next = NULL;    // 将新的尾结点的后继指针设为空
    // 将原尾结点插入到头结点的后面
    list->head->prev = tail;    // 将原首结点的前驱指针指向原尾结点
    tail->prev = NULL;  // 原尾结点的前驱指针设为空
    tail->next = list->head;    // 原尾点的后继指针指向原首结点
    list->head = tail;  // 链表头指针指向原尾结点
}

/* Add all the elements of the list 'o' at the end of the
 * list 'l'. The list 'other' remains empty but otherwise valid. */
void listJoin(list *l, list *o) {
    if (o->head)    //如果链表不为空
        o->head->prev = l->tail;    // 将链表o 的首结点连接到链表尾部

    if (l->tail)    // 如果链表l 的尾结点存在
        l->tail->next = o->head;    // 链表l 的尾结点指向链表o 的首结点
    else    // 如果链表l为空，则链表l的头指针指向链表o 的首结点
        l->head = o->head;

    if (o->tail) l->tail = o->tail; // 如果链表o 有尾结点，则将链表l 的尾指针指向链表o 的尾结点
    l->len += o->len;

    /* Setup other as an empty list. */
    o->head = o->tail = NULL;   // 将链表o 的头、尾指针置为NULL
    o->len = 0; // 将链表o 的长度置为0
}
