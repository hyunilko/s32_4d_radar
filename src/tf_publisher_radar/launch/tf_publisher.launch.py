import os

from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import LogInfo
from launch.actions import DeclareLaunchArgument, IncludeLaunchDescription, OpaqueFunction
from launch_ros.actions import ComposableNodeContainer, Node
from launch_ros.descriptions import ComposableNode
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch.substitutions import Command, LaunchConfiguration
from launch.conditions import IfCondition

def launch_setup(context, *args, **kwargs):

    urdf_model = LaunchConfiguration('urdf_model').perform(context)

    return [
        # :x: rviz2
        Node(
            condition=IfCondition(LaunchConfiguration("use_rviz").perform(context)),
            namespace='rviz2',
            package='rviz2',
            executable='rviz2',
            name='rviz2',
            output='log',
            arguments=['-d', LaunchConfiguration('rviz_config')],
        ),
        Node(
            condition=IfCondition(LaunchConfiguration("publish_tf").perform(context)),
            package='robot_state_publisher',
            executable='robot_state_publisher',
            parameters=[{'robot_description': Command(['xacro ', LaunchConfiguration('urdf_model')])}]
        ),
        Node(
            package='tf_publisher_radar',
            executable='colorbar_publisher.py',
            name='colorbar_publisher',
            output='log',
            parameters=[{
                'topic':        '/radar_velocity_colorbar',
                'min_velocity': -30,
                'max_velocity':  30,
                'label':        'Velocity (m/s)',
                'width_px':     120,
                'height_px':    500,
            }],
        ),
    ]

def generate_launch_description():
    package_path = get_package_share_directory("tf_publisher_radar")

    declared_arguments = [
        DeclareLaunchArgument("use_rviz", default_value='true'),
        DeclareLaunchArgument("publish_tf", default_value='true'),
        DeclareLaunchArgument("urdf_model", default_value=os.path.join(package_path, "urdf", "unified_radars.urdf")),
        DeclareLaunchArgument("rviz_config", default_value=os.path.join(package_path, "rviz", "unified_radars.rviz")),
    ]                                            # unified_radars.rviz multi_radars_each_pub.rviz

 
    return LaunchDescription(
        declared_arguments + [OpaqueFunction(function=launch_setup)]
    )
