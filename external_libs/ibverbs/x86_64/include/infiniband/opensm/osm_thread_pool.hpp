/*
 * Copyright (c) 2018 Mellanox Technologies LTD. ALL RIGHTS RESERVED.
 * See file LICENSE for terms.
 */

#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include <pthread.h>
#include <queue>
#include <list>

#include "osm_armgr_types.hpp"

#define DEFAULT_THREADPOOL_THREADS_NUMBER 10

namespace OSM {

typedef std::queue< class OSMThreadPoolTask * > TaskQueue;
typedef std::list< pthread_t > ListThreads;

class OSMThreadPool {

    osm_log_t          *m_osm_log_;           /** Pointer to osm log */
    TaskQueue           m_task_queue_;
    ListThreads         m_threads_;
    bool                m_stop_;
    bool                m_init_;

    pthread_mutex_t     m_queue_lock_;
    pthread_cond_t      m_queue_cond_;


public:

    OSMThreadPool(osm_log_t *p_osm_log) :
        m_osm_log_(p_osm_log), m_stop_(false), m_init_(false) {}

    int Init(uint16_t num_threads);

    void Stop() { m_stop_ = true; }
    void AddTask(OSMThreadPoolTask *p_task);

    //the method executed by each thread
    void ThreadRun();

    ~OSMThreadPool();

};

class OSMThreadPoolTasksCollection {

private:

    uint16_t                m_num_tasks_in_progress_;
    pthread_mutex_t         m_tasks_lock_;
    pthread_cond_t          m_tasks_cond_;
protected:
    bool                    m_is_init_;
    osm_log_t               *m_osm_log_;           /** Pointer to osm log */

public:

    OSMThreadPoolTasksCollection(osm_log_t *p_osm_log) :
        m_num_tasks_in_progress_(0),
        m_is_init_(false),
        m_osm_log_(p_osm_log){}

    ~OSMThreadPoolTasksCollection();

    void Init();

    void AddTaskToThreadPool(OSMThreadPool *p_thread_pool,
                             OSMThreadPoolTask *p_task);

    void WaitForTasks();

    //this should be called before each tasks ends
    void OnTaskEnd();

    osm_log_t *GetOsmLog() { return m_osm_log_; }

};

} //end of namespace OSM

#endif //THREAD_POOL_H
