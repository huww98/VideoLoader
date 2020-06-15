#pragma once

#define PY_SSIZE_T_CLEAN
#include <Python.h>

namespace huww {

class pyref {
  protected:
    PyObject *ptr;
public:
    pyref(): ptr(nullptr) {}
    pyref(PyObject *ref): ptr(ref) {}
    PyObject *get() const { return this->ptr; }
    explicit operator bool() const { return this->ptr != nullptr; }
};

class owned_pyref;
class borrowed_pyref: public pyref {
  public:
    borrowed_pyref(PyObject *ref) : pyref(ref) {}
    borrowed_pyref(const owned_pyref &ref);
    owned_pyref own();
};

class owned_pyref: public pyref {
  public:
    owned_pyref() {}
    owned_pyref(PyObject *ref) : pyref(ref) {}
    owned_pyref(borrowed_pyref &ref);

    owned_pyref &operator=(owned_pyref &&ref) noexcept;
    owned_pyref(owned_pyref &&ref) noexcept;
    owned_pyref &operator=(const owned_pyref &ref);
    owned_pyref(const owned_pyref &ref);

    ~owned_pyref();
    borrowed_pyref borrow() const { return borrowed_pyref(*this); }
    PyObject *transfer();
};

} // namespace huww
