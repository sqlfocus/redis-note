本系列文档摘录redis的实现逻辑，备忘


注意事项
--------
1) 源码redis-3.2.0
2) 源码目录: ~/src
3) 除代码注释外, 重要的想法将以.brief文件的形式放置在~目录中


Redis简介
---------
Redis是一个开源的使用ANSI C语言编写、支持网络、可基于内存亦可持久化的日志型、
Key-Value数据库，并提供多种语言的API

Redis is what is called a key-value store, often referred to as NoSQL 
database, 采用键值存储，通常称为非关系型数据库


常用的指令
----------
    SET  xxx x-value        设置某个关键字，指定其值
    SETNX xxx x-val         如果不存在则设置关键字, 指定其值
    GET  xxx                获取关键字的值
    DEL  xxx                删除关键字
    INCR xxx                增加关键字的值
    EXPIRE xxx              设置过期时限
    TTL xxx                 查看过期时限


支持的数据类型
--------------
    list
        RPUSH   list  val   在尾部插入数据
        LPUSH   list  val   在头部插入数据
        LLEN    list        统计链表长度
        LRANGE  list  beg-id end-id 罗列始末id之间的元素
        LPOP    list        弹出头部数据
        RPOP    list        弹出尾部数据

    set
        SADD    set  val    集合中加入元素
        SREM    set  val    从集合移除元素
        SISMEMBER   set val 判断是否在集合中
        SUNION  set1 set2 ...   组合两个或多个集合，返回组合结果

    sorted set
        ZADD    set  sorted-score val   排序集合加入元素
        ZRANGE  set beg-id end-id       按照sorted-score排序后，按区间输出

    hash
        HSET    key:key-val val1:val1-val   插入hash表
        HMSET   key:key-val val1:val1-val val2:val2-val ...
                                            多个值插入hash表
        HGETALL key:key-val                 获取key对应的所有val
        HGET    key:key-val val1            获取key对应的某个val
        HINCRBY key:key-val val1            增加某个键值的值
        HDEL    key:key-val val1            删除某个键值


持久化方法，RDB/AOF
-------------------
RDB: 将当前内存中的数据库快照保存到磁盘文件中, 在Redis重启动时, RDB程序
        可以通过载入RDB文件来还原数据库的状态
AOF: 持久化记录服务器执行的所有写操作命令, 并在服务器启动时, 通过重新执行
        这些命令来还原数据集

