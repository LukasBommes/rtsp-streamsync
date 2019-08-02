// gcc ../video_cap/src/time_cvt.cpp src/stream_sync.cpp src/stream_sync_test.cpp ../video_cap/src/video_cap.cpp `pkg-config --cflags --libs libavformat libswscale opencv4` --std=c++17

#include "stream_sync.hpp"

void draw_motion_vectors(cv::Mat frame, MVS_DTYPE *motion_vectors, MVS_DTYPE num_mvs) {
    for (MVS_DTYPE i = 0; i < num_mvs; ++i) {
        cv::Point start_pt, end_pt;
        start_pt.y = motion_vectors[i*10 + 4];  // src_y
        start_pt.x = motion_vectors[i*10 + 3];  // src_x
        end_pt.y = motion_vectors[i*10 + 6];  // dst_y
        end_pt.x = motion_vectors[i*10 + 5];  // dst_x
        cv::arrowedLine(frame, start_pt, end_pt, cv::Scalar(0, 0, 255), 1, cv::LINE_AA, 0, 0.1);
    }
}

int main () {

    std::vector<const char*> cams;
    //cams.push_back("vid.mp4");
    //cams.push_back("vid.mp4");
    //cams.push_back("vid.mp4");
    //cams.push_back("vid.mp4");
    cams.push_back("rtsp://user:SIMTech2019@172.20.114.22:554/cam/realmonitor?channel=1&subtype=0");
    cams.push_back("rtsp://user:SIMTech2019@172.20.114.23:554/cam/realmonitor?channel=1&subtype=0");
    cams.push_back("rtsp://user:SIMTech2019@172.20.114.24:554/cam/realmonitor?channel=1&subtype=0");
    cams.push_back("_rtsp://user:SIMTech2019@172.20.114.25:554/cam/realmonitor?channel=1&subtype=0"); // simulate a wrong camera url
    cams.push_back("rtsp://user:SIMTech2019@172.20.114.26:554/cam/realmonitor?channel=1&subtype=0");
    cams.push_back("rtsp://user:SIMTech2019@172.20.114.27:554/cam/realmonitor?channel=1&subtype=0");
    cams.push_back("rtsp://user:SIMTech2019@172.20.114.28:554/cam/realmonitor?channel=1&subtype=0");

    StreamSynchronizer stream_synchronizer(cams);

    // for every camera create an output window
    for(std::size_t cap_id = 0; cap_id < cams.size(); cap_id++) {
        std::stringstream window_name;
        window_name << "frame_" << cap_id;
        cv::namedWindow(window_name.str(), cv::WINDOW_NORMAL);
        cv::resizeWindow(window_name.str(), 640, 360);
    }

    int step = 0;
    while (1) {

        SSFramePacket frame_packet = stream_synchronizer.get_frame_packet();

        std::cout << "##### Step " << step++ << " #####" << std::endl;

        // loop over the items in the frame packet
        for(std::size_t cap_id = 0; cap_id < frame_packet.size(); cap_id++) {

            std::cout << "cap_id: " << cap_id
                      << " | frame_status " << (*frame_packet[cap_id]).frame_status;

            // show frame only if it is valid
            if((*frame_packet[cap_id]).frame_status != FRAME_OKAY) {
                std::cout << std::endl;
                continue;
            }

            std::cout << std::fixed
                      << " | timestamp: " << (*frame_packet[cap_id]).timestamp
                      << " | frame_height: " << (*frame_packet[cap_id]).height
                      << " | frame_width: " << (*frame_packet[cap_id]).width
                      << " | frame_type: " << (*frame_packet[cap_id]).frame_type
                      << " | num_mvs: " << (*frame_packet[cap_id]).num_mvs
                      << std::endl;

            // make cv mat from data buffer
            cv::Mat cv_frame((*frame_packet[cap_id]).height, (*frame_packet[cap_id]).width, CV_MAKETYPE(CV_8U, 3), (*frame_packet[cap_id]).frame);

            draw_motion_vectors(cv_frame, (*frame_packet[cap_id]).motion_vectors, (*frame_packet[cap_id]).num_mvs);

            // show frames
            std::stringstream window_name;
            window_name << "frame_" << cap_id;
            cv::imshow(window_name.str(), cv_frame);

            free((*frame_packet[cap_id]).frame);
            free((*frame_packet[cap_id]).motion_vectors);
        }
        // if user presses "ESC" stop program
        char c=(char)cv::waitKey(1);
        if(c==27) {
            break;
        }
    }

    cv::destroyAllWindows();
    return 0;
}
