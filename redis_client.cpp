#include "redis_client.h"
#include "stdio.h"

CRedis::CRedis(const char* host, unsigned short port):
    m_context(nullptr),m_cmd_num(0){

    m_host = host;
    m_port = port;
    m_commit_time.tv_nsec = 0;
    m_commit_time.tv_sec = 0;
}

CRedis::~CRedis(){
    if(m_context){
        redisFree(m_context);
    }
}

int CRedis::connect(){    
    if(m_context){
        if(!m_context->err){
            return 0;
        }
        
        // reconnect
        redisFree(m_context);
    }
    m_cmd_num = 0;
    
    m_context = redisConnectWithTimeout(m_host.c_str(), m_port, TIMEOUT);
    if(m_context == nullptr){
        //printf("redis connect failed\n");
        return -1;
    }
    if(m_context->err){
        //printf("redis connect failed : %d\n", m_context->err);
        redisFree(m_context);
        m_context = nullptr;
        return -1;
    }
    printf("connected\n");
    return 0;
}

int CRedis::disconnect(){    
    if(m_context){
        redisFree(m_context);
        m_context = nullptr;
    }
    return 0;
}

int CRedis::post_commit(const char *req){    
    if(!m_context){        
        return -1;
    }
    if(m_context->err){
        printf("post_commit failed, err : %d\n", m_context->err);
        return -1;
    }
    int ret = redisAppendCommand(m_context, req);
    if(ret == 0){
        if(m_cmd_num == 0){
            clock_gettime(CLOCK_MONOTONIC, &m_commit_time);
        }
        m_cmd_num++;
    }else{
        printf("redisAppendCommand failed\n");
    }
    return ret;
}

int CRedis::commit_done(){    
    if(m_cmd_num == 0){
        return 0;
    }

    uint32_t n = m_cmd_num;
    for(uint32_t i=0; i<n; i++){
       redisReply *reply = _get_result();
        if(!reply){
            break;
        }        
        freeReplyObject(reply);
        m_cmd_num--;
    }

    return m_cmd_num;
}

int CRedis::get_cmd_num(){    
    return m_cmd_num;
}

redisReply* CRedis::_get_result(){    
    if(!m_context){
        return nullptr;
    }

    void *reply = nullptr;
    int status = redisGetReply(m_context , &reply);
    if(status != REDIS_OK){
        // TODO : save cmd to cache, retry when reconnect
        // use cache out side of m_context, m_context->obuf may not reliable
        return nullptr;
    }
    return (redisReply*)reply;
}

double CRedis::get_timediff(){
    struct timespec end;
    clock_gettime(CLOCK_MONOTONIC, &end);
    
    double milliseconds = end.tv_nsec >= m_commit_time.tv_nsec
                        ? (end.tv_nsec - m_commit_time.tv_nsec) / 1e6 + (end.tv_sec - m_commit_time.tv_sec) * 1e3
                        : (m_commit_time.tv_nsec - end.tv_nsec) / 1e6 + (end.tv_sec - m_commit_time.tv_sec - 1) * 1e3;
    return milliseconds;
}

//////////////////////////////////////////////////////////////////////////////////
#include <unistd.h>
#include <stdio.h>

CRedisClient::CRedisClient()
{    
    m_running = true;
    
    m_thread = std::thread(&CRedisClient::task_run, this);
}

CRedisClient::~CRedisClient(){
    m_running = false;
    m_thread.join();

    map< string, CRedis*>::iterator it = m_redis.begin();        
    for(; it!=m_redis.end(); it++){
        delete it->second;
    }    
}

int CRedisClient::add_process(const char* identity, const char* host, uint16_t port){
    std::lock_guard<std::mutex> lock(m_mutex);

    string id(identity);
    if(m_redis.find(id) != m_redis.end()){
        // already exist;
        return 0;
    }
    
    CRedis* r = new CRedis(host, port);
    m_redis.insert(pair<string, CRedis* > (id, r));
    
    return 0;
}

int CRedisClient::command_without_reply(const char* identity, const char *cmd){
    std::lock_guard<std::mutex> lock(m_mutex);
    string id(identity);

    // write to cache           
    if(m_cache.find(id) == m_cache.end()){
        queue<string> q;
        m_cache.insert(std::pair<string, queue<string> >(id, q));
    }
    if(m_cache[id].size() < MAX_REDIS_CACHE){
        m_cache[id].push(string(cmd));

        return 0;
    }        
    
    return -1;
}

CRedis* CRedisClient::_get_available_credis(string id){    
    if(m_redis.find(id) == m_redis.end()){        
        return nullptr;
    }
   
    return m_redis[id];
}

bool CRedisClient::_check_redis_ready_for_run(CRedis* r){
    if(r->get_cmd_num() >= MAX_REDIS_DISPATCH_NUM || r->get_timediff() >= MAX_REDIS_DISPATCH_TIME){
        return true;
    }
    return false;
}

bool CRedisClient::check_command_done(){
    
    std::lock_guard<std::mutex> lock(m_mutex);

    map< string, queue<string > >::iterator cit = m_cache.begin();
    for(; cit!=m_cache.end(); cit++){
        if(!cit->second.empty()){
            return false;
        }
    }

    map< string, CRedis*>::iterator it = m_redis.begin();
    for(; it!=m_redis.end(); ++it){
        if(it->second->get_cmd_num() > 0){
            return false;
        }
    }    

    return true;
}

void CRedisClient::task_run(){    

    map< string, CRedis* > redis_run;    

    while(m_running){        
        
        // check cache
        
        {            
            std::lock_guard<std::mutex> lock(m_mutex);
            map< string, queue<string > >::iterator it = m_cache.begin();            
            for(; it!=m_cache.end(); ++it){
                if(!it->second.empty()){
                    CRedis* r = _get_available_credis(it->first);                    
                    if(r){                        
                        if(redis_run.find(it->first) == redis_run.end()){
                            redis_run.insert(std::pair<string, CRedis*>(it->first, r));
                        } 
                        uint32_t n=0;                       
                        while(!it->second.empty() && n <= MAX_REDIS_DISPATCH_NUM){
                            if(0 != redis_run[it->first]->post_commit(it->second.front().c_str())){
                                break;
                            }
                            it->second.pop();
                            n++;
                        }                                                                         
                    
                    }
                }
            }
        }

        // real run, get r(credis) out of mutex
        if(!redis_run.empty()){
            map< string, CRedis* >::iterator it = redis_run.begin();
            for(; it!=redis_run.end();){
                if(0 == it->second->connect()){
                    if(_check_redis_ready_for_run(it->second)){                
                        if(0 == it->second->commit_done()){
                            it = redis_run.erase(it);
                            continue;
                        }
                    }
                }                                
                it++;
            }
        }
        else{
            usleep(1);
        }
        
    }    
}

int CRedisClient::exit(){
    m_running = false;
    return 0;
}