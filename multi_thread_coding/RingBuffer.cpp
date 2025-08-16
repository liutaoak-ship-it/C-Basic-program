#include<iostream>
#include<queue>
#include<mutex>
#include<thread>
#include<condition_variable>
#include<atomic>
#include<semaphore.h>

using namespace std;
#define MAX_SIZE 10000

class RingBuffer {
private:
    condition_variable cv_produce;
    condition_variable cv_consume;
    mutex mtx;
    vector<int> vec;
    int readIndex;
    int writeIndex;
    int cap;
public:
    void push(int val) {
        {
            unique_lock<mutex> lock(mtx);
            cv_produce.wait(lock, [this]{return readIndex != (writeIndex + 1) % cap;});
            vec[writeIndex] = val;
            writeIndex = (writeIndex+1) % cap;
        }
        cv_consume.notify_all();
    }

    int pop() {
        int val;
        {
            unique_lock<mutex> lock(mtx);
            cv_consume.wait(lock, [this]{return readIndex != writeIndex;});
            val = vec[readIndex];
            readIndex = (readIndex+1) % cap;
        }
        cv_produce.notify_all();
        return val;
    }
    RingBuffer(int size):cap(size){
        vec.resize(cap);
        readIndex = 0;
        writeIndex = 0;
    }
};

template<typename T>
class RingBufferOpt{
    private:
    condition_variable cv_produce;
    condition_variable cv_consume;
    mutex mtx;
    vector<T> vec;
    int readIndex;
    int writeIndex;
    int cap;
    atomic<bool> closed{false}; 
public:
    void push(T& val) {
        {
            unique_lock<mutex> lock(mtx);
            cv_produce.wait(lock, [this]{return readIndex != (writeIndex + 1) % cap;});
            if(closed.load()) return;
            vec[writeIndex] = val;
            writeIndex = (writeIndex+1) % cap;
        }
        cv_consume.notify_all();
    }

    bool pop(T &val) {
        {
            unique_lock<mutex> lock(mtx);
            cv_consume.wait(lock, [this]{return readIndex != writeIndex;});
            if(closed.load() || readIndex == writeIndex) return false;
            val = vec[readIndex];
            readIndex = (readIndex+1) % cap;
        }
        cv_produce.notify_all();
        return true;
    }
    RingBufferOpt(int size):cap(size){
        vec.resize(cap);
        readIndex = 0;
        writeIndex = 0;
    }

    void stop() {
        closed.store(true);
        cv_consume.notify_all();
        cv_produce.notify_all();
    }
};

class RingBufferForSem { //性能高，跨平台性差
public:
    RingBufferForSem(int size) : buffer(size), capacity(size) {
        sem_init(&empty, 0, size); // 初始空闲空间=容量
        sem_init(&full, 0, 0);     // 初始数据量=0
        sem_init(&mutex, 0, 1);     // 二进制信号量作互斥锁
    }

    void push(int data) {
        sem_wait(&empty);  // 等待空闲空间
        sem_wait(&mutex);  // 加锁
        buffer[write_pos] = data;
        write_pos = (write_pos + 1) % capacity;
        sem_post(&mutex);  // 解锁
        sem_post(&full);   // 增加数据量
    }

    int pop() {
        sem_wait(&full);   // 等待数据
        sem_wait(&mutex);
        int data = buffer[read_pos];
        read_pos = (read_pos + 1) % capacity;
        sem_post(&mutex);
        sem_post(&empty);  // 增加空闲空间
        return data;
    }

private:
    std::vector<int> buffer;
    int capacity, read_pos = 0, write_pos = 0;
    sem_t empty, full, mutex;
};


void testRingBuffer() {
    RingBuffer rb(10);
    thread producer([&]{
        for (int i = 0; i < MAX_SIZE; i++) {
            rb.push(i);
            // cout << "Produced: " << i << endl;
        }
    });

    thread consumer([&]{
        for (int i = 0; i < MAX_SIZE; i++) {
            int val = rb.pop();
            // cout << "Consumed: " << val << endl;
        }
    });

    producer.join();
    consumer.join();
}


void testRingBufferOpt() {
    RingBufferOpt<int> rb(10);
    thread producer([&]{
        for (int i = 0; i < MAX_SIZE; i++) {
            rb.push(i);
            // cout << "Produced: " << i << endl;
        }
        rb.stop(); // 停止生产
    });

    thread consumer([&]{
        int val;
        while (rb.pop(val)) {
            // cout << "Consumed: " << val << endl;
        }
    });

    producer.join();
    consumer.join();
}


void testRingBufferForSem() {
    RingBufferForSem rb(10);
    thread producer([&]{
        for (int i = 0; i < MAX_SIZE; i++) {
            rb.push(i);
            // cout << "Produced: " << i << endl;
        }
    });

    thread consumer([&]{
        for (int i = 0; i < MAX_SIZE; i++) {
            int val = rb.pop();
            // cout << "Consumed: " << val << endl;
        }
    });

    producer.join();
    consumer.join();
}

int main() {
    // RingBufferOpt<int> buf(10);
    RingBufferForSem buf(10);
    auto start = chrono::high_resolution_clock::now();
    testRingBuffer();
    auto end = chrono::high_resolution_clock::now();
    cout << "Time taken: " 
         << chrono::duration_cast<chrono::microseconds>(end - start).count() 
         << " microseconds" << endl;
    testRingBufferOpt();
    auto end1 = chrono::high_resolution_clock::now();
    cout << "Time taken: " 
         << chrono::duration_cast<chrono::microseconds>(end1 - end).count() 
         << " microseconds" << endl;
    testRingBufferForSem();
    auto end2 = chrono::high_resolution_clock::now();
    cout << "Time taken: " 
         << chrono::duration_cast<chrono::microseconds>(end2 - end1).count() 
         << " microseconds" << endl;
}