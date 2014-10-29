/*
 * =====================================================================================
 *
 *       Filename:  cpu_affinity.cpp
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  2014年10月29日 05时42分22秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  YOUR NAME (), 
 *   Organization:  
 *
 * =====================================================================================
 */
#include <stdlib.h>
#include <sched.h>
#include "cpu_affinity.h"
#include <sys/types.h>
#include <unistd.h>
#include "glog/logging.h"
#include <vector>
#include <string>
#include <pthread.h>

namespace 
{
    void tokenize(const std::string& str, std::vector<std::string>& tokens,
            char const * delimiters = ",")
    {
        size_t pos, lastPos = 0;


        while(true)
        {
            pos = str.find_first_of(delimiters, lastPos);
            if(pos == std::string::npos)
            {
                pos = str.length();

                if(pos != lastPos)
                    tokens.push_back(std::string(str.data()+lastPos,
                                (size_t)pos-lastPos ));

                break;
            }
            else
            {
                if(pos != lastPos)
                    tokens.push_back(std::string(str.data()+lastPos,
                                (size_t)pos-lastPos ));
            }

            lastPos = pos + 1;
        }
    }
}

namespace conet
{
    int parse_affinity(char const * txt, std::vector<int> *cpu_sets)
    {
        cpu_sets->clear();
        std::string t (txt);
        std::vector<std::string> sets;
        tokenize(t, sets, ",");
        for(size_t i=0; i<sets.size(); ++i)
        {
            int s = atoi(sets[i].c_str());
            cpu_sets->push_back(s);
        }
        return sets.size(); 
    }

    int set_cur_thread_cpu_affinity(int cpu_id)
    {
        cpu_set_t  mask;
        CPU_ZERO(&mask);
        CPU_SET(cpu_id, &mask);
        int ret = 0;
        pthread_t tid = pthread_self();
        ret = pthread_setaffinity_np(tid, sizeof(mask), &mask);
        if (ret) {
            LOG(ERROR)<<"set tid:"<<tid<<" to  cpu:"<<cpu_id<<" affinity failed, ret:"<<ret;
        } else {
            LOG(INFO)<<"set tid:"<<tid<<" to  cpu:"<<cpu_id<<" affinity success";
        }
        return ret;
    }

}
