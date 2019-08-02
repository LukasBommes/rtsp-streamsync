#ifndef HEADERFILE_H
#define HEADERFILE_H

#include <iostream>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <sstream>
#include <string>
#include <algorithm>
#include <cmath>
#include <numeric>
#include <functional>

// OpenCV
#include <opencv2/opencv.hpp>

#include "../../video_cap/src/video_cap_validator.hpp"
#include "exceptions.hpp"

/*
*    Combines video frame, motion vectors, timestamp and other data read from the streams
*
*/

#define FRAME_OKAY  0
#define FRAME_DROPPED  1
#define FRAME_READ_ERROR  2
#define CAP_BROKEN  3

struct FrameData {
    double timestamp;
    uint8_t *frame;
    int height;
    int width;
    MVS_DTYPE *motion_vectors;
    MVS_DTYPE num_mvs;
    char frame_type[2];
    int frame_status;
};

typedef std::vector<std::shared_ptr<FrameData> > SSFramePacket;

// need FrameData and SSFramePacket type
#include "frame_packet_deque.hpp"
#include "frame_queue.hpp"

//typedef std::vector<std::unique_ptr<SharedQueue<std::shared_ptr<FrameData> > > > SSFrameBuffer;
typedef std::vector<std::unique_ptr<FrameQueue> > SSFrameBuffer;


/*
*    Implements synchronization of multiple streams
*
*/

class StreamSynchronizer {

private:

    /* configuration parameters */
    double max_initial_stream_offset;  // in seconds
    int max_read_errors;  // if reading of a frame subsequently fails this often raise an error to indicate connection loss

    std::vector<const char*> cams;
    std::vector<VideoCapWithValidator> caps;
    std::vector<std::thread> threads;
    SSFrameBuffer frame_buffers;
    std::unique_ptr<FramePacketDeque> frame_packet_buffer;

    /* for frame buffer rate control */
    std::condition_variable cv;
    std::mutex frame_buffer_mutex;
    double initial_avg_buffer_size;

    /* sequentially opens cameras (streams) based on a given connection string */
    void open_cams(void);

    /* background threads to read frames from stream and push them into the frame buffers */
    void read_frames(std::size_t cap_id);

    /* determines which of the input buffers has the most recent timestamp at it's front */
    int get_query_timestamp(double& query_timestamp);

    /* compute maximum initial stream offset */
    double max_stream_offset(void);

    /* determine the minimum length of all frame buffers */
    std::size_t min_frame_buffer_size(void);

    /* condition which returns true once all frame buffers contain the query timestamp */
    bool all_streams_passed_query_time(double query_timestamp);

    /* create packets of frame_data which are very close in time (synchronized) */
    SSFramePacket assemble_frame_packet(double query_timestamp, std::size_t query_cap_id);

    /* background thread which continuosly creates synchronized packets of frames */
    void generate_frame_packets(void);


public:

    /* constructor */
    StreamSynchronizer() {};

    /* overloaded constructor */
    StreamSynchronizer(std::vector<const char*> cams,
        double max_initial_stream_offset = 30.0,
        int max_read_errors = 3);

    /* destructor */
    ~StreamSynchronizer();

    /* initialization method called by the constructor (needed for Python C API) */
    void init(std::vector<const char*> cams,
        double max_initial_stream_offset,
        int max_read_errors);

    /* Retrieve the next synchronized frame packet if available, otherwise block */
    SSFramePacket get_frame_packet(void);
};

#endif
