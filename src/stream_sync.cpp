#include "stream_sync.hpp"


void StreamSynchronizer::open_cams(void) {
    for(std::size_t i = 0; i < this->cams.size(); i++) {
        VideoCapWithValidator cap;
        if (!cap.open(this->cams[i])) {
            cap.mark_invalid();
            std::cerr << "Failed to open stream " << i << "." << std::endl;
        }
        else {
            std::cout << "Opened stream " << i << "." << std::endl;
        }
        this->caps.push_back(cap);
    }
}


void StreamSynchronizer::read_frames(std::size_t cap_id) {
    int errors = 0; // for error counting
    //int step = 0; // for simulating breakdown

    while(1) {
        // if capture device is broken just idle
        // TODO: resume capture device if new frames are available
        if(!this->caps[cap_id].is_valid()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        uint8_t *np_frame = NULL;
        int width = 0;
        int height = 0;

        MVS_DTYPE *motion_vectors = NULL;
        MVS_DTYPE num_mvs = 0;
        char frame_type[2] = "?";

        double frame_timestamp = 0;

        // create a single FrameData object for every frame and use a shared pointer for management
        std::shared_ptr<FrameData> frame_data = std::make_shared<FrameData>();

        bool success = this->caps[cap_id].read(&np_frame, &width, &height, frame_type, &motion_vectors, &num_mvs, &frame_timestamp);

        // simulate a breakdown
        //if((cap_id == 4) && (step++ >= 400)) success = false;

        if (!success) {
            std::cerr << "Could not read the next frame from stream " << cap_id << "." << std::endl;
            errors++;
            (*frame_data).frame_status = FRAME_READ_ERROR;
            if(errors >= this->max_read_errors) {
                this->caps[cap_id].mark_invalid();
            }
        }
        else {
            errors = 0;

            // Since VideoCap::read allocates new memory for motion_vectors on every
            // call, no copying of the array is required. However, array under np_frame
            // gets reused on every call, so it has to be memcopied to a new buffer here.
            int frame_size = width * height * 3;
            uint8_t *np_frame_cp = new uint8_t[frame_size];
            std::copy(np_frame, np_frame+frame_size, np_frame_cp);

            (*frame_data).timestamp = frame_timestamp;
            (*frame_data).frame = np_frame_cp;
            (*frame_data).height = height;
            (*frame_data).width = width;
            (*frame_data).motion_vectors = motion_vectors;
            (*frame_data).num_mvs = num_mvs;
            strcpy((*frame_data).frame_type, frame_type);
            (*frame_data).frame_status = FRAME_OKAY;
        }

        // prevent access to the frame buffer during synchronization
        this->frame_buffers[cap_id]->push(std::move(frame_data));

        // notify frame packet generator thread
        this->cv.notify_one();
    }
}


int StreamSynchronizer::get_query_timestamp(double& query_timestamp) {

    std::vector<double> oldest_timestamps(this->frame_buffers.size());

    // lookup the oldest timestamps from all frame buffers
    for(std::size_t cap_id = 0; cap_id < this->frame_buffers.size(); cap_id++) {

        if(!this->caps[cap_id].is_valid()) {
            continue;
        }

        std::shared_ptr<FrameData> front_item;
        if(this->frame_buffers[cap_id]->front(front_item)) {
            if((*front_item).frame_status == FRAME_OKAY) {  // consider only valid frames
                oldest_timestamps[cap_id] = (*front_item).timestamp;
            }
        }
    }

    // compute which of those timestamps is the most recent and to which cap_id it belongs
    std::size_t query_cap_id = std::max_element(oldest_timestamps.begin(), oldest_timestamps.end()) - oldest_timestamps.begin();
    query_timestamp = *std::max_element(oldest_timestamps.begin(), oldest_timestamps.end());

    return query_cap_id;
}


double StreamSynchronizer::max_stream_offset(void) {

    std::vector<double> timestamps;
    double min_timestamp;
    double max_timestamp;

    // get timestamps at same buffer positions
    for(std::size_t cap_id = 0; cap_id < this->frame_buffers.size(); cap_id++) {

        std::shared_ptr<FrameData> buffer_item;

        if(!this->caps[cap_id].is_valid())
            continue;

        // check if there is an item at the buffer front (oldest frame) and if so take it's timestamp
        if(this->frame_buffers[cap_id]->front(buffer_item))
            timestamps.push_back((*buffer_item).timestamp);
    }

    if(timestamps.empty())
        return -1.0;

    min_timestamp = *std::min_element(timestamps.begin(), timestamps.end());
    max_timestamp = *std::max_element(timestamps.begin(), timestamps.end());

    return double(max_timestamp - min_timestamp);
}


std::size_t StreamSynchronizer::min_frame_buffer_size(void) {
    std::vector<std::size_t> sizes;
    for(std::size_t cap_id = 0; cap_id < this->caps.size(); cap_id++) {
        if(!this->caps[cap_id].is_valid())
            continue;

        sizes.push_back(this->frame_buffers[cap_id]->size());
    }
    return *std::min_element(sizes.begin(), sizes.end());;
}


bool StreamSynchronizer::all_streams_passed_query_time(double query_timestamp) {
    for(std::size_t cap_id = 0; cap_id < this->frame_buffers.size(); cap_id++) {
        std::shared_ptr<FrameData> frame_data;

        // do not consider invalid streams
        if(!this->caps[cap_id].is_valid())
            continue;

        // lookup the most recently pushed item
        if(!this->frame_buffers[cap_id]->back(frame_data))
            return false; // queue is empty

        // if any of the newest timestamps is not yet the query timestamp return false
        if((*frame_data).timestamp < query_timestamp)
            return false;
    }
    // only if all newest timestamps are newer than query_timestamp
    return true;
}


SSFramePacket StreamSynchronizer::assemble_frame_packet(double query_timestamp, std::size_t query_cap_id) {

    SSFramePacket frame_packet;

    // loop over all other frame buffers
    for(std::size_t cap_id = 0; cap_id < this->frame_buffers.size(); cap_id++) {

        std::shared_ptr<FrameData> frame_data, frame_data_tmp;

        // if cap is broken do not consider it during synchronization
        if(!this->caps[cap_id].is_valid()) {
            // since we do not retrieve any FrameData from buffer, we need to create an empty one first
            frame_data = std::make_shared<FrameData>();
            (*frame_data).frame_status = CAP_BROKEN;
            frame_packet.push_back(std::move(frame_data));
            continue;
        }

        // for the input buffer with the most recent timestamp simply get the rightmost frame
        if(cap_id == query_cap_id) {
            this->frame_buffers[cap_id]->pop(frame_data); // actually, this should not block and if it is empty an empty frame packet should be generated
            frame_packet.push_back(std::move(frame_data));
            continue;
        }

        while(1) {
            // try to read out the front item
            if(!this->frame_buffers[cap_id]->front(frame_data_tmp)) {
                frame_data = std::make_shared<FrameData>();
                (*frame_data).frame_status = FRAME_DROPPED;
                frame_packet.push_back(std::move(frame_data));
                break;
            }

            // if the frame is invalid it has no timestamp for
            // synchronization, so just remove it from the buffer
            if((*frame_data_tmp).frame_status != FRAME_OKAY) {
                this->frame_buffers[cap_id]->pop(true); // pop item and free allocated memory
                break;
            }

            // if the frame is valid remove items from the buffer until it's timestamp matches the query timestamp
            if((*frame_data_tmp).timestamp <= query_timestamp) { // the "=" is important in case the timestamp is identical to the query timestamp
                this->frame_buffers[cap_id]->pop(true);  // remove item from the input buffer and free memory
                frame_data = std::move(frame_data_tmp);
            }
            else {
                frame_packet.push_back(std::move(frame_data));
                break;
            }
        }
    }

    return frame_packet;
}


void StreamSynchronizer::generate_frame_packets(void) {

    // wait until every (valid) buffer has at least one frame stored
    std::cout << "Waiting for buffers to fill up... ";
    while(1) {
        std::size_t min_buffer_size = this->min_frame_buffer_size();
        if(min_buffer_size >= 1) {
            break;
        }
    }
    std::cout << "[OK]" << std::endl;

    std::cout << "Checking if streams can be synchronized... ";
    double max_offset = this->max_stream_offset();
    if(max_offset > this->max_initial_stream_offset) {
        std::stringstream error;
        error << "Stream timestamps have a relative offset of "
              << max_offset << " seconds. This exceeds the maximum allowed "
              << "offset of " << this->max_initial_stream_offset
              << " seconds. Ensure all cameras use the same NTP server.";
        throw StreamProcessingError(error.str());
    }
    else if(max_offset == -1.0) {
        std::stringstream error;
        error << "Stream offset can not be computed. Ensure the specified streams are available.";
        throw StreamProcessingError(error.str());
    }
    else {
        std::cout << "(relative offset " << max_offset
                  << " seconds) [OK]" << std::endl;
    }

    // continuously generate new synchronized frame packets and put them in the output buffer
    while(1) {

        // print buffer sizes
        std::size_t total_size = 0;
        std::cout << "Frame buffer sizes: ";
        for(std::size_t cap_id = 0; cap_id < this->frame_buffers.size(); cap_id++) {
            std::size_t s = this->frame_buffers[cap_id]->size();
            total_size += s;
            std::cout << s << " | ";
        }
        std::cout << "total = " << total_size << std::endl;

        // wait until all of the (valid) buffers has an element
        std::unique_lock<std::mutex> lk(this->frame_buffer_mutex);
        this->cv.wait(lk, [this]{return (this->min_frame_buffer_size() > 0);});
        lk.unlock();

        // get most recent of all oldest timestamps (the timestamps on the buffer front)
        double query_timestamp;
        std::size_t query_cap_id = this->get_query_timestamp(query_timestamp);

        // wait until each queue has passed this timepoint (queue back has this or a newer timestamp)
        lk.lock();
        this->cv.wait(lk, std::bind(&StreamSynchronizer::all_streams_passed_query_time, this, query_timestamp));
        lk.unlock();

        // now pop all older timestamps up to this timepoint from the buffers and put frame data into a packet
        SSFramePacket frame_packet = this->assemble_frame_packet(query_timestamp, query_cap_id);

        this->frame_packet_buffer->push(std::move(frame_packet));
    }
}


StreamSynchronizer::StreamSynchronizer(std::vector<const char*> cams,
    double max_initial_stream_offset,
    int max_read_errors) {

    this->init(cams, max_initial_stream_offset, max_read_errors);
}


StreamSynchronizer::~StreamSynchronizer() {
    // stop background threads
    for(std::size_t i = 0; i < this->threads.size(); i++) {
        this->threads[i].std::thread::~thread();
    }

    // release capture devices
    for(std::size_t i = 0; i < this->caps.size(); i++) {
        this->caps[i].release();
    }
}


void StreamSynchronizer::init(std::vector<const char*> cams,
    double max_initial_stream_offset,
    int max_read_errors) {

    this->cams = cams;
    this->max_initial_stream_offset = max_initial_stream_offset;
    this->max_read_errors = max_read_errors;

    this->frame_packet_buffer = std::make_unique<FramePacketDeque>(1);

    this->open_cams();

    // create frame buffers
    for(std::size_t i = 0; i < this->caps.size(); i++) {
        std::unique_ptr<FrameQueue> frame_buffer = std::make_unique<FrameQueue>();
        this->frame_buffers.push_back(std::move(frame_buffer));
    }

    // start background threads to read frames into frame buffers
    for(std::size_t i = 0; i < this->caps.size(); i++) {
        this->threads.push_back(
            std::thread(&StreamSynchronizer::read_frames, this, i)
        );
    }

    // start background thread to generate synchronized frame packets
    this->threads.push_back(
        std::thread(&StreamSynchronizer::generate_frame_packets, this)
    );
}


SSFramePacket StreamSynchronizer::get_frame_packet(void) {
    return this->frame_packet_buffer->pop();
}
