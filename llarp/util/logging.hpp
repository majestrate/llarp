#pragma once

// Header for making actual log statements such as llarp::log::Info and so on work.

#include <string>
#include <string_view>
#include <array>

#include <source_location>
#include <ranges>

#include <spdlog/spdlog.h>

namespace llarp::log
{

// Not ready to pollute these deprecation warnings everywhere yet
#if 0
#define LOKINET_LOG_DEPRECATED(Meth) \
  [[deprecated("Use formatted log::" #Meth "(cat, fmt, args...) instead")]]
#else
#define LOKINET_LOG_DEPRECATED(Meth)
#endif


  struct RingBufferSink
  {
  };


}

// Deprecated loggers (in the top-level llarp namespace):
namespace llarp
{
  inline std::shared_ptr<log::RingBufferSink> logRingBuffer = nullptr;


void LogDebug(auto &&... args)
{

}

void LogInfo(auto &&...)
{

}

  void LogWarn(auto &...)
  {

   
  }


void LogError(auto &&...)
{ 

  
}




}  // namespace llarp
