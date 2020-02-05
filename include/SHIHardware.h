#pragma once
#include <Arduino.h>

namespace SHI {

class SHIHardware {
  public:
    virtual void resetWithReason(String reason, bool restart)=0;
    virtual void errLeds(void)=0;
    virtual void setupWatchdog()=0;
    virtual void feedWatchdog()=0;
    virtual void disableWatchdog()=0;
    virtual String getNodeName()=0;
    void logInfo(String name, const char *func, String message) {
      log("INFO:"+name+"."+func+"() "+message);
    };
    void logWarn(String name, const char *func, String message) {
      log("WARN:"+name+"."+func+"() "+message);
    };
    void logError(String name, const char *func, String message) {
      log("ERROR:"+name+"."+func+"() "+message);
    };
  protected:
    virtual void log(String message);
};

}
