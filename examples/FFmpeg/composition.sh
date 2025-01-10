#!/bin/bash
VID_PATH=$1
MULTICAST_IP=$2
MULTICAST_PORT=$3

source /opt/amd/ama/ma35/scripts/setup.sh

ffmpeg -re -nostdin -hide_banner -loglevel error -hwaccel ama -hwaccel_device /dev/ama_transcoder0 \
 -c:v h264_ama -out_fmt yuv420p -resize 640x540 -stream_loop -1 -i ${VID_PATH}/Johnny_1280x720_60.mp4 \
 -c:v h264_ama -out_fmt yuv420p -resize 640x540 -stream_loop -1 -i ${VID_PATH}/KristenAndSara_1280x720_60.mp4 \
 -c:v h264_ama -out_fmt yuv420p -resize 640x540 -stream_loop -1 -i ${VID_PATH}/vidyo4_720p_60fps.mp4 \
 -c:v h264_ama -out_fmt yuv420p -resize 640x540 -stream_loop -1 -i ${VID_PATH}/vidyo1_720p_60fps.mp4 \
 -c:v jpeg_ama -out_fmt yuv420p -resize 640x540 -framerate 60 -i ${VID_PATH}/side.jpg \
 -c:v jpeg_ama -out_fmt yuv420p -resize 320x240 -framerate 60 -i ${VID_PATH}/icon.jpg \
 -c:v h264_ama -out_fmt yuv420p -resize 320x240 -stream_loop -1 -i ${VID_PATH}/vidyo3_720p_60fps.mp4 \
 -filter_complex "[4:v]fps=60[s];[5:v]fps=60[o];[0:v][1:v][2:v][3:v][s][o][6:v]\
 compositor_ama=inputs=7:out_res=1920x1080:enable_background=1:background=yellow:\
 input_params=\
 (out_x=0|out_y=0)\
 (out_x=640|out_y=0)\
 (out_x=0|out_y=540)\
 (out_x=640|out_y=540)\
 (out_x=1280|out_y=240|border_inner_size=40)\
 (out_x=1280|out_y=540|zorder=1)\
 (out_x=1280|out_y=0|zorder=2)[i]; \
 [i]zmq=bind_address=tcp\\\://127.0.0.1\\\:5555[d]" -map '[d]' -c:v h264_ama -b:v 5M -f mpegts udp://${MULTICAST_IP}:${MULTICAST_PORT}?pkt_size=1316 &

pid=$!
printf "Interactive mode.\n"
sleep 10

for i in 1 3 7 15 31 63 127; do
    printf "compositor_ama inputs_enabled=$i" | ./zmq_send.py "tcp://localhost:5555"
    sleep 5;
done

printf "compositor_ama input=5" | ./zmq_send.py "tcp://localhost:5555"
for i in `seq 1 9`; do
    printf "compositor_ama alpha=0.$i" |  ./zmq_send.py "tcp://localhost:5555"
    sleep 1
done
printf "compositor_ama alpha=1" |  ./zmq_send.py "tcp://localhost:5555"
for i in v h; do
    printf "compositor_ama flip=$i" |  ./zmq_send.py "tcp://localhost:5555"
    sleep 3
done

printf "compositor_ama input=4" | ./zmq_send.py "tcp://localhost:5555"
printf "compositor_ama border_color=red" | ./zmq_send.py "tcp://localhost:5555"
for i in `seq 0 8 200`; do
    printf "compositor_ama border_inner_size=$i" |  ./zmq_send.py "tcp://localhost:5555"
    sleep 1
done
printf "compositor_ama border_inner_size=4" |  ./zmq_send.py "tcp://localhost:5555"
sleep 3

printf "compositor_ama input=6" | ./zmq_send.py "tcp://localhost:5555"
for i in 'out_x=1600' 'out_y=840' 'out_x=1280'; do
    printf "compositor_ama $i" | ./zmq_send.py "tcp://localhost:5555"
    sleep 3
done
sleep 5
kill $pid

wait
printf "Done.\n"
 