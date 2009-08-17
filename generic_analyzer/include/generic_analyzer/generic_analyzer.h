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

// Author: Kevin Watts

#ifndef GENERIC_ANALYZER_H
#define GENERIC_ANALYZER_H

#include <map>
#include <ros/ros.h>
#include <vector>
#include <string>
#include "diagnostic_msgs/DiagnosticStatus.h"
#include "diagnostic_msgs/KeyValue.h"
#include "diagnostic_analyzer/diagnostic_analyzer.h"
#include "diagnostic_analyzer/diagnostic_item.h"
#include "XmlRpcValue.h"

namespace diagnostic_analyzer {

/*!
 *\brief GenericAnalyzer is most basic DiagnosticAnalyzer
 * 
 * GenericAnalyzer analyzes diagnostics from list of topics and returns
 * processed diagnostics data. All analyzed status messages are prepended by
 * '/FirstPrefix/SecondPrefix', where FirstPrefix is common to all analyzers
 * (ex: 'PRE') and SecondPrefix is from this analyzer (ex: 'Sensors').
 */
class GenericAnalyzer : public DiagnosticAnalyzer
{

public:
  GenericAnalyzer();

  /*!
   *\brief Initializes GenericAnalyzer from namespace
   *
   * NodeHandle is given private namespace to initialize (ex: ~Sensors)
   * Parameters of NodeHandle must follow this form. See DiagnosticAggregator
   * for instructions on passing these to the aggregator.
   *\verbatim
   * PowerSystem:
   *   type: GenericAnalyzer
   *   prefix: Power System
   *   expected: [ 
   *     'IBPS 0',
   *     'IBPS 1']
   *   startswith: [
   *     'Smart Battery']
   *   name: [
   *     'Power Node 1018']
   *   contains: [
   *     'Battery']
   *\param first_prefix : Prefix for all analyzers (ex: 'Robot')
   *\param n : NodeHandle in full namespace
   */
  bool init(std::string first_prefix, const ros::NodeHandle &n);
  ~GenericAnalyzer();

  /*!
   *\brief Analyzes DiagnosticStatus messages
   * 
   */
  std::vector<diagnostic_msgs::DiagnosticStatus*> analyze(std::map<std::string, diagnostic_item::DiagnosticItem*> msgs);

  std::string getPrefix() { return full_prefix_; } 
  std::string getName()  { return nice_name_; }
 
private:
  bool other_;

  std::string nice_name_;
  std::string full_prefix_;

  std::vector<std::string> expected_;
  std::vector<std::string> startswith_;
  std::vector<std::string> contains_;
  std::vector<std::string> name_;

  /*!
   *\brief Stores items by name
   */
  std::map<std::string, diagnostic_item::DiagnosticItem*> items_;
  
  void updateItems(std::vector<diagnostic_msgs::DiagnosticStatus*> to_analyze);
    
  std::vector<diagnostic_msgs::DiagnosticStatus*> toAnalyzeOther(std::map<std::string, diagnostic_item::DiagnosticItem*> msgs);
    
  std::vector<diagnostic_msgs::DiagnosticStatus*> toAnalyze(std::map<std::string, diagnostic_item::DiagnosticItem*> msgs);






};


}
#endif //GENERIC_ANALYZER_H
