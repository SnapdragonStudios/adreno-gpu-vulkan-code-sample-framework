// Copyright (c) 2021, Qualcomm Innovation Center, Inc. All rights reserved.
// SPDX-License-Identifier: BSD-3-Clause

#include "Worker.h"
#include "os_common.h"
#include <cassert>


// **********************************************
// **********************************************
// CWorker
// **********************************************
// **********************************************

//-----------------------------------------------------------------------------
void CWorker::WorkerThreadProc()
//-----------------------------------------------------------------------------
{
    //
    // EVERYTHING in here needs to be done thread safely.
    // Potentially multiple threads are running this function (and other threads
    // interacting with the work queue).
    //

    // One more worker started.  Hello!
    m_WorkersRunning.Lock();

    // Wait to be told do do something
    while(true)
    {
        // LOGI("Worker %d: Waiting for something to do...", uiWhichWorker);

        // Wait for the semaphore to indicate available work.  Each call to Wait should get ONE piece of work from the queue
        m_WorkAvailable.Wait();

        // Get the work (will wait until work is available).
        Work work;
        {
            std::lock_guard<std::mutex> lock( m_WaitingWorkQueueMutex );
            work = m_WaitingWorkQueue.front();
            m_WaitingWorkQueue.pop();
        }

        if( work.lpStartAddress )
        {
            // Do some work!
            // LOGI("Worker %d: Working...", uiWhichWorker);
            (work.lpStartAddress)(work.pParam);

            // After work is done we can reduce the number of 'inflight' jobs.
            m_WorkInFlight.Unlock();
        }
        else
        {
            // A nullptr start address (empty Work) indicates a request to shutdown this thread.
            // LOGI("Worker %d: Leaving!", uiWhichWorker);

            // Shutdown request is treated like a 'regular' job.
            m_WorkInFlight.Unlock();
            break;
        }
    }

    // One less worker running.  We are out!
    m_WorkersRunning.Unlock();
}

//-----------------------------------------------------------------------------
CWorker::CWorker() : m_WorkAvailable(0), m_WorkersRunning(0), m_WorkInFlight(0)
//-----------------------------------------------------------------------------
{
    m_Name = "Worker";
}

//-----------------------------------------------------------------------------
CWorker::~CWorker()
//-----------------------------------------------------------------------------
{
    Terminate();
}

//-----------------------------------------------------------------------------
uint32_t CWorker::Initialize(const char *pName, uint32_t uiDesiredThreads)
//-----------------------------------------------------------------------------
{
    if (pName != nullptr)
    {
        if (pName != nullptr)
        {
            m_Name = pName;
        }
    }

    // If desired number of threads passed in use that.
    // Otherwise spawn one per core
    uint32_t uiNumCores = OS_GetNumCores();

    LOGI("System has %d core[s] available", uiNumCores);
    if(uiNumCores > MAX_CPU_CORES)
    {
        uiNumCores = MAX_CPU_CORES;
        LOGI("Only monitoring %d core[s]", uiNumCores);
    }
    else if( uiNumCores == 0 )
    {
        // was unable to determing number of cores so just assume 1 !
        uiNumCores = 1;
        LOGI("Unable to query number of cores, assuming %d", uiNumCores);
    }

    uint32_t uiNumWorkers;

    if(uiDesiredThreads == 0)
        uiNumWorkers = uiNumCores;
    else
        uiNumWorkers = uiDesiredThreads;

    if(uiNumWorkers == 0)
    {
        // Nothing to start
        LOGE("Not starting any worker threads!");
        return uiNumWorkers;
    }

    // Create the Worker Information
    m_Workers.clear();
    m_Workers.reserve(uiNumWorkers);

    // Create and startup the worker threads
    for(uint32_t uiIndx = 0; uiIndx < uiNumWorkers; uiIndx++)
    {
        m_Workers.emplace_back( std::thread{ &CWorker::WorkerThreadProc, this } );
    }

    return uiNumWorkers;
}

//-----------------------------------------------------------------------------
void CWorker::Terminate()
//-----------------------------------------------------------------------------
{
    for( size_t i = 0; i < m_Workers.size(); ++i )
    {
        // Send an 'empty' job for each of the workers (signals to stop running).
        DoWork( Work{}, 10 * 1000 );
    }

    // Potentially the threads pushed jobs on to the m_WaitingWorkQueue between us pushing the 'empty' jobs and them getting processed.
    // What do we do with that?  We could either run the jobs here (single threaded) or just punt.  What happens if another CWorker is
    // processing jobs that push work on to our queue?

    // Wait for all the threads to be done.
    m_WorkersRunning.WaitAndLock();
    m_WorkersRunning.Unlock();

    // Join the workers (need to do this before we can call the thread destructor)
    for(auto& worker: m_Workers )
        worker.join();

    // Clean up all trace of the workers.
    m_Workers.clear();
}

//-----------------------------------------------------------------------------
void CWorker::FinishAllWork()
//-----------------------------------------------------------------------------
{
    if(m_Workers.empty())
    {
        LOGE("Unable to FinishAllWork: Worker has not been set up");
        return;
    }

    m_WorkInFlight.WaitAndLock();
    m_WorkInFlight.Unlock();

    // LOGI("(%s) All Work Finished!", m_Name.c_str());
}

//-----------------------------------------------------------------------------
bool CWorker::IsAllWorkDone()
//-----------------------------------------------------------------------------
{
    if (m_Workers.empty())
    {
        LOGE("Unable to call IsAllWorkDone: Worker has not been set up");
        return true;    // Since it can't start work, it must all be done :)
    }

    if( !m_WorkInFlight.TryWaitAndLock() )
    {
        // Could not get lock - work is still in flight.
        return false;
    }
    // Got the lock, so no work in flight.  Although there could be at this point if another thread got in already!
    m_WorkInFlight.Unlock();
    return true;
}


//-----------------------------------------------------------------------------
void CWorker::DoWork(void (*lpStartAddress) (void *), void *pParam, uint32_t WaitTimeMS)
//-----------------------------------------------------------------------------
{
    DoWork({lpStartAddress, pParam}, WaitTimeMS);
}

//-----------------------------------------------------------------------------
void CWorker::DoWork(Work&& work, uint32_t WaitTimeMS)
//-----------------------------------------------------------------------------
{
    if(m_Workers.empty())
    {
        LOGE("Unable to DoWork: Worker has not been set up");
        return;
    }

    // Indicate we have work in flight (do first!)
    m_WorkInFlight.Lock();

    // Add to the work queue, could be executed as soon as its mutex unlocks.
    {
        std::lock_guard<std::mutex> lock( m_WaitingWorkQueueMutex );
        m_WaitingWorkQueue.push( std::move(work) );
    }

    // Indicate to the workers that there is something to be done.
    // LOGI("(%s) DoWork posting WorkAvailable", m_Name.c_str());
    m_WorkAvailable.Post();
}
