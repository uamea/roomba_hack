#!/usr/bin/env python3
import rospy
from sensor_msgs.msg import Joy
from std_msgs.msg import Header
import pygame
import sys

class KeyboardJoyController:
    def __init__(self):
        rospy.init_node('keyboard_joy_controller')
        self.pub = rospy.Publisher('/joy', Joy, queue_size=10)
        self.rate = rospy.Rate(20)  # 20Hz

        # Axes: Match the axes used by roomba_teleop.cpp
        # - axes[0], axes[1]: Basic movement
        # - axes[6], axes[7]: Directional commands
        self.axes = [0.0] * 8
        # Buttons: 17 buttons
        self.buttons = [0] * 17

        # Key mapping updated to match roomba_teleop.cpp
        self.key_map = {
            # Axes - roomba_teleop.cpp uses axes[0] for angular.z and axes[1] for linear.x
            pygame.K_LEFT:   ('axes', 0, -1.0),  # angular left (counterclockwise)
            pygame.K_RIGHT:  ('axes', 0, 1.0),   # angular right (clockwise)
            pygame.K_UP:     ('axes', 1, 1.0),   # forward
            pygame.K_DOWN:   ('axes', 1, -1.0),  # backward
            
            # Directions - roomba_teleop.cpp uses axes[6] and axes[7] for directional movement
            pygame.K_a:      ('axes', 6, -1.0),  # left directional
            pygame.K_d:      ('axes', 6, 1.0),   # right directional
            pygame.K_w:      ('axes', 7, 1.0),   # forward directional
            pygame.K_s:      ('axes', 7, -1.0),  # backward directional
            
            # Buttons - match the function in roomba_teleop.cpp
            pygame.K_q:      ('buttons', 0),     # Stop movement
            pygame.K_e:      ('buttons', 1),     # Enable movement
            pygame.K_z:      ('buttons', 2),     # Manual mode
            pygame.K_c:      ('buttons', 3),     # Auto mode
            pygame.K_f:      ('buttons', 5),     # Extra button
            pygame.K_v:      ('buttons', 6),     # Undock
            pygame.K_b:      ('buttons', 7),     # Dock
            
            # Additional buttons (not directly used by roomba_teleop.cpp)
            pygame.K_u:      ('buttons', 8),
            pygame.K_o:      ('buttons', 9),
            pygame.K_1:      ('buttons', 10),
            pygame.K_2:      ('buttons', 11),
            pygame.K_t:      ('buttons', 12),
            pygame.K_y:      ('buttons', 13),
            pygame.K_g:      ('buttons', 14),
            pygame.K_h:      ('buttons', 15),
            pygame.K_p:      ('buttons', 16),
        }

        pygame.init()
        pygame.display.set_mode((400, 400))  # Small window to capture keyboard events
        
        rospy.loginfo("""Keyboard Joy Controller (pygame) started. Focus the pygame window and use:
- Arrow keys: Basic movement
- WASD: Directional movement
- Q: Stop movement, E: Enable movement
- Z: Manual mode, C: Auto mode
- V: Undock, B: Dock""")

    def run(self):
        while not rospy.is_shutdown():
            # Reset axes/buttons
            self.axes = [0.0] * 8
            self.buttons = [0] * 17

            # Process pygame events
            for event in pygame.event.get():
                if event.type == pygame.QUIT:
                    pygame.quit()
                    sys.exit(0)

            keys = pygame.key.get_pressed()
            for key, mapping in self.key_map.items():
                if keys[key]:
                    if mapping[0] == 'axes':
                        self.axes[mapping[1]] = mapping[2]
                    elif mapping[0] == 'buttons':
                        self.buttons[mapping[1]] = 1

            joy_msg = Joy()
            joy_msg.header = Header()
            joy_msg.header.stamp = rospy.Time.now()
            joy_msg.axes = self.axes[:]
            joy_msg.buttons = self.buttons[:]
            rospy.loginfo(joy_msg)
            self.pub.publish(joy_msg)
            self.rate.sleep()

if __name__ == '__main__':
    try:
        node = KeyboardJoyController()
        node.run()
    except rospy.ROSInterruptException:
        pass
