#include <deque>
#include <thread>
#include <mutex>
#include <condition_variable>


/** Thread-safe ring buffer for SSFramePacket
*
*  A ring buffer implementation based on std::deque. Pop() retrieves a new
*  frame_packet from the deque front (oldest frame_packet) and blocks if the
*  deque is empty. If no maxsize is specified, push() never blocks and inserts a
*  new frame_packet in the back of the deque. If maxsize is specified the deque
*  grows until this size is reached. Further calls to push() will remove the
*  oldest frame_packet from the deque and only then insert the new frame_packet,
*  keeping the size of the deque constant.
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
        SSFramePacket frame_packet = this->deque_.front();
        this->deque_.pop_front();
        return frame_packet;
    }

    void pop(SSFramePacket& frame_packet) {
        std::unique_lock<std::mutex> mlock(this->mutex_);
        while (this->deque_.empty())
        {
          this->cond_.wait(mlock);
        }
        frame_packet = this->deque_.front();
        this->deque_.pop_front();
    }

    void push(const SSFramePacket& frame_packet) {
        std::unique_lock<std::mutex> mlock(this->mutex_);
        if (this->maxsize > 0 && (this->deque_.size() == this->maxsize)) { // deque is full
            SSFramePacket frame_packet = this->deque_.front();
            for (std::size_t cap_id = 0; cap_id < frame_packet.size(); cap_id++) {
                free((*frame_packet[cap_id]).frame);
                free((*frame_packet[cap_id]).motion_vectors);
            }
            this->deque_.pop_front();
        }
        this->deque_.push_back(frame_packet);
        mlock.unlock();
        this->cond_.notify_one();
    }

    void push(SSFramePacket&& frame_packet) {
        std::unique_lock<std::mutex> mlock(this->mutex_);
        if (this->maxsize > 0 && (this->deque_.size() == this->maxsize)) { // deque is full
            SSFramePacket frame_packet = this->deque_.front();
            for (std::size_t cap_id = 0; cap_id < frame_packet.size(); cap_id++) {
                free((*frame_packet[cap_id]).frame);
                free((*frame_packet[cap_id]).motion_vectors);
            }
            this->deque_.pop_front();
        }
        this->deque_.push_back(std::move(frame_packet));
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
