## Go语言
---

-   ### 线程调度器 GMP 机制与原理
    <div align=center>
        <img src="../pictures/gmp.png" width = "500" height = "400" alt="图片名称" />
    </div>
    
    #### 模型分析
    -   **G（Goroutine）**：存在于两个主要的队列，一个是Processor本身拥有的本地队列，另外一个是全局队列（Global Queue）
        -   本地队列有约束，最多不能超过256个，超过则会添加到全局队列里面去
        -   全局队列默认无约束，是以前老调度器遗留下来的兜底逻辑，已经被弱化了
    -   **M（Machine）**：每个 M 都代表了 1 个内核线程，OS 调度器负责把内核线程分配到 CPU 的核上执行，这部分交给操作系统去运作，并不做干扰
        -   M 与 P 的数量没有绝对关系，当队列中有很多就绪任务时，优先寻找空闲的M，如果没有空闲的M，就会创建M，因此即使 P 的**默认数量**是 1，也有可能会创建很多个 M 出来
    -   **P（Processor）**：一种逻辑队列，专门用于存放各个goroutine的地方
        -   个数由环境变量 $GOMAXPROCS 或者是由 runtime 的方法 GOMAXPROCS() 决定
        -   在程序启动时就会数量全部创建出来，创建goroutine就会优先放在这些本地队列里面去

    #### 调度设计策略
    -   线程复用
        -   work stealing 机制：当Machine（线程）绑定的Processor上没有Goroutine时，会尝试从其他Processor的队列中偷取Goroutine，如果其他的Processor也都没有Goroutine，则尝试从全局队列里面偷取
        -   hand off 机制：当Machine（线程）绑定上面的Goroutine产生了系统调用（耗时较长），Machine（线程）会主动释放Processor，让出给其余的Machine（线程）去绑定并调度，而当前的Machine（线程）则会继续继续等待系统调用的完成
    -   并行：
        -   最多有GOMAXPROCS个P在同时运行，可以约束最大并行的数量，更好地利用CPU资源
    -   抢占：
        -   与一般的用户态协程（Coroutine）不一样，Goroutine只能最多占据CPU 10ms的时间，一旦时间到达，必须让出CPU，否则有可能其余的Goroutine会被饿死，Goroutine是允许抢占式调度的，这就是 Goroutine 不同于 Coroutine 的一个地方。

    #### go func() 调度流程
    <div align=center>
        <img src="../pictures/gmp_run.png" width = "500" height = "400" alt="图片名称" />
    </div>

    -   go func () 创建一个 Goroutine
    -   优先保存在Processor的本地队列，满了就会放到全局队列
    -   Goroutine 只能运行在 Machine（线程） 中，一个 Machine（线程）运行前必须先持有一个 Processor，M 与 P 是 1：1 的关系
    -   Machine（线程） 会从 Processor 的本地队列弹出一个可执行状态的 Goroutine 来执行，如果 Processor 的本地队列为空，就会想其他的 Processor 偷取一个可执行的 G 来执行
    -   Machine（线程）执行某一个 Goroutine 时候如果发生了 syscall 或则其余阻塞操作，Machine（线程） 会阻塞
    -   如果当前有一些 Goroutine 就绪待执行，runtime 会把这个Machine（线程） 从 Processor 中摘除 (detach)，释放Processor，然后尝试从空闲Machine（线程）中获取，如果没有空闲的Machine（线程）则创建一个
---
