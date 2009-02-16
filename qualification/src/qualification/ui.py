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

### Author: Kevin Watts

import roslib
import roslib.packages
roslib.load_manifest('qualification')

import rospy
import roslaunch
import roslaunch.pmon

import os
import sys
import datetime
import glob
import wx
import time
from wx import xrc
from wx import html

import thread
from xml.dom import minidom

from srv import *
from test import *

from cStringIO import StringIO
import struct

from invent_client import Invent

TESTS_DIR = os.path.join(roslib.packages.get_pkg_dir('qualification'), 'tests')
RESULTS_DIR = os.path.join(roslib.packages.get_pkg_dir('qualification'), 'results')

class SerialPanel(wx.Panel):
  def __init__(self, parent, resource, qualification_frame):
    wx.Panel.__init__(self, parent)
    
    self._manager = qualification_frame
    
    self._panel = resource.LoadPanel(self, 'serial_panel')
    self._sizer = wx.BoxSizer(wx.HORIZONTAL)
    
    self._sizer.Add(self._panel, 1, wx.EXPAND|wx.ALIGN_CENTER_VERTICAL)
    self.SetSizer(self._sizer)
    self.Layout()
    self.Fit()
    
    self._test_button = xrc.XRCCTRL(self._panel, 'test_button')
    self._serial_text = xrc.XRCCTRL(self._panel, 'serial_text')
    self._rework_reason = xrc.XRCCTRL(self._panel, 'rework_reason')
    
    self._test_button.Bind(wx.EVT_BUTTON, self.on_test)
        
    self._panel.Bind(wx.EVT_CHAR, self.on_char)
    self._panel.SetFocus()
    
    
  def on_test(self, event):
    # Get the first 7 characters of the serial
    serial = self._serial_text.GetValue()
    
    rework = self._rework_reason.GetValue()

    if (self._manager.has_test(serial)):
      if (len(rework) > 5):
        wx.CallAfter(self._manager.start_tests, serial, rework)
      else:
        wx.CallAfter(self._manager.start_tests, serial)
    else:
      wx.MessageBox('No test defined for that serial number.','Error - Invalid serial number', wx.OK|wx.ICON_ERROR, self)
  
  def on_char(self, event):
    # 347 is the keycode sent at the beginning of a barcode
    if (event.GetKeyCode() == 347):
      # Clear the old contents and put focus in the serial box so the rest of input goes there
      self._serial_text.Clear()
      self._serial_text.SetFocus()
      
class InstructionsPanel(wx.Panel):
  def __init__(self, parent, resource, qualification_frame, file):
    wx.Panel.__init__(self, parent)
    
    self._manager = qualification_frame
    
    self._panel = resource.LoadPanel(self, 'instructions_panel')
    self._sizer = wx.BoxSizer(wx.HORIZONTAL)
    
    self._html_window = xrc.XRCCTRL(self._panel, 'html_window')
    self._continue_button = xrc.XRCCTRL(self._panel, 'continue_button')
    self._html_window.LoadFile(file)
    
    self._continue_button.Bind(wx.EVT_BUTTON, self.on_continue)
    self._continue_button.SetFocus()
    
    self._sizer.Add(self._panel, 1, wx.EXPAND|wx.ALIGN_CENTER_VERTICAL)
    self.SetSizer(self._sizer)
    self.Layout()
    self.Fit()
    
    self._cancel_button = xrc.XRCCTRL(self._panel, 'cancel_button')
    self._cancel_button.Bind(wx.EVT_BUTTON, self.on_cancel)
    
  def on_continue(self, event):
    self._manager.test_startup()
    
  def on_cancel(self, event):
    self._manager.cancel("Cancel button pressed")
    
class WaitingPanel(wx.Panel):
  def __init__(self, parent, resource, qualification_frame, name):
    wx.Panel.__init__(self, parent)
    
    self._manager = qualification_frame
    
    self._panel = resource.LoadPanel(self, 'waiting_panel')
    self._sizer = wx.BoxSizer(wx.HORIZONTAL)

    self._name = name
    
    self._progress_label = xrc.XRCCTRL(self._panel, 'progress_label')
    self._progress_label.SetLabel("Test '%s' in progress..."%(name))
    
    self._sizer.Add(self._panel, 1, wx.EXPAND|wx.ALIGN_CENTER_VERTICAL)
    self.SetSizer(self._sizer)
    self.Layout()
    self.Fit()
    
    self._cancel_button = xrc.XRCCTRL(self._panel, 'cancel_button')
    self._cancel_button.Bind(wx.EVT_BUTTON, self.on_cancel)

  def on_cancel(self, event):
    self._manager.cancel("Cancel button pressed during test %s" % (self._name))
    
class PrestartupPanel(wx.Panel):
  def __init__(self, parent, resource, qualification_frame):
    wx.Panel.__init__(self, parent)
    
    self._manager = qualification_frame
    
    self._panel = resource.LoadPanel(self, 'prestartup_panel')
    self._sizer = wx.BoxSizer(wx.HORIZONTAL)
    
    self._progress_label = xrc.XRCCTRL(self._panel, 'progress_label')
    self.set_progress_label('Running') # pre_startup scripts...')
    
    self._sizer.Add(self._panel, 1, wx.EXPAND|wx.ALIGN_CENTER_VERTICAL)
    self.SetSizer(self._sizer)
    self.Layout()
    self.Fit()
    
    # Cancel button added to prestartup panel
    # Cancel function must work if called
    self._cancel_button = xrc.XRCCTRL(self._panel, 'cancel_button')
    self._cancel_button.Bind(wx.EVT_BUTTON, self.on_cancel)

  def set_progress_label(self, label):
    self._progress_label.SetLabel(label)

  def on_cancel(self, event):
    self._manager.cancel("Cancel button pressed on startup")
    
class ImagePanel(wx.Panel):
  def __init__(self, parent, id, image_data):
    wx.Panel.__init__(self, parent)
    
    wx.InitAllImageHandlers();
    
    stream = StringIO(image_data)
    self._image = wx.ImageFromStream(stream)
    self.scale_bitmap()
    
    self.Bind(wx.EVT_PAINT, self.on_paint)
    self.Bind(wx.EVT_ERASE_BACKGROUND, self.on_erase_background)
    self.Bind(wx.EVT_SIZE, self.on_size)
    
    self.SetSize(wx.Size(self._image.GetWidth(), self._image.GetHeight()))
    
    self._needs_scale = False
    
  def scale_bitmap(self):
    size = self.GetSize()
    image = self._image.Scale(size.GetWidth(), size.GetHeight())
    self._bitmap = wx.BitmapFromImage(image)

  def on_size(self, event):
    event.Skip()
    self._needs_scale = True
    self.Refresh()
    
  def on_paint(self, event):
    dc = wx.BufferedPaintDC(self)
    if (self._needs_scale):
      self.scale_bitmap()
      self._needs_scale = False
    dc.DrawBitmap(self._bitmap, 0, 0, False)
  
  def on_erase_background(self, event):
    pass
    
class PlotsPanel(wx.Panel):
  def __init__(self, parent, resource, qualification_frame, test_name):
    wx.Panel.__init__(self, parent)
    
    self._manager = qualification_frame
    
    self._panel = resource.LoadPanel(self, 'plots_panel')
    self._sizer = wx.BoxSizer(wx.HORIZONTAL)
    
    self._sizer.Add(self._panel, 1, wx.EXPAND)
    self.SetSizer(self._sizer)
    self.Layout()
    
    self._scrolled_window = xrc.XRCCTRL(self._panel, 'scrolled_window')
    self._test_label = xrc.XRCCTRL(self._panel, 'test_label')
    self._pass_button = xrc.XRCCTRL(self._panel, 'pass_button')
    self._fail_button = xrc.XRCCTRL(self._panel, 'fail_button')
    self._retry_button = xrc.XRCCTRL(self._panel, 'retry_button')
    
    self._pass_button.Bind(wx.EVT_BUTTON, self.on_pass)
    self._fail_button.Bind(wx.EVT_BUTTON, self.on_fail)
    self._retry_button.Bind(wx.EVT_BUTTON, self.on_retry)
    
    self._scrolled_sizer = wx.BoxSizer(wx.VERTICAL)
    self._scrolled_window.SetSizer(self._scrolled_sizer)
    self._scrolled_window.SetScrollRate(0, 20)
    
    self._test_label.SetLabel(test_name)
    
    self._pass_button.SetFocus()
    
    self._cancel_button = xrc.XRCCTRL(self._panel, 'cancel_button')
    self._cancel_button.Bind(wx.EVT_BUTTON, self.on_cancel)
    
  def show_plots(self, msg):
    self._msg = msg
    
    i = 1
    for p in msg.plots:
      box = wx.StaticBox(self._scrolled_window, wx.ID_ANY, "Plot %d"%(i))
      sizer = wx.StaticBoxSizer(box, wx.VERTICAL)
      i += 1
      
      textbox = wx.TextCtrl(self._scrolled_window, wx.ID_ANY, p.text, wx.DefaultPosition, wx.Size(-1, 100), wx.TE_MULTILINE|wx.TE_READONLY)
      sizer.Add(textbox, 0, wx.EXPAND)
      
      if (len(p.image) > 0):
        plot = ImagePanel(self._scrolled_window, wx.ID_ANY, p.image)
        sizer.Add(plot, 1, wx.EXPAND)
      
      self._scrolled_sizer.Add(sizer, 1, wx.EXPAND)
    
  def on_pass(self, event):
    self._manager.subtest_passed(self._msg)
  
  def on_fail(self, event):
    failure_reason = wx.GetTextFromUser("Record the reason for failure:", "Failure Reason", "", self)
    self._manager.subtest_failed(self._msg, failure_reason)
  
  def on_retry(self, event):
    self._manager.retry_subtest()
    
  def on_cancel(self, event):
    self._manager.cancel("Cancel button pressed")
    
class ResultsPanel(wx.Panel):
  def __init__(self, parent, resource, qualification_frame):
    wx.Panel.__init__(self, parent)
    
    self._manager = qualification_frame
    
    self._panel = resource.LoadPanel(self, 'results_panel')
    self._sizer = wx.BoxSizer(wx.HORIZONTAL)
    
    self._sizer.Add(self._panel, 1, wx.EXPAND|wx.ALIGN_CENTER_VERTICAL)
    self.SetSizer(self._sizer)
    self.Layout()
    self.Fit()
    
    self._submit_button = xrc.XRCCTRL(self._panel, 'submit_button')
    self._textbox = xrc.XRCCTRL(self._panel, 'results_text')

    self._notesbox = xrc.XRCCTRL(self._panel, 'notes_text')
    self._submit_button.Bind(wx.EVT_BUTTON, self.on_submit)
    self._submit_button.SetFocus()
    
  def set_results(self, results):
    self._textbox.Clear()
    for r in results:
      self._textbox.AppendText("------------------------------------------------\n")
      
      text = r['text']
      msg = r['msg']
      i = 1
      for p in msg.plots:
        text += '\n------\n'
        text += 'Plot %d:\n'%(i)
        text += p.text
        i += 1
      
      
      self._textbox.AppendText("Subtest '%s':\n"%(r['test_name']))
      self._textbox.AppendText("Result: %s\n"%(r['result']))
      if (r.has_key('failure_reason')):
        self._textbox.AppendText("Failure reason: %s\n"%(r['failure_reason']))
      self._textbox.AppendText("Text: %s\n"%(text))
    
  def on_submit(self, event):
      wx.CallAfter(self._manager.submit_results, self._textbox.GetValue(), self._notesbox.GetValue())

class RoslaunchProcessListener(roslaunch.pmon.ProcessListener):
  def __init__(self):
    self._died_badly = False
    
  def has_any_process_died_badly(self):
    return self._died_badly
  def process_died(self, process_name, exit_code):
    status = 'Failed!'
    if exit_code == 0 or exit_code == None:
      status = 'OK'

    print "Process %s died with exit code %s. %s" % (process_name, exit_code, status)
    if exit_code != None and exit_code != 0:
      self._died_badly = True

class QualificationFrame(wx.Frame):
  def __init__(self, parent):
    wx.Frame.__init__(self, parent, wx.ID_ANY, "Qualification")

    self._result_service = None
    
    # Load test directory
    tests_xml_path = os.path.join(TESTS_DIR, 'tests.xml')
    self._tests = {}
    try:
      doc = minidom.parse(tests_xml_path)
    except IOError:
      print >> sys.stderr, "Could not load tests description from '%s'"%(tests_xml_path)
      sys.exit()

    # Loads tests by serial number of part
    tests = doc.getElementsByTagName('test')
    for test in tests:
      self._tests[test.attributes['serial'].value] = test.attributes['name'].value
    
    # Load the XRC resource
    xrc_path = os.path.join(roslib.packages.get_pkg_dir('qualification'), 'xrc/gui_test.xrc')
    self._res = xrc.XmlResource(xrc_path)

    # Load the main panel
    self._root_panel = self._res.LoadPanel(self, 'main_panel')
    
    self._log = xrc.XRCCTRL(self._root_panel, "log")
    self._top_panel = xrc.XRCCTRL(self._root_panel, "top_panel")
    self._top_sizer = wx.BoxSizer(wx.HORIZONTAL)
    self._top_panel.SetSizer(self._top_sizer)
    
    self.reset()
    self.log("Startup")
    
    self.Bind(wx.EVT_CLOSE, self.on_close)
    
    self._startup_launch = None
    self._shutdown_launch = None
    self._core_launch = None
    self._subtest_launch = None
    
    self._spin_timer = wx.Timer(self, wx.ID_ANY)
    self.Bind(wx.EVT_TIMER, self.on_spin, self._spin_timer)
    self._spin_timer.Start(100)
    
    self._username = None
    self._password = None

  def log(self, msg):
    self._log.AppendText(datetime.datetime.now().strftime("%Y%m%d_%I%M%S:") + msg + '\n')
    self._log.Refresh()
    self._log.Update()
    
  def set_top_panel(self, panel):
    self._current_panel = panel
    self._top_sizer.Clear(True)
    self._top_sizer.Add(self._current_panel, 1, wx.EXPAND|wx.ALIGN_CENTER_VERTICAL)
    self._top_panel.Layout()
    
  def reset(self):
    self.set_top_panel(SerialPanel(self._top_panel, self._res, self))
    
  def has_test(self, serial):
    return self._tests.has_key(serial[0:7])
    
  def launcher_spin_thread(self, launcher):
    launcher.spin()
    
  # Launches program, queries for rework reason  
  def start_tests(self, serial, rework_reason = ''):
    short_serial = serial[0:7]
    test_dir = os.path.join(TESTS_DIR, self._tests[short_serial])
    self._test_dir = test_dir
    self.log('Starting test %s'%(self._tests[short_serial]))
    
    self._tests_start_date = datetime.datetime.now()
    self._current_serial = serial
    
    self._current_test = Test()
    self._current_test.load(test_dir)
    
    self._results = []
    self._subtest = None
    
    print self._current_test.subtests

    if (len(self._current_test.subtests) == 0):
      wx.MessageBox('Test %s has no subtests defined'%(self._tests[short_serial]), 'No tests', wx.OK|wx.ICON_ERROR, self)
      return
    
    self._core_launch = self.launch_core()
    
    if (self._result_service != None):
      self._result_service.shutdown()
      self._result_service = None

    self._result_service = rospy.Service('test_result', TestResult, self.subtest_callback)
    
    if (len(rework_reason) > 0):
      invent = self.get_inventory_object()
      if (invent != None):
        invent.setNote(self._current_serial, "Hardware rework, reason given: %s"%(rework_reason))
    
    if (self._current_test.getInstructionsFile() != None):
      self.set_top_panel(InstructionsPanel(self._top_panel, self._res, self, os.path.join(test_dir, self._current_test.getInstructionsFile())))
    else:
      self.test_startup()
      
  def test_startup(self):
    panel = PrestartupPanel(self._top_panel, self._res, self)
    self.set_top_panel(panel)
    panel.set_progress_label('Initializing pre-startup sequence')
    time.sleep(3)
    wx.CallAfter(self.test_startup_real(panel))
    
  # Called from InstructionsPanel, runs through prestartup scripts
  def test_startup_real(self, panel):
    test_dir = self._test_dir
    # Run any pre_startup scripts synchronously
    if (len(self._current_test.pre_startup_scripts) > 0):
      for script in self._current_test.pre_startup_scripts:
        # Set script label in waiting panel
        log_message = 'Running pre_startup script [%s]...'%(script)
        self.log(log_message)
        panel.set_progress_label(log_message)
        
        # If cancel button pressed in pre-startup, cancel all tests
        # Cancel button called to cancel()...should work

        listener = RoslaunchProcessListener()
        
        launch = self.launch_script(os.path.join(test_dir, script), listener)
        if (launch == None):
          s = 'Could not load roslaunch script "%s"! Press OK to cancel test.' % (script)
          wx.MessageBox(s, 'Invalid roslaunch file', wx.OK|wx.ICON_ERROR, self)
          self.cancel(s)
          return
        
        launch.spin(); # Spin until it finishes
        
        if (listener.has_any_process_died_badly()):
          s = 'Pre_startup script %s died badly! Press OK to cancel test.' % (script)
          wx.MessageBox(s, 'Pre_startup Failed', wx.OK|wx.ICON_ERROR, self)
          # Record that pre-startup failed 
          self.prestartup_failed(script)
          return
    else:
      self.log('No pre_startup scripts')
      
    # Run the startup script if we have one
    if (self._current_test.getStartupScript() != None):
      self.log('Running startup script...')
      panel.set_progress_label('Running startup script')
      self._startup_launch = self.launch_script(os.path.join(test_dir, self._current_test.getStartupScript()), None)
      if (self._startup_launch == None):
        s = 'Could not load roslaunch script "%s"'%(self._current_test.getStartupScript())
        wx.MessageBox(s, 'Invalid roslaunch file. Press OK to cancel.', wx.OK|wx.ICON_ERROR, self)
        self.cancel(s)
        return
    else:
      self.log('No startup script')
      
    self.start_subtest(0)
    
  def start_subtest(self, index):
    self._subtest_index = index
    self._subtest = self._current_test.subtests[index]
    
    self.set_top_panel(WaitingPanel(self._top_panel, self._res, self, self._subtest))
    
    self.launch_pre_subtest()

    script = os.path.join(self._current_test.getDir(), self._subtest)
    self._subtest_launch = self.launch_script(script, None)
    if (self._subtest_launch == None):
      s = 'Could not load roslaunch script "%s"'%(script)
      wx.MessageBox(s, 'Invalid roslaunch file. Press OK to cancel.', wx.OK|wx.ICON_ERROR, self)
      self.cancel(s)
      return
    
  def prestartup_failed(self, script):
    # Adds "result" of prestartup failure, allows user to log it
    r = {}
    r['test_name'] = "Prestartup failure"
    r['result'] = "FAIL"
    r['text'] = "Qualification prestartup script %s failed! Hardware cannot be qualified" % (script)
    r['msg'] = None
    r['failure_reason'] = script

    self._results.append(r)

    self.show_results()

  def show_results(self):
    panel = ResultsPanel(self._top_panel, self._res, self)
    self.set_top_panel(panel)
    
    panel.set_results(self._results)
    
  def get_inventory_object(self):
    if (self._username != None and self._password != None):
      invent = Invent(self._username, self._password)
      if (invent.login() == False):
        self._username = None
        self._password = None
      else:
        return invent
    
    dialog = self._res.LoadDialog(self, 'username_password_dialog')
    xrc.XRCCTRL(dialog, 'text').Wrap(300)
    dialog.Layout()
    dialog.Fit()
    username_ctrl = xrc.XRCCTRL(dialog, 'username')
    password_ctrl = xrc.XRCCTRL(dialog, 'password')
    username_ctrl.SetFocus()
    
    # These values don't come through in the xrc file
    username_ctrl.SetMinSize(wx.Size(200, -1))
    password_ctrl.SetMinSize(wx.Size(200, -1))
    if (dialog.ShowModal() == wx.ID_OK):
      username = username_ctrl.GetValue()
      password = password_ctrl.GetValue()

      invent = Invent(username, password)
      if (invent.login() == False):
        return self.get_inventory_object()
      
      self._username = username
      self._password = password
      
      return invent
    
    return None
    
  def submit_results(self, summary, notes=''):
    start_time_string = self._tests_start_date.strftime("%Y%m%d_%I%M") # Seconds removed
    
    date_string = self._tests_start_date.strftime("%m/%d/%Y")
    time_string = self._tests_start_date.strftime("%I:%m")

    invent = self.get_inventory_object()
    if (invent != None):
      status_str = self._results[len(self._results)-1]['result']    
      invent.setKV(self._current_serial, "Test Status", status_str)
      test_log = "Test run on %s at %s, status: %s."%(date_string, time_string, status_str)
      invent.setNote(self._current_serial, test_log)
      
      if notes != '':
        summary = summary + "\n Notes: %s" % notes

      prefix = start_time_string + "/" # Put prefix first so images sorted by date
      invent.add_attachment(self._current_serial, prefix + "summary", "text/plain", summary)
      
      # Change so it records type of plot with image 
      # Not just image1, etc
      i = 1
      for r in self._results:
        msg = r['msg']
        if msg is not None:
          for p in msg.plots:
            if (len(p.image) > 0):
              invent.add_attachment(self._current_serial, prefix + p.title, "image/" + p.image_format, p.image)
              i += 1
            
      self.log('Results submitted')
    
    fname = os.path.join(RESULTS_DIR, '%s_%s.test' % (self._current_serial, start_time_string))
    f = open(fname, 'w')
    f.write(summary)
    f.close()
    
    self.log('Results logged to %s'%(fname))
    
    self.reset()

  def discard_results(self):
    self.log('Results discarded')
    self.reset()
    
  def next_subtest(self):
    if (self._subtest_index + 1 >= len(self._current_test.subtests)):
      self.test_finished()
      self.show_results()
    else:
      self.start_subtest(self._subtest_index + 1)
    
  def show_plots(self, msg):
    panel = PlotsPanel(self._top_panel, self._res, self, self._subtest)
    self.set_top_panel(panel)
    
    panel.show_plots(msg)
  
  def launch_pre_subtest(self):
    if (self._subtest == None):
      return
    
    if (self._current_test.pre_subtests.has_key(self._subtest)):
      script = os.path.join(self._current_test.getDir(), self._current_test.pre_subtests[self._subtest])
      pre_launcher = self.launch_script(script, None)
      if (pre_launcher == None):
        s = 'Could not load post-subtest roslaunch script "%s"'%(script)
        wx.MessageBox(s, 'Invalid roslaunch file', wx.OK|wx.ICON_ERROR, self)
        self.cancel(s)
      else:
        pre_launcher.spin()

  def launch_post_subtest(self):
    if (self._subtest == None):
      return
    
    if (self._current_test.post_subtests.has_key(self._subtest)):
      script = os.path.join(self._current_test.getDir(), self._current_test.post_subtests[self._subtest])
      post_launcher = self.launch_script(script, None)
      if (post_launcher == None):
        s = 'Could not load post-subtest roslaunch script "%s"'%(script)
        wx.MessageBox(s, 'Invalid roslaunch file', wx.OK|wx.ICON_ERROR, self)
        self.cancel(s)
      else:
        post_launcher.spin()
  
  def subtest_result(self, msg, failure_reason):
    str_result = "PASS"
    if (msg.result == TestResultRequest.RESULT_FAIL or len(failure_reason) != 0):
      str_result = "FAIL"
    
    self.log('Subtest "%s" result: %s'%(self._subtest, str_result))
    r = {}
    r['test_name'] = self._subtest
    r['result'] = str_result
    r['text'] = msg.text_result
    r['msg'] = msg
    failed = False
    if (len(failure_reason) != 0):
      r['failure_reason'] = failure_reason
      failed = True
    self._results.append(r)
    
    self.launch_post_subtest()
    
    if (failed):
      self.test_finished()
      self.show_results()
    else:
      self.next_subtest()
    
  def subtest_passed(self, msg):
    self.subtest_result(msg, "")
    
  def subtest_failed(self, msg, failure_reason):
    self.subtest_result(msg, failure_reason)
    
  def subtest_finished(self, msg):
    self.log('Subtest "%s" finished'%(self._subtest))
    self._subtest_launch.stop()
    self._subtest_launch = None
    
    if (msg.result == TestResultRequest.RESULT_PASS):
      self.subtest_passed(msg)
    elif (msg.result == TestResultRequest.RESULT_FAIL):
      self.subtest_failed(msg, "Automated failure")
    elif (msg.result == TestResultRequest.RESULT_HUMAN_REQUIRED):
      self.log('Subtest "%s" needs human response'%(self._subtest))
      self.show_plots(msg)
    
  def subtest_callback(self, msg):
    wx.CallAfter(self.subtest_finished, msg)
    return TestResultResponse()
    
  def retry_subtest(self):
    self.log('Retrying subtest "%s"'%(self._subtest))
    self.start_subtest(self._subtest_index)
    
  def launch_core(self):
    self.log('Launching ros core services')
    
    # Create a roslauncher
    config = roslaunch.ROSLaunchConfig()
    config.master.auto = config.master.AUTO_RESTART
    
    launcher = roslaunch.ROSLaunchRunner(config)
    launcher.launch()
    
    return launcher
    
  def launch_script(self, script, process_listener_object):
    self.log('Launching roslaunch file %s'%(script))
    
    # Create a roslauncher
    config = roslaunch.ROSLaunchConfig()

    # Try loading the XML file
    try:
      loader = roslaunch.XmlLoader()
      loader.load(script, config)
      
      # Bring up the nodes
      launcher = roslaunch.ROSLaunchRunner(config)
      if (process_listener_object != None):
        launcher.pm.add_process_listener(process_listener_object)
        
      launcher.launch()
    except roslaunch.RLException, e:
      self.log('Failed to launch roslaunch file %s: %s'%(script, e))
      return None
    
    return launcher
    
  def on_spin(self, event):
    if (self._subtest_launch != None):
      self._subtest_launch.spin_once()
    
    if (self._startup_launch != None):
      self._startup_launch.spin_once()
      
    if (self._shutdown_launch != None):
      self._shutdown_launch.spin_once()
      
    if (self._core_launch != None):
      self._core_launch.spin_once()
    
  def stop_launches(self):
    if (self._subtest_launch != None):
      self._subtest_launch.stop()
    
    if (self._startup_launch != None):
      self._startup_launch.stop()
      
    if (self._shutdown_launch != None):
      self._shutdown_launch.stop()
      
    if (self._core_launch != None):
      self._core_launch.stop()
      
    self._startup_launch = None
    self._shutdown_launch = None
    self._core_launch = None
    self._subtest_launch = None
    
  def test_finished(self):
    self.launch_post_subtest()
    
    if (self._current_test.getShutdownScript() != None):
      self.log('Running shutdown script...')
      self._shutdown_launch = self.launch_script(os.path.join(self._current_test.getDir(), self._current_test.getShutdownScript()), None)
      if (self._shutdown_launch == None):
        s = 'Could not load roslaunch script "%s"'%(self._current_test.getShutdownScript())
        wx.MessageBox(s, 'Invalid roslaunch file', wx.OK|wx.ICON_ERROR, self)
        self.log('No shutdown script: %s' % (s))
        self.log('SHUT DOWN POWER BOARD MANUALLY')
        self.reset()
        return

      self._shutdown_launch.spin()
    else:
      self.log('No shutdown script')
        
    self.stop_launches()
    
    self._current_test = None
    self._subtest = None
    self._subtest_index = -1
    
  def cancel(self, reason):
    self.log('Test canceled, reason: %s'%(reason))
    
    self.test_finished()
    self.reset()
  
  def on_close(self, event):
    event.Skip()
    
    self.stop_launches()
    

class QualificationApp(wx.App):
  def OnInit(self):
    rospy.init_node("Qualifier", anonymous=True, disable_rostime=True)
    
    self._frame = QualificationFrame(None)
    self._frame.SetSize(wx.Size(800,800))
    self._frame.Layout()
    self._frame.Centre()
    self._frame.Show(True)
    
    return True

if __name__ == '__main__':
  try:
    app = QualificationApp(0)
    app.MainLoop()
  except Exception, e:
    print e
    
  print 'quit'

