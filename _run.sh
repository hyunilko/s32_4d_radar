#!/usr/bin/env bash

sudo ./setup_can0.sh

#gnome-terminal -- bash -c "source install/local_setup.bash; ros2 launch tf_publisher_radar tf_publisher.launch.py; exec bash"
#gnome-terminal -- bash -c "cd /home/ubuntu/install/HesaiLidar_ROS_2.0; source install/local_setup.bash; ros2 launch hesai_ros_driver start.py"
#gnome-terminal -- bash -c "source install/local_setup.bash; ros2 launch s32_radar listener.launch.py; exec bash"

shopt -s nullglob

for pattern in 'debug-log*' 'core.*' '*.txt'; do
    mapfile -t files < <(find . -maxdepth 1 -type f -name "$pattern" -printf '%P\n')
    if [ ${#files[@]} -gt 0 ]; then
        rm -- "${files[@]}"
    fi
done

ulimit -c unlimited

source install/local_setup.bash
ros2 launch s32_radar radar.launch.py 2>&1 | tee "debug-log-$(date +%F-%H%M%S).txt"
