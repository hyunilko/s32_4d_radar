#!/bin/bash

CURRENT_DIR=$(pwd)

cleanup() {
    # 1. Create New terminal (same path as main terminal)
    gnome-terminal --title="📡 New Main terminal" --working-directory="$CURRENT_DIR" \
        -- bash -c "echo '※ New Main terminal activated ※'; exec bash" &
    
    # 2. Terminate all ROS2 resources
    echo "- Terminating all ROS nodes..."
    ros2 daemon stop
    pkill -f "ros2 launch" 2>/dev/null
    pkill -f "ros2 run" 2>/dev/null

    # 3. Close all terminal windows (including main)
    echo "- Closing all terminal windows..."
    pkill -f "gnome-terminal.*TF Publisher"
    pkill -f "gnome-terminal.*HesaiLidar"
    #pkill -f "gnome-terminal.*Radar Listener"
    pkill -f "run_radar.launch.py"

    echo "[System shutdown completed successfully]"
    exit 0
}

# Trap Ctrl+C signal
trap cleanup SIGINT

source install/local_setup.bash

gnome-terminal --title="📡 TF Publisher" -- bash -c \
    "source install/local_setup.bash; ros2 launch tf_publisher_radar tf_publisher.launch.py; exec bash"

gnome-terminal --title="📡 HesaiLidar" -- bash -c \
"cd /home/ubuntu/install/HesaiLidar_ROS_2.0; source install/local_setup.bash; ros2 launch hesai_ros_driver start.py"

# gnome-terminal --title="📡 Radar Listener" -- bash -c \
#     "source install/local_setup.bash; ros2 launch au_4d_radar listener.launch.py; exec bash"

echo "============================================"
echo "  System running... Press Ctrl+C to:"
echo "  1. Auto-create recovery terminal"
echo "  2. Terminate all ROS nodes"
echo "  3. Close all terminal windows"
echo "============================================"

files=(debug-log*)
if [ -e "${files[0]}" ]; then
    rm debug-log*
fi

files2=(core.*)
if [ -e "${files2[0]}" ]; then
    rm core.*
fi

ulimit -c unlimited

ros2 launch au_4d_radar radar.launch.py 2>&1 | tee "debug-log-$(date +%F-%H%M%S).txt"

cleanup