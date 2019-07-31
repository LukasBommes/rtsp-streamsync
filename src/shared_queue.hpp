#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>


template <typename T>
class SharedQueue
{
 public:

  T pop()
  {
    std::unique_lock<std::mutex> mlock(mutex_);
    while (queue_.empty())
    {
      cond_.wait(mlock);
    }
    auto item = queue_.front();
    queue_.pop();
    return item;
  }

  void pop(T& item)
  {
    std::unique_lock<std::mutex> mlock(mutex_);
    while (queue_.empty())
    {
      cond_.wait(mlock);
    }
    item = queue_.front();
    queue_.pop();
  }

  void push(const T& item)
  {
    std::unique_lock<std::mutex> mlock(mutex_);
    queue_.push(item);
    mlock.unlock();
    cond_.notify_one();
  }

  void push(T&& item)
  {
    std::unique_lock<std::mutex> mlock(mutex_);
    queue_.push(std::move(item));
    mlock.unlock();
    cond_.notify_one();
  }

  bool front(T& item)
  {
    std::lock_guard<std::mutex> mlock(mutex_);
    if (queue_.empty())
    {
      return false;
    }
    item = queue_.front();
    return true;
  }

  bool back(T& item)
  {
    std::lock_guard<std::mutex> mlock(mutex_);
    if (queue_.empty())
    {
      return false;
    }
    item = queue_.back();
    return true;
  }

  std::size_t size(void)
  {
    std::size_t size;
    std::unique_lock<std::mutex> mlock(mutex_);
    size = queue_.size();
    mlock.unlock();
    return size;
  }

 private:
  std::queue<T> queue_;
  std::mutex mutex_;
  std::condition_variable cond_;
};
