#!/usr/bin/env python
#
# Software License Agreement (BSD License)
#
# Copyright (c) 2008, Willow Garage, Inc.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
#  * Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
#  * Redistributions in binary form must reproduce the above
#    copyright notice, this list of conditions and the following
#    disclaimer in the documentation and/or other materials provided
#    with the distribution.
#  * Neither the name of the Willow Garage nor the names of its
#    contributors may be used to endorse or promote products derived
#    from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
# FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
# COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
# INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
# BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
# CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
# ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
# POSSIBILITY OF SUCH DAMAGE.

# Author: Kevin Watts

import roslib
roslib.load_manifest('qualification')
import rospy, sys, time
import subprocess
from optparse import OptionParser

from std_msgs.msg import *
from robot_srvs.srv import *
from std_srvs.srv import *

from robot_mechanism_controllers.srv import *
from robot_mechanism_controllers import controllers
from mechanism_control import mechanism

spawn_controller = rospy.ServiceProxy('spawn_controller', SpawnController)
kill_controller = rospy.ServiceProxy('kill_controller', KillController) 

class SendMessageOnSubscribe(rospy.SubscribeListener):
    def __init__(self, msg):
        self.msg = msg
        print "Waiting for subscriber..."

    def peer_subscribe(self, topic_name, topic_publish, peer_publish):
        peer_publish(self.msg)
        sleep(0.1)


def xml_for_hold(name, p, i, d, iClamp):
    return """
<controller name="%s_controller" type="JointPositionControllerNode">
  <joint name="%s_joint">
  <pid p="%d" i="%d" d="%d" iClamp="%d" />
</controller>""" % (name, name, p, i, d, iClamp)

def hold_joint(name, p, i, d, iClamp, holding):
    try:
        resp = spawn_controller(xml_for_hold(name, p, i, d, iClamp))
        if ord(resp.ok[0]) != 0:
            holding.append(resp.name[0])
            return True
    except Exception, e:
        print "Failed to spawn holding controller %s" % name
    return False

def set_controller(controller, command):
    pub = rospy.Publisher('/' + controller + '/set_command', Float64,
                              SendMessageOnSubscribe(Float64(command)))

def hold_arm(side, pan_angle, holding):
    hold_joint("%s_gripper_palm" % side, 15, 0, 1, 5, holding)
    set_controller("%s_gripper_palm_controller" % side, float(0.0))
    
    hold_joint("%s_forearm_roll" % side, 20, 3, 4, 2, holding)
    set_controller("%s_forearm_roll_controller" % side, float(0.0))

    hold_joint("%s_wrist_flex" % side, 20, 3, 4, 2, holding)
    set_controller("%s_wrist_flex_controller" % side, float(3.0))

    hold_joint("%s_elbow_flex" % side, 50, 15, 8, 2, holding)
    set_controller("%s_elbow_flex_controller" % side, float(3.0))

    hold_joint("%s_upper_arm_roll" % side, 20, 2, 1.0, 1.0, holding)
    set_controller("%s_upper_arm_roll_controller" % side, float(0.0))

    hold_joint("%s_shoulder_lift" % side, 35, 7, 4, 3, holding)
    set_controller("%s_shoulder_lift_controller" % side, float(3.0))

    hold_joint("%s_shoulder_pan" % side, 70, 6, 8, 4, holding)
    set_controller("%s_shoulder_pan_controller" % side, float(pan_angle))

def main():
    if len(sys.argv) < 3:
        print "Can't load arm, need <joint> <controller_path>"
        sys.exit(1)
    
    # Pull side (l or r) from param server
    side = rospy.get_param("full_arm_test/side")
        
    joint = side + sys.argv[1]
    controller_file = open(sys.argv[2])
    # Put side in to controller xml string
    controller_xml = controller_file.read() % side 
    
    rospy.wait_for_service('kill_controller')


    rospy.wait_for_service('spawn_controller')
    rospy.init_node('arm_test_' + side, anonymous=True)
    
    holding = []
    try:
        # Hold both arms in place
        hold_side('r', -0.7, holding)
        hold_side('l', 0.7, holding)
        
        # Kill controller for given joint
        kill_controller(joint + '_controller')
        holding.remove(joint + '_controller')
        
        sleep(1.5)
        
        # Spawn test controller and run test
        spawn_controller(controller_xml)
        holding.append('test_controller') # always called test_controller
        
        while not rospy.is_shutdown():
            sleep(0.5)

    # Kill everything
    finally:
        for name in holding:
            kill_controller(holding)

if __name__ == '__main__':
    main()
