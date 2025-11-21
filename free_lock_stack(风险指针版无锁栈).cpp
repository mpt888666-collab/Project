#include <iostream>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>
#include <chrono>
#include <random>
#include <set>
#include <cassert>
#define MAX_SIZE 100
struct hazard_pointer {
    std::atomic<std::thread::id> thread_id;
    std::atomic<void*> hp_pointer;
};

hazard_pointer hazard_pointers[MAX_SIZE];

class own_hp {
public:
    own_hp(): hp(nullptr) {
        for (auto & hazard_pointer : hazard_pointers) {
            std::thread::id old_id;
            if (hazard_pointer.thread_id.compare_exchange_strong(old_id, std::this_thread::get_id())) {
                hp = &hazard_pointer;
                hp->hp_pointer.store(nullptr);
                break;
            }
        }
        if (!hp) {
            throw std::runtime_error("hp is null");
        }
    }

    own_hp(const own_hp&) = delete;
    own_hp& operator=(const own_hp&) = delete;
    ~own_hp() {
        if (hp) {
            hp->thread_id.store(std::thread::id());
            hp->hp_pointer.store(nullptr);
        }
    }

    std::atomic<void*>& get_pointer() {
        return hp->hp_pointer;
    }
private:
    hazard_pointer* hp;
};

std::atomic<void*>& get_current_thread_hazard_pointer() {
    thread_local static own_hp hp;
    return hp.get_pointer();
}

template<typename T>
class free_lock_stack {
private:
    class node {
        friend class free_lock_stack<T>;
    private:
        std::shared_ptr<T> _data;
        node* _next;
        explicit node(const T& data) : _data(std::make_shared<T>(data)), _next(nullptr) {}
    };

private:
    std::atomic<node*> _head;
    std::atomic<int> _pop_thread_num;
    std::atomic<node*> _to_be_delete_head;

public:
    free_lock_stack() : _head(nullptr), _pop_thread_num(0), _to_be_delete_head(nullptr) {}

    free_lock_stack(const free_lock_stack&) = delete;
    free_lock_stack& operator=(const free_lock_stack&) = delete;

    void push(const T& data) {
        node* new_node = new node(data);
        new_node->_next = _head.load();
        do {
            new_node->_next = _head.load();
        }while (!_head.compare_exchange_weak(new_node->_next, new_node));
    }


    std::shared_ptr<T> pop() {
        node* old_head = _head.load();
        std::atomic<void*>& hp = get_current_thread_hazard_pointer();
        do {
            auto temp = old_head;
            do {
                temp = old_head;
                hp.store(old_head);
                old_head = _head.load();
            }while (temp != old_head);
        }while (old_head && !_head.compare_exchange_strong(old_head, old_head->_next));
        hp.store(nullptr);

        std::shared_ptr<T> res;
        if (old_head) {
            res.swap(old_head->_data);
            if (for_each_hazard_pointers_for_this_node(old_head)) {
                try_reclaim(old_head);
            }else {
                delete old_head;
            }
            delete_not_hazard_pointer();
        }
        return res;


    }

    void try_reclaim(node* old_node) {
        add_chain_delete_node(old_node);
    }

    void add_chain_delete_node(node * old_node) {
        add_chain_delete_list(old_node, old_node);
    }


    void add_chain_delete_list(node* first, node* last) {
        do {
            last->_next = _to_be_delete_head.load();
        }while (!_to_be_delete_head.compare_exchange_weak(last->_next, first));
    }

    bool for_each_hazard_pointers_for_this_node(node* node) {
        for (auto & hazard_pointer : hazard_pointers)
        {
            if (hazard_pointer.hp_pointer.load() == static_cast<void*>(node))
            {
                return true;
            }
        }
        return false;
    }

    void delete_not_hazard_pointer() {
        node* old_to_be_delete_head = _to_be_delete_head.exchange(nullptr);
        while (old_to_be_delete_head) {
            node *next = old_to_be_delete_head->_next;
            if (for_each_hazard_pointers_for_this_node(old_to_be_delete_head)) {
                add_chain_delete_node(old_to_be_delete_head);
            }else {
                delete old_to_be_delete_head;
            }
            old_to_be_delete_head = next;
        }
    }

};
