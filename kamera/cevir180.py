#!/usr/bin/env python3
# SPDX-License-Identifier: LicenseRef-dwifk0-All-Rights-Reserved
# Copyright (C) 2026 Ahmet Efe Nezli
"""
Bir goruntu konusunu 180 derece cevirip baska konuya basar.

Neden ayri dugum: kamera donaniminda flip/rotate kontrolu YOK (v4l2-ctl ile
bakildi, icSpring 32e6:9211'de yalnizca parlaklik/kontrast var). usb_cam de
cevirme yapmiyor. Tek yol yeniden yayin.

Maliyet: 640x480'de cv2.rotate ~1 ms, 15 Hz'de ihmal edilebilir.

    python3 cevir180.py --girdi /camera/ham_on --cikti /camera/image_raw
"""
import argparse
import rclpy
from rclpy.node import Node
from rclpy.qos import qos_profile_sensor_data
from sensor_msgs.msg import Image
from cv_bridge import CvBridge
import cv2


class Cevir(Node):
    def __init__(self, girdi, cikti):
        super().__init__("cevir180")
        self.k = CvBridge()
        self.pub = self.create_publisher(Image, cikti, qos_profile_sensor_data)
        self.create_subscription(Image, girdi, self.geldi, qos_profile_sensor_data)
        self.get_logger().info("180 cevirme: %s -> %s" % (girdi, cikti))

    def geldi(self, m):
        try:
            g = self.k.imgmsg_to_cv2(m, desired_encoding="bgr8")
            d = cv2.rotate(g, cv2.ROTATE_180)
            y = self.k.cv2_to_imgmsg(d, encoding="bgr8")
            y.header = m.header          # zaman damgasi KORUNUR
            self.pub.publish(y)
        except Exception as e:                                   # noqa: BLE001
            self.get_logger().error("cevirme hatasi: %s" % e, throttle_duration_sec=5.0)


def main():
    a = argparse.ArgumentParser()
    a.add_argument("--girdi", required=True)
    a.add_argument("--cikti", required=True)
    n = a.parse_args()
    rclpy.init()
    d = Cevir(n.girdi, n.cikti)
    rclpy.spin(d)


if __name__ == "__main__":
    main()
