#!/usr/bin/env bash

if [ -z "$1" ]; then
    echo "== build only =="
else
    echo "== remove and build =="
    rm -rf install build log

    export COLCON_PREFIX_PATH=$(echo "$COLCON_PREFIX_PATH" | tr ':' '\n' | grep -v "s32_4d_radar/install\|au_4d_radar_test/install" | paste -sd:)
    export AMENT_PREFIX_PATH=$(echo "$AMENT_PREFIX_PATH" | tr ':' '\n' | grep -v "s32_4d_radar/install\|au_4d_radar_test/install" | paste -sd:)
    export CMAKE_PREFIX_PATH=$(echo "$CMAKE_PREFIX_PATH" | tr ':' '\n' | grep -v "s32_4d_radar/install\|au_4d_radar_test/install" | paste -sd:)
fi

#colcon build --cmake-args -DCMAKE_BUILD_TYPE=Release --packages-select s32_radar radar_msgs mon_msgs tf_publisher_radar
#colcon build --cmake-args -DCMAKE_BUILD_TYPE=RelWithDebInfo --packages-select s32_radar radar_msgs mon_msgs tf_publisher_radar
colcon build --cmake-args -DCMAKE_BUILD_TYPE=Debug -DCMAKE_EXPORT_COMPILE_COMMANDS=ON --packages-select s32_radar radar_msgs mon_msgs tf_publisher_radar

