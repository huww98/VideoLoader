#include "PyRef.h"

#include <utility>

namespace huww {
BorrowedPyRef::BorrowedPyRef(const OwnedPyRef& ref) : ptr(ref.get()) {}
OwnedPyRef BorrowedPyRef::own() { return OwnedPyRef(*this); }

PyObject *OwnedPyRef::transfer() {
    auto ptr = this->ptr;
    this->ptr = nullptr;
    return ptr;
}
OwnedPyRef &OwnedPyRef::operator=(const OwnedPyRef &ref) {
    this->ptr = ref.ptr;
    Py_XINCREF(this->ptr);
    return *this;
}
OwnedPyRef &OwnedPyRef::operator=(OwnedPyRef &&ref) {
    std::swap(ref.ptr, this->ptr);
    return *this;
}
OwnedPyRef::OwnedPyRef(BorrowedPyRef &ref) : ptr(ref.get()) { Py_XINCREF(this->ptr); }
OwnedPyRef::OwnedPyRef(OwnedPyRef &&ref) noexcept { *this = std::move(ref); }
OwnedPyRef::OwnedPyRef(const OwnedPyRef &ref) { *this = ref; }


OwnedPyRef::~OwnedPyRef() { Py_XDECREF(this->ptr); }

}
