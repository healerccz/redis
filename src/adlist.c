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
 * 创建一个空链表
 **/
list *listCreate(void)
{
    struct list *list;  // 定义指向链表类型的指针

    if ((list = zmalloc(sizeof(*list))) == NULL)    // 为链表申请空间，zmalloc()函数是作者对函数malloc()的封装，里面包含了作者对该函数出错的处理
        return NULL;
    list->head = list->tail = NULL; // 将指向链表头结点和尾结点的指针初始化为空
    list->len = 0;  // 将链表长度初始化为0
    list->dup = NULL;   // 将链表复制函数指针初始化为NULL
    list->free = NULL;  // 将链表释放函数指针初始化为NULL
    list->match = NULL; // 将链表比较函数指针初始化为NULL
    return list;   // 返回指向新创建链表的指针，即新创建的链表首结点的指针
}


/**
 * 将链表清空(表头结点仍存在)
 **/
void listEmpty(list *list)
{
    unsigned long len;  // 用于存储链表长度
    listNode *current, *next;   // 指向链表的当前结点和下一结点

    current = list->head;   // 将指向当前结点的指针初始化为链表的首结点地址
    len = list->len;    // 初始化链表长度
    /**
     * 先释放结点的值(结点值不为空)，后释放结点
     * 因为结点的值类型不确定，需要定义函数去释放结点的值，防止内存泄露
     **/
    while(len--) {
        next = current->next;   // 初始化next 指针
        if (list->free) list->free(current->value); // 若list 的释放函数存在，则调用该函数释放该结点的值
        zfree(current); // 释放当前结点
        current = next; // 指针后移
    }
    list->head = list->tail = NULL; // 将头指针和尾指针赋值为空
    list->len = 0;  // 链表长度设为空
}

/**
 * 释放整个链表(包括头结点)
 **/
void listRelease(list *list)
{
    listEmpty(list);    // 释放链表的非头结点
    zfree(list);    //释放链表的头结点
}

/* Add a new node to the list, to head, containing the specified 'value'
 * pointer as value.
 *
 * On error, NULL is returned and no operation is performed (i.e. the
 * list remains unaltered).
 * On success the 'list' pointer you pass to the function is returned. */

/**
 * 头插法向链表中添加一个结点
 **/
list *listAddNodeHead(list *list, void *value)
{
    listNode *node;

    if ((node = zmalloc(sizeof(*node))) == NULL)    // 为新结点分配空间
        return NULL;    // 错误时返回NULL指针
    node->value = value;    // 给新结点赋值
    if (list->len == 0) {   // 如果当前链表长度为0
        list->head = list->tail = node; // 将链表的头、尾指针指向新结点
        node->prev = node->next = NULL; // 将新结点的前驱和后继指针指向NULL
    } else {    // 不是链表的第一个结点
        node->prev = NULL;  // 将链表的前驱结点指向空
        node->next = list->head;    // 新结点的后继指针指向链表原首结点
        list->head->prev = node;    // 链表原首结点的前驱结点指向新创建结点
        list->head = node;  // 链表的头结点指向新创建的结点
    }
    list->len++;    // 链表长度加一
    return list;    // 返回指向头结点的指针
}

/* Add a new node to the list, to tail, containing the specified 'value'
 * pointer as value.
 *
 * On error, NULL is returned and no operation is performed (i.e. the
 * list remains unaltered).
 * On success the 'list' pointer you pass to the function is returned. */

/**
 * 尾插法向链表中添加一个结点
 **/
list *listAddNodeTail(list *list, void *value)
{
    listNode *node;

    if ((node = zmalloc(sizeof(*node))) == NULL)    // 为新结点申请空间
        return NULL;    // 发生错误时返回空指针
    node->value = value;    // 为新结点的值赋值
    if (list->len == 0) {   // 如果当前链表长度为0
        list->head = list->tail = node; // 将链表的头指针和尾指针指向新创建的结点
        node->prev = node->next = NULL; // 新结点的前驱指针和后继指针指向NULL
    } else {    // 当前链表长度不为0
        node->prev = list->tail;    // 新创建的的结点的前驱指针指向链表的尾结点
        node->next = NULL;  // 新创建的结点的后继指针指向NULL
        list->tail->next = node;    // 链表原最后的结点的后继指针指向新创建的结点（即指向新的最后的结点）
        list->tail = node;  // 链表尾指针后移，指向尾结点（即新创建的结点）
    }
    list->len++;    // 链表长度加一
    return list;    // 返回指向头结点的指针
}

/**
 * 在链表old_node 结点前/后插入新结点
 * 当after 值为0时，在old_node 前插入新结点
 * 当after 值为非0时，在old_node 后插入新结点
 **/
list *listInsertNode(list *list, listNode *old_node, void *value, int after) {
    listNode *node;

    if ((node = zmalloc(sizeof(*node))) == NULL)    // 为新结点分配空间
        return NULL;    // 发生错误时，返回NULL指针
    node->value = value;    // 为新结点值域赋值
    if (after) {    // after 非0， 在old_node 后插入新结点
        node->prev = old_node;  // 新结点的前驱指针指向old_node 结点
        node->next = old_node->next;    // 新结点的后继指针指向old_node 的下一结点
        if (list->tail == old_node) {   // 如果old_node 为链表的尾结点
            list->tail = node;  // 新结点成为链表的尾结点，尾指针指向新创建的结点
        }
    } else {    // after 为0时，在old_node 前插入新结点
        node->next = old_node;  // 新结点的后继指针指向old_node 结点
        node->prev = old_node->prev;    // 新结点的前驱指针指向old_node 的前驱结点
        if (list->head == old_node) {   // 如果old_node 为链表的首结点
            list->head = node;  // 新结点成为首结点
        }
    }
    if (node->prev != NULL) {   // 新结点不是首结点
        node->prev->next = node;    // 让新结点的前驱结点指向新结点
    }
    if (node->next != NULL) {   // 如果新结点不是尾结点
        node->next->prev = node;    // 新结点的后继结点指向新结点
    }   
    list->len++;    // 链表的长度加一
    return list;    // 返回指向链表头结点的指针
}

/* Remove the specified node from the specified list.
 * It's up to the caller to free the private value of the node.
 *
 * This function can't fail. */

/**
 * 删除指定结点(node结点)
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
 * 获取链表的迭代器
 * direction 决定了迭代器的类型(正向迭代器或反向迭代器)
 * #define AL_START_HEAD 0  // 正向，从头到尾方向移动
 * #define AL_START_TAIL 1  // 反向，从尾到头方向移动
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

/**
 * 释放迭代器的内存空间
 **/
void listReleaseIterator(listIter *iter) {
    zfree(iter);
}

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

/**
 * 迭代器的自增
 * 对于正向迭代器，迭代器指向当前结点的后继结点或者NULL
 * 对与反向迭代器，迭代器指向当前结点的前驱结点或者NULL
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
 * 拷贝链表orig
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
 * 在链表list 中查找值与key 指向的值相同的结点地址
 * 成功时返回链表中第一个与key 指向的值相同的结点的地址
 * 失败时(包括未找到)则返回NULL指针
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
 * 返回指向下标为index 的结点的指针
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
/**
 * 将链表o 加入到链表l 后面，并将链表o 置为空链表(只含头结点)
 **/
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
