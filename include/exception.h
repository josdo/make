#include <cstdarg>
#include <cstdio>
#include <stdexcept>
#include <string>

class PrintfException : public std::exception {
   public:
    PrintfException(const char* format, ...) : _msg() {
        char buf[1000];
        va_list ap;
        va_start(ap, format);
        std::vsnprintf(buf, sizeof(buf), format, ap);
        va_end(ap);
        _msg.assign(buf);
    }
    virtual const char* what() const noexcept override { return _msg.c_str(); }

   private:
    std::string _msg;
};