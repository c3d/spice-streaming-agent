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
    OpenError(const char *msg, const char *filename, int saved_errno)
        : IOError(msg, saved_errno), filename(filename) {}

    /*! The base message followed by the file name */
    int format_message(char *buffer, size_t size) const noexcept override;

protected:
    const char *filename;
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
    ReadError(const char *msg, const char *operation, int saved_errno)
        : IOError(msg, saved_errno), operation(operation) {}

    /*! Format a message in the form "Read error reading operation" */
    int format_message(char *buffer, size_t size) const noexcept override;

protected:
    const char *operation;
};

/*! Protocol errors while reading incoming messages, i.e. in-message inconsistencies.
 *  Note that protocol errors often have no associated errno, since the problem is with
 *  the content of the message, which the operating system does not care about. */
class ProtocolError : public ReadError
{
public:
    ProtocolError(const char *msg, const char *operation, int saved_errno = 0)
        : ReadError(msg, operation, saved_errno) {}
};

/*! Violations of the protocol which reside from incorrect data in an incoming message.
 *  In this case, we can report what was received and what was expected */
class MessageDataError : public ProtocolError
{
public:
    /*! Construct an error that records what was actually received and the expected value */
    MessageDataError(const char *msg, const char *operation,
                     size_t received, size_t expected, int saved_errno = 0)
        : ProtocolError(msg, operation, saved_errno), received(received), expected(expected) {}

    /*! Format the message, reporting expected and received values */
    int format_message(char *buffer, size_t size) const noexcept override;

protected:
    size_t received;
    size_t expected;
};

/*! Violations of the protocol deriving from the length of the message */
class MessageLengthError : public MessageDataError
{
public:
    /*! Construct an error that records what was actually received and the expected value */
    MessageLengthError(const char *operation, size_t received, size_t expected,
                       int saved_errno = 0)
        : MessageDataError(received < expected ? "message is too short" : "message is too long",
                           operation, received, expected, saved_errno) {}

    /*! Format the message, reporting expected and received values */
    int format_message(char *buffer, size_t size) const noexcept override;

};

/*! Errors related to option parsing.
 *  If a plugin does not recognize a specific option, it should throw an OptionError directly.
 *  Note that the agent may retry the same option with a different plugin in that case. */
class OptionError : public Error
{
public:
    /*! Create an option error with a given base message and the invalid option */
    OptionError(const char *msg, const char *option) : Error(msg), option(option) {}

    /*! Generates a message indicating which option was not recognized. */
    int format_message(char *buffer, size_t size) const noexcept override;

protected:
    const char *option;
};

/*! Errors related to the value for known options.
 *  If a plugin does not accept a specific option value, it should throw OptionValueError.
 *  Note that the agent may retry the same option value with a different plugin in that case. */
class OptionValueError : public OptionError
{
public:
    /*! Create an option value error with a given base message, option name and value */
    OptionValueError(const char *msg, const char *option, const char *value)
        : OptionError(msg, option), value(value) {}

    /*! Generate a message indicating which value was invalid and for which option. */
    int format_message(char *buffer, size_t size) const noexcept override;

protected:
    const char *value;
};

/*! Exception thrown if interrupted by SIGINT/SIGTERM during system call */
class QuitRequested : public Error
{
public:
    /*! Abort current I/O operation and quit */
    QuitRequested() : Error("quit requested") {}
};

}} // namespace spice::streaming_agent

#endif // SPICE_STREAMING_AGENT_ERRORS_HPP
