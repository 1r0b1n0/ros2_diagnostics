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

/// Author: Blaise Gassend

#ifndef __DIAGNOSTIC_STATUS__UPDATE_FUNCTIONS_H__
#define __DIAGNOSTIC_STATUS__UPDATE_FUNCTIONS_H__

#include <diagnostic_updater/diagnostic_updater.h>
#include <math.h>

namespace diagnostic_updater
{

/**
 * \brief A structure that holds the constructor parameters for the
 * FrequencyStatus class.
 */

struct FrequencyStatusParam
{
	/**
	 * \brief Creates a filled-out FrequencyStatusParam.
	 */
 
	FrequencyStatusParam(double *min_freq, double *max_freq, double tolerance = 0.1, int window_size = 5) :
    min_freq_(min_freq), max_freq_(max_freq), tolerance_(tolerance), window_size_(window_size)
  {}

  /**
	 * \brief Minimum acceptable frequency.
	 *
	 * A pointer is used so that the value can be updated.
	 */

  double *min_freq_;

  /**
	 * \brief Maximum acceptable frequency.
	 *
	 * A pointer is used so that the value can be updated.
	 */

  double *max_freq_;

  /**
	 * \brief Tolerance with which bounds must be satisfied.
	 *
	 * Acceptable values are from *min_freq_ * (1 + torelance_) to *max_freq_ *
	 * (1 + tolerance_). 
	 *
	 * Common use cases are to set tolerance_ to zero, or to assign the same
	 * value to *max_freq_ and min_freq_.
	 */

  double tolerance_;

	/**
	 * \brief Number of events to consider in the statistics.
	 */
  int window_size_;
};

/**
 * \brief A diagnostic task that monitors the frequency of an event.
 *
 * This diagnostic task monitors the frequency of calls to its tick method,
 * and creates corresponding diagnostics.
 */

class FrequencyStatus : public ComposableDiagnosticTask
{
private:                                         
  const FrequencyStatusParam params_;

  int count_;
  std::vector <ros::Time> times_;
  std::vector <int> seq_nums_;
  int hist_indx_;
  boost::mutex lock_;

public:
	/**
	 * \brief Constructs a FrequencyStatus class with the given parameters.
	 */
	
	FrequencyStatus(const FrequencyStatusParam &params) : 
    ComposableDiagnosticTask("Frequency Status"), params_(params), 
    times_(params_.window_size_), seq_nums_(params_.window_size_)
  {
    clear();
  }
  
  /**
	 * \brief Resets the statistics.
	 */
	
	void clear()
  {
    boost::mutex::scoped_lock lock(lock_);
    ros::Time curtime = ros::Time::now();
    count_ = 0;

    for (int i = 0; i < params_.window_size_; i++)
    {
      times_[i] = curtime;
      seq_nums_[i] = count_;
    }

    hist_indx_ = 0;
  }

  /**
	 * \brief Signals that an event has occurred.
	 */
	
	void tick()
  {
    boost::mutex::scoped_lock lock(lock_);
    //ROS_DEBUG("TICK %i", count_);
    count_++;
  }

protected:
	virtual void split_run(diagnostic_updater::DiagnosticStatusWrapper &summary, 
      diagnostic_updater::DiagnosticStatusWrapper &details)
  {
    boost::mutex::scoped_lock lock(lock_);
    ros::Time curtime = ros::Time::now();
    int curseq = count_;
    int events = curseq - seq_nums_[hist_indx_];
    double window = (curtime - times_[hist_indx_]).toSec();
    double freq = events / window;
    seq_nums_[hist_indx_] = curseq;
    times_[hist_indx_] = curtime;
    hist_indx_ = (hist_indx_ + 1) % params_.window_size_;

    if (events == 0)
    {
      summary.summary(2, "No events recorded.");
    }
    else if (freq < *params_.min_freq_ * (1 - params_.tolerance_))
    {
      summary.summary(2, "Frequency too low.");
    }
    else if (freq > *params_.max_freq_ * (1 + params_.tolerance_))
    {
      summary.summary(2, "Frequency too high.");
    }
    else
    {
      summary.summary(0, "Desired frequency met");
    }

    details.addf("Events in window", "%f", events);
    details.addf("Events since startup", "%f", count_);
    details.addf("Duration of window (s)", "%f", window);
    details.addf("Actual frequency (Hz)", "%f",freq);
    if (*params_.min_freq_ == *params_.max_freq_)
      details.addf("Target frequency (Hz)", "%f",*params_.min_freq_);
    if (*params_.min_freq_ > 0)
      details.addf("Minimum acceptable frequency (Hz)", "%f",
          *params_.min_freq_ * (1 - params_.tolerance_));
    if (finite(*params_.max_freq_))
      details.addf("Maximum acceptable frequency (Hz)", "%f",
          *params_.max_freq_ * (1 + params_.tolerance_));
  }
};

/**
 * \brief A structure that holds the constructor parameters for the
 * TimeStampStatus class.
 */

struct TimeStampStatusParam
{
	/**
	 * \brief Creates a filled-out TimeStampStatusParam.
	 */

	TimeStampStatusParam(const double min_acceptable = -1, const double max_acceptable = 5) :
    max_acceptable_(max_acceptable), min_acceptable_(min_acceptable)
  {}
  
  /**
	 * \brief Maximum acceptable difference between two timestamps.
	 */

	double max_acceptable_;
  
	/**
	 * \brief Minimum acceptable difference between two timestamps.
	 */
  
	double min_acceptable_;

};
  
/**
 * \brief Default TimeStampStatusParam. This is like calling the
 * constructor with no arguments.
 */

static TimeStampStatusParam DefaultTimeStampStatusParam = TimeStampStatusParam();

/**
 * \brief Diagnostic task to monitor the interval between events.
 *
 * This diagnostic task monitors the difference between consecutive events,
 * and creates corresponding diagnostics. An error occurs if the interval
 * between consecutive events is too large or too small. An error condition
 * will only be reported during a single diagnostic report unless it
 * persists. Tallies of errors are also maintained to keep track of errors
 * in a more persistent way.
 */

class TimeStampStatus : public ComposableDiagnosticTask
{
private:
  void init()
  {
    early_count_ = 0;
    late_count_ = 0;
    zero_count_ = 0;
    zero_seen_ = false;
    max_delta_ = 0;
    min_delta_ = 0;
    deltas_valid_ = false;
  }

public:
  /**
	 * \brief Constructs the TimeStampStatus with the given parameters.
	 */

  TimeStampStatus(const TimeStampStatusParam &params) : 
    ComposableDiagnosticTask("Timestamp Status"), 
    params_(params)
  {
    init();
  }
  
  /**
	 * \brief Constructs the TimeStampStatus with the default parameters.
	 */

  TimeStampStatus() : 
    ComposableDiagnosticTask("Timestamp Status") 
  {
    init();
  }

  /**
	 * \brief Signals an event. Timestamp stored as a double.
	 *
	 * \param stamp The timestamp of the event that will be used in computing
	 * intervals.
	 */

  void tick(double stamp)
  {
    boost::mutex::scoped_lock lock(lock_);

    if (stamp == 0)
    {
      zero_seen_ = true;
    }
    else
    {
      double delta = ros::Time::now().toSec() - stamp;

      if (!deltas_valid_ || delta > max_delta_)
        max_delta_ = delta;

      if (!deltas_valid_ || delta < min_delta_)
        min_delta_ = delta;

      deltas_valid_ = true;
    }
  }

  /**
	 * \brief Signals an event.
	 *
	 * \param t The timestamp of the event that will be used in computing
	 * intervals.
	 */

  void tick(const ros::Time t)
  {
    tick(t.toSec());
  }

protected:
  virtual void split_run(diagnostic_updater::DiagnosticStatusWrapper &summary, 
      diagnostic_updater::DiagnosticStatusWrapper &details)
  {
    boost::mutex::scoped_lock lock(lock_);

    summary.summary(0, "Timestamps are reasonable.");
    if (!deltas_valid_)
    {
      summary.summary(1, "No data since last update.");
    }
    else 
    {
      if (min_delta_ < params_.min_acceptable_)
      {
        summary.summary(2, "Timestamps too far in future seen.");
        early_count_++;
      }
      
      if (max_delta_ > params_.max_acceptable_)
      {
        summary.summary(2, "Timestamps too far in past seen.");
        late_count_++;
      }

      if (zero_seen_)
      {
        summary.summary(2, "Zero timestamp seen.");
        zero_count_++;
      }
    }

    details.addf("Earliest timestamp delay:", "%f", min_delta_);
    details.addf("Latest timestamp delay:", "%f", max_delta_);
    details.addf("Earliest acceptable timestamp delay:", "%f", params_.min_acceptable_);
    details.addf("Latest acceptable timestamp delay:", "%f", params_.max_acceptable_);
    details.add("Late diagnostic update count:", late_count_); 
    details.add("Early diagnostic update count:", early_count_); 
    details.add("Zero seen diagnostic update count:", zero_count_); 

    deltas_valid_ = false;
    min_delta_ = 0;
    max_delta_ = 0;
    zero_seen_ = false;
  }

private:
  TimeStampStatusParam params_;
  int early_count_;
  int late_count_;
  int zero_count_;
  bool zero_seen_;
  double max_delta_;
  double min_delta_;
  bool deltas_valid_;
  boost::mutex lock_;
};

};

#endif
