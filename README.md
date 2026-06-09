Building a verification environment for the AU 4D Radar Sensor
===========

## Introduction
This is a guide document for building a development environment to self-verify the operation of the AU 4D Radar Sensor.

## How to build the au_4d_radar

**Download AU 4D Radar ROS2 Test Source code**:
```bash
$ git clone https://github.com/auradar/au_4d_radar_test.git -b [branch]
$ mkdir -p au_4d_radar_test/src
```

**Download AU 4D Radar ROS2 Source code**:
```bash
$ cd au_4d_radar_test/src
$ git clone https://github.com/auradar/au_4d_radar.git -b [branch]
```

**Download radar messages**:
```bash
$ cd au_4d_radar_test/src
$ git clone https://github.com/ros-perception/radar_msgs.git
```

**Download monitor messages**:
```bash
$ cd au_4d_radar_test/src
$ git clone https://github.com/auradar/mon_msgs.git -b v2.0
```

**Download tf_publisher_radar**:
```bash
$ cd au_4d_radar_test/src
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
$ cd au_4d_radar_test
$ source _build.sh
```

###  Run Packages:
```bash
$ cd au_4d_radar_test
$ source _run.sh
```

## Package Install
> When you first install the RADAR ROS2 driver on your host, you will need to install the following packages:

### Install FlatBuffers:

**1. Download the FlatBuffers**
```bash
export VERSION=v25.2.10
git clone https://github.com/google/flatbuffers.git flatbuffers_$VERSION
cd flatbuffers_$VERSION
git checkout $VERSION
git submodule update --init --recursive
```

**2. Build and install the FlatBuffers**
```cmake
mkdir build
cd build

# Configure to build both static and shared libraries
cmake .. -DCMAKE_BUILD_TYPE=Release \
      -DFLATBUFFERS_STATIC_FLATC=ON \
      -DFLATBUFFERS_BUILD_SHAREDLIB=OFF \
      -DFLATBUFFERS_BUILD_CPP17=ON \
      -DCMAKE_INSTALL_PREFIX=/usr/local

make -j$(nproc)
sudo make install
```

### Install xacro:
```bash
sudo apt install ros-humble-xacro
```

### Install Eigen:
```bash
sudo apt-get install libeigen3-dev
```