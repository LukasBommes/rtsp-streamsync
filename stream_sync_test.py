import cv2
import numpy as np

from stream_sync import StreamSynchronizer


if __name__ == "__main__":

    def finalize():
        cv2.destroyAllWindows()
        print("N = {}".format(len(max_dts)))
        print("mean dt_max = {}".format(np.mean(max_dts)))
        print("std dt_max = {}".format(np.std(max_dts)))

    cams = [
        # cam_0
        {"source": "rtsp://user:SIMTech2019@172.20.114.22:554/cam/realmonitor?channel=1&subtype=0",
         "calibration_parameters": "camera_calibration/calibration_parameters/0",
         "frame_width": 1920,
         "frame_height": 1080,
         "frame_rate": 15.0},
        # cam 1
        {"source": "rtsp://user:SIMTech2019@172.20.114.23:554/cam/realmonitor?channel=1&subtype=0",
         "calibration_parameters": "camera_calibration/calibration_parameters/1",
         "frame_width": 1920,
         "frame_height": 1080,
         "frame_rate": 15.0},
        # cam 2
        {"source": "rtsp://user:SIMTech2019@172.20.114.24:554/cam/realmonitor?channel=1&subtype=0",
         "calibration_parameters": "camera_calibration/calibration_parameters/0",
         "frame_width": 1920,
         "frame_height": 1080,
         "frame_rate": 15.0},
        # cam 3
        {"source": "rtsp://user:SIMTech2019@172.20.114.25:554/cam/realmonitor?channel=1&subtype=0",
         "calibration_parameters": "camera_calibration/calibration_parameters/1",
         "frame_width": 1920,
         "frame_height": 1080,
         "frame_rate": 15.0},
        # cam 4
        {"source": "rtsp://user:SIMTech2019@172.20.114.26:554/cam/realmonitor?channel=1&subtype=0",
         "calibration_parameters": "camera_calibration/calibration_parameters/0",
         "frame_width": 1920,
         "frame_height": 1080,
         "frame_rate": 15.0},
        # cam 5
        {"source": "rtsp://user:SIMTech2019@172.20.114.27:554/cam/realmonitor?channel=1&subtype=0",
         "calibration_parameters": "camera_calibration/calibration_parameters/1",
         "frame_width": 1920,
         "frame_height": 1080,
         "frame_rate": 15.0},
        # cam 6
        {"source": "rtsp://user:SIMTech2019@172.20.114.28:554/cam/realmonitor?channel=1&subtype=0",
         "calibration_parameters": "camera_calibration/calibration_parameters/0",
         "frame_width": 1920,
         "frame_height": 1080,
         "frame_rate": 15.0}
    ]

    stream_synchronizer = StreamSynchronizer(cams)


    for cap_id, cam in enumerate(cams):
        cv2.namedWindow("camera_{}".format(cap_id), cv2.WINDOW_NORMAL)
        cv2.resizeWindow("camera_{}".format(cap_id), 640, 360)

    try:

        step = 0
        max_dts = []
        while True:

            print("##### Step", step, "#####")
            step += 1;

            frame_packet = stream_synchronizer.get_frame_packet()

            if not frame_packet:
                raise RuntimeError("Received invalid Frame Packet")

            timestamps = []
            for cap_id, frame_data in frame_packet.items():

                print("cap_id:", cap_id, "| frame_status", frame_data["frame_status"], end=" ")
                # show frame only if it is valid
                if frame_data["frame_status"] != 0:
                    print()
                    continue

                # compute the maximum inter-packet time delta
                timestamps.append(frame_data["timestamp"])

                print(("| timestamp:", frame_data["timestamp"],
                       " | frame_type:", frame_data["frame_type"],
                       " | mvs shape:", np.shape(frame_data["motion_vector"])))

                cv2.imshow("camera_{}".format(cap_id), frame_data["frame"])

            max_dts.append(np.max(timestamps) - np.min(timestamps))

            if cv2.waitKey(1) & 0xFF == ord('q'):
               break

        finalize()

    except KeyboardInterrupt:
        finalize()
