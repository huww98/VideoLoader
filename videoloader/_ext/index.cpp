#include <iostream>

#define PY_SSIZE_T_CLEAN
#include <Python.h>

static PyMethodDef videoLoaderMethods[] = {
    {NULL, NULL, 0, NULL}
};

static struct PyModuleDef videoLoaderModule = {
    .m_base = PyModuleDef_HEAD_INIT,
    .m_name = "videoloader._ext",
    .m_doc = nullptr,
    .m_size = -1, /* size of per-interpreter state of the module,
                     or -1 if the module keeps state in global variables. */
    .m_methods = videoLoaderMethods,
};

PyMODINIT_FUNC PyInit__ext(void) {
    return PyModule_Create(&videoLoaderModule);
}
