#!/usr/bin/env python3
import rclpy
from rclpy.node import Node
from sensor_msgs.msg import Image
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import matplotlib.cm as cm
import numpy as np


class ColorbarPublisher(Node):
    def __init__(self):
        super().__init__('colorbar_publisher')

        self.declare_parameter('topic', '/radar_velocity_colorbar')
        self.declare_parameter('min_velocity', -30)
        self.declare_parameter('max_velocity', 30)
        self.declare_parameter('label', 'Velocity (m/s)')
        self.declare_parameter('width_px', 120)
        self.declare_parameter('height_px', 500)

        topic     = self.get_parameter('topic').value
        self._vmin = self.get_parameter('min_velocity').value
        self._vmax = self.get_parameter('max_velocity').value
        self._label = self.get_parameter('label').value
        self._w    = self.get_parameter('width_px').value
        self._h    = self.get_parameter('height_px').value

        self._pub = self.create_publisher(Image, topic, 1)
        # publish once at startup, then every 5 s (image is static)
        self._publish()
        self.create_timer(5.0, self._publish)
        self.get_logger().info(f'Colorbar published on {topic}  [{self._vmin}, {self._vmax}]')

    def _publish(self):
        dpi = 100
        fig_w = self._w / dpi
        fig_h = self._h / dpi

        fig, ax = plt.subplots(figsize=(fig_w, fig_h), dpi=dpi)
        fig.patch.set_facecolor('#303030')

        norm = plt.Normalize(vmin=self._vmin, vmax=self._vmax)
        cb = fig.colorbar(
            cm.ScalarMappable(norm=norm, cmap='rainbow'),
            cax=ax,
            orientation='vertical',
        )
        cb.set_label(self._label, color='white', fontsize=9)
        cb.ax.yaxis.set_tick_params(color='white', labelcolor='white', labelsize=8)
        cb.outline.set_edgecolor('white')

        fig.tight_layout(pad=0.4)
        fig.canvas.draw()

        w, h = fig.canvas.get_width_height()
        buf = np.frombuffer(fig.canvas.buffer_rgba(), dtype=np.uint8).reshape(h, w, 4)
        buf = buf[:, :, :3].copy()  # RGBA → RGB
        plt.close(fig)

        msg = Image()
        msg.header.stamp = self.get_clock().now().to_msg()
        msg.header.frame_id = ''
        msg.height = h
        msg.width  = w
        msg.encoding = 'rgb8'
        msg.is_bigendian = False
        msg.step = w * 3
        msg.data = buf.tobytes()
        self._pub.publish(msg)


def main(args=None):
    rclpy.init(args=args)
    node = ColorbarPublisher()
    rclpy.spin(node)
    node.destroy_node()
    rclpy.shutdown()


if __name__ == '__main__':
    main()
