#ifndef __CLUSTER_H
#define __CLUSTER_H

/*-----------------------------------------------------------------------------
 * Redis cluster data structures, defines, exported API.
 *----------------------------------------------------------------------------*/

#define CLUSTER_SLOTS 16384
#define CLUSTER_OK 0          /* Everything looks ok */
#define CLUSTER_FAIL 1        /* The cluster can't work */
#define CLUSTER_NAMELEN 40    /* sha1 hex length */
#define CLUSTER_PORT_INCR 10000 /* Cluster port = baseport + PORT_INCR */

/* The following defines are amount of time, sometimes expressed as
 * multiplicators of the node timeout value (when ending with MULT). */
#define CLUSTER_DEFAULT_NODE_TIMEOUT 15000
#define CLUSTER_DEFAULT_SLAVE_VALIDITY 10 /* Slave max data age factor. */
#define CLUSTER_DEFAULT_REQUIRE_FULL_COVERAGE 1
#define CLUSTER_FAIL_REPORT_VALIDITY_MULT 2 /* Fail report validity. */
#define CLUSTER_FAIL_UNDO_TIME_MULT 2 /* Undo fail if master is back. */
#define CLUSTER_FAIL_UNDO_TIME_ADD 10 /* Some additional time. */
#define CLUSTER_FAILOVER_DELAY 5 /* Seconds */
#define CLUSTER_DEFAULT_MIGRATION_BARRIER 1
#define CLUSTER_MF_TIMEOUT 5000 /* Milliseconds to do a manual failover. */
#define CLUSTER_MF_PAUSE_MULT 2 /* Master pause manual failover mult. */
#define CLUSTER_SLAVE_MIGRATION_DELAY 5000 /* Delay for slave migration. */

/* Redirection errors returned by getNodeByQuery(). */
#define CLUSTER_REDIR_NONE 0          /* Node can serve the request. */
#define CLUSTER_REDIR_CROSS_SLOT 1    /* -CROSSSLOT request. */
#define CLUSTER_REDIR_UNSTABLE 2      /* -TRYAGAIN redirection required */
#define CLUSTER_REDIR_ASK 3           /* -ASK redirection required. */
#define CLUSTER_REDIR_MOVED 4         /* -MOVED redirection required. */
#define CLUSTER_REDIR_DOWN_STATE 5    /* -CLUSTERDOWN, global state. */
#define CLUSTER_REDIR_DOWN_UNBOUND 6  /* -CLUSTERDOWN, unbound slot. */

struct clusterNode;

/* clusterLink encapsulates everything needed to talk with a remote node. 
 * 集群模式时, 维护与对端节点通信的数据结构 */
typedef struct clusterLink {
    mstime_t ctime;             /* 连接创建时间 */
    int fd;                     /* tcp插口描述符 */
    sds sndbuf;                 /* Packet send buffer */
    sds rcvbuf;                 /* Packet reception buffer */
    struct clusterNode *node;   /* 对端节点表述结构 */
} clusterLink;

/* Cluster node flags and macros. */
#define CLUSTER_NODE_MASTER 1     /* The node is a master */
#define CLUSTER_NODE_SLAVE 2      /* The node is a slave */
#define CLUSTER_NODE_PFAIL 4      /* Failure? Need acknowledge */
#define CLUSTER_NODE_FAIL 8       /* The node is believed to be malfunctioning */
#define CLUSTER_NODE_MYSELF 16    /* This node is myself */
#define CLUSTER_NODE_HANDSHAKE 32 /* We have still to exchange the first ping */
#define CLUSTER_NODE_NOADDR   64  /* We don't know the address of this node */
#define CLUSTER_NODE_MEET 128     /* Send a MEET message to this node */
#define CLUSTER_NODE_MIGRATE_TO 256 /* Master elegible for replica migration. */
#define CLUSTER_NODE_NULL_NAME "\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000"

#define nodeIsMaster(n) ((n)->flags & CLUSTER_NODE_MASTER)
#define nodeIsSlave(n) ((n)->flags & CLUSTER_NODE_SLAVE)
#define nodeInHandshake(n) ((n)->flags & CLUSTER_NODE_HANDSHAKE)
#define nodeHasAddr(n) (!((n)->flags & CLUSTER_NODE_NOADDR))
#define nodeWithoutAddr(n) ((n)->flags & CLUSTER_NODE_NOADDR)
#define nodeTimedOut(n) ((n)->flags & CLUSTER_NODE_PFAIL)
#define nodeFailed(n) ((n)->flags & CLUSTER_NODE_FAIL)

/* Reasons why a slave is not able to failover. */
#define CLUSTER_CANT_FAILOVER_NONE 0
#define CLUSTER_CANT_FAILOVER_DATA_AGE 1
#define CLUSTER_CANT_FAILOVER_WAITING_DELAY 2
#define CLUSTER_CANT_FAILOVER_EXPIRED 3
#define CLUSTER_CANT_FAILOVER_WAITING_VOTES 4
#define CLUSTER_CANT_FAILOVER_RELOG_PERIOD (60*5) /* seconds. */

/* 本结构记录了下线报告信息, node->fail_reports. */
typedef struct clusterNodeFailReport {
    struct clusterNode *node;  /* 报告目标节点已经下线的节点 */
    mstime_t time;             /* 最后一次收到下线报告的时间, 用于检查
                                    下线报告是否过期 */
} clusterNodeFailReport;

/* 保存一个节点的当前状态 */
typedef struct clusterNode {
    mstime_t ctime;                 /* 节点创建时间; 握手超时以此为基准 */
    char name[CLUSTER_NAMELEN];     /* Node name, hex string, sha1-size */
    int flags;                      /* 节点标识, CLUSTER_NODE_... 
                                        CLUSTER_NODE_MEET: 标识meet
                                        CLUSTER_NODE_MYSELF: 对应标识myself
                                        CLUSTER_NODE_MASTER: 标识master
                                        CLUSTER_NODE_SLAVE: 标识slave
                                        CLUSTER_NODE_PFAIL: 标识fail?
                                        CLUSTER_NODE_FAIL: 标识fail
                                        CLUSTER_NODE_HANDSHAKE: 标识handshake
                                        CLUSTER_NODE_NOADDR: 标识noaddr
                                        CLUSTER_NODE_MIGRATE_TO: 由主节点设置
                                        */
    uint64_t configEpoch;           /* 实现故障转移, 配置时间 
                                       Last configEpoch observed */
    unsigned char slots[CLUSTER_SLOTS/8]; 
                                    /* 处理的槽位标识数组 */
    int numslots;                   /* 处理的槽数量 */
    /* 从节点复制对应主节点的内容, 当主节点故障时, 从节点竞选出主
     * 节点, 并负责原主节点的槽位 */
    int numslaves;                  /* ->slaves[]大小 */
    struct clusterNode **slaves;    /* 本节点为主节点时, 从节点数组 */
    struct clusterNode *slaveof;    /* 本节点为从节点时, 指向主节点; 即使
                                       本节点为从节点, 也可能为空, 如果没
                                       有主节点在节点表中 */
    mstime_t ping_sent;             /* 上次向此节点发送ping的时间 */
    mstime_t pong_received;         /* 上次从此节点收到pong的时间 */
    mstime_t fail_time;      /* Unix time when FAIL flag was set */
    mstime_t voted_time;     /* Last time we voted for a slave of this master */
    mstime_t repl_offset_time;  /* Unix time we received offset for this node */
    mstime_t orphaned_time;     /* Starting time of orphaned master condition */
    long long repl_offset;      /* Last known repl offset for this node. */
    char ip[NET_IP_STR_LEN];        /* 地址(Latest known) */
    int port;                       /* 端口号(Latest known) */
    clusterLink *link;              /* 连接节点的信息,
                                       TCP/IP link with this node */
    list *fail_reports;         /* 记录了所有其他节点对该节点的下线报告 */
} clusterNode;

/* 在当前节点的视角下, 集群目前所处的状态 */
typedef struct clusterState {
    clusterNode *myself;        /* 当前节点 */
    uint64_t currentEpoch;      /* 当前的选举时间 */
    int state;                  /* 集群状态, CLUSTER_OK, CLUSTER_FAIL... */
    int size;                   /* 至少处理着一个槽的节点的数量 */
    dict *nodes;                /* 集群节点集, 节点名-> clusterNode */
    dict *nodes_black_list;     /* Nodes we don't re-add for a few seconds. */
    clusterNode *migrating_slots_to[CLUSTER_SLOTS];
    clusterNode *importing_slots_from[CLUSTER_SLOTS];
                                /* 槽位转移信息 */
    clusterNode *slots[CLUSTER_SLOTS];
                                /* 处理槽的节点信息汇总, 便于查找槽是否
                                 * 分配以及分配给谁; 各个节点也记录了自
                                 * 身的槽信息, slots[]/numslots, 便于
                                 * 内部操作加速, 如同步自身的槽信息等 */
    zskiplist *slots_to_keys;   /* 槽位到键值的跳表 */
    /* The following fields are used to take the slave state on elections. */
    mstime_t failover_auth_time; /* Time of previous or next election. */
    int failover_auth_count;    /* Number of votes received so far. */
    int failover_auth_sent;     /* True if we already asked for votes. */
    int failover_auth_rank;     /* This slave rank for current auth request. */
    uint64_t failover_auth_epoch; /* Epoch of the current election. */
    int cant_failover_reason;   /* Why a slave is currently not able to
                                   failover. See the CANT_FAILOVER_* macros. */
    /* Manual failover state in common. */
    mstime_t mf_end;            /* Manual failover time limit (ms unixtime).
                                   It is zero if there is no MF in progress. */
    /* Manual failover state of master. */
    clusterNode *mf_slave;      /* Slave performing the manual failover. */
    /* Manual failover state of slave. */
    long long mf_master_offset; /* Master offset the slave needs to start MF
                                   or zero if stil not received. */
    int mf_can_start;           /* If non-zero signal that the manual failover
                                   can start requesting masters vote. */
    /* The followign fields are used by masters to take state on elections. */
    uint64_t lastVoteEpoch;     /* 上次选举时间 */
    int todo_before_sleep; /* Things to do in clusterBeforeSleep(). */
    long long stats_bus_messages_sent;  /* Num of msg sent via cluster bus. */
    long long stats_bus_messages_received; /* Num of msg rcvd via cluster bus.*/
} clusterState;

/* clusterState todo_before_sleep flags. */
#define CLUSTER_TODO_HANDLE_FAILOVER (1<<0)
#define CLUSTER_TODO_UPDATE_STATE (1<<1)
#define CLUSTER_TODO_SAVE_CONFIG (1<<2)
#define CLUSTER_TODO_FSYNC_CONFIG (1<<3)

/* Redis cluster messages header */

/* Note that the PING, PONG and MEET messages are actually the same exact
 * kind of packet. PONG is the reply to ping, in the exact format as a PING,
 * while MEET is a special PING that forces the receiver to add the sender
 * as a node (if it is not already in the list). */
#define CLUSTERMSG_TYPE_PING 0          /* Ping */
#define CLUSTERMSG_TYPE_PONG 1          /* Pong (reply to Ping) */
#define CLUSTERMSG_TYPE_MEET 2          /* Meet "let's join" message */
#define CLUSTERMSG_TYPE_FAIL 3          /* Mark node xxx as failing */
#define CLUSTERMSG_TYPE_PUBLISH 4       /* Pub/Sub Publish propagation */
#define CLUSTERMSG_TYPE_FAILOVER_AUTH_REQUEST 5 /* May I failover? */
#define CLUSTERMSG_TYPE_FAILOVER_AUTH_ACK 6     /* Yes, you have my vote */
#define CLUSTERMSG_TYPE_UPDATE 7        /* Another node slots configuration */
#define CLUSTERMSG_TYPE_MFSTART 8       /* Pause clients for manual failover */

/* Initially we don't know our "name", but we'll find it once we connect
 * to the first node, using the getsockname() function. Then we'll use this
 * address for all the next messages. 
 *
 * 对应MEET/PING/PONG消息, 发送消息者选中的节点的信息 */
typedef struct {
    char nodename[CLUSTER_NAMELEN];     /* 被选中的节点的名字 */
    uint32_t ping_sent;         /* 最后一次向此节点发送PING消息的时间戳 */
    uint32_t pong_received;     /* 最后一次从此节点收到PONG消息的时间戳 */
    char ip[NET_IP_STR_LEN];    /* IP address last time it was seen */
    uint16_t port;              /* port last time it was seen */
    uint16_t flags;             /* node->flags copy */
    uint16_t notused1;          /* Some room for future improvements. */
    uint32_t notused2;
} clusterMsgDataGossip;

/* 用于扩散某个节点失效的消息 */
typedef struct {
    char nodename[CLUSTER_NAMELEN];     /* 失效的节点名 */
} clusterMsgDataFail;

/* 集群内订阅的广播消息 */
typedef struct {
    uint32_t channel_len;       /* channel参数的长度 */
    uint32_t message_len;       /* message参数的长度 */
    /* We can't reclare bulk_data as bulk_data[] since this structure is
     * nested. The 8 bytes are removed from the count during the message
     * length computation. */
    unsigned char bulk_data[8]; /* 保存channel参数 + message参数 */
} clusterMsgDataPublish;

typedef struct {
    uint64_t configEpoch; /* Config epoch of the specified instance. */
    char nodename[CLUSTER_NAMELEN]; /* Name of the slots owner. */
    unsigned char slots[CLUSTER_SLOTS/8]; /* Slots bitmap. */
} clusterMsgDataUpdate;

union clusterMsgData {
    /* PING, MEET and PONG */
    struct {
        /* Array of N clusterMsgDataGossip structures */
        clusterMsgDataGossip gossip[1];
    } ping;

    /* FAIL */
    struct {
        clusterMsgDataFail about;
    } fail;

    /* PUBLISH */
    struct {
        clusterMsgDataPublish msg;
    } publish;

    /* UPDATE */
    struct {
        clusterMsgDataUpdate nodecfg;
    } update;
};

#define CLUSTER_PROTO_VER 0 /* Cluster bus protocol version. */

/* 集群模式, 节点间交互的消息头 */
typedef struct {
    char sig[4];        /* Siganture "RCmb" (Redis Cluster message bus). */
    uint32_t totlen;    /* 消息总长度 */
    uint16_t ver;       /* Protocol version, currently set to 0. */
    uint16_t notused0;  /* 2 bytes not used. */
    uint16_t type;      /* 消息类型:  
                           CLUSTERMSG_TYPE_PING
                           CLUSTERMSG_TYPE_PONG
                           CLUSTERMSG_TYPE_MEET*/
    uint16_t count;     /* 消息正文包含的节点信息数量, 只在发送MEET/PING
                           /PONG这三种gossip消息时使用 */
    uint64_t currentEpoch;  /* The epoch accordingly to the sending node. */
    uint64_t configEpoch;   /* The config epoch if it's a master, or the last
                               epoch advertised by its master if it is a
                               slave. */
    uint64_t offset;    /* Master replication offset if node is a master or
                           processed replication offset if node is a slave. */
    char sender[CLUSTER_NAMELEN];           /* 发送节点名 */
    unsigned char myslots[CLUSTER_SLOTS/8]; /* 发送者目前的槽指派信息 */
    char slaveof[CLUSTER_NAMELEN];          /* 主节点名或NULL */
    char notused1[32];  /* 32 bytes reserved for future usage. */
    uint16_t port;                          /* 发送者端口号 */
    uint16_t flags;                         /* 发送者的标识 */
    unsigned char state;        /* 发送者角度的集群状态 */
    unsigned char mflags[3];    /* Message flags: CLUSTERMSG_FLAG[012]_... */
    union clusterMsgData data;  /* 消息正文 */
} clusterMsg;

#define CLUSTERMSG_MIN_LEN (sizeof(clusterMsg)-sizeof(union clusterMsgData))

/* Message flags better specify the packet content or are used to
 * provide some information about the node state. */
#define CLUSTERMSG_FLAG0_PAUSED (1<<0) /* Master paused for manual failover. */
#define CLUSTERMSG_FLAG0_FORCEACK (1<<1) /* Give ACK to AUTH_REQUEST even if
                                            master is up. */

/* ---------------------- API exported outside cluster.c -------------------- */
clusterNode *getNodeByQuery(client *c, struct redisCommand *cmd, robj **argv, int argc, int *hashslot, int *ask);
int clusterRedirectBlockedClientIfNeeded(client *c);
void clusterRedirectClient(client *c, clusterNode *n, int hashslot, int error_code);

#endif /* __CLUSTER_H */
