// ReSharper disable CppMemberFunctionMayBeConst

#include "python-streamlink.h"

#include "utils.hpp"

#include <frameobject.h> // TODO: move to "python-x.h"

#include <sstream>

namespace streamlink {
    bool loaded = false;
    bool loadingFailed = false;

    PyObject* module;
    namespace methods
    {
        PyObject* new_session;
    }

    using ThreadState = PyGILState_STATE;

    ThreadState AcquireThreadState()
    {
        return PyGILState_Ensure();
    }

    void ReleaseThreadState(ThreadState state) {
        PyGILState_Release(state);
    }

    std::string PyStringToString(PyObject* pyStr)
    {
        ssize_t len;
        auto cstr = PyUnicode_AsUTF8AndSize(pyStr, &len);
        auto str = std::string(cstr, len);

        return str;
    }
    std::string GetExceptionInfo()
    {
        if (!PyErr_Occurred()) return "";

        std::string message{};

        PyObject* type, * value, * traceback;
        PyErr_Fetch(&type, &value, &traceback);

        if (traceback) {
            PyErr_NormalizeException(&type, &value, &traceback);

            message.append("Traceback (most recent call last): \n");

            auto tb = reinterpret_cast<PyTracebackObject *>(traceback);
            while (tb) {
                {
                    std::stringstream tb_line{};
                    tb_line << "  File \"" << PyUnicode_AsUTF8(tb->tb_frame->f_code->co_filename) <<  "\", line " << tb->tb_lineno << ", in " << PyUnicode_AsUTF8(tb->tb_frame->f_code->co_name) << std::endl;
                    message.append(tb_line.str());
                }
                tb = tb->tb_next;
            }
        }

        message.append(reinterpret_cast<PyTypeObject*>(type)->tp_name);

        auto s = std::string(PyUnicode_AsUTF8(PyObject_Str(value)));
        if (!s.empty()) {
            message.append(": ");
            message.append(s);
            message.append("\n");
        }

        PyErr_Clear(); // required?

        return message;
    }
    void LogFailure()
    {
        FF_LOG(LOG_ERROR, "Failed to initialize streamlink plugin: %s", GetExceptionInfo().c_str());
    }

    void Initialize()
    {
        auto FireInitializationFailure = [](bool log = true) -> void
        {
            loaded = false;
            loadingFailed = true;
            if (log)
                LogFailure();
            PyEval_ReleaseThread(PyThreadState_Get());
        };
        if (!Py_IsInitialized())
        {
            FF_LOG(LOG_INFO, "initializing Python...");
            // TODO make this configurable via properties.
            std::string python_path{R"(A:\obs-debug-install\data\obs-plugins\obs-streamlink\-1Python38)"};
            auto widstr = std::wstring(python_path.begin(), python_path.end());
            Py_SetPythonHome(widstr.c_str());
            Py_Initialize();
        }

        // TODO: when to release?
        PyGILState_Ensure();
        PyRun_SimpleString("import sys; print(f'sys.version = {sys.version}'); print(f'sys.path = {sys.path}');");
        PyRun_SimpleString("import site; print(site.getsitepackages());");

        module = PyImport_ImportModule("streamlink");
        if (module == nullptr) return FireInitializationFailure();

        methods::new_session = PyObject_GetAttrString(module, static_cast<const char*>("Streamlink"));
        if (methods::new_session == nullptr) return FireInitializationFailure();
        if (!PyCallable_Check(methods::new_session)) return FireInitializationFailure(false);

        loaded = true;
        PyEval_ReleaseThread(PyThreadState_Get());
    }

    PyObjectHolder::PyObjectHolder(PyObject* underlying, bool inc) : underlying(underlying)
    {
        if (inc)
            Py_INCREF(underlying);
    }
    PyObjectHolder::~PyObjectHolder()
    {
        if (underlying != nullptr)
        {
            ThreadGIL state = ThreadGIL();
            Py_DECREF(underlying);
        }
    }
    PyObjectHolder::PyObjectHolder(PyObjectHolder&& another) noexcept
    {
        underlying = another.underlying;
        another.underlying = nullptr;
    }
    PyObjectHolder& PyObjectHolder::operator=(PyObjectHolder&& another) noexcept
    {
        underlying = another.underlying;
        another.underlying = nullptr;

        return *this;
    }

    Stream::Stream(PyObject* u) : PyObjectHolder(u)
    {
    }
    Stream::Stream(Stream&& another) noexcept : PyObjectHolder(std::move(another))
    {
    }
    std::vector<char> Stream::Read(const size_t readSize)
    {
        const auto readFunc = PyObject_GetAttrString(underlying, "read");
        if (!readFunc)
            throw invalid_underlying_object();
        auto iucallableHolder = PyObjectHolder(readFunc, false);
        if (!PyCallable_Check(readFunc)) throw invalid_underlying_object();

        auto args = PyTuple_Pack(1, PyLong_FromSize_t(readSize));
        auto argsGuard = PyObjectHolder(args, false);

        const auto result = PyObject_Call(readFunc, args, nullptr);
        if (!result)
            throw call_failure(GetExceptionInfo().c_str());
        auto resultGuard = PyObjectHolder(result, false);

        char* buf1;
        ssize_t readLen;
        PyBytes_AsStringAndSize(result, &buf1, &readLen);

        return {buf1, buf1 + readLen};
    }
    void Stream::Close()
    {
        auto args = PyTuple_New(0);
        auto argsGuard = PyObjectHolder(args, false);

        auto closeCallable = PyObject_GetAttrString(underlying, "close");
        if (closeCallable == nullptr)
            throw invalid_underlying_object();
        auto closeCallableHolder = PyObjectHolder(closeCallable, false);
        if (!PyCallable_Check(closeCallable)) throw invalid_underlying_object();
        auto result = PyObject_Call(closeCallable, args, nullptr);
        if (result == nullptr)
            throw call_failure(GetExceptionInfo().c_str());
    }
    StreamInfo::StreamInfo(std::string name, PyObject* u) : PyObjectHolder(u), name(std::move(name))
    {

    }
    StreamInfo::StreamInfo(StreamInfo&& another) noexcept : PyObjectHolder(std::move(another))
    {
        name = another.name;
    }
    PyObject* StreamInfo::Open()
    {
        auto callable = PyObject_GetAttrString(underlying, "open");
        if (!callable) throw invalid_underlying_object();
        auto callableGuard = PyObjectHolder(callable, false);
        if (!PyCallable_Check(callable)) throw invalid_underlying_object();

        auto args = PyTuple_New(0);
        auto argsGuard = PyObjectHolder(args, false);
        auto result = PyObject_Call(callable, args, nullptr);

        if (result == nullptr)
            throw call_failure(GetExceptionInfo().c_str());
        return result;
    }

    ThreadGIL::ThreadGIL()
    {
        state = PyGILState_Ensure();
    }

    ThreadGIL::~ThreadGIL()
    {
        PyGILState_Release(state);
    }
}

streamlink::Session::Session()
{
    //while (!IsDebuggerPresent())
        //Sleep(100);
    //DebugBreak();
    if (!loaded) throw not_loaded();
    auto args = PyTuple_New(0);
    underlying = PyObject_Call(methods::new_session, args, nullptr);
    Py_DECREF(args);

    if (underlying == nullptr)
        throw call_failure(GetExceptionInfo().c_str());

    //Py_INCREF(underlying);
    set_option = PyObject_GetAttrString(underlying, "set_option");
    if (set_option == nullptr) throw call_failure(GetExceptionInfo().c_str());
    set_optionGuard = PyObjectHolder(set_option, false);
    if (!PyCallable_Check(set_option)) throw invalid_underlying_object();
}


streamlink::Session::~Session()
{
}


namespace streamlink {
    std::map<std::string, StreamInfo> Session::GetStreamsFromUrl(const std::string& url)
    {
        auto streamsCallable = PyObject_GetAttrString(underlying, "streams");
        if (streamsCallable == nullptr) throw call_failure(GetExceptionInfo().c_str());
        auto streamsCallableGuard = PyObjectHolder(streamsCallable, false);
        if (!PyCallable_Check(streamsCallable)) throw invalid_underlying_object();

        if (!loaded) throw not_loaded();
        auto urlStrObj = PyUnicode_FromStringAndSize(url.c_str(), static_cast<ssize_t>(url.size()));
        auto urlStrObjGuard = PyObjectHolder(urlStrObj, false);
        auto args = PyTuple_Pack(1, urlStrObj);
        auto argsGuard = PyObjectHolder(args, false);

        auto result = PyObject_Call(streamsCallable, args, nullptr);
        if (result == nullptr)
            throw call_failure(GetExceptionInfo().c_str());

        auto resultGuard = PyObjectHolder(result, false);
        auto items = PyDict_Items(result);
        auto itemsGuard = PyObjectHolder(items, false);

        auto size = PyList_Size(items);
        auto ret = std::map<std::string, StreamInfo>();
        for (int i = 0; i < size; i++)
        {
            auto itemTuple = PyList_GetItem(items, i); // borrowed
            if (PyTuple_Size(itemTuple) != 2) continue;
            auto nameObj = PyTuple_GetItem(itemTuple, 0);
            auto valueObj = PyTuple_GetItem(itemTuple, 1); // no need to +ref, done by `StreamInfo` ctor
            auto name = PyStringToString(nameObj);

            ret.emplace(std::piecewise_construct, std::forward_as_tuple(name), std::forward_as_tuple(name, valueObj));
            //ret.emplace(name, StreamInfo(name, valueObj));
        }
        return ret;
    }
}

void streamlink::Session::SetOption(std::string const& name, PyObject* value)
{
    if (!loaded) throw not_loaded();
    auto nameObj = PyUnicode_FromStringAndSize(name.c_str(), static_cast<ssize_t>(name.size()));
    auto nameObjGuard = PyObjectHolder(nameObj, false);

    auto args = PyTuple_Pack(2, nameObj, value);
    auto argsGuard = PyObjectHolder(args, false);

    auto result = PyObject_Call(set_option, args, nullptr);
    if (result == nullptr)
        throw call_failure(GetExceptionInfo().c_str());
    Py_DECREF(result);
}

void streamlink::Session::SetOptionString(std::string const& name, std::string const& value)
{
    auto valueObj = PyUnicode_FromStringAndSize(value.c_str(), static_cast<ssize_t>(value.size()));
    auto valueObjGuard = PyObjectHolder(valueObj, false);
    SetOption(name, valueObj);
}

void streamlink::Session::SetOptionDouble(std::string const& name, double value)
{
    auto valueObj = PyFloat_FromDouble(value);
    auto valueObjGuard = PyObjectHolder(valueObj, false);
    SetOption(name, valueObj);
}

void streamlink::Session::SetOptionInt(std::string const& name, long long value)
{
    auto valueObj = PyLong_FromLongLong(value);
    auto valueObjGuard = PyObjectHolder(valueObj, false);
    SetOption(name, valueObj);
}

void streamlink::Session::SetOptionBool(std::string const& name, bool value)
{
    auto valueObj = PyBool_FromLong(value);
    auto valueObjGuard = PyObjectHolder(valueObj, false);
    SetOption(name, valueObj);
}
