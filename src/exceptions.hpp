#ifndef FILENAME_H
#define FILENAME_H

#include <stdexcept>

/*
*   Excpetion to indicate an error related to stream processing.
*
*/

class StreamProcessingError : public std::runtime_error
{
public:
  StreamProcessingError(const std::string& what_arg): std::runtime_error(what_arg) {}
  StreamProcessingError(const char* what_arg): std::runtime_error(what_arg) {}
};

#endif
