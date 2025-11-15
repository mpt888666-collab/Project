#include <memory>
#include <atomic>
template <typename T, int size>
class unlockCircleQueue : private std::allocator<T>{
public:
    explicit unlockCircleQueue():_head(0), _tail(0), _tail_update(0), _data(std::allocator<T>::allocate(size)){}

    unlockCircleQueue(const unlockCircleQueue&) = delete;
    unlockCircleQueue& operator=(const unlockCircleQueue&) = delete;

    ~unlockCircleQueue() {
        while (_head != _tail) {
            std::allocator_traits<std::allocator<T>>::destroy(*this, _data + _head);
            _head.store((_head + 1) % size);
        }
        std::allocator<T>::deallocate(_data, size);
    }

    template <typename ...Args>
    bool emplace(Args &&...args) {
        int t;
        do {
            t = _tail.load(std::memory_order_relaxed);
            if ((t + 1) % size == _head.load(std::memory_order_acquire)) {
                return false;
            }
        }while (!_tail.compare_exchange_strong(t, (t + 1) % size, std::memory_order_acq_rel, std::memory_order_relaxed));
        std::allocator_traits<std::allocator<T>>::construct(*this, _data + t,std::forward<Args>(args)...);
        int tail_up = t;
        do {
            tail_up = t;
        }while (!_tail_update.compare_exchange_strong(tail_up, (t + 1) % size, std::memory_order_release, std::memory_order_relaxed));
        return true;
    }

    bool push(const T& val) {
        return emplace(val);
    }

    bool pop(T&& val) {
        return emplace(std::forward<T>(val));
    }

    bool try_pop(T& val) {
        int h;
        do {
            h = _head.load(std::memory_order_relaxed);
            if (h == _tail.load(std::memory_order_acquire)) {
                return false;
            }
            if (h == _tail_update.load(std::memory_order_acquire)) {
                return false;
            }
            val = _data[h];
        }while (!_head.compare_exchange_strong(h, (h + 1) % size, std::memory_order_acq_rel, std::memory_order_relaxed));
        return true;
    }

    std::shared_ptr<T> pop() {
        int h;
        std::shared_ptr<T> res;
        do {
            h = _head.load(std::memory_order_relaxed);
            if (h == _tail.load(std::memory_order_acquire)) {
                return std::make_shared<T>();
            }
            if (h == _tail_update.load(std::memory_order_acquire)) {
                return std::make_shared<T>();
            }
            res = std::make_shared<T>(_data[h]);
        }while (!_head.compare_exchange_strong(h, (h + 1) % size, std::memory_order_acq_rel, std::memory_order_relaxed));
        return res;
    }
private:
    std::atomic<int> _head;
    std::atomic<int> _tail;
    std::atomic<int> _tail_update;
    T *_data;
};
