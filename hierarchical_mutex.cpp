//层级锁，只有当前线程层级值比要家的锁的层级值高就可以加锁，否则抛出异常，有效避免死锁
//线程初始层级是一个很大的数，构造函数传入改锁的层级值
//避免死锁也可以同时加锁std::lock(mtx1, mtx2,...)，配合领养所std::lock_guard<std::mutex> lock(mtx1, std::adopt_lock)...  此外，还有c++17引入的scoped_lock(mtx1, ...);

class hierarchical_mutex {
public:
    explicit hierarchical_mutex(unsigned long hierarchical_value) : _hierarchical_value(hierarchical_value), _previous_hierarchical_value(0){}

    hierarchical_mutex(const hierarchical_mutex&) = delete;
    hierarchical_mutex& operator=(const hierarchical_mutex&) = delete;

    void lock() {
        check_for_violation();
        _mtx.lock();
        update_hierarchy_value();
    }

    void unlock() {
        if (_this_thread_hierarchical_value != _hierarchical_value) throw std::out_of_range("hierarchical value is not correct");
        _mtx.unlock();
        _this_thread_hierarchical_value = _previous_hierarchical_value;
    }

    bool try_lock() {
        check_for_violation();
        if (!_mtx.try_lock()) {
            return false;
        }
        update_hierarchy_value();
        return true;
    }
private:
    std::mutex _mtx;
    unsigned long _hierarchical_value;
    unsigned long _previous_hierarchical_value;
    static thread_local unsigned long _this_thread_hierarchical_value;

    void check_for_violation() const {
        if (_this_thread_hierarchical_value <= _hierarchical_value) throw std::out_of_range("hierarchical value is too high");
    }

    void update_hierarchy_value() {
        _previous_hierarchical_value = _this_thread_hierarchical_value;
        _this_thread_hierarchical_value = _hierarchical_value;
    }
};

thread_local unsigned long hierarchical_mutex::_this_thread_hierarchical_value(ULONG_MAX);
