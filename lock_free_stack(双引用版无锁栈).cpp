template<typename T>
class lock_free_stack {
private:
    struct node;

    struct counted_ptr {
        node* ptr;
        unsigned external_count;
        counted_ptr() : ptr(nullptr), external_count(0) {}
        explicit counted_ptr(const T& val) : ptr(new node(val)), external_count(1) {}
    };

    struct node {
        std::shared_ptr<T> data;
        std::atomic<int> internal_count;
        counted_ptr next;

        explicit node(const T& v)
            : data(std::make_shared<T>(v)), internal_count(0) {
            next.ptr = nullptr;
            next.external_count = 0;
        }
    };

    std::atomic<counted_ptr> head;

    static void release_internal(node* n) {
        if (n->internal_count.fetch_sub(1, std::memory_order_acq_rel) == 1) {
            delete n;
        }
    }

public:
    lock_free_stack() {
        counted_ptr init;
        head.store(init);
    }

    void push(const T& val) {
        counted_ptr new_head(val);
        do {
            new_head.ptr->next = head.load();
        }while (!head.compare_exchange_weak(new_head.ptr->next, new_head));
    }



    std::shared_ptr<T> pop() {
        counted_ptr old_head = head.load();
        for (;;) {
            counted_ptr new_node;
            do {
                new_node = old_head;
                ++new_node.external_count;
            }while (!head.compare_exchange_weak(old_head, new_node));
            old_head = new_node;
            auto ptr = old_head.ptr;
            if (!ptr) return std::shared_ptr<T>();

            if (head.compare_exchange_weak(old_head, ptr->next)) {
                std::shared_ptr<T> res;
                res.swap(ptr->data);

                int increase_count = old_head.external_count - 2;
                if (ptr->internal_count.fetch_add(increase_count) == - increase_count) delete ptr;
                return res;
            }else {
                if (ptr->internal_count.fetch_sub(1) == 1) delete ptr;
            }
        }
    }
};
