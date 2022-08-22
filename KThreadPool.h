#ifndef THREAD_POOL_KTHREADPOOL_H_
#define THREAD_POOL_KTHREADPOOL_H_

#include <condition_variable>
#include <functional>
#include <future>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

class KThreadPool {
   public:
    KThreadPool(size_t nthreads = 4);
    ~KThreadPool();
    KThreadPool(const KThreadPool&) = delete;
    KThreadPool& operator=(const KThreadPool&) = delete;

   public:
    template <class F, class... Args>
    auto execute(F&& f, Args&&... args) -> std::future<decltype(f(args...))>;

   private:
    void runThread();

   private:
    std::vector<std::thread> m_threads;
    std::queue<std::function<void()>> m_tasks;
    size_t m_size;
    bool m_isterminated;
    std::mutex m_mtx;
    std::condition_variable m_cond;
};

KThreadPool::KThreadPool(size_t nthreads)
    : m_size(nthreads), m_isterminated(false) {
    for (size_t i = 0; i < m_size; ++i) {
        m_threads.emplace_back(&KThreadPool::runThread, this);
    }
}

KThreadPool::~KThreadPool() {
    m_isterminated = true;
    m_cond.notify_all();
    for (auto& th : m_threads) {
        th.join();
    }
}

template <class F, class... Args>
auto KThreadPool::execute(F&& f, Args&&... args)
    -> std::future<decltype(f(args...))> {
    using return_type = decltype(f(args...));

    auto task = std::make_shared<std::packaged_task<return_type()>>(
        std::bind(std::forward<F>(f), std::forward<Args>(args)...));

    std::future<return_type> ret = task->get_future();
    {
        std::unique_lock<std::mutex> lk(m_mtx);
        if (m_isterminated)
            throw std::runtime_error("execute after pool closed.\n");
        m_tasks.emplace([task]() { (*task)(); });
        m_cond.notify_one();
    }
    return ret;
}

void KThreadPool::runThread() {
    while (true) {
        std::unique_lock<std::mutex> lk(m_mtx);
        m_cond.wait(lk, [this] {
            return this->m_isterminated || !this->m_tasks.empty();
        });
        if (m_isterminated && m_tasks.empty())
            return;
        auto task = std::move(m_tasks.front());
        m_tasks.pop();
        lk.unlock();
        task();
    }
}

#endif