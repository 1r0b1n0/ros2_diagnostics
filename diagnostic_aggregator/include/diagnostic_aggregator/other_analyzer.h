/*********************************************************************
 * Software License Agreement (BSD License)
 *
 *  Copyright (c) 2009, Willow Garage, Inc.
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above
 *     copyright notice, this list of conditions and the following
 *     disclaimer in the documentation and/or other materials provided
 *     with the distribution.
 *   * Neither the name of the Willow Garage nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 *  FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 *  COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 *  BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 *  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 *  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 *  LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 *  ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 *********************************************************************/

/**!< \author Kevin Watts */

#ifndef OTHER_ANALYZER_H
#define OTHER_ANALYZER_H

#include <string>
#include <ros/ros.h>
#include "diagnostic_aggregator/generic_analyzer_base.h"

namespace diagnostic_aggregator {

/*
 *\brief OtherAnalyzer analyzes any messages that haven't been analyzed by other Analyzers
 *
 * OtherAnalyzer is not loaded as a plugin. It is created by the Aggregator, and called seperately.
 *
 */
class OtherAnalyzer : public GenericAnalyzerBase
{
public:
  /*!
   *\brief Default constructor. OtherAnalyzer isn't loaded by pluginlib
   */
  OtherAnalyzer() { }

  ~OtherAnalyzer() { }

  bool init(std::string path)
  {
	  return GenericAnalyzerBase::init(path + "/Other", "Other", 5.0);
  }

  /*
   *\brief OtherAnalyzer cannot be initialized with a NodeHandle
   */
  bool init(const std::string base_path, const ros::NodeHandle &n)
  {
	  ROS_ERROR("OtherAnalyzer was attempted to initialize with a NodeHandle. This analyzer cannot be used as a plugin.");
	  return false;
  }

  /*
   *\brief Returns true for all items
   */
  bool match(std::string name) const { return true; }



};

}


#endif // OTHER_ANALYZER_H
