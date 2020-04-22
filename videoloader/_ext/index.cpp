#include <iostream>

#define PY_SSIZE_T_CLEAN
#include <Python.h>

static PyMethodDef videoLoaderMethods[] = {
    {NULL, NULL, 0, NULL}        /* Sentinel */
};

static struct PyModuleDef videoLoaderModule = {
    PyModuleDef_HEAD_INIT,
    "videoloader._ext",   /* name of module */
    nullptr, /* module documentation, may be NULL */
    -1,       /* size of per-interpreter state of the module,
                 or -1 if the module keeps state in global variables. */
    videoLoaderMethods,
};

PyMODINIT_FUNC PyInit__ext(void) {
    return PyModule_Create(&videoLoaderModule);
}
