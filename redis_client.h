#ifndef __REDIS_CLIENT_H__
#define __REDIS_CLIENT_H__

/*
    description : cpp type api for redis based on hiredis
    version : v0.1 by chaos          
    TODO : sentinel
*/

#include "hiredis/hiredis.h"
#include <string>
#include <stdio.h>
#include <mutex>
#include <map>
#include <thread>
#include <vector>
#include <queue>
#include <memory>
#include <time.h>

using namespace std;

// not thread safe
class CRedis{
    public:
        CRedis(const char* host, uint16_t port = 6379);
        ~CRedis();
        CRedis( const CRedis& ) = delete;
        CRedis& operator=( const CRedis& ) = delete;
    public:    
        //sync operation
        int connect();
        int disconnect();

        //should call commit_done/get_result later
        int post_commit(const char *req);
        int commit_done();
        double get_timediff();
        int get_cmd_num();
        vector<string> get_result();
    protected:
        redisReply* _get_result();        

    protected:
        const struct timeval TIMEOUT = { 3, 0 }; // 3 seconds

        string              m_host;
        uint16_t            m_port;

        redisContext        *m_context;
        uint32_t            m_cmd_num;
        
        struct timespec     m_commit_time;
};


//////////////////////////////////////////////////////////////////////////////////

const uint32_t MAX_REDIS_CACHE = 100000;
const uint32_t MAX_REDIS_DISPATCH_NUM = 100;
const uint32_t MAX_REDIS_DISPATCH_TIME = 10; // millisecond

class CRedisClient{
    public:
        CRedisClient();
        ~CRedisClient();
        CRedisClient( const CRedisClient& ) = delete;
        CRedisClient& operator=( const CRedisClient& ) = delete;
    public:
        int add_process(const char* identity, const char* host, uint16_t port = 6379);
        //int command(const char* identity, vector<string> &cmd, std::function< int(vector<string>&) > callback);
        int command_without_reply(const char* identity, const char *cmd);

        // for test only
        bool check_command_done();
        int exit();
    protected:
        void task_run();
        CRedis* _get_available_credis(string id);
        bool _check_redis_ready_for_run(CRedis* r);

    protected:        

        std::thread                                     m_thread;
        bool                                            m_running;
        std::mutex                                      m_mutex;
        map< string, CRedis* >                          m_redis;        
        map< string, queue<string > >                   m_cache;

};


#endif