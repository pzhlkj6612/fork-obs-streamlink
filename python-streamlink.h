#pragma once

#define PY_SSIZE_T_CLEAN
// https://stackoverflow.com/questions/16200997/why-doesnt-include-python-h-work
#ifdef _DEBUG // NOLINT(*-reserved-identifier)
// https://github.com/pybind/pybind11/pull/3497/files#diff-39c6730bb01adf8717187cd92922902820e0a665e450b0fe1d54f8b68ab7f39b
#include <yvals.h>
#if _MSVC_STL_VERSION >= 143
#include <crtdefs.h>
#endif
#undef _DEBUG
#include <Python.h>
#define _DEBUG
#else
#include <Python.h>
#endif

#include <string>
#include <functional>
#include <map>
#include <stdexcept>

namespace streamlink {
    extern bool loaded;
    extern bool loadingFailed;

    extern PyObject* module;
    namespace methods
    {
        extern PyObject* new_session;
    }

    class ThreadGIL {
        PyGILState_STATE state;
    public:
        ThreadGIL();
        ~ThreadGIL();

        ThreadGIL(ThreadGIL& another) = delete;
        ThreadGIL(ThreadGIL&& another) = default;
        ThreadGIL& operator=(ThreadGIL& another) = delete;
        ThreadGIL& operator=(ThreadGIL&& another) = default;
    };

    std::string PyStringToString(PyObject* pyStr);

    std::string GetExceptionInfo();

    void LogFailure();

    void Initialize();

    class PyObjectHolder
    {
    public:
        PyObject* underlying;
        PyObjectHolder() : PyObjectHolder(nullptr, false) {}
        PyObjectHolder(PyObject* underlying, bool inc = true);
        virtual ~PyObjectHolder();
        PyObjectHolder(PyObjectHolder&& another)noexcept;

        PyObjectHolder& operator=(PyObjectHolder&& another) noexcept;
    };
    class not_loaded : public std::exception {};
    class call_failure : public std::runtime_error
    {
    public:

        call_failure() = default;

        explicit call_failure(char const* message)
            : std::runtime_error(message)
        { }
    };
    class invalid_underlying_object : public std::exception {};
    class stream_ended : public std::exception {};

    class Stream : public PyObjectHolder
    {
    public:
        Stream(PyObject* u);

        Stream(Stream&) = delete;
        Stream(Stream&& another) noexcept;

        int Read(unsigned char* buf, const int len);
        void Close();

    };

    class StreamInfo : public PyObjectHolder
    {
    public:
        std::string name;
        StreamInfo(std::string name, PyObject* u);
        StreamInfo(StreamInfo&& another) noexcept;

        PyObject* Open();
    };

    class Session : public PyObjectHolder {
    private:
        PyObject* set_option;
        PyObjectHolder set_optionGuard;
    public:
        Session();
        ~Session() override;

        std::map<std::string, StreamInfo> GetStreamsFromUrl(std::string const& url);

        void SetOption(std::string const& name, PyObject* value);
        void SetOptionString(std::string const& name, std::string const& value);
        void SetOptionDouble(std::string const& name, double value);
        void SetOptionInt(std::string const& name, long long value);
        void SetOptionBool(std::string const& name, bool value);
    };
}
