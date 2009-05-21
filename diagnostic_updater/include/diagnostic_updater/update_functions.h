#ifndef __FREQUENCY_STATUS_DIAGNOSTIC_H__
#define __FREQUENCY_STATUS_DIAGNOSTIC_H__

#include <diagnostic_updater/diagnostic_updater.h>
#include <math.h>

namespace diagnostic_updater
{

/*class UpdateClass
{
public:
  UpdateFunction(std::string name) : name_(name);

  DiagnosticUpdateFunction func()
  {
    return boost::bind(&update_func, this);
  }

    

protected:
  virtual void update_func(DiagnosticStatusWrapper &stat)
  {

  }

private:
  std::string name_;
  std::vector<>
}*/

  
class FrequencyStatus
{
public:
  FrequencyStatus(double &min_freq, double& max_freq, double tolerance = 0.1, int window_size = 5) : min_freq_(min_freq), max_freq_(max_freq)
  {
    tolerance_ = tolerance;
    window_size_ = window_size;

    clear();
  }

  void clear()
  {
    ros::Time curtime = ros::Time::now();
    count_ = 0;

    for (int i = 0; i < window_size_; i++)
    {
      times_.push_back(curtime);
      seq_nums_.push_back(count_);
    }

    hist_indx_ = 0;
  }

  void tick()
  {
    count_++;
  }

  void operator()(diagnostic_updater::DiagnosticStatusWrapper &stat)
  {
    stat.name = "Frequency Status";

    ros::Time curtime = ros::Time::now();
    int curseq = count_;
    int events = curseq - seq_nums_[hist_indx_];
    double interval = (curtime - times_[hist_indx_]).toSec();
    double freq = events / interval;
    seq_nums_[hist_indx_] = curseq;
    times_[hist_indx_] = curtime;
    hist_indx_ = (hist_indx_ + 1) % window_size_;

    if (freq < min_freq_ * (1 - tolerance_))
    {
      stat.summary(2, "Frequency too low.");
    }
    else if (freq > max_freq_ * (1 + tolerance_))
    {
      stat.summary(2, "Frequency too high.");
    }
    else
    {
      stat.summary(0, "Desired frequency met");
    }

    stat.addv("Events in interval", events);
    stat.addv("Duration of interval (s)", interval);
    stat.addv("Actual frequency (Hz)", freq);
    if (min_freq_ == max_freq_)
      stat.addv("Target frequency (Hz)", min_freq_);
    if (min_freq_ > 0)
      stat.addv("Minimum acceptable frequency (Hz)", 
          min_freq_ * (1 - tolerance_));
    if (finite(max_freq_))
      stat.addv("Maximum acceptable frequency (Hz)", 
          max_freq_ * (1 - tolerance_));
  }

private:
  double &min_freq_;
  double &max_freq_;

  int count_;
  double tolerance_;
  int window_size_;
  std::vector <ros::Time> times_;
  std::vector <int> seq_nums_;
  int hist_indx_;
};

};

#endif
