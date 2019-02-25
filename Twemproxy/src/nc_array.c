/*
    twemproxy基本数据结构之一：数组
    这种自定义的数组类型模拟“栈”的使用，含有函数top, push, pop，所有的操作都利用指针进行
    （1）array_create
        函数为elem元素申请空间的同时为自定义数组结构体也申请内存空间
    （2）array_init
        函数只对elem元素申请空间，而将结构体数组空间问题交给调用者
    （3）array_destroy
        销毁函数对应于create函数，将元素elem以及结构体内存都释放
    （4）array_deinit
        销毁函数对应于init函数，只释放结构体内的元素elem
    （5）array_idx
        求出这个元素在内存空间上的相对偏移量，再从偏移量除以每个元素的size，则得到indx
    （6）array_push
        利用已被使用的元素空间大小(size以及nelem记录)，返回接下来可以使用的内存空间指针（已插入数据的数组尾部），而这里并不会进行字段的赋值，这个赋值交给了调用者，当push的时候遇到内存申请空间不足的情况下，则需要再申请空间，再申请的大小为原来空间大小的2倍
    （7）array_pop
        将已插入数据的数组尾部的最后一个元素的起始地址返回，并不进行空间释放，防止再申请，因为可以覆盖的写入数据
    （8）array_get
        从起始地址+indx*size，则为需要的元素地址，返回其指针
    （9）array_top
        直接调用array_get(n-1)则可以返回数组尾部元素
    （10）array_swap
        交换两个指针即可
    （11）array_sort
        将元素排序，需要传入可以比较器
    （12）array_each
        遍历数组内的每一个元素，利用传金来的func函数进行处理

*/
#include <stdlib.h>

#include <nc_core.h>

struct array *
array_create(uint32_t n, size_t size)
{
    struct array *a;

    ASSERT(n != 0 && size != 0);

    // 为这个a结构体申请内存空间，堆上申请
    a = nc_alloc(sizeof(*a));
    if (a == NULL) {
        return NULL;
    }
    // 为elem元素申请内存空间
    a->elem = nc_alloc(n * size);
    if (a->elem == NULL) {
        nc_free(a);
        return NULL;
    }

    a->nelem = 0;
    a->size = size;
    a->nalloc = n;

    return a;
}

// create 对应 destroy， 同时释放了结构体的空间
void
array_destroy(struct array *a)
{
    array_deinit(a);
    nc_free(a);
}

// 不同于array_create在于init不对结构体的空间做干涉，堆上不含 *a 的空间，是局部变量，会随时释放
rstatus_t
array_init(struct array *a, uint32_t n, size_t size)
{
    ASSERT(n != 0 && size != 0);

    a->elem = nc_alloc(n * size);
    if (a->elem == NULL) {
        return NC_ENOMEM;
    }

    a->nelem = 0;
    a->size = size;
    a->nalloc = n;

    return NC_OK;
}

// deinit 对应 init，只释放了结构体内部的n * size空间，即elem，元素空间
void
array_deinit(struct array *a)
{
    ASSERT(a->nelem == 0);

    if (a->elem != NULL) {
        nc_free(a->elem);
    }
}

// 求出这个元素在内存空间上的相对偏移量，再从偏移量除以每个元素的size，则得到indx
uint32_t
array_idx(struct array *a, void *elem)
{
    uint8_t *p, *q;
    uint32_t off, idx;

    ASSERT(elem >= a->elem);

    // 得到的是元素块在内存的起始位置
    p = a->elem;
    q = elem;

    // elem这个元素的内存地址减去起始地址则为偏移地址量
    off = (uint32_t)(q - p);

    // 诊断是否整除，按道理来说是该整除的
    ASSERT(off % (uint32_t)a->size == 0);

    idx = off / (uint32_t)a->size;

    return idx;
}

void *
array_push(struct array *a)
{
    void *elem, *new;
    size_t size;

    if (a->nelem == a->nalloc) {

        /* the array is full; allocate new array */
        size = a->size * a->nalloc;
        // 申请两倍的空间，不会只申请一部分，防止经常性调用系统函数申请内存空间
        new = nc_realloc(a->elem, 2 * size);
        if (new == NULL) {
            return NULL;
        }

        a->elem = new;
        a->nalloc *= 2;
    }

    // 这个是起始地址加上偏移量，就到n的那个内存空间位置，将其返回需要调用者自己设置字段值，这里只是将偏移位置传出
    elem = (uint8_t *)a->elem + a->size * a->nelem;
    a->nelem++;

    return elem;
}

void *
array_pop(struct array *a)
{
    void *elem;

    ASSERT(a->nelem != 0);

    // 从起始地址+偏移量，则为需要的元素地址，这里先进行了n--操作，实则是n-1
    a->nelem--;
    elem = (uint8_t *)a->elem + a->size * a->nelem;

    return elem;
}

void *
array_get(struct array *a, uint32_t idx)
{
    void *elem;

    ASSERT(a->nelem != 0);
    ASSERT(idx < a->nelem);

    // 从起始地址+偏移量，则为需要的元素地址
    elem = (uint8_t *)a->elem + (a->size * idx);

    return elem;
}

void *
array_top(struct array *a)
{
    ASSERT(a->nelem != 0);

    return array_get(a, a->nelem - 1);
}

void
array_swap(struct array *a, struct array *b)
{
    struct array tmp;

    tmp = *a;
    *a = *b;
    *b = tmp;
}

/*
 * Sort nelem elements of the array in ascending order based on the
 * compare comparator.
 */
void
array_sort(struct array *a, array_compare_t compare)
{
    ASSERT(a->nelem != 0);

    qsort(a->elem, a->nelem, a->size, compare);
}

/*
 * Calls the func once for each element in the array as long as func returns
 * success. On failure short-circuits and returns the error status.
 */
rstatus_t
array_each(struct array *a, array_each_t func, void *data)
{
    uint32_t i, nelem;

    ASSERT(array_n(a) != 0);
    ASSERT(func != NULL);

    // 对2个 解析配置池 转换到 服务器池内
    // array_n(a) = 2
    for (i = 0, nelem = array_n(a); i < nelem; i++) {
        void *elem = array_get(a, i);
        rstatus_t status;

        status = func(elem, data);
        if (status != NC_OK) {
            return status;
        }
    }

    return NC_OK;
}
