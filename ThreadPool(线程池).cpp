class ThreadPool {
    using Task = std::packaged_task<void()>;
public:
    ThreadPool(const ThreadPool &) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    static ThreadPool& GetInstance() {
        static ThreadPool _instance;
        return _instance;
    }

    template<typename F, typename ...Args>
    auto commit(F&& f, Args&&... args) -> std::future<decltype(std::forward<F>(f)(std::forward<Args>(args)...))> {
        using RetType = decltype(std::forward<F>(f)(std::forward<Args>(args)...));
        if (_stop) return std::future<RetType>{};
        auto task = std::make_shared<std::packaged_task<RetType()>>(std::bind(std::forward<F>(f), std::forward<Args>(args)...));
        std::future<RetType> res = task->get_future();
        {
            std::lock_guard<std::mutex> lock(_mtx);
            _work_queue.emplace([task] { (*task)(); });
        }
        _cv.notify_one();
        return res;
    }

    int get_thread_num() {
        return _thread_num.load();
    }
    ~ThreadPool() {
        stop();
    }
private:
    explicit ThreadPool(int num = std::thread::hardware_concurrency()) : _stop(false), _thread_num(num){
        start();
    }

    void start() {
        for (int i = 0; i < _thread_num; ++i) {
            _threads.emplace_back([this]() {
                while (true) {
                    Task task;
                    {
                        std::unique_lock<std::mutex> lock(_mtx);
                        _cv.wait(lock, [this] {
                            return _stop || !_work_queue.empty();
                        });
                        if (_stop && _work_queue.empty()) return;
                        task = std::move(_work_queue.front());
                        _work_queue.pop();
                    }
                    --_thread_num;
                    task();
                    ++_thread_num;
                }
            });
        }
    }

    void stop() {
        _stop.store(true);
        _cv.notify_all();
        for (auto& thread : _threads) {
            if (thread.joinable()) thread.join();
        }
    }
private:
    std::mutex _mtx;
    std::condition_variable _cv;
    std::queue<Task> _work_queue;
    std::vector<std::thread> _threads;
    std::atomic<bool> _stop;
    std::atomic<int> _thread_num;
};
