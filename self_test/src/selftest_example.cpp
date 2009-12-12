/*
 * Copyright (c) 2008, Willow Garage, Inc.
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the <ORGANIZATION> nor the names of its
 *       contributors may be used to endorse or promote products derived from
 *       this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */


#include <ros/ros.h>

#include "diagnostic_msgs/SelfTest.h"

#include "self_test/self_test.h"

#include <stdexcept>

//  using namespace std;

class MyNode
{
public:

  // self_test::Sequencer is the handles sequencing driver self-tests.
  self_test::Sequencer self_test_;

  // A value showing statefulness of tests
  double some_val;

  ros::NodeHandle nh_;

  MyNode() : self_test_()
  {
    // If any setup work needs to be done before running the tests,
    // a pretest can be defined. It is just like any other test, but
    // doesn't actually do any testing.
    self_test_.add("Pretest", this, &MyNode::pretest );

    // Tests added will be run in the order in which they are added. Each
    // test has a name that will be automatically be filled in the
    // DiagnosticStatus message.
    self_test_.add("ID Lookup",                 this, &MyNode::test1);
    self_test_.add("Exception generating test", this, &MyNode::test2);
    self_test_.add("Value generating test",     this, &MyNode::test3);
    self_test_.add("Value testing test",        this, &MyNode::test4);
    
    // If any cleanup work needs to be done after running the tests,
    // a posttest can be defined. It is just like any other test, but 
    // doesn't actually do any testing.
    self_test_.add("Posttest", this, &MyNode::pretest );
  }

  void pretest(diagnostic_updater::DiagnosticStatusWrapper& status)
  {
    printf("Doing preparation stuff before we run our test.\n");
    
    status.summary(0, "Pretest completed successfully.");
    
    some_val = 1.0;
  }

  // All tests take a reference to a DiagnosticStatusWrapper message which they should populate
  // The default values are status.level = 2 (ERROR), and status.message = "No message was set"
  // The status.name is automatically set to the name that was passed to add.
  // A DiagnosticStatusWrapper is used instead of a DiagnosticStatus
  // because it provides useful convenience methods.
  void test1(diagnostic_updater::DiagnosticStatusWrapper& status)
  {
    // Look up ID here
    char ID[] = "12345";
    bool lookup_successful = true;

    if (lookup_successful)
    {
      status.summary(0, "ID Lookup successful");
      
      // Using setID on the selftest pushes the ID to an accessible location 
      self_test_.setID(ID);

    } else {
      status.summary(2, "ID Lookup failed");
    }
  }

  // Tests do not necessarily need to catch their exceptions.
  void test2(diagnostic_updater::DiagnosticStatusWrapper& status)
  {
    // Note, we start setting our status to success.  Since our
    // exception is not caught, however, the SelfTest class will
    // change level to ERROR.  This wouldn't be common practice And I
    // would instead recommend not changing the status to success
    // until after the exception might be generated.

    status.level = 0;

    // Exceptions of time runtime_error will actually propagate messages
    throw std::runtime_error("we did something that threw an exception");

    // Here's where we would report success if we'd made it past
    status.summary(0, "We made it past the exception throwing statement.");
  }

  // The state of the node can be changed as the tests are operating
  void test3(diagnostic_updater::DiagnosticStatusWrapper& status)
  {
    // Do something that changes the state of the node
    some_val += 41.0;

    status.add("some value", some_val);
    status.summary(0, "We successfully changed the value.");
  }

  void test4(diagnostic_updater::DiagnosticStatusWrapper& status)
  {
    if (some_val == 42.0)
    {
      status.summary(0, "We observed the change in value");
    } 
    else
    {
      status.summaryf(2, "We failed to observe the change in value, it is currently %f.", some_val);
    }
  }

  void posttest(diagnostic_updater::DiagnosticStatusWrapper& status)
  {
    printf("Doing cleanup stuff after we run our test.\n");
    status.summary(0, "Posttest completed successfully.");
  }

  bool spin()
  {
    while (nh_.ok())
    {
      //Do something important...
      usleep(1000000);
      
      // Calling checkTest tells the SelfTest class that it's ok
      // to actually run the test.  This gives you flexibility to
      // keep the SelfTest service from necessarily interrupting
      // other operations.
      self_test_.checkTest();
    }
    return true;
  }
};

int
main(int argc, char** argv)
{
  ros::init(argc, argv, "my_node");

  MyNode n;

  n.spin();

  return(0);
}
