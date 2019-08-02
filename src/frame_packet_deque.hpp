#include <deque>
#include <thread>
#include <mutex>
#include <condition_variable>


/** Thread-safe ring buffer for SSFramePacket
*
*  A ring buffer implementation based on std::deque. Pop() retrieves a new item
*  from the deque front (oldest item) and blocks if the deque is empty. If no
*  maxsize is specified, push() never blocks and inserts a new item in the back
*  of the deque. If maxsize is specified the deque grows until this size is
*  reached. Further calls to push() will remove the oldest item from the deque
*  and only then insert the new item, keeping the size of the deque constant.
*
*   @param maxsize If <= 0 (default) do not limit the size of the deque. If > 0
*       allow the deque to reach at most this size.
*/
class FramePacketDeque {
public:
    FramePacketDeque(std::size_t maxsize=-1) {
        this->maxsize = maxsize;
    }

    SSFramePacket pop() {
        std::unique_lock<std::mutex> mlock(this->mutex_);
        while (this->deque_.empty())
        {
          this->cond_.wait(mlock);
        }
        auto item = this->deque_.front();
        this->deque_.pop_front();
        return item;
    }

    void pop(SSFramePacket& item) {
        std::unique_lock<std::mutex> mlock(this->mutex_);
        while (this->deque_.empty())
        {
          this->cond_.wait(mlock);
        }
        item = this->deque_.front();
        this->deque_.pop_front();
    }

    void push(const SSFramePacket& item) {
        std::unique_lock<std::mutex> mlock(this->mutex_);
        if (this->maxsize > 0 && (this->deque_.size() == this->maxsize)) { // deque is full
            this->deque_.pop_front();
        }
        this->deque_.push_back(item);
        mlock.unlock();
        this->cond_.notify_one();
    }

    void push(SSFramePacket&& item) {
        std::unique_lock<std::mutex> mlock(this->mutex_);
        if (this->maxsize > 0 && (this->deque_.size() == this->maxsize)) { // deque is full
            this->deque_.pop_front();
        }
        this->deque_.push_back(std::move(item));
        mlock.unlock();
        this->cond_.notify_one();
    }

    std::size_t size(void) {
      std::size_t size;
      std::unique_lock<std::mutex> mlock(mutex_);
      size = this->deque_.size();
      mlock.unlock();
      return size;
    }

private:
    std::size_t maxsize;
    std::deque<SSFramePacket> deque_;
    std::mutex mutex_;
    std::condition_variable cond_;
};
