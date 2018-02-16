/* Errors that may be thrown by the agent
 *
 * \copyright
 * Copyright 2018 Red Hat Inc. All rights reserved.
 */
#ifndef SPICE_STREAMING_AGENT_ERRORS_HPP
#define SPICE_STREAMING_AGENT_ERRORS_HPP

#include <exception>
#include <stddef.h>

namespace spice {
namespace streaming_agent {

/*! Base class for all errors in the SPICE streaming agent */
class Error : public std::exception
{
public:
    /*! Constructor takes the base message returned by what()
     *  The message must remain valid over the whole lifetime of the exception.
     *  It is recommended to only use static C strings, since formatting of any
     *  dynamic argument is done by 'format_message' */
    Error(const char *message) : exception(), message(message) { }

    /*! Return the base error message.
     *  This method overrides the standard 'what' method in std::exception
     *  and returns the message that was given to the constructor. */
    virtual const char *what() const noexcept override;

    /*! Format the message with dynamic arguments in the given buffer
     *  This method is to be overriden by derived classes to format the dynamic
     *  arguments of the exception at the point where this is needed.
     *  It returns the number of bytes that would be required for the whole message. */
    virtual int format_message(char *buffer, size_t size) const noexcept;

    /*! Log the error message to the syslog
     *  This method logs the formatted error message to the syslog.
     *  It returns *this in order to allow logging at throw points using:
     *      throw Error("message").syslog(); */
    const Error &syslog() const noexcept;

protected:
    const char *message;
};

/*! I/O errors that may use the standard 'errno' value */
class IOError : public Error
{
public:
    /*! Constructor takes a base message and a value of errno.
     *  The value of errno may be zero for I/O errors that have no associated errno */
    IOError(const char *msg, int saved_errno) : Error(msg), saved_errno(saved_errno) {}

    /*! Write a message that includes the base message followed by the errno-derived message.
     *  The errno-derived message results from strerror, if saved_errno was set. */
    int format_message(char *buffer, size_t size) const noexcept override;

protected:
    int append_strerror(char *buffer, size_t size, int written) const noexcept;

protected:
    int saved_errno;
};

/*! Error happening while opening a file */
class OpenError : public IOError
{
public:
    /*! Create an error taking the base message, the file and the system's errno value */
    OpenError(const char *msg, std::string filename, int saved_errno)
        : IOError(msg, saved_errno), filename(filename) {}

    /*! The base message followed by the file name */
    int format_message(char *buffer, size_t size) const noexcept override;
protected:
    std::string filename;
};

/*! Errors that happens while writing a message */
class WriteError : public IOError
{
public:
    /*! Create a write error associated with an 'operation'.
     *  The operation is used to indicate what is being written.
     *  The saved_errno can be used to report standard operating system errors. */
    WriteError(const char *msg, const char *operation, int saved_errno)
        : IOError(msg, saved_errno), operation(operation) {}

    /*! Format a message in the form "Write error writing operation" */
    int format_message(char *buffer, size_t size) const noexcept override;
protected:
    const char *operation;
};

/*! Errors that happen while reading a message */
class ReadError : public IOError
{
public:
    /*! Create a read error associated with an operating system errno if available */
    ReadError(const char *msg, int saved_errno): IOError(msg, saved_errno) {}
};

}} // namespace spice::streaming_agent

#endif // SPICE_STREAMING_AGENT_ERRORS_HPP
