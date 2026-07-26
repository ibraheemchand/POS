#include "core/logger.h"
#include <QDateTime>
#include <iostream>
#include <fstream>
#include <mutex>

namespace pos {
namespace { const char* levelName(LogLevel level) { switch(level) { case LogLevel::Trace:return "TRACE"; case LogLevel::Debug:return "DEBUG"; case LogLevel::Info:return "INFO"; case LogLevel::Warning:return "WARNING"; case LogLevel::Error:return "ERROR"; case LogLevel::Critical:return "CRITICAL"; } return "UNKNOWN"; } }
Logger& Logger::instance(){static Logger logger;return logger;}
void Logger::configure(const std::filesystem::path& directory,LogLevel minimum){std::scoped_lock lock(mutex_);directory_=directory;minimum_=minimum;activeDate_.clear();rotateIfNeeded();}
void Logger::rotateIfNeeded(){const auto today=QDate::currentDate().toString("yyyy-MM-dd");if(today==activeDate_&&file_.is_open())return;if(directory_.empty())directory_=std::filesystem::current_path()/"logs";std::filesystem::create_directories(directory_);if(file_.is_open())file_.close();file_.open(directory_/(today.toStdString()+".log"),std::ios::out|std::ios::app);if(!file_)throw std::runtime_error("cannot open log file");activeDate_=today;}
void Logger::log(LogLevel level,const QString& message){if(static_cast<int>(level)<static_cast<int>(minimum_))return;std::scoped_lock lock(mutex_);rotateIfNeeded();const auto line=QString("%1 [%2] %3\n").arg(QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs),levelName(level),message).toStdString();file_<<line;file_.flush();std::clog<<line;std::clog.flush();}
void Logger::logException(const QString& context,const std::exception& exception){log(LogLevel::Error,context+": "+QString::fromUtf8(exception.what()));}
void Logger::flush(){std::scoped_lock lock(mutex_);if(file_.is_open())file_.flush();std::clog.flush();}
}
