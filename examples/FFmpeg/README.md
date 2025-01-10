# Simple 0MQ Interactive 2D Processor Demo:

Scripts in this directory demonstrate how to perform real-time interactive 2D processing on MA35D card.

## Prerequisite:

1. Ensure that Python module __zmq__ is installed: pip install zmq

## Execution:

1. Download all video, image, shell and Python files into a target directory.
1. Ensure that both __zmq_send.py__ and __composition.sh__ are executable.
1. Execute __composition.sh__ \<Video Path\> \<MULTICAST_IP\> \<MULTICAST_PORT\>, where \<Video Path\> is the base directory of the video contents, \<MULTICAST_IP\> and \<MULTICAST_PORT\> are multicast IP and port values, e.g., __./composition.sh . 239.0.0.1 8000. (Note that multicast addresses can be replace by unicast ones for this demo.)

1. To view the video, on a system that is in the same subnet, run ffplay udp://239.0.0.1:8000 

