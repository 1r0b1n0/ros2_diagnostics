/*********************************************************************
* Software License Agreement (BSD License)
* 
*  Copyright (c) 2008, Willow Garage, Inc.
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

#ifndef DIAGNOSTICUPDATER_HH
#define DIAGNOSTICUPDATER_HH

#include <stdexcept>
#include <vector>
#include <string>

#include "ros/node_handle.h"
#include "ros/this_node.h"

#include "diagnostic_msgs/DiagnosticArray.h"
#include "diagnostic_updater/DiagnosticStatusWrapper.h"

#include <boost/thread.hpp>

/* Old main page, now mostly obsolete. Should delete before M3 once ROS API
is better documented.

@mainpage

@htmlinclude manifest.html

@b diagnostic_updater.h defines the Updater class, which
simplifies writing of diagnostic publishing code, by allowing a set of
registered callbacks to be published at a fixed rate.

<hr>

@section topics ROS topics

Subscribes to (name/type):
- None

Publishes to (name / type):

@section parameters ROS parameters

Reads the following parameters from the parameter server

- @b "diagnostic_period" : @b [double] period at which diagnostics should be sent in seconds (Default: 1)

*/

namespace diagnostic_updater
{
  
typedef boost::function<void(DiagnosticStatusWrapper&)> TaskFunction;
typedef boost::function<void(diagnostic_msgs::DiagnosticStatus&)> UnwrappedTaskFunction;

/**
 * \brief DiagnosticTask is an abstract base class for diagnostic tasks.
 *
 * Subclasses will be provided for generating common diagnostic
 * information.
 *
 * A DiagnosticTask has a name, and a function that is called to cleate a
 * DiagnosticStatusWrapper.
 */

class DiagnosticTask
{
public:
	/**
	 * \brief Constructs a DiagnosticTask setting its name in the process.
	 */
  DiagnosticTask(const std::string name) : name_(name)
  {}

	/**
	 * \brief Returns the name of the DiagnosticTask.
	 */
  const std::string &getName()
  {
    return name_;
  }

  /**
	 * \brief Fills out this Task's DiagnosticStatusWrapper.
	 */
  virtual void run(diagnostic_updater::DiagnosticStatusWrapper &stat) = 0;

	/**
	 * Virtual destructor as this is a base class.
	 */
  virtual ~DiagnosticTask()
  {}

private:
  const std::string name_;
};

/**
 * \brief a DiagnosticTask based on a boost::function.
 */

template <class T>
class GenericFunctionDiagnosticTask : public DiagnosticTask
{
public:
	/**
	 * Constructs a GenericFunctionDiagnosticTask based on the given name and
	 * function.
	 *
	 * \param name Name of the function.
	 *
	 * \param fn Function to be called when DiagnosticTask::run is called.
	 */
  GenericFunctionDiagnosticTask(const std::string &name, boost::function<void(T&)> fn) : 
    DiagnosticTask(name), fn_(fn)
  {}
  
  virtual void run(DiagnosticStatusWrapper &stat)
  {
    fn_(stat);
  }
  
private:
  const std::string name_;
  const TaskFunction fn_;
};

typedef GenericFunctionDiagnosticTask<diagnostic_msgs::DiagnosticStatus> UnwrappedFunctionDiagnosticTask;
typedef GenericFunctionDiagnosticTask<DiagnosticStatusWrapper> FunctionDiagnosticTask;

/**
 * \brief A DiagnosticTask that can be combined with others into a
 * composite DiagnosticStatusWrapper.
 *
 * The combination operation is done by using a CombinationDiagnosticTask.
 * 
 * A typical use is to allow a generic diagnostic such as the \ref
 * FrequencyStatus to be augmented with node-specific key-value pairs.
 *
 * This is an abstract base class. Sub-classes should redefine split_run.
 */

class ComposableDiagnosticTask : public DiagnosticTask
{
public:
  /**
	 * \brief Contructs a ComposableDiagnosticTask with the specified name.
	 */
	ComposableDiagnosticTask(const std::string name) : DiagnosticTask(name)
  {}
  
  void run(diagnostic_updater::DiagnosticStatusWrapper &stat)
  {
    split_run(stat, stat);
  }

protected:
  friend class CombinationDiagnosticTask;

	/**
	 * \brief Partially fills out a DiagnosticStatusWrapper.
	 *
	 * This method is called for a number of CombinationDiagnosticTask
	 * instances when generating a DiagnosticStatusWrapper. The summary and
	 * key-value pairs are placed in separate DiagnosticStatusWrapper
	 * instances because the summaries need to be merged together.
	 *
	 * \param summary Place to store the summary (level and message) for this task.
	 * These will be merged with the summary for the other tasks using
	 * diagnostic_updater::DiagnosticStatusWrapper::mergeSummary.
	 *
	 * \param details Place to store the key-value pairs created by this
	 * task. 
	 */
  virtual void split_run(diagnostic_updater::DiagnosticStatusWrapper &summary, 
      diagnostic_updater::DiagnosticStatusWrapper &details) = 0;
};

/**
 * \brief Merges CombinationDiagnosticTask into a single DiagnosticTask.
 *
 * The CombinationDiagnosticTask allows multiple ComposableDiagnosticTask
 * instances to be combined into a single DiagnosticStatus. The output of
 * the combination has the max of the status levels, and a concatenation of
 * the non-zero-level messages.
 */

class CombinationDiagnosticTask : public DiagnosticTask
{
public:
	/**
	 * \brief Constructs a CombinationDiagnosticTask with the given name.
	 */
  CombinationDiagnosticTask(const std::string name) : DiagnosticTask(name)
  {}

	/**
	 * \brief Runs each child and merges their outputs.
	 */
  virtual void run(DiagnosticStatusWrapper &stat)
  {
    DiagnosticStatusWrapper summary;
    stat.summary(0, "");
    
    for (std::vector<ComposableDiagnosticTask *>::iterator i = tasks_.begin();
        i != tasks_.end(); i++)
    {
      (*i)->split_run(summary, stat);
    
      stat.mergeSummary(summary.level, summary.message);
    }
  }
  
  /**
	 * \brief Adds a child CombinationDiagnosticTask.
	 *
	 * This CombinationDiagnosticTask will be called each time this
	 * CombinationDiagnosticTask is run.
	 */
	void addTask(ComposableDiagnosticTask *t)
  {
    tasks_.push_back(t);
  }

private:
  std::vector<ComposableDiagnosticTask *> tasks_;
};

/**
 * \brief Internal use only.
 *
 * Base class for diagnostic_updater::Updater and self_test::Dispatcher.
 * The class manages a collection of diagnostic updaters. It contains the
 * common functionality used for producing diagnostic updates and for
 * self-tests.
 */

class DiagnosticTaskVector
{
protected:
  /**
	 * \brief Class used to represent a diagnostic task internally in
	 * DiagnosticTaskVector.
	 */
	
	class DiagnosticTaskInternal
  {
  public:
    DiagnosticTaskInternal(const std::string name, TaskFunction f) :
      name_(name), fn_(f)
    {}

    void run(diagnostic_updater::DiagnosticStatusWrapper &stat) const
    {
      stat.name = name_;
      fn_(stat);
    }

    const std::string &getName() const
    {
      return name_;
    }

  private:
    std::string name_;
    TaskFunction fn_;
  };

  boost::mutex lock_;

  /**
	 * \brief Returns the vector of tasks.
	 */
  const std::vector<DiagnosticTaskInternal> &getTasks()
  {
    return tasks_;
  }

public:    
  /**
	 * \brief Add a DiagnosticTask embodied by a name and function to the
	 * DiagnosticTaskVector
   *
	 * \param name Name to autofill in the DiagnosticStatusWrapper for this task.
	 * 
	 * \param f Function to call to fill out the DiagnosticStatusWrapper.
	 * This function need not remain valid after the last time the tasks are
	 * called, and in particular it need not be valid at the time the
	 * DiagnosticTaskVector is destructed.
   */

  void add(const std::string &name, TaskFunction f)
  {
    DiagnosticTaskInternal int_task(name, f);
    addInternal(int_task);
  }

  /**
   * \brief Add a DiagnosticTask to the DiagnosticTaskVector
   *
   * \param task The DiagnosticTask to be added. It must remain live at
   * least until the last time its diagnostic method is called. It need not be
   * valid at the time the DiagnosticTaskVector is destructed.
   */

  void add(DiagnosticTask &task)
  {
    TaskFunction f = boost::bind(&DiagnosticTask::run, &task, _1);
    add(task.getName(), f);
  }
  
  /**
	 * \brief Add a DiagnosticTask embodied by a name and method to the
	 * DiagnosticTaskVector
   *
	 * \param name Name to autofill in the DiagnosticStatusWrapper for this task.
	 *
	 * \param c Class instance the method is being called on.
	 * 
	 * \param f Method to call to fill out the DiagnosticStatusWrapper.
	 * This method need not remain valid after the last time the tasks are
	 * called, and in particular it need not be valid at the time the
	 * DiagnosticTaskVector is destructed.
   */
  template <class T>
  void add(const std::string name, T *c, void (T::*f)(diagnostic_updater::DiagnosticStatusWrapper&))
  {
    DiagnosticTaskInternal int_task(name, boost::bind(f, c, _1));
    addInternal(int_task);
  }
  
private:
  /**
	 * Allows an action to be taken when a task is added. The Updater class
	 * uses this to immediately publish a diagnostic that says that the node
	 * is loading.
	 */
	virtual void addedTaskCallback(DiagnosticTaskInternal &)
  {}
  std::vector<DiagnosticTaskInternal> tasks_;
  
protected:
	/**
	 * Common code for all add methods.
	 */
  void addInternal(DiagnosticTaskInternal &task)
  {
    boost::mutex::scoped_lock lock(lock_);
    tasks_.push_back(task); 
    addedTaskCallback(task);
  }
};

/**
 * \brief Manages a list of diagnostic tasks, and calls them in a
 * rate-limited manner.
 *
 * This class manages a list of diagnostic tasks. Its update function
 * should be called frequently. At some predetermined rate, the update
 * function will cause all the diagnostic tasks to run, and will collate
 * and publish the resulting diagnostics. The publication rate is
 * determined by the "~/diagnostic_period" ros parameter.
 *
 * The class also allows an update to be forced when something significant
 * has happened, and allows a single message to be broadcast on all the
 * diagnostics if normal operation of the node is suspended for some
 * reason.
 */

class Updater : public DiagnosticTaskVector
{
public:
  bool verbose_;
  
  /**
	 * \brief Constructs an updater class.
	 *
	 * \param h Node handle from which to get the diagnostic_period
	 * parameter.
	 */

  Updater(ros::NodeHandle h) : node_handle_(h)
  {
    setup();
  }
  
  /**
	 * \brief Causes the diagnostics to update if the inter-update interval
	 * has been exceeded.
	 */

  void update()
  {
    ros::Time now_time = ros::Time::now();
    if (now_time < next_time_) {
      return;
    }

    force_update();
  }

  /**
	 * \brief Forces the diagnostics to update.
	 *
	 * Useful if the node has undergone a drastic state change that should be
	 * published immediately.
	 */

  void force_update()
  {
    private_node_handle_.param("diagnostic_period", period_, 1.0);
    next_time_ = ros::Time::now() + ros::Duration().fromSec(period_);
    
    if (node_handle_.ok())
    {
      std::vector<diagnostic_msgs::DiagnosticStatus> status_vec;

      boost::mutex::scoped_lock lock(lock_); // Make sure no adds happen while we are processing here.
      const std::vector<DiagnosticTaskInternal> &tasks = getTasks();
      for (std::vector<DiagnosticTaskInternal>::const_iterator iter = tasks.begin();
           iter != tasks.end(); iter++)
      {
        diagnostic_updater::DiagnosticStatusWrapper status;

        status.name = iter->getName();
        status.level = 2;
        status.message = "No message was set";

        iter->run(status);

        status_vec.push_back(status);

        if (verbose_ && status.level)
          ROS_WARN("Non-zero diagnostic status. Name: '%s', status %i: '%s'", status.name.c_str(), status.level, status.message.c_str());
      }

      publish(status_vec);
    }
  }

  /**
	 * \brief Returns the interval between updates.
	 */

  double getPeriod()
  {
    return period_;
  }

  // Destructor has trouble because the node is already shut down.
  /*~Updater()
  {
    // Create a new node handle and publisher because the existing one is 
    // probably shut down at this stage.
    
    ros::NodeHandle newnh; 
    node_handle_ = newnh; 
    publisher_ = node_handle_.advertise<diagnostic_msgs::DiagnosticArray>("/diagnostics", 1);
    broadcast(2, "Node shut down"); 
  }*/

  /**
	 * \brief Output a message on all the known DiagnosticStatus.
	 *
	 * Useful if something drastic is happening such as shutdown or a
	 * self-test.
	 *
	 * \param lvl Level of the diagnostic being output.
	 *
	 * \param msg Status message to output.
	 */

	void broadcast(int lvl, const std::string msg)
  {
    std::vector<diagnostic_msgs::DiagnosticStatus> status_vec;
      
    const std::vector<DiagnosticTaskInternal> &tasks = getTasks();
    for (std::vector<DiagnosticTaskInternal>::const_iterator iter = tasks.begin();
        iter != tasks.end(); iter++)
    {
      diagnostic_updater::DiagnosticStatusWrapper status;

      status.name = iter->getName();
      status.summary(lvl, msg);

      status_vec.push_back(status);
    }

    publish(status_vec);
  }

private:
  /**
	 * Publishes a single diagnostic status.
	 */
	void publish(diagnostic_msgs::DiagnosticStatus &stat)
  {
    std::vector<diagnostic_msgs::DiagnosticStatus> status_vec;
    status_vec.push_back(stat);
    publish(status_vec);
  }

  /**
	 * Publishes a vector of diagnostic statuses.
	 */
  void publish(std::vector<diagnostic_msgs::DiagnosticStatus> &status_vec)
  {
    for  (std::vector<diagnostic_msgs::DiagnosticStatus>::iterator 
        iter = status_vec.begin(); iter != status_vec.end(); iter++)
    {
      iter->name = 
        ros::this_node::getName().substr(1) + std::string(": ") + iter->name;
    }
    diagnostic_msgs::DiagnosticArray msg;
    msg.set_status_vec(status_vec);
    msg.header.stamp = ros::Time::now(); // Add timestamp for ROS 0.10
    publisher_.publish(msg);
  }

  /**
	 * Publishes on /diagnostics and reads the diagnostic_period parameter.
	 */
	void setup()
  {
    publisher_ = node_handle_.advertise<diagnostic_msgs::DiagnosticArray>("/diagnostics", 1);
    private_node_handle_ = ros::NodeHandle("~");

    private_node_handle_.param("diagnostic_period", period_, 1.0);
    next_time_ = ros::Time::now();

    verbose_ = false;
  }

	/**
	 * Causes a placeholder DiagnosticStatus to be published as soon as a
	 * diagnostic task is added to the Updater.
	 */
  virtual void addedTaskCallback(DiagnosticTaskInternal &task)
  {
    DiagnosticStatusWrapper stat;
    stat.name = task.getName();
    stat.summary(2, "Node starting up");
    publish(stat);
  }

  ros::NodeHandle private_node_handle_;
  ros::NodeHandle node_handle_;
  ros::Publisher publisher_;

  ros::Time next_time_;

  double period_;

};

};

/**
 * This class is deprecated. Use diagnostic_updater::Updater instead.
 */

template <class T>
class DiagnosticUpdater : public diagnostic_updater::Updater
{
public:
  ROSCPP_DEPRECATED DiagnosticUpdater(T *n) : diagnostic_updater::Updater(ros::NodeHandle()), owner_(n)
  {
    complain();
  }

  // This constructor goes away now that ros::node is gone
  /*
  ROSCPP_DEPRECATED DiagnosticUpdater(T *c, ros::Node &n) : diagnostic_updater::Updater(ros::NodeHandle()), owner_(c)
  {
    complain();
  }
  */

  ROSCPP_DEPRECATED DiagnosticUpdater(T *c, ros::NodeHandle &h) : diagnostic_updater::Updater(h), owner_(c)
  {
    complain();
  }

  using diagnostic_updater::Updater::add;

  void addUpdater(void (T::*f)(diagnostic_msgs::DiagnosticStatus&))
  {
    diagnostic_updater::UnwrappedTaskFunction f2 = boost::bind(f, owner_, _1);
    diagnostic_msgs::DiagnosticStatus stat;
    f2(stat); // Get the function to fill out its name.
    boost::shared_ptr<diagnostic_updater::UnwrappedFunctionDiagnosticTask> 
      fcls(new diagnostic_updater::UnwrappedFunctionDiagnosticTask(stat.name, f2));
    tasks_vect_.push_back(fcls);
    add(*fcls);
  }

private:
  void complain()
  {
    ROS_WARN("DiagnosticUpdater is deprecated, please use diagnostic_updater::Updater instead.");
  }
  
  T *owner_;
  std::vector<boost::shared_ptr<diagnostic_updater::UnwrappedFunctionDiagnosticTask> > tasks_vect_;
};

#endif
