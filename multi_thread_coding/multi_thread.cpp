#include<iostream>
#include<queue>
#include<mutex>
#include<thread>
#include<condition_variable>

#define MAX_SIZE 10
using namespace std;

class SafeBuffer { //simple single thread coding
private:
    queue<int> data_queue;
    mutex mtx;
    condition_variable cv;
public:
    SafeBuffer(){

    }
    void push(int val) {
        unique_lock<mutex> lock(mtx);
        cv.wait(lock, [this]{ return data_queue.size() < MAX_SIZE; });
        data_queue.push(val);
        cv.notify_one();
    }

    int pop(){
        unique_lock<mutex> lock(mtx);
        cv.wait(lock, [this]{ return !data_queue.empty(); });
        int val = data_queue.front();
        data_queue.pop();
        cv.notify_one();
        return val;
    }
};

class MultiBuffer{
private:
    queue<int> data_queue;
    mutex mtx;
    condition_variable cv_consume;
    condition_variable cv_produce;
public:
    MultiBuffer(){

    }
    void push(int val) {
        unique_lock<mutex> lock(mtx);
        cv_produce.wait(lock, [this]{ return data_queue.size() < MAX_SIZE; });
        data_queue.push(val);
        cv_consume.notify_all();
    }

    int pop(){
        unique_lock<mutex> lock(mtx);
        cv_consume.wait(lock, [this]{ return !data_queue.empty(); });
        int val = data_queue.front();
        data_queue.pop();
        cv_produce.notify_all();
        return val;
    }
};


class MultiBuffer_Opt_Space{
private:
    queue<int> data_queue;
    mutex mtx;
    condition_variable cv_consume;
    condition_variable cv_produce;
public:
    MultiBuffer(){

    }
    void push(int val) {
        {
            unique_lock<mutex> lock(mtx);
            cv_produce.wait(lock, [this]{ return data_queue.size() < MAX_SIZE; });
            data_queue.push(val);
        }
        cv_consume.notify_all();
    }

    int pop(){
        int val;
        {
            unique_lock<mutex> lock(mtx);
            cv_consume.wait(lock, [this]{ return !data_queue.empty(); });
            val = data_queue.front();
            data_queue.pop();
        }
        cv_produce.notify_all();
        return val;
    }
};

int main(){
    MultiBuffer buf;
    thread producer([&]{
        for (int i = 0; i < 10; i++) buf.push(i);
    });
    thread consumer([&]{
        for (int i = 0; i < 10; i++) cout << buf.pop() << " ";
    });
    producer.join();
    consumer.join();
}