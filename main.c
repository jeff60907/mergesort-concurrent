#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "threadpool.h"
#include "list.h"

//#define USAGE "usage: ./sort [thread_count] [input_count]\n"
#define USAGE "usage: ./sort [thread_count] [file_name]\n"

struct {
    pthread_mutex_t mutex;
    int cut_thread_count;
} data_context;

static llist_t *tmp_list;
static llist_t *the_list = NULL;

static int thread_count = 0, data_count = 0, max_cut = 0;
static tpool_t *pool = NULL;

llist_t *merge_list(llist_t *a, llist_t *b)
{
    llist_t *_list = list_new();
    node_t *current = NULL;
    while (a->size && b->size) {
        int cmp = strcmp((char *)a->head->data, (char *)b->head->data);
        llist_t *small = (llist_t *)
                         ((intptr_t) a * (cmp <= 0) + (intptr_t) b * (cmp > 0));
        if (current) {
            current->next = small->head;
            current = current->next;
        } else {
            _list->head = small->head;
            current = _list->head;
        }
        small->head = small->head->next;
        --small->size;
        ++_list->size;
        current->next = NULL;
    }

    llist_t *remaining = (llist_t *) ((intptr_t) a * (a->size > 0) +
                                      (intptr_t) b * (b->size > 0));
    if (current) current->next = remaining->head;
    _list->size += remaining->size;
    free(a);
    free(b);
    return _list;
}

llist_t *merge_sort(llist_t *list)
{
    if (list->size < 2)
        return list;
    int mid = list->size / 2;
    llist_t *left = list;
    llist_t *right = list_new();
    right->head = list_nth(list, mid);
    right->size = list->size - mid;
    list_nth(list, mid - 1)->next = NULL;
    left->size = mid;
    return merge_list(merge_sort(left), merge_sort(right));
}

void merge(void *data)
{
    llist_t *_list = (llist_t *) data;
    if (_list->size < (uint32_t) data_count) {
        pthread_mutex_lock(&(data_context.mutex));
        llist_t *_t = tmp_list;
        if (!_t) {
            tmp_list = _list;
            pthread_mutex_unlock(&(data_context.mutex));
        } else {
            tmp_list = NULL;
            pthread_mutex_unlock(&(data_context.mutex));
            task_t *_task = (task_t *) malloc(sizeof(task_t));
            _task->func = merge;
            _task->arg = merge_list(_list, _t);
            tqueue_push(pool->queue, _task);
        }
    } else {
        the_list = _list;
        task_t *_task = (task_t *) malloc(sizeof(task_t));
        _task->func = NULL;
        tqueue_push(pool->queue, _task);
        list_print(_list);
    }
}

void cut_func(void *data)
{
    llist_t *list = (llist_t *) data;
    pthread_mutex_lock(&(data_context.mutex));
    int cut_count = data_context.cut_thread_count;
    if (list->size > 1 && cut_count < max_cut) {
        ++data_context.cut_thread_count;
        pthread_mutex_unlock(&(data_context.mutex));

        /* cut list */
        int mid = list->size / 2;
        llist_t *_list = list_new();
        _list->head = list_nth(list, mid);
        _list->size = list->size - mid;
        list_nth(list, mid - 1)->next = NULL;
        list->size = mid;

        /* create new task: left */
        task_t *_task = (task_t *) malloc(sizeof(task_t));
        _task->func = cut_func;
        _task->arg = _list;
        tqueue_push(pool->queue, _task);

        /* create new task: right */
        _task = (task_t *) malloc(sizeof(task_t));
        _task->func = cut_func;
        _task->arg = list;
        tqueue_push(pool->queue, _task);
    } else {
        pthread_mutex_unlock(&(data_context.mutex));
        merge(merge_sort(list));
    }
}

static void *task_run(void *data)
{
    (void) data;
    while (1) {
        task_t *_task = tqueue_pop(pool->queue);
        if (_task) {
            if (!_task->func) {
                tqueue_push(pool->queue, _task);
                break;
            } else {
                _task->func(_task->arg);
                free(_task);
            }
        }
    }
    pthread_exit(NULL);
}

void check_data()
{
    FILE *word, *data;
    data = fopen("output.txt", "r");
    word = fopen("words.txt", "r");
    char output[16], answer[16];
    int count = 0;
    while(fgets(output, sizeof(output), data) != NULL) {
        fgets(answer, sizeof(answer), word);
        if(strcmp(answer, output) != 0) {
            printf("Wrong %d data %s\n", ++count, output);
        }
        ++count;
    }
    printf("data are right\n");
    fclose(word);
    fclose(data);
}

int main(int argc, char const *argv[])
{
    const char *input_file;
    char line[64];
    FILE *fp;

    if (argc < 3) {
        printf(USAGE);
        return -1;
    }
    thread_count = atoi(argv[1]);
    input_file = argv[2];
    fp = fopen(input_file, "r");
    max_cut = thread_count * (thread_count <= data_count) +
              data_count * (thread_count > data_count) - 1;

    /* Read data */
    the_list = list_new();

    /* FIXME: remove all all occurrences of printf and scanf*/
    while(fgets(line, sizeof(line), fp)!= NULL) {
        line[strlen(line)-1] = '\0';
        list_add(the_list, line);
    }

    /* initialize tasks inside thread pool */
    pthread_mutex_init(&(data_context.mutex), NULL);
    data_context.cut_thread_count = 0;
    tmp_list = NULL;
    pool = (tpool_t *) malloc(sizeof(tpool_t));
    tpool_init(pool, thread_count, task_run);

    /* launch the first task */
    task_t *_task = (task_t *) malloc(sizeof(task_t));
    _task->func = cut_func;
    _task->arg = the_list;
    tqueue_push(pool->queue, _task);

    /* release thread pool */
    tpool_free(pool);
    fclose(fp);
    check_data();
    return 0;
}



