#ifndef EXCEPTION_H
#define EXCEPTION_H

#include <cstdarg>
#include <cstdio>
#include <string>

class PrintfException : public std::exception {
   public:
    PrintfException(const char* format, ...) : _msg() {
        char buf[1000];
        va_list ap;
        va_start(ap, format);
        vsnprintf(buf, sizeof(buf), format, ap);
        _msg.assign(buf);
    }
    PrintfException() : _msg() {}
    void set_msg(const char* format, va_list ap) {
        char buf[1000];
        vsnprintf(buf, sizeof(buf), format, ap);
        _msg.assign(buf);
    }

    virtual const char* what() const noexcept override { return _msg.c_str(); }

   private:
    std::string _msg;
};

#endif  // EXCEPTION_H