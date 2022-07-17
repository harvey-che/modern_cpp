#ifndef _LIST_TS_H
#define _LIST_TS_H

#include <memory> // shared_ptr, unique_ptr
#include <mutex>  // mutex, lock_guard, unique_lock
#include <functional> // std::function


// Thread-safe singly-linked list
template <typename T>
class list_ts {

    private:
        struct node_ts {
            std::mutex                  m;
            T                           data;
            std::unique_ptr<node_ts>    next;  // each node can only be pointed by its parent

            node_ts() = default;
            node_ts(const T &data): data(data) {}
            ~node_ts() = default;
        };
        node_ts head;
    
    public:
        list_ts() = default;
        list_ts(const list_ts &) = delete;   // Not allow to copy
        list_ts& operator=(const list_ts &) = delete; // Not allow to assign
        ~list_ts();

        void push_front(const T &data);
        
        template<typename F>
        void for_each(F func);  //TODO std::function works here ?

        template<typename F>
        bool find_first_if(F p, T &data);

        template<typename F>
        void update_first_if(F p, const T &data, bool add=true);

        template<typename F>
        void remove_if(F p);

        // template<typename F>
        // int count();
};


template <typename T>
list_ts<T>::~list_ts() {
    remove_if([](auto i) {return true;});
}

template<typename T>
template<typename F>
void list_ts<T>::for_each(F func) {

    std::unique_lock<std::mutex> curr_locker(head.m);
    for (node_ts *next = head.next.get(); next != nullptr; next = next->next.get()) {
        // Lock curr -> Lock next -> Unlock curr
        std::unique_lock<std::mutex> next_locker(next->m);
        curr_locker.unlock();

        func(next->data);

        curr_locker = std::move(next_locker);
    }
}

template <typename T>
void list_ts<T>::push_front(const T &data) {

    std::unique_ptr<node_ts> new_node = std::make_unique<node_ts>(data);

    std::lock_guard<std::mutex> locker(head.m);
    new_node->next = std::move(head.next);  // next is unique_ptr, so need std::move
    head.next = std::move(new_node);
}


template<typename T>
template<typename F>
bool list_ts<T>::find_first_if(F p, T &data) {

    std::unique_lock<std::mutex> curr_locker(head.m);
    for (node_ts *next = head.next.get(); next != nullptr; next = next->next.get()) {
        // Lock curr -> Lock next -> Unlock curr
        std::unique_lock<std::mutex> next_locker(next->m);
        curr_locker.unlock();

        if (p(next->data)) {

            data = next->data;
            return true;
        } 

        curr_locker = std::move(next_locker);
    }

    return false;
}

template<typename T>
template<typename F>
void list_ts<T>::update_first_if(F p, const T &data, bool add) {

    {
        // Try to find and update existing node
        std::unique_lock<std::mutex> curr_locker(head.m);
        for (node_ts *next = head.next.get(); next != nullptr; next = next->next.get()) {
            // Lock curr -> Lock next -> Unlock curr
            std::unique_lock<std::mutex> next_locker(next->m);
            curr_locker.unlock();

            if (p(next->data)) {
                // Found and update
                next->data = data;
                return;
            } 
            curr_locker = std::move(next_locker);
        }
    }
    
    // If not found, push new node
    //   Need unlock head.m before call push_front(), since push_front() will lock head.m
    if (add)
        push_front(data);

}

template<typename T>
template<typename F>
void list_ts<T>::remove_if(F p) {
    
    std::unique_lock<std::mutex> curr_locker(head.m);
    for (node_ts *curr = &head, *next = head.next.get(); next != nullptr; next = curr->next.get()) {
        // Lock curr -> Lock next
        std::unique_lock<std::mutex> next_locker(next->m);
        
        if (p(next->data)) {

            //WARN: this move() will set next->next = nullptr,
            // so the iter expression in for-loop should NOT be next = next->next.get(),
            // otherwise, next would be nullptr and the for-loop would iterate only once.
            curr->next = std::move(next->next);  
            next_locker.unlock();
        } else {
            // Unlock curr while holding next
            curr_locker.unlock();
            curr = next;
            curr_locker = std::move(next_locker);
        }
    }
}

#endif