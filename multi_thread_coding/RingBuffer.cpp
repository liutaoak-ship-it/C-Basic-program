#include<iostream>
#include<queue>
#include<mutex>
#include<thread>
#include<condition_variable>
#include<atomic>

using namespace std;

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


int main() {
    RingBufferOpt<int> buf(10);
    thread producer([&]{
        for (int i = 0; i < 10; i++) buf.push(i);
    });
    thread consumer([&]{
        for (int i = 0; i < 10; i++) {
            int val;
            buf.pop(val);
            cout << val << " ";
        }
        cout << endl;
    });
    producer.join();
    consumer.join();
}