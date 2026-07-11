import os

from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch_ros.actions import Node


def generate_launch_description():
    config = os.path.join(
        get_package_share_directory("hexa_display"), "config", "face.yaml"
    )
    return LaunchDescription(
        [
            Node(
                package="hexa_display",
                executable="face_node",
                name="face_node",
                output="screen",
                parameters=[config],
            )
        ]
    )
