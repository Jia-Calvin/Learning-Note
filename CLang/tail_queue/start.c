#include <stdio.h>
#include <stdlib.h>
#include <sys/queue.h>


// 尾队列数据结构如下图所示：               
//                                            Entry                              Entry                                 Entry
//                              - - - - >- - - - - - - - - -            - - >- - - - - - - - - -            - - >- - - - - - - - - -
//                              |        |                 |            |    |                 |            |    |                 |                      
//                              |        |      data       |            |    |      data       |            |    |      data       |                      
//            Head              |        |                 |            |    |                 |            |    |                 |                          
//     | - - - - - - - - |< - - + - - -  | - - - - - - - - |< - -       |    | - - - - - - - - |< -         |    | - - - - - - - - |< - - - - -               
//     | |  *tqh_first | |- - - |     |  | |  *tqe_next  | |- - + - - - -    | |  *tqe_next  | |- + - - - - |    | |  *tqe_next  | |- - |     | 
//     | - - - - - - - - |            |  | - - - - - - - - |    |            | - - - - - - - - |  |              | - - - - - - - - |    |     |           
//     | |  **tqh_last | |- - -       - -| |  **tqe_prev | |    | - - - - - -| |  **tqe_prev | |  | - - - - - - -| |  **tqe_prev | |    |     |       
//     | - - - - - - - - |    |          | - - - - - - - - |                 | - - - - - - - - |                 | - - - - - - - - |    |     |       
//                            |          |                 |                 |                 |                 |                 |    |     |   
//                            |          |                 |                 |                 |                 |                 |    |     |       
//                            |          |      data       |                 |      data       |                 |      data       |    - - - + - - - > NULL        
//                            |          |                 |                 |                 |                 |                 |          |       
//                            |          |                 |                 |                 |                 |                 |          |           
//                            |          |                 |                 |                 |                 |                 |          |       
//                            |          - - - - - - - - - -                 - - - - - - - - - -                 - - - - - - - - - -          |           
//                            |                                                                                                               |   
//                            - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - |                                                                                                                
// 
// 
// （1）头节点中不包含数据，头节点的所占的内存空间大小是与entry实体中TAILQ_ENTRY一致的，所以他们之间可以强制类型转换（源码中许多地方都用到）
// （2）使用c语言sys/queue.h里面的队列时，需要将TAILQ_ENTRY（包含了tqe_next与tqe_prev）和数据包裹在同一个结构体当中使用
// （3）struct tailq_entry **tqe_prev 指向（其保存的值是它们的地址，所以才能够指向）前一个entry的tqe_next或者是head的tqh_first
// （4）解释一下**，指针的指针的用处，例如说struct tailq_entry **tqe_prev/**tqh_last，这其实可以从字面上直接观察得到，
//     **tqe_prev意味着需要解两次引用才可以得到类型struct tailq_entry，也就是说我们可以完全不管第一次解引用到的类型，而我们只需要保证第二次解引用必须为struct tailq_entry
//     实际上也是这样的，当我们 *tqe_prev = 得到的是前一个tailq_entry的tqe_next里的值，而再解一次*(*tqe_prev)，则根据tqe_next可以得到自身的结构体struct tailq_entry的一个起始地址。
// （5）解释一下TAILQ_PREV(elm, headname, field)的原理：
//     #define	TAILQ_PREV(elm, headname, field)     (*(((struct headname *)((elm)->field.tqe_prev))->tqh_last))	
//     解释： 首先它利用(elm)->field.tqe_prev，拿到了前一个实体的tqe_next地址，这时候它强制类型转换（感觉是因为无法转去实体，才转去头节点，但其实都一样），那么此时tqe_next地址开始到
//     **tqe_prev（是前一个实体的）结束这段地址就被认为了头节点，其实数据类型是完全一样的，所以只是简单的转换而已，那么利用taq_last与tqe_prev在内存块的同个地方，将其->tqh_last其实是再tqe_prev
//     寻找到了再前一个实体的tqe_next地址，然后利用解引用*号，找到了elem的前一个实体的地址位置，之后就完成了找到前一个实体的位置了。
// （6）TAILQ_INSERT_TAIL（插入到尾队列后面），源码中这一步很容易造成混淆*(head)->tqh_last = (elm)，其实它不是改变tqh_last的指向，它是为了改变原来的最后一个实体的tqe_next，原因很简单，它
//      它解除了一次引用(*(head->tqh_last))，这时候对其赋值是对最后一个实体中的tqe_next赋值的，也就是说这句语句是为了改变原本在队列里的最后一个实体的tqe_next，让它指向最新的elem。另外一个语句：
//      (head)->tqh_last = &TAILQ_NEXT((elm), field)，这时候没有解开引用，改变的就是本身tqh_last的值，这时候赋予其elm中tqe_next的起始地址（用了引用，没毛病）
// （7）TAILQ_INIT在文中已经说了



// 24字节
struct tailq_entry
{
    // 2个8字节
    TAILQ_ENTRY(tailq_entry)
    entries;
    // 4字节 + 4字节(字节对齐)
    int value;
};

int main()
{
    // 申明头节点
    TAILQ_HEAD(tailq_head_name, tailq_entry)
    tailq_head;
    struct tailq_entry *iterm;
    printf("%lu\n", sizeof(int));
    printf("%lu\n", sizeof(struct tailq_entry *));
    printf("%lu\n", sizeof(struct tailq_entry **));
    printf("%lu\n", sizeof(struct tailq_entry));

    // 需要注明的是：
    // sizeof(struct tailq_entry *) = 8，存放指针所需要的空间
    // sizeof(struct tailq_entry) = 24，存放结构体 tailq_entry 所需要的空间

    TAILQ_INIT(&tailq_head);
    // TAILQ_INIT初始化后，队列头节点的有以下结构图，地址为假设的
    // 可以看到此时的tqh_last指向的是tqh_first的地址（为了在插入时对边界依然有效）
    //           0x111            0x111 + 8 = 0x119（一般）
    // - - - - - - - - - - - - - - - - - - - - - - - - - -
    // |     tqh_first = 0x0    |     tqh_last = 0x111   |
    // - - - - - - - - - - - - - - - - - - - - - - - - - -
    printf("%p\n", &tailq_head.tqh_first);
    printf("%p\n", tailq_head.tqh_first);
    printf("%p\n", &tailq_head.tqh_last);
    printf("%p\n", tailq_head.tqh_last);

    printf("\n");
    for (int i = 0; i < 10; i++)
    {
        iterm = malloc(sizeof(struct tailq_entry));
        iterm->value = i;
        TAILQ_INSERT_TAIL(&tailq_head, iterm, entries);
    }

    struct tailq_entry *first = tailq_head.tqh_first;
    printf("%p\n", first);
    printf("%d\n", first->value);

    // struct tailq_entry *first_next = TAILQ_NEXT(first, entries);
    struct tailq_entry *first_next = first->entries.tqe_next;
    printf("%p\n", first_next);
    printf("%d\n", first_next->value);

    struct tailq_entry *first_next_next = first_next->entries.tqe_next;
    printf("%p\n", first_next_next);
    printf("%d\n", first_next_next->value);

    struct tailq_entry **last = tailq_head.tqh_last;
    printf("%p\n", last);

    printf("\n");
    printf("%d\n", TAILQ_PREV(first_next_next, tailq_head_name, entries)->value);
    printf("%p\n", (*((struct tailq_head_name *)(first_next_next->entries.tqe_prev))->tqh_last));
    printf("%p\n", first_next);
    printf("%p\n", ((struct tailq_head_name *)(first_next_next->entries.tqe_prev))->tqh_last);
    printf("%p\n", &first->entries.tqe_next);

    // FOREACH 的用法
    printf("\n");
    struct tailq_entry *iterm_tmp;
    TAILQ_FOREACH(iterm_tmp, &tailq_head, entries)
    {
        printf("value is %d\n", iterm_tmp->value);
    }

    return 0;
}