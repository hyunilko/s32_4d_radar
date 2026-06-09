"""Launch a AuRadar4D and a listener in a component container."""

import launch
from launch_ros.actions import ComposableNodeContainer
from launch_ros.descriptions import ComposableNode


def generate_launch_description():
    """Generate launch description with multiple components."""
    container = ComposableNodeContainer(
        name='radar_container',
        namespace='',
        package='rclcpp_components',
        executable='component_container',
        composable_node_descriptions=[
            ComposableNode(
                package='s32_radar',
                plugin='s32_radar::device_radar_node',
                name='device_radar_node',
                extra_arguments=[{'log_level': 'RELEASE'}]
            )
        ],
        output='screen',
    )

    return launch.LaunchDescription([container])
