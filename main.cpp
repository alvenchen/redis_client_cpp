
#include "redis_client.h"
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <vector>
#include <ctime>
#include <time.h>
#include <chrono>
#include <unistd.h>

using namespace std;

const char* identidy = "test";
const char* identidy2 = "test2";

uint32_t get_timestamp_from_str(const char* date){
    uint32_t year, month, day;
    if(3 != sscanf(date, "%u-%u-%u", &year, &month, &day)){
        return 0;
    }

    struct tm  tm = {0};
    tm.tm_year = year - 1900;
    tm.tm_mon = month - 1;
    tm.tm_mday = day;

    return mktime(&tm);
}

vector<string> get_key_by_day(uint32_t domain_id, string day){    
    vector<string> keys;
    uint32_t timestamp = get_timestamp_from_str(day.c_str()); 
    if(0 == timestamp){
        return keys;
    }

    char tmp[32] = {0};
    for(uint16_t i=0; i<1440; i++){
        snprintf(tmp, sizeof(tmp), "%u%u", domain_id, timestamp+60*i);
        keys.push_back(string(tmp));
    }
    //printf("%s\n", keys[0].c_str());

    return keys;
}

int insert_by_key(CRedisClient *client, uint32_t domain_id, string day){
    vector<string> keys = get_key_by_day(domain_id, day);    
    printf("keys %s\n", keys[0].c_str());
    char value[32] = {0};
    char cmd[128] = {0};
    int err_code;
    for(int i=0; i<keys.size(); i++){
        for(int j=0; j<4; j++){
            for(int k=0; k<250; k++){
                err_code = rand() % 600;
                snprintf(value, sizeof(value), "192.168.%u.%u_%u", j, k, err_code);
                snprintf(cmd, sizeof(cmd), "lpush %s %s", keys[i].c_str(), value);
                while(0 != client->command_without_reply(identidy, cmd)){
                    usleep(1);
                }
                /*
                while(0 != client->command_without_reply(identidy2, cmd)){
                    usleep(1);
                } 
                */                               
            }
        }
    }

    return 0;
}

void print_timediff(const char* prefix, const struct timespec& start, const struct timespec& end)
{
    double milliseconds = end.tv_nsec >= start.tv_nsec
                        ? (end.tv_nsec - start.tv_nsec) / 1e6 + (end.tv_sec - start.tv_sec) * 1e3
                        : (start.tv_nsec - end.tv_nsec) / 1e6 + (end.tv_sec - start.tv_sec - 1) * 1e3;
    printf("%s: %lf milliseconds\n", prefix, milliseconds);
}

int main(){

    printf("start\n");
    CRedisClient *client = new CRedisClient();
    client->add_process(identidy, "127.0.0.1");
    //client->add_process(identidy2, "127.0.0.1");

    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    string day = "2019-04-05";
    uint32_t key_base = 24350;

    for(int i=0; i<10; i++){
        insert_by_key(client, key_base+i, day);
    }

    clock_gettime(CLOCK_MONOTONIC, &end);
    print_timediff("insert time", start, end);

    
    while(!client->check_command_done()){
        usleep(1);
    }

    insert_by_key(client, key_base+100, day);
    while(!client->check_command_done()){
        usleep(1);
    }
    

    printf("exit");
    client->exit();

    clock_gettime(CLOCK_MONOTONIC, &end);
    print_timediff("elapse time", start, end);

    return 0;
}