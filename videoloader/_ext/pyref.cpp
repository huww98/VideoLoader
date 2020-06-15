#include "pyref.h"

#include <utility>

namespace huww {
borrowed_pyref::borrowed_pyref(const owned_pyref &ref) : pyref(ref.get()) {}
owned_pyref borrowed_pyref::own() { return owned_pyref(*this); }

PyObject *owned_pyref::transfer() {
    auto ptr = this->ptr;
    this->ptr = nullptr;
    return ptr;
}
owned_pyref &owned_pyref::operator=(const owned_pyref &ref) {
    this->ptr = ref.ptr;
    Py_XINCREF(this->ptr);
    return *this;
}
owned_pyref &owned_pyref::operator=(owned_pyref &&ref) noexcept {
    std::swap(ref.ptr, this->ptr);
    return *this;
}
owned_pyref::owned_pyref(borrowed_pyref &ref) : pyref(ref.get()) { Py_XINCREF(this->ptr); }
owned_pyref::owned_pyref(owned_pyref &&ref) noexcept { *this = std::move(ref); }
owned_pyref::owned_pyref(const owned_pyref &ref) { *this = ref; }

owned_pyref::~owned_pyref() { Py_XDECREF(this->ptr); }

} // namespace huww
