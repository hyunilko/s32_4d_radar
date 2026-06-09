Building a verification environment for the S32 4D Radar Sensor
===============================================================

## Introduction

This is a guide document for building a development environment to self-verify the operation of the S32 Radar Sensor.

## How to build the s32_4d_radar

**Download AU 4D Radar ROS2 Test Source code**:

```bash
$ git clone https://github.com/auradar/s32_4d_radar.git -b [branch]
$ mkdir -p s32_4d_radar/src
```

**Download AU 4D Radar ROS2 Source code**:

```bash
$ cd s32_4d_radar/src
$ git clone https://github.com/auradar/s32_radar.git -b [branch]
```

**Download radar messages**:

```bash
$ cd s32_4d_radar/src
$ git clone https://github.com/ros-perception/radar_msgs.git
```

**Download monitor messages**:

```bash
$ cd s32_4d_radar/src
$ git clone https://github.com/auradar/mon_msgs.git -b v2.0
```

**Download tf_publisher_radar**:

```bash
$ cd s32_4d_radar/src
$ git clone https://github.com/auradar/tf_publisher_radar.git
```

### Frame ID Naming convention:

> If you want to display an identifier instead of frame_id, enter frame_id: identifier in the `system_info.yaml` file.

```yaml
radars:
  27c06058: FRONT RIGHT
  0b089dfa: REAR LEFT
```

### Build Packages:

```bash
$ cd s32_4d_radar
$ source _build.sh
```

### Run Packages:

```bash
$ cd s32_4d_radar
$ source _run.sh
```

## Package Install

> When you first install the RADAR ROS2 driver on your host, you will need to install the following packages:

Install xacro:

```bash
sudo apt install ros-humble-xacro
```

### Install Eigen:

```bash
sudo apt-get install libeigen3-dev
```
