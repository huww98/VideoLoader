#pragma once

#define PY_SSIZE_T_CLEAN
#include <Python.h>

namespace huww {
class OwnedPyRef;
class BorrowedPyRef {
  private:
    PyObject *ptr;

  public:
    BorrowedPyRef(PyObject *ref) : ptr(ref) {}
    BorrowedPyRef(const OwnedPyRef &ref);
    PyObject *get() const { return this->ptr; }
    OwnedPyRef own();
};

class OwnedPyRef {
  private:
    PyObject *ptr;

  public:
    OwnedPyRef() : ptr(nullptr) {}
    OwnedPyRef(PyObject *ref) : ptr(ref) {}
    OwnedPyRef(BorrowedPyRef &ref);

    OwnedPyRef &operator=(OwnedPyRef &&ref);
    OwnedPyRef(OwnedPyRef &&ref) noexcept;
    OwnedPyRef &operator=(const OwnedPyRef &ref);
    OwnedPyRef(const OwnedPyRef &ref);

    ~OwnedPyRef();
    PyObject *get() const { return this->ptr; }
    BorrowedPyRef borrow() const { return BorrowedPyRef(*this); }
    PyObject *transfer();
};

} // namespace huww
