typedef struct sl_link_t {
    struct sl_link_t *next; /**< The next element */
} sl_link_t;

typedef struct slist_t {
    sl_link_t *head;    /**< Head of the list */
    sl_link_t *tail;    /**< Tail of the list for fast append */
    int head_offset; /**< Offset of head into element structure */
} slist_t;

typedef struct foo {
    slist_t list;
} foo;
#define NULL 0

#define SL_FOREACH(cur_elem, sl_head)                               \
    for (sl_link_t *cur_elem = (sl_head)->head;                     \
         cur_elem != NULL;                                          \
         cur_elem = cur_elem->next)

int main() {
    foo bar;
    SL_FOREACH(cur, &bar.list) {
    }
    return 0;
}
