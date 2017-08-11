#include "csapp.h"

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

// personal port number
#define PORTNUM 5870
// default port if not specific
#define DEFPORT 80

// dbg convenience
#define DEBUG
#ifdef DEBUG
#define dbg_printf(...) printf(__VA_ARGS__)
#else
#define dbg_printf(...)
#endif

/* Information about a connected client. reference: tiny.c */
typedef struct {
    struct sockaddr_storage addr;    // Socket address
    socklen_t addrlen;          // Socket address length
    int *connfd;                 // Client connection file descriptor
    char host[MAXLINE];         // Client host
    char port[MAXLINE];         // Client service (port)
} client_info;

typedef struct block_t {
    char *object;
    char url[MAXLINE];
    int readcnt;
    size_t block_size;
    struct block_t *next_block;
    struct block_t *prev_block;
    sem_t read_mutex;
    sem_t write_mutex;
} cache_block;

typedef struct {
    cache_block *head;
    size_t size;
    sem_t read_mutex;
    sem_t write_mutex;
} cache;

// const string in header
static const char *header_user_agent = "User-Agent: Mozilla/5.0"
                                    " (X11; Linux x86_64; rv:45.0)"
                                    " Gecko/20100101 Firefox/45.0\r\n";
static const char *header_connection = "Connection: close\r\n";
static const char *header_proxy_connection = "Proxy-Connection: close\r\n";
static const char *header_host = "Host: %s\r\n";
static const char *host_key = "Host";
static const char *user_agent_key = "User-Agent";
static const char *connection_key = "Connection";
static const char *proxy_connection_key = "Proxy-Connection";
static const char *get_str = "GET ";
static const char *header_end = "\r\n";
static const char *default_path = "/index.html";
static const char *length_key = "Content-Length";

// functions
void handle_get(int connfd);
void parse_url(char *url, char *hostname, char *path, int *port);
void build_header(char *header, char *hostname, char *path, rio_t *rio);
void *thread(void *vargp);

void init_cache();
void write_unblock(cache_block *cur_block);
void write_block(cache_block *cur_block);
void read_unlock(cache_block *cur_block);
void read_lock(cache_block *cur_block);
cache_block *find_block(char *url);
void save_block(char *url, char *obj, size_t size);
void cache_evict(size_t size);
void access_block(cache_block *cur_block, int connfd);
void block_update(cache_block *cur_block);
void print_list();

// shared cache
cache share_cache;
cache_block *tail;

int main(int argc, char **argv) {
    dbg_printf(">>> main\n");
    int listenfd;
    pthread_t tid;

    if(argc != 2){
        dbg_printf("usage :%s <port> \n",argv[0]);
        exit(0);
    }

    Signal(SIGPIPE,SIG_IGN);
    if((listenfd = Open_listenfd(argv[1])) < 0){
        exit(0);
    }

    init_cache();
    while(1){
        client_info client_data;
        client_info *client = &client_data;
        client->addrlen = sizeof(client->addr);
        client->connfd = malloc(sizeof(int));
        *client->connfd = accept(listenfd,
                                (SA *) &client->addr, &client->addrlen);
        getnameinfo((SA *) &client->addr, client->addrlen,
                    client->host, MAXLINE, client->port, MAXLINE, 0);
        dbg_printf("Connected to %s, %s\n", client_data.host, client_data.port);
        Pthread_create(&tid, NULL, thread, client->connfd);
    }
    exit(0);
}

void *thread(void *vargp) {
    int connfd = *((int *)vargp);
    Pthread_detach(pthread_self());
    Free(vargp);
    handle_get(connfd);
    Close(connfd);
    return NULL;
}

void handle_get(int connfd){
    dbg_printf(">>> handle_get\n");
    char buf[MAXLINE], request[MAXLINE], url[MAXLINE], version;
    rio_t rio, serve_rio;
    char hostname[MAXLINE], path[MAXLINE], client_header[MAXLINE];
    int port, clientfd;
    size_t size;

    rio_readinitb(&rio, connfd);
    rio_readlineb(&rio, buf, MAXLINE);

    if(sscanf(buf, "%s %s HTTP/1.%c", request, url, &version) != 3
       || (version != '0' && version != '1')){
        dbg_printf("bad input\n");
        return;
    }
    if(strcmp(request, "GET") != 0){
        dbg_printf("not GET\n");
        return;
    }

    char buf_url[MAXLINE];
    strcpy(buf_url, url);
    cache_block *cur_block;
    if((cur_block = find_block(buf_url)) != NULL){
        dbg_printf("block saved\n");
        access_block(cur_block, connfd);
        return;
    }

    parse_url(url, hostname, path, &port);
    dbg_printf("hostname: %s, path: %s, port: %d\n", hostname, path, port);
    build_header(client_header, hostname, path, &rio);

    char portStr[MAXLINE];
    sprintf(portStr, "%d", port);
    if((clientfd = open_clientfd(hostname, portStr)) < 0){
        dbg_printf("Fail connect client\n");
        return;
    }

    rio_readinitb(&serve_rio, clientfd);
    rio_writen(clientfd, client_header, strlen(client_header));
    size_t n;
    char buf_object[MAX_OBJECT_SIZE];
    while((n = rio_readlineb(&serve_rio, buf, MAXLINE)) != 0){
        char *length;
        if((length = strstr(buf, length_key)) != NULL){
            size = (size_t)atoi(length + 16);
        }
        if(size < MAX_OBJECT_SIZE){
            memcpy(buf_object, buf, n);
        }
        rio_writen(connfd, buf, n);
    }
    char buf_size[size];
    memcpy(buf_size, buf_object, size);
    dbg_printf("buf_size: %zu, buf_obj: %zu\n", strlen(buf_size), strlen(buf_object));
    dbg_printf("obg size: %zu\n", size);
    if(size < MAX_OBJECT_SIZE && size > 0){
        save_block(buf_url, buf_object, size);
    }
    print_list();
    dbg_printf("after save\n");
    Close(clientfd);
    return;
}

void print_list(){
    dbg_printf("-----------------------------\n");
    cache_block *cur = share_cache.head;
    while(cur != NULL){
        dbg_printf("size: %zu\n", cur->block_size);
        dbg_printf("url: %s\n", cur->url);
        dbg_printf("obj: %s\n", cur->object);
        cur = cur->next_block;
    }
    dbg_printf("----------------\n");
    cur = tail;
    while(cur != NULL){
        dbg_printf("size: %zu\n", cur->block_size);
        dbg_printf("url: %s\n", cur->url);
        dbg_printf("obj: %s\n", cur->object);
        cur = cur->prev_block;
    }
    dbg_printf("-----------------------------\n");
}

void save_block(char *url, char *obj, size_t size){
    dbg_printf("<<< save_block\n");
    if(share_cache.size + size > MAX_CACHE_SIZE){
        dbg_printf("need evict\n");
        cache_evict(share_cache.size + size - MAX_CACHE_SIZE);
    }
    cache_block *cur = malloc(sizeof(cache_block));
    cur->object = malloc(sizeof(char)*size);
    cur->block_size = size;
    sem_init(&cur->read_mutex, 0, 1);
    sem_init(&cur->write_mutex, 0, 1);
    memcpy(cur->object, obj, size);
    memcpy(cur->url, url, strlen(url));
    cur->prev_block = NULL;
    dbg_printf("size url: %zu\n", strlen(cur->url));
    dbg_printf("size obg: %zu\n", strlen(cur->object));
    dbg_printf("block url: %s\n", cur->url);
    if(share_cache.head != NULL){
        dbg_printf("block head != nULL\n");
        P(&share_cache.write_mutex);
        cur->next_block = share_cache.head;
        share_cache.head->prev_block = cur;
        share_cache.head = cur;
        V(&share_cache.write_mutex);
    }else{
        dbg_printf("head == NULL\n");
        share_cache.head = cur;
        cur->next_block = NULL;
        share_cache.head->next_block = NULL;
    }
    if(tail == NULL){
        tail = cur;
    }
    P(&share_cache.write_mutex);
    share_cache.size += size;
    V(&share_cache.write_mutex);
    print_list();
}

void  cache_evict(size_t size){
    dbg_printf("<<< cache_evict\n");
    if(size <= 0 || share_cache.head == NULL){
        return;
    }
    cache_block *cur_block = tail;
    size_t evicted = 0;
    while(cur_block != NULL && evicted <= size){
        dbg_printf("cur size: %zu\n", cur_block->block_size);
        write_block(cur_block);
        evicted += cur_block->block_size;
        free(cur_block->object);
        cur_block->object = NULL;
        write_unblock(cur_block);
        dbg_printf("update\n");
        cur_block = cur_block->prev_block;
    }
    if(cur_block != NULL){
        dbg_printf("change tail\n");
        write_block(tail);
        tail = cur_block;
        write_unblock(tail);
    }else{
        dbg_printf("tail = null\n");
        tail = NULL;
    }
    dbg_printf("update cache size\n");
    P(&share_cache.write_mutex);
    share_cache.size -= evicted;
    share_cache.size = share_cache.size>0?share_cache.size:0;
    V(&share_cache.write_mutex);
}

cache_block *find_block(char *url){
    dbg_printf("<<< find_block\n");
    cache_block *cur_block = share_cache.head;
    while(cur_block != NULL){
        read_lock(cur_block);
        if(strcmp(cur_block->url, url) == 0){
            dbg_printf("block exist\n");
            read_unlock(cur_block);
            return cur_block;
        }
        dbg_printf("block url: %s\n", cur_block->url);
        dbg_printf("block obj: %s\n", cur_block->object);
        dbg_printf("block size: %zu\n", cur_block->block_size);
        read_unlock(cur_block);
        cur_block = cur_block->next_block;
    }
    dbg_printf("block not exist\n");
    return NULL;
}

void access_block(cache_block *cur_block, int connfd){
    dbg_printf("<<< access_block\n");
    read_lock(cur_block);
    rio_writen(connfd, cur_block->object, cur_block->block_size);
    read_unlock(cur_block);
    block_update(cur_block);
}

void block_update(cache_block *cur_block){
    dbg_printf("<<< block_update\n");
    write_block(cur_block);
    if(cur_block->prev_block != NULL){
        write_block(cur_block->prev_block);
        cur_block->prev_block->next_block = cur_block->next_block;
        write_unblock(cur_block->prev_block);
    }
    if(cur_block->next_block != NULL){
        write_block(cur_block->next_block);
        cur_block->next_block->prev_block = cur_block->prev_block;
        write_unblock(cur_block->next_block);
    }else{
        write_block(tail);
        if(cur_block->prev_block != NULL){
            tail = cur_block->prev_block;
        }
        write_unblock(tail);
    }
    if(cur_block != share_cache.head){
        write_block(share_cache.head);
        cur_block->next_block = share_cache.head;
        share_cache.head = cur_block;
        write_unblock(share_cache.head);
    }
    write_unblock(cur_block);
}

void parse_url(char *url, char *hostname, char *path, int *port){
    dbg_printf(">>> parse_url\n");
    dbg_printf("url: %s\n", url);
    *port = DEFPORT;
    char *temp, *temp_1;
    if((temp = strstr(url, "//")) != NULL){
        temp = temp + 2;
    }else{
        temp = url;
    }
    if((temp_1 = strstr(temp, ":")) != NULL){
        *temp_1 = '\0';
        sscanf(temp, "%s", hostname);
        strncpy(hostname, temp, strlen(temp));
        *temp_1 = ':';
        if((strstr(temp, "/")) != NULL){
            sscanf(temp_1 + 1, "%d%s", port, path);
        }else{
            *port = atoi(temp_1 + 1);
            sscanf(default_path, "%s", path);
        }
    }else{
        if((temp_1 = strstr(temp, "/")) == NULL){
            sscanf(default_path, "%s", path);
        }else{
            sscanf(temp_1, "%s", path);
            *temp_1 = '\0';
            sscanf(temp, "%s", hostname);
            *temp_1 = '/';
        }
    }
    dbg_printf("hostname: %s, path: %s, port: %d\n",hostname, path, *port);
}

void build_header(char *header, char *hostname, char *path, rio_t *rio){
    dbg_printf(">>> build_header\n");
    dbg_printf("hostname: %s, path: %s\n", hostname, path);
    char buf[MAXLINE], host[MAXLINE], remain[MAXLINE], request[MAXLINE];
    sprintf(request, "%s%s HTTP/1.0%s", get_str, path, header_end);
    while(rio_readlineb(rio, buf, MAXLINE) > 0){
        if(strcmp(buf, header_end) == 0){
            break;
        }
        if(strstr(buf, host_key) != NULL){
            strcpy(host, buf);
        }else if(strstr(buf, user_agent_key) != NULL){
            continue;
        }else if(strstr(buf, connection_key) != NULL){
            continue;
        }else if(strstr(buf, proxy_connection_key) != NULL){
            continue;
        }else{
            strcat(remain, buf);
        }
    }

    if(strlen(host) == 0)
    {
        sprintf(host, header_host, hostname);
    }

    sprintf(header, "%s%s%s%s%s%s%s", request, host, header_connection,
           header_proxy_connection, header_user_agent, remain, header_end);
    dbg_printf("header: %s\n", header);
}

void read_lock(cache_block *cur_block)
{
    dbg_printf("<<< read_lock\n");
    P(&cur_block->read_mutex);
    cur_block->readcnt = cur_block->readcnt + 1;
    if (cur_block->readcnt == 1)
    {
        P(&cur_block->write_mutex);
    }
    V(&cur_block->read_mutex);
}

void read_unlock(cache_block *cur_block)
{
    dbg_printf("<<< read_unblock\n");
    P(&cur_block->read_mutex);
    cur_block->readcnt = cur_block->readcnt - 1;
    if (cur_block->readcnt == 0)
    {
        V(&cur_block->write_mutex);
    }
    V(&cur_block->read_mutex);
}

void write_block(cache_block *cur_block){
    dbg_printf("<<< write_block\n");
    P(&cur_block->write_mutex);
}

void write_unblock(cache_block *cur_block){
    dbg_printf("<<< write_unblock\n");
    V(&cur_block->write_mutex);
}

void init_cache(){
    dbg_printf("<<< init_cache\n");
    share_cache.size = 0;
    share_cache.head = NULL;
    tail = NULL;
    sem_init(&share_cache.read_mutex, 0, 1);
    sem_init(&share_cache.write_mutex, 0, 1);
}

