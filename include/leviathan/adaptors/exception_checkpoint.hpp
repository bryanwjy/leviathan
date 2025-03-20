// Copyright 2025, Bryan Wong

#include <Python.h>

namespace lev {
namespace py {

class [[nodiscard]] exception_checkpoint {
public:
    explicit exception_checkpoint() noexcept : tstate{PyThreadState_GET()} {
        LEV_ASSERT(tstate != nullptr);
        PyErr_Fetch(tstate, &exc_type, &exc_value, &exc_tb);
    }

    exception_checkpoint(exception_checkpoint const&) = delete;
    exception_checkpoint& operator=(exception_checkpoint const&) = delete;

    exception_checkpoint(exception_checkpoint&& other) noexcept
        : tstate{exchange(other.tstate, nullptr)}
        , exc_type{other.exc_type}
        , exc_value{other.exc_value}
        , exc_tb{other.exc_tb} {}

    exception_checkpoint& operator=(exception_checkpoint&& other) noexcept {
        tstate = exchange(other.tstate, nullptr);
        exc_type = other.exc_type;
        exc_value = other.exc_value;
        exc_tb = other.exc_tb;
        return *this;
    }

    ~exception_checkpoint() noexcept {
        if (tstate) {
            PyErr_Restore(tstate, exc_type, exc_value, exc_tb);
        }
    }

private:
    PyThreadState* tstate;
    PyObject *exc_type, *exc_value, *exc_tb;
};

} // namespace py
} // namespace lev
