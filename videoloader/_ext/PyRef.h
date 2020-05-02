#pragma once

#define PY_SSIZE_T_CLEAN
#include <Python.h>

namespace huww {

class PyRef {
  protected:
    PyObject *ptr;
public:
    PyRef(): ptr(nullptr) {}
    PyRef(PyObject *ref): ptr(ref) {}
    PyObject *get() const { return this->ptr; }
    explicit operator bool() const { return this->ptr != nullptr; }
};

class OwnedPyRef;
class BorrowedPyRef: public PyRef {
  public:
    BorrowedPyRef(PyObject *ref) : PyRef(ref) {}
    BorrowedPyRef(const OwnedPyRef &ref);
    OwnedPyRef own();
};

class OwnedPyRef: public PyRef {
  public:
    OwnedPyRef() {}
    OwnedPyRef(PyObject *ref) : PyRef(ref) {}
    OwnedPyRef(BorrowedPyRef &ref);

    OwnedPyRef &operator=(OwnedPyRef &&ref) noexcept;
    OwnedPyRef(OwnedPyRef &&ref) noexcept;
    OwnedPyRef &operator=(const OwnedPyRef &ref);
    OwnedPyRef(const OwnedPyRef &ref);

    ~OwnedPyRef();
    BorrowedPyRef borrow() const { return BorrowedPyRef(*this); }
    PyObject *transfer();
};

} // namespace huww
