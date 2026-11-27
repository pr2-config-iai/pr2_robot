#!/usr/bin/env python3

import rclpy
from rclpy.duration import Duration
from rclpy.node import Node
import PyKDL
from sensor_msgs.msg import Imu
from pr2_mechanism_controllers.msg import Odometer
from threading import Lock
from math import fabs, pi
from diagnostic_msgs.msg import DiagnosticArray, DiagnosticStatus, KeyValue

EPS = 0.0001

class ImuMonitor:
    def __init__(self, node):
        self.node = node
        self.lock = Lock()

        # reset state
        self.dist = 0.0
        self.drift = -1.0
        self.last_angle = 0.0
        self.start_angle = 0.0
        self.start_time = self.now()

        # subscribe to topics
        self.imu_sub = node.create_subscription(Imu, 'torso_lift_imu/data', self.imu_cb, 10)
        self.odom_sub = node.create_subscription(Odometer, 'base_odometry/odometer', self.odom_cb, 10)

        # diagnostics
        self.pub_diag = node.create_publisher(DiagnosticArray, '/diagnostics', 1)

        self.diag_timer = node.create_timer(1.0, self.publish_diagnostics)

    def now(self):
        return self.node.get_clock().now()

    def imu_cb(self, msg):
        with self.lock:
            rot = PyKDL.Rotation.Quaternion(msg.orientation.x, msg.orientation.y,
                                            msg.orientation.z, msg.orientation.w)
            (r, p, self.last_angle) = rot.GetRPY()

    def odom_cb(self, msg):
        with self.lock:
            dist = msg.distance + (msg.angle * 0.25)

            # check if base moved
            if dist > self.dist + EPS:
                self.start_time = self.now()
                self.start_angle = self.last_angle
                self.dist = dist

            # do imu test if possible
            if self.now() > self.start_time + Duration(seconds=10.0):
                self.drift = fabs(self.start_angle - self.last_angle)*180/(pi*10)
                self.start_time = self.now()
                self.start_angle = self.last_angle
                self.last_measured = self.now()

    def publish_diagnostics(self):
        with self.lock:
            # publish diagnostics
            d = DiagnosticArray()
            d.header.stamp = self.now().to_msg()
            ds = DiagnosticStatus()
            ds.name = "imu_node: Imu Drift Monitor"
            if self.drift < 0.5:
                ds.level = DiagnosticStatus.OK
                ds.message = 'OK'
            elif self.drift < 1.0:
                ds.level = DiagnosticStatus.WARN
                ds.message = 'Drifting'
            else:
                ds.level = DiagnosticStatus.ERROR
                ds.message = 'Drifting'
            drift = self.drift
            if self.drift < 0:
                last_measured = 'No measurements yet, waiting for base to stop moving before measuring'
                drift = 'N/A'
            else:
                age = (self.now() - self.last_measured).nanoseconds / 1e9
                if age < 60:
                    last_measured = '%f seconds ago'%age
                elif age < 3600:
                    last_measured = '%f minutes ago'%(age/60)
                else:
                    last_measured = '%f hours ago'%(age/3600)
            ds.values = [
                KeyValue(key='Last measured', value=last_measured),
                KeyValue(key='Drift (deg/sec)', value=str(drift)) ]
            d.status = [ds]
            self.pub_diag.publish(d)

def main():
    rclpy.init()
    node = Node('imu_monitor')

    monitor = ImuMonitor(node)

    try:
        rclpy.spin(node)
    except (KeyboardInterrupt, rclpy.executors.ExternalShutdownException):
        pass


if __name__ == '__main__':
    main()
