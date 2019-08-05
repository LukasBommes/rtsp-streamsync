# Video Stream Synchronizer

Stream-Synchronizer is a C++/Python library for synchronized retrieval of video frames from multiple IP network cameras (via Real Time Streaming Protocol). A call to the provided `get_frame_packet()` method yields a packet containing one video frame per camera. These frames are guaranteed to have been captured at approximately the same wall time, thus representing the scene at the same point in time.

Synchronization is needed for some applications, for example visual object tracking or people counting in multi-camera environments.

A usage example can be found in `stream_sync_test.py`.

Important Limitations:
- Can only be used with RTSP streams of senders which generate RTCP sender reports, e.g. ONVIF compatible IP cameras.
- All cameras need to run at the same frame rate.

## Quickstart

A Dockerfile is provided that sets up the environment and allows to run a test script for the Stream-Synchronizer.

Stream-Synchronizer depends on the [H.264 Motion Vector Capture](https://github.com/LukasBommes/sfmt-videocap) library which is automatically pulled and installed in the provided Dockerfile.

To install the sfmt-streamsync module simply build the Docker image with
```
sudo docker-compose build
```
and run the container with
```
sudo docker-compose up -d
```
Now, start an interactive shell session in the container via
```
sudo docker exec -it stream_sync_dev
```
Edit the camera source URLs in `stream_sync_test.py` to match those of your IP cameras and run the example with
```
python3 stream_sync_test.py
```
If everything works well, a graphics window showing the live stream for each camera should pop up. The streams within these windows should be synchronized which can be validated easily if the frame timestamps are shown as overlays on the video (see fig. 1 and fig. 2). Most IP cameras provide an option for this.

<center>
<img src="imgs/example_output_2.png" alt="example_output_gui" width="500"/><br>
<p>Fig. 1 - Example graphical output for multiple streams. Note the synchronized timers.</p></center>

<center>
<img src="imgs/example_output.png" alt="example_output_terminal" width="500"/><br><p>Fig. 2 - Example terminal output for multiple streams showing the sender time stamps for each frame.</p>
</center>

## Installation

If you plan to use the sfmt-streamsync module in your own project, you can install it by cloning the repo and building the source as described below.

<details>
  <summary>Installation on host (Ubuntu 18.04)</summary>

Install git
```
apt-get update && apt-get install -y git
```
Clone the git repository for the H.264 Video Capture and run the installer script for installing dependencies
```
mkdir -p /home && cd home && \
git clone https://sfmt-auto:Ow36ODbBoSSezciC@github.com/LukasBommes/h264-videocap.git video_cap && \
cd video_cap && \
chmod +x install.sh && \
./install.sh
```
Set environment variables (to permanently store them, append to `~/.profile` and source `~/.profile`)
```
export PATH="$PATH:$/home/bin"
export PKG_CONFIG_PATH="$PKG_CONFIG_PATH:/home/ffmpeg_build/lib/pkgconfig"
```
Compile the source for H.264 Video Capture and make the python wrapper
```
cd /home/video_cap && python3 setup.py install
```
Now, clone the Stream-Synchronizer repository
```
cd /home && git clone https://sfmt-auto:Ow36ODbBoSSezciC@github.com/LukasBommes/sfmt-streamsync.git stream_sync && \
```
Build sources and install python wrapper
```
cd /home/stream_sync && python3 setup.py install
```  
</details>

<details>
<summary>Installation in Docker image</summary>

```
FROM ubuntu:18.04

ENV HOME "/home"

RUN apt-get update && \
  apt-get install -y \
    git && \
    rm -rf /var/lib/apt/lists/*

###############################################################################
#
#							Python h264-videocap module
#
###############################################################################

# Build h264-videocap from source
RUN cd $HOME && \
  git clone -b "v1.0.0" https://sfmt-auto:Ow36ODbBoSSezciC@github.com/LukasBommes/sfmt-videocap.git video_cap && \
  cd video_cap && \
  chmod +x install.sh && \
  ./install.sh

# Set environment variables
ENV PATH "$PATH:$HOME/bin"
ENV PKG_CONFIG_PATH "$PKG_CONFIG_PATH:$HOME/ffmpeg_build/lib/pkgconfig"

RUN cd $HOME/video_cap && \
  python3 setup.py install

###############################################################################
#
#							Python stream-sync module
#
###############################################################################

# Build stream-sync from source
RUN cd $HOME && \
  git clone https://sfmt-auto:Ow36ODbBoSSezciC@github.com/LukasBommes/sfmt-streamsync.git stream_sync && \
  cd $HOME/stream_sync && \
  python3 setup.py install

WORKDIR $HOME

CMD ["sh", "-c", "tail -f /dev/null"]
```
</details>

## Python API

#### Class :: StreamSynchronizer()

| Methods | Description |
| --- | --- |
| StreamSynchronizer() | Constructor |
| get_frame_packet() | Open a video file or url |

##### Method :: StreamSynchronizer()

Constructor.

| Parameter | Type | Description |
| --- | --- | --- |
| cams | list of dict | Each list entry represents one IP camera and must have at least the key "source" and a RTSP stream URL, such as "rtsp://xxx.xxx.xxx.xxx:554", as a value. The synchronizer is designed for RTSP streams. Opening of video files will work, but leads to undesired behaviour. |
| max_initial_stream_offset | double | If the initial temporal offset between any of the video streams is larger than this threshold a StreamProcessingError is thrown and the program halts. |
| max_read_errors | int | If more subsequent frame reads than specified by this value fail, the stream status is changed and all subsequent frames from this stream will have status "CAP_BROKEN". |
| frame_packet_buffer_maxsize | int | The generated synchronized frame packets are put into an output buffer with this maximum size. If frame packets are generated at a faster rate than they are consumed, the oldest packet in the buffer is overwritten. If set to -1, then the frame packet buffer can grow unlimited.|

##### Method :: get_frame_packet()

Retrieves the next synchronized frame packet once available. Blocks until a new synchronized frame packet becomes becomes available.

Takes no input arguments and returns the synchronized frame packet as a dictionary with the structure:
```
frame_packet = {0: frame_data_0, 1: frame_data_1, ..., N: frame_data_N }
```
The integer keys refer to the camera ID of the stream and the frame_data sub-dictionaries contain the following keys with the according data:

| Key | Value Type | Value Description |
| --- | --- | --- |
| frame_status | string | Either "FRAME_OKAY" if the frame inside the frame packet is valid. If a camera could not be opened or reading a frame failed more then `max_read_errors` subsequent times, the frame status is "CAP_BROKEN". If one of the sync-buffers underruns, frame status is "FRAME_DROPPED" and if any error occurred during reading of the frame it is set to "FRAME_READ_ERROR". If the frame status is not "FRAME_OKAY", all other dict values are set to None. |
| timestamp | double | UTC wall time of each frame in the format of a UNIX timestamp. In case, input is a video file, the timestamp is derived from the system time. If the input is an RTSP stream the timestamp marks the time the frame was send out by the sender (e.g. IP camera). Thus, the timestamp represents the wall time at which the frame was taken rather then the time at which the frame was received. This allows e.g. for accurate synchronization of multiple RTSP streams. In order for this to work, the RTSP sender needs to generate RTCP sender reports which contain a mapping from wall time to stream time. Not all RTSP senders will send sender reports as it is not part of the standard. If IP cameras are used which implement the ONVIF standard, sender reports are always sent and thus timestamps can always be computed. If frame_status is not "FRAME_OKAY" None is returned. |
| frame | numpy array | Array of dtype uint8 shape (h, w, 3) containing the decoded video frame. w and h are the width and height of this frame in pixels. If frame_status is not "FRAME_OKAY" None is returned.  |
| frame_type | string | Unicode string representing the type of frame. Can be `"I"` for a keyframe, `"P"` for a frame with references to only past frames and `"B"` for a frame with references to both past and future frames. A `"?"` string indicates an unknown frame type. If frame_status is not "FRAME_OKAY" None is returned. |
| motion_vector | numpy array | Array of dtype int64 and shape (N, 10) containing the N motion vectors of the frame. Each row of the array corresponds to one motion vector. The columns of each vector have the following meaning (also refer to [AVMotionVector](https://ffmpeg.org/doxygen/4.1/structAVMotionVector.html) in FFMPEG documentation): <br>- 0: source: Where the current macroblock comes from. Negative value when it comes from the past, positive value when it comes from the future.<br>- 1: w: Width and height of the vector's macroblock.<br>- 2: h: Height of the vector's macroblock.<br>- 3: src_x: x-location of the vector's origin in source frame (in pixels).<br>- 4: src_y: y-location of the vector's origin in source frame (in pixels).<br>- 5: dst_x: x-location of the vector's destination in the current frame (in pixels).<br>- 6: dst_y: y-location of the vector's destination in the current frame (in pixels).<br>- 7: motion_x: src_x = dst_x + motion_x / motion_scale<br>- 8: motion_y: src_y = dst_y + motion_y / motion_scale<br>- 9: motion_scale: see definiton of columns 7 and 8<br>Note: If no motion vectors are present in a frame, e.g. if the frame is an `I` frame an empty numpy array of shape (0, 10) and dtype int64 is returned. If frame_status is not "FRAME_OKAY" None is returned. |

For an explanation of motion vectors and frame types refer to the documentation of the [H.264 Video Capture Class](https://github.com/LukasBommes/sfmt-videocap).


## Algorithm Explanation

##### Overview

Fig. 3 shows the overall architecture of the stream synchronization. The sender side consists of multiple IP cameras which are synchronized via NTP to a common reference time (UTC). Streams are transmitted to the receiver over network via Real Time Streaming Protocol (RTSP). The H.264 Video Capture module is used to read and decode frames from the streams and estimate UTC timestamps for each frame. For each stream an individual FIFO queue is used to buffer frames and timestamps. In regular intervals, an algorithm is scheduled that removes the relative offset between the streams. Once the offset is removed, a frame is taken from each buffer and put into a packet. Frames inside a frame packet have very similar timestamps which means they were taken at the same point in time.

<center>
<img src="imgs/framework.png" alt="stream_sync_framework" width="300"/><br>
<p>Fig. 3 - Overview of the synchronization algorithm of the Stream-Synchronizer.</p>
</center>

##### Frame Timestamp Estimation

Every frame is time stamped with the current UTC time when sent out by the IP camera. Estimation of this timestamp is done by the H.264 Video Capture module and described in its [documentation](https://github.com/LukasBommes/sfmt-videocap).

##### Initial Offset Check

Before the synchronization algorithms starts, the maximum initial stream offset between all streams is estimated by collecting one frame per stream and comparing their timestamps. If the estimated offset is larger than the value specified in `max_initial_stream_offset`, an error is thrown and the program halts.

##### Stream Offset Compensation

To create the next synchronized frame packet the following steps are performed:

1. Wait for all buffers to have `>= 1` frame<br>
<img src="imgs/step_0.png" alt="stream_offset_compensation_0" width="200"/><br>

2. Find most recent timestamp `T_q` at buffer fronts<br>
<img src="imgs/step_1.png" alt="stream_offset_compensation_1" width="200"/><br>

3. Wait until all buffers have a frame with timestamp `T >= T_q`<br>
<img src="imgs/step_2.png" alt="stream_offset_compensation_2" width="200"/><br>

4. Pop all frames with timestamps `T < T_q` from buffer fronts, remember last popped frame<br>
<img src="imgs/step_3.png" alt="stream_offset_compensation_3" width="200"/><br>

5. Make a packet of the last popped frames<br>
<img src="imgs/step_4.png" alt="stream_offset_compensation_4" width="200"/><br>

The frame packet is then enqueued in the output buffer and can be used in another thread.

Step 3 make sure that the buffer levels stay constant and that almost every frame in the buffers gets processed after the very first synchronization is performed.  

##### Frame Packet Buffer

Stream offset compensation runs as fast as frames are enqueued in the frame buffers. E.g. if the cameras run at 15 Hz, then the stream offset generation generates ~ 15 frame packets per second. Packets are enqueued in the frame packet buffer and can be retrieved by calling the `get_frame_packet()` method. Depending on the `frame_packet_buffer_maxsize` parameter the output buffer is limited in size or can grow unlimited. In both cases `get_frame_packet()` blocks until at least one frame packet is available. Thus, it is best practice to simply call `get_frame_packet()` as fast as possible.

If processing of packets is slower than packet generation, it is recommended to limit the maximum size of the buffer. In this case it acts a ring buffer and the oldest packets are overwritten as soon as a newer packet is available. In this case `get_frame_packet()` always returns the most recent frame packet.

##### Error Handling

The algorithm is equipped with capabilities to handle different kinds of errors:

- **Frame read error:** If reading of the next frame from a stream fails, e.g. because of a connection loss, a frame with status "FRAME_READ_ERROR" is enqueued in the frame buffer. If more then `max_read_errors` occur subsequently, the stream is deactivated and subsequent frames from this stream have status "CAP_BROKEN".

- **Camera connection loss:** If the provided stream URL can not be opened or too many subsequent read error occured, the stream produces frames with status "CAP_BROKEN".


##### Performance Benchmark


## About

This software is written by **Lukas Bommes, M.Sc.** - [A*Star SIMTech, Singapore](https://www.a-star.edu.sg/simtech)<br>


#### License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.
