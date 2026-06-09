## tf_publisher_radar package
## pre-requisite
Following pakcages should be installed before compile:
```
sudo apt install ros-humble-xacro ros-humble-robot-state-publisher
```

## How to build
Unzip the downloaded archive in the ros2 workspace and compile:
```
colcon build --packages-select tf_publisher_radar
```

## How to launch
Source the workspace and launch:
```
source install/local_setup.bash
ros2 launch tf_publisher_radar tf_publisher.launch.py
```
