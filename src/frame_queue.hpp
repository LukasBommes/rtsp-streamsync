#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <memory>


class FrameQueue
{
 public:

  std::shared_ptr<FrameData> pop(bool free_mem=false)
  {
    std::unique_lock<std::mutex> mlock(mutex_);
    while (queue_.empty())
    {
      cond_.wait(mlock);
    }
    std::shared_ptr<FrameData> frame_data = queue_.front();
    if (free_mem) {
        free((*frame_data).frame);
        free((*frame_data).motion_vectors);
    }
    queue_.pop();
    return frame_data;
  }

  void pop(std::shared_ptr<FrameData>& frame_data, bool free_mem=false)
  {
    std::unique_lock<std::mutex> mlock(mutex_);
    while (queue_.empty())
    {
      cond_.wait(mlock);
    }
    frame_data = queue_.front();
    if (free_mem) {
        free((*frame_data).frame);
        free((*frame_data).motion_vectors);
    }
    queue_.pop();
  }

  void push(const std::shared_ptr<FrameData>& frame_data)
  {
    std::unique_lock<std::mutex> mlock(mutex_);
    queue_.push(frame_data);
    mlock.unlock();
    cond_.notify_one();
  }

  void push(std::shared_ptr<FrameData>&& frame_data)
  {
    std::unique_lock<std::mutex> mlock(mutex_);
    queue_.push(std::move(frame_data));
    mlock.unlock();
    cond_.notify_one();
  }

  bool front(std::shared_ptr<FrameData>& frame_data)
  {
    std::lock_guard<std::mutex> mlock(mutex_);
    if (queue_.empty())
    {
      return false;
    }
    frame_data = queue_.front();
    return true;
  }

  bool back(std::shared_ptr<FrameData>& frame_data)
  {
    std::lock_guard<std::mutex> mlock(mutex_);
    if (queue_.empty())
    {
      return false;
    }
    frame_data = queue_.back();
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
  std::queue<std::shared_ptr<FrameData>> queue_;
  std::mutex mutex_;
  std::condition_variable cond_;
};
