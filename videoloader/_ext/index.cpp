#include <iostream>

#define PY_SSIZE_T_CLEAN
#include <Python.h>

#include "PyRef.h"
#include "videoloader.h"

using namespace huww;

static PyMethodDef videoLoaderMethods[] = {{NULL, NULL, 0, NULL}};

static struct PyModuleDef videoLoaderModule = {
    .m_base = PyModuleDef_HEAD_INIT,
    .m_name = "videoloader._ext",
    .m_doc = nullptr,
    .m_size = -1, /* size of per-interpreter state of the module,
                     or -1 if the module keeps state in global variables. */
    .m_methods = videoLoaderMethods,
};

struct PyVideoLoader {
    PyObject_HEAD;
    videoloader::VideoLoader videoLoader;
};

static PyObject *VideoLoader_AddVideoFile(PyVideoLoader *self, PyObject *args) {
    PyBytesObject *file_path_obj;
    if (!PyArg_ParseTuple(args, "O&", PyUnicode_FSConverter, &file_path_obj))
        return nullptr;

    auto file_path = PyBytes_AsString((PyObject *)file_path_obj);
    if (file_path == nullptr)
        return nullptr;

    Py_DECREF(file_path_obj);
    try {
        self->videoLoader.addVideoFile(file_path);
        return Py_None;
    } catch (std::runtime_error e) {
        PyErr_SetString(PyExc_RuntimeError, e.what());
        return nullptr;
    }
}

static PyMethodDef VideoLoader_methods[] = {
    {"add_video_file", (PyCFunction) VideoLoader_AddVideoFile, METH_VARARGS, nullptr},
    {NULL},
};

static PyTypeObject PyVideoLoaderType = {
    .ob_base = PyVarObject_HEAD_INIT(NULL, 0) // clang-format off
    .tp_name = "videoloader._ext._VideoLoader", // clang-format on
    .tp_doc = "Custom objects",
    .tp_basicsize = sizeof(PyVideoLoader),
    .tp_itemsize = 0,
    .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
    .tp_new = PyType_GenericNew,
    .tp_methods = VideoLoader_methods,
};

PyMODINIT_FUNC PyInit__ext(void) {
    if (PyType_Ready(&PyVideoLoaderType) < 0)
        return nullptr;

    BorrowedPyRef loaderType((PyObject *)&PyVideoLoaderType);
    OwnedPyRef m = PyModule_Create(&videoLoaderModule);
    if (m.get() == nullptr)
        return nullptr;

    if (PyModule_AddObject(m.get(), "_VideoLoader", loaderType.get()) < 0) {
        return nullptr;
    }

    return m.transfer();
}
