//============================================================================================================
//
//
//                  Copyright (c) 2023, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================
#pragma once

// Standard Headers
#include <vector>
#include <thread>
#include <mutex>
#include <queue>
#include <cassert>

#if !defined(MAX_CPU_CORES)
    #define MAX_CPU_CORES       4
#endif // !defined(MAX_CPU_CORES)

/// Sempahore.  Post increases the counter, Wait allows a thread through if the counter is greater than zero and then decreases the counter.
/// Uses C++ std syncronization primitives.
/// @ingroup System
class Semaphore
{
public:
    Semaphore( uint32_t initialCount ) : m_Counter( initialCount ) {}

    /// Increases the internal counter.
    /// Will notify (wake) (a single) thread waiting on this Semaphore.
    /// @note Thread safe
    void Post()
    {
        {
            std::lock_guard<std::mutex> lock( m_Mutex );
            ++m_Counter;
        }
        m_Condition.notify_one();
    }

    /// Waits until the internal counter is non zero and then decreases the counter and returns.
    /// @note Thread safe
    void Wait()
    {
        std::unique_lock<std::mutex> lock( m_Mutex );
        while( !m_Counter ) // spurious wake-up could get us out of the condition wait without notify_xx having been called on it (so loop until m_Counter is non zero)
        {
            m_Condition.wait( lock );
        }
        --m_Counter;
    }

private:
    std::mutex              m_Mutex;
    uint32_t                m_Counter;      // protected by m_Mutex
    std::condition_variable m_Condition;
};


/// 'Reversed' Semaphore.  Lock can be called multiple times, a waiting thread is only allowed through when the lock count is 0.
/// Uses C++ std syncronization primitives.
/// @ingroup System
class ReverseSemaphore
{
public:
    ReverseSemaphore( uint32_t initialLockCount ) : m_Counter( initialLockCount ) {}

    /// Decrease (unlock) the internal counter and notify/wakeup one waiting thread IF the internal count decreased to zero.
    /// Asserts if Unlock sends the internal count negative (undefined behaviour)
    /// @note Thread safe
    void Unlock()
    {
        uint32_t count;
        {
            std::lock_guard<std::mutex> lock( m_Mutex );
            assert( m_Counter!=0 );
            count = --m_Counter;
        }
        // potentially another thread gets in here and has already incremented m_Counter.  In that case a 'Waiter' will be woken up to find m_Counter is not zero and it needs to go wait again.
        if (count == 0)
            m_Condition.notify_one();
    }

    /// Increase (lock) the internal counter (will not allow any subsequent WaitAndLock through)
    /// @note Thread safe
    void Lock()
    {
        std::lock_guard<std::mutex> lock( m_Mutex );
        ++m_Counter;
    }

    /// Wait until the internal counter is zero, once/if it is increase the counter and return.
    /// @note Thread safe
    void WaitAndLock()
    {
        std::unique_lock<std::mutex> lock( m_Mutex );
        while( m_Counter ) // spurious wake-up could get us out of the condition wait without notify_xx having been called on it (so loop until m_Counter is zero).  Also Lock getting hit between Unlock telling us to wake up and us actually doing so. 
        {
            m_Condition.wait( lock );
        }
        ++m_Counter;
    }

    /// Attempt to do WaitAndLock (but without waiting)
    /// @return true if internal counter was zero and we were able to then increase it. 
    /// @return false if internal counter was not zero on entry.
    /// @note Thread safe
    bool TryWaitAndLock()
    {
        std::lock_guard<std::mutex> lock( m_Mutex );
        if( m_Counter )
        {
            return false;
        }
        else
        {
            ++m_Counter;
            return true;
        }
    }

private:
    std::mutex              m_Mutex;
    uint32_t                m_Counter;      // protected by m_Mutex
    std::condition_variable m_Condition;

};


// Some work that we want the worker to do
struct Work
{
    // Funtion pointer
    void (*lpStartAddress) (void *) = nullptr;

    // Funtion parameter block
    void *pParam = nullptr;
};


/// The thread worker class.
/// Creates a number of worker threads that can then be given work to do (via DoWork / DoWork2)
/// Holds the threads, the work queue, and the associated syncronization primitives.
/// @ingroup System
class CWorker
{
public:
    // Constructor/Destructor
    CWorker();
    ~CWorker();

    /// Initialize this worker with the given number of threads
    uint32_t    Initialize(const char *pName, uint32_t uiDesiredThreads = 0);

    uint32_t    NumThreads() { return (uint32_t)m_Workers.size(); }

    /// Wait for all outstanding work to be done
    void        FinishAllWork();
    /// @return true if all outstanding work is done
    bool        IsAllWorkDone();

    /// Add this 'work' to the waiting work queue (will call the lpStartAddress function pointer some time in the future)
    /// @note Thread safe.
    void        DoWork(void (*lpStartAddress) (void *), void *pParam, uint32_t WaitTimeMS);

    struct ParameterWrapperBase {};

    template<typename Func, typename... Args>
    struct FunctionParamWrapper : ParameterWrapperBase
    {
        FunctionParamWrapper(Func&& func, Args... args) : m_func(+func), m_args(std::forward<Args>(args)... )
        {
        }
        void                (*m_func)(Args...);
        std::tuple<Args...>   m_args;
    };

    /// Add the lambda function to the waiting work queue (will execute the lambda some time in the future).
    /// Wraps DoWork with nicer/safer symntatical sugar.
    /// @param lambda function to execute
    /// @param args arguments to be passed to the lambda
    /// @note Thread safe.
    /// @note DOES NOT support lambdas with captures (essentially the lambda is treated as a function pointer)
    template<typename Func, typename... Args>
    void        DoWork2( Func&& lambda, Args... args ) {

        using tWrapper = FunctionParamWrapper<Func, Args...>;

        tWrapper* pParams = new tWrapper( std::forward<Func>(lambda), std::forward<Args>(args)... );

        auto lambdaWrap = []( void* voidParams ) {
            tWrapper* pParams = static_cast<tWrapper*>(voidParams);
            std::apply( pParams->m_func, std::move(pParams->m_args) );
            delete pParams;
        };

        DoWork( +lambdaWrap, pParams, 1000 );
    }

    void        Terminate();

protected:
    void DoWork(Work&& work, uint32_t WaitTimeMS);

    /// Function run by the WorkerInfo::m_Thread, loops.
    void WorkerThreadProc();

protected:
    std::string             m_Name;

    /// The individual workers (each is likely to on its own thread).
    std::vector<std::thread> m_Workers;

    /// Queue that holds the work we have been asked to do (protected by a mutex).
    std::queue<Work>        m_WaitingWorkQueue;
    std::mutex              m_WaitingWorkQueueMutex;

    /// Semaphore to control when work is available.
    Semaphore               m_WorkAvailable;
    /// ReverseSemaphore to indicate when things are either in the waiting queue or being processed (count should 'loosely' match m_WaitingWorkQueue.size() )
    ReverseSemaphore        m_WorkInFlight;
    /// ReverseSemaphore to indicate how many workers are executing (primarily used for safe shutdown)
    ReverseSemaphore        m_WorkersRunning;
};

