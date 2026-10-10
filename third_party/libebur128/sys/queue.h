/*
 * Simple BSD queue.h implementation for STAILQ used by libebur128
 */
#pragma once

#ifndef _USE_MATH_DEFINES
#define _USE_MATH_DEFINES
#endif

#define STAILQ_HEAD(name, type)                                \
struct name {                                                  \
    struct type *stqh_first; /* first element */               \
    struct type **stqh_last; /* addr of last next element */   \
}

#define STAILQ_HEAD_INITIALIZER(head)                          \
    { NULL, &(head).stqh_first }

#define STAILQ_ENTRY(type)                                     \
struct {                                                       \
    struct type *stqe_next;  /* next element */                \
}

#define STAILQ_INIT(head) do {                                 \
    (head)->stqh_first = NULL;                                 \
    (head)->stqh_last = &(head)->stqh_first;                   \
} while (0)

#define STAILQ_INSERT_TAIL(head, elm, field) do {              \
    (elm)->field.stqe_next = NULL;                             \
    *(head)->stqh_last = (elm);                                \
    (head)->stqh_last = &(elm)->field.stqe_next;               \
} while (0)

#define STAILQ_FIRST(head)      ((head)->stqh_first)
#define STAILQ_NEXT(elm, field) ((elm)->field.stqe_next)
#define STAILQ_EMPTY(head)      ((head)->stqh_first == NULL)

#define STAILQ_FOREACH(var, head, field)                       \
    for((var) = STAILQ_FIRST(head);                            \
       (var);                                                  \
       (var) = STAILQ_NEXT(var, field))

#define STAILQ_REMOVE_HEAD(head, field) do {                   \
    if (((head)->stqh_first = (head)->stqh_first->field.stqe_next) == NULL) \
        (head)->stqh_last = &(head)->stqh_first;               \
} while (0)
