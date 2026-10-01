#pragma once

//You can use this threadpool class in your code to reduce the overhead of spawning threads.
//Als het goed is de problemen gefixt. (als het goed is fingers crossed)

class ThreadPool; //Forward declare

class Worker
{
public:
    //Instantiate the worker class by passing and storing the threadpool as a reference
    explicit Worker(ThreadPool& s) : pool(s) {}

    void operator()();

private:
    ThreadPool& pool;
};

class ThreadPool
{
public:
    explicit ThreadPool(size_t numThreads)
    {
        for (size_t i = 0; i < numThreads; ++i)
        {
            workers.emplace_back(Worker(*this)); //emplace ipv push met thread want ...
        }
    }

    ~ThreadPool()
    {
        {
            std::unique_lock<std::mutex> lock(queue_mutex);
            stop = true; // stop all threads
        }
        cv.notify_all(); //Wakeup zodat ze kunnen stoppen

        for (auto& thread : workers)
            if (thread.joinable())
                thread.join();
    }


    template <class T>
    [[nodiscard]] auto enqueue(T task) -> std::future<decltype(task())>
    {
        using return_type = decltype(task());


        //Wrap the function in a packaged_task so we can return a future object
        auto wrapper = std::make_shared<std::packaged_task<return_type()>>(std::move(task));

        //Scope to restrict critical section
        {
            //lock our queue and add the given task to it
            std::unique_lock<std::mutex> lock(queue_mutex);

            if (stop) {
                throw std::runtime_error("enqueue on stopped threadpool");
            }
            tasks.push_back([wrapper]() { (*wrapper)(); });
        }

        cv.notify_one(); // Wakker één slaperige worker op om de taak uit te voeren!
        return wrapper->get_future();
    }

private:
    friend class Worker; //Gives access to the private variables of this class

    std::vector<std::thread> workers;
    std::deque<std::function<void()>> tasks;


    std::mutex queue_mutex; //Lock for our queue
    std::condition_variable cv;
    bool stop = false;
};

inline void Worker::operator()()
{
    while (true)
    {
        std::function<void()> task;

        //Scope to restrict critical section
        //This is important because we don't want to hold the lock while executing the task,
        //because that would make it so only one task can be run simultaneously (aka sequantial)
        {
            std::unique_lock<std::mutex> locker(pool.queue_mutex);

            pool.cv.wait(locker, [this] {
                return pool.stop || !pool.tasks.empty();
                });


            if (pool.stop && pool.tasks.empty()) return;

            task = pool.tasks.front();
            pool.tasks.pop_front();
        }

        task();
    }
}