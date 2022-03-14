#include <malloc.h>
#include <memory.h>
#include "array_list.h"

struct array_list_t {
    void *data;
    size_t item_size;
    int capacity;
    int size;
};

static void ensure_capacity(array_list_t *list, int new_size);

static inline void *item_at(array_list_t *list, int index);

array_list_t *array_list_create(size_t item_size, int initial_capacity) {
    array_list_t *list = calloc(1, sizeof(array_list_t));
    list->data = malloc(initial_capacity * item_size);
    list->item_size = item_size;
    list->capacity = initial_capacity;
    list->size = 0;
    return list;
}

void array_list_destroy(array_list_t *list) {
    free(list->data);
    free(list);
}

void *array_list_get(array_list_t *list, int index) {
    if (index >= list->size || index < 0) return NULL;
    return item_at(list, index);
}

void *array_list_add(array_list_t *list, int index) {
    if (index > list->size) return NULL;
    ensure_capacity(list, list->size + 1);
    if (index < 0) {
        list->size += 1;
        return item_at(list, list->size - 1);
    } else {
        memmove(item_at(list, index), item_at(list, index + 1), (list->size - index) * list->item_size);
        list->size += 1;
        return item_at(list, index);
    }
}

int array_list_size(const array_list_t *list) {
    return list->size;
}

static void ensure_capacity(array_list_t *list, int new_size) {
    if (list->capacity >= new_size) return;
    list->capacity *= 2;
    list->data = realloc(list->data, list->capacity * list->item_size);
}

static inline void *item_at(array_list_t *list, int index) {
    return list->data + index * list->item_size;
}