//c++11后这是安全的，推荐
template<typename T>
class Singleton2 {
public:
    Singleton2(const Singleton2&) = delete;
    Singleton2& operator=(const Singleton2&) = delete;

protected:

    static std::shared_ptr<T> GetInstance() {
        static std::shared_ptr<T> instance = std::shared_ptr<T>(new T());
        return instance;
    }
    Singleton2() = default;
};

//----------------------------------------------------------------------
//c++11之前

//饿汉式.只有一个线程初始化，不存在多线程访问问题
template<typename T>
class SingletonHungry {
public:
    SingletonHungry(const SingletonHungry&) = delete;
    SingletonHungry& operator=(const SingletonHungry&) = delete;

protected:
    static T& GetInstance() {
        return _instance;
    }

    SingletonHungry() = default;

    static T* _instance;
};
//类外初始化
template<typename T>
T* SingletonHungry<T>::_instance = new T();

//懒汉式

template<typename T>
class SingletonPointer {
public:
    SingletonPointer(const SingletonPointer&) = delete;
    SingletonPointer& operator=(const SingletonPointer&) = delete;

protected:
    static std::shared_ptr<T> GetInstance() {
      //双重锁检测有一个问题，主要因为new一个对象时发生
      //1.发生分配内存（allocate）
      //2.调用构造函数构造对象（construct）
      //3.将地址赋值给指针变量
      //而现实中2和3的步骤可能颠倒，所以有可能在一些编译器中通过优化是1，3，2的调用顺序，其他线程取到的指针就是非空，别的线程看_instance不为空就直接用了，但还没有构造完成，发生未知错误
        // if (_instance != nullptr) {
        //     return _instance;
        // }
        // _mtx.lock();
        // if (_instance != nullptr) {
        //     _mtx.unlock();
        //     return _instance;
        // }
        // _instance = std::shared_ptr<T>(new T());
        // _mtx.unlock();
        // return _instance;

      //下面的方法更为现代
        static std::once_flag flag;
        std::call_once(flag, []() {
            _instance = std::shared_ptr<T>(new T());
        });
        return _instance;
    }
    SingletonPointer() = default;

    static std::mutex _mtx;
    static std::shared_ptr<T> _instance;
};
template<typename T>
std::shared_ptr<T> SingletonPointer<T> ::_instance = nullptr;
