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

struct PyVideo {
    PyObject_HEAD;
    videoloader::Video video;
};

static void PyVideo_dealloc(PyVideo *v) {
    v->video.~Video();
    Py_TYPE(v)->tp_free((PyObject *)v);
}

static PyObject *PyVideo_sleep(PyVideo *self, PyObject *args) {
    try {
        self->video.sleep();
    } catch (std::exception &e) {
        PyErr_SetString(PyExc_RuntimeError, e.what());
        return nullptr;
    }
    return Py_None;
}

static PyObject *PyVideo_isSleeping(PyVideo *self, PyObject *args) {
    try {
        return self->video.isSleeping() ? Py_True : Py_False;
    } catch (std::exception &e) {
        PyErr_SetString(PyExc_RuntimeError, e.what());
        return nullptr;
    }
}

static auto dlTensorCapsuleName = "dltensor";

static PyObject *PyVideo_getBatch(PyVideo *self, PyObject *args) {
    OwnedPyRef iterator = PyObject_GetIter(args);
    if (iterator.get() == nullptr) {
        return nullptr;
    }
    std::vector<int> indices;
    while (true) {
        OwnedPyRef item = PyIter_Next(iterator.get());
        if (PyErr_Occurred()) {
            return nullptr;
        }
        if (item.get() == nullptr) {
            break;
        }
        auto idx = PyLong_AsLong(item.get());
        if (PyErr_Occurred()) {
            return nullptr;
        }
        indices.push_back(idx);
    }

    try {
        auto dlPack = self->video.getBatch(indices);
        return PyCapsule_New(
            dlPack.release(), dlTensorCapsuleName, [](PyObject *cap) {
                auto p = PyCapsule_GetPointer(cap, dlTensorCapsuleName);
                if (p != nullptr) {
                    videoloader::VideoDLPack::free(static_cast<DLTensor *>(p));
                }
            });
    } catch (std::exception &e) {
        PyErr_SetString(PyExc_RuntimeError, e.what());
        return nullptr;
    }
}

static PyMethodDef Video_methods[] = {
    {"sleep", (PyCFunction)PyVideo_sleep, METH_NOARGS, nullptr},
    {"is_sleeping", (PyCFunction)PyVideo_isSleeping, METH_NOARGS, nullptr},
    {"get_batch", (PyCFunction)PyVideo_getBatch, METH_O, nullptr},
    {nullptr},
};

static PyTypeObject PyVideoType = {
    .ob_base = PyVarObject_HEAD_INIT(nullptr, 0) // clang-format off
    .tp_name = "videoloader._ext._Video", // clang-format on
    .tp_basicsize = sizeof(PyVideo),
    .tp_itemsize = 0,
    .tp_dealloc = (destructor)PyVideo_dealloc,
    .tp_flags = Py_TPFLAGS_DEFAULT,
    .tp_methods = Video_methods,
};

struct PyVideoLoader {
    PyObject_HEAD;
    videoloader::VideoLoader videoLoader;
};

static PyObject *VideoLoader_AddVideoFile(PyVideoLoader *self, PyObject *args) {
    std::string file_path_str;
    {
        PyBytesObject *_file_path_obj;
        if (!PyArg_ParseTuple(args, "O&", PyUnicode_FSConverter,
                              &_file_path_obj))
            return nullptr;

        OwnedPyRef file_path_obj((PyObject *)_file_path_obj);
        auto file_path = PyBytes_AsString(file_path_obj.get());
        if (file_path == nullptr)
            return nullptr;
        file_path_str = file_path;
    }

    try {
        auto video = self->videoLoader.addVideoFile(file_path_str);

        OwnedPyRef pyVideo = _PyObject_New(&PyVideoType);
        if (pyVideo.get() == nullptr)
            return nullptr;
        new (&((PyVideo *)pyVideo.get())->video)
            videoloader::Video(std::move(video));
        return pyVideo.transfer();
    } catch (std::exception &e) {
        PyErr_SetString(PyExc_RuntimeError, e.what());
        return nullptr;
    }
}

static PyMethodDef VideoLoader_methods[] = {
    {"add_video_file", (PyCFunction)VideoLoader_AddVideoFile, METH_VARARGS,
     nullptr},
    {nullptr},
};

static PyTypeObject PyVideoLoaderType = {
    .ob_base = PyVarObject_HEAD_INIT(nullptr, 0) // clang-format off
    .tp_name = "videoloader._ext._VideoLoader", // clang-format on
    .tp_basicsize = sizeof(PyVideoLoader),
    .tp_itemsize = 0,
    .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
    .tp_methods = VideoLoader_methods,
    .tp_new = PyType_GenericNew,
};

PyMODINIT_FUNC PyInit__ext(void) {
    if (PyType_Ready(&PyVideoLoaderType) < 0)
        return nullptr;
    BorrowedPyRef loaderType((PyObject *)&PyVideoLoaderType);

    if (PyType_Ready(&PyVideoType) < 0)
        return nullptr;
    BorrowedPyRef videoType((PyObject *)&PyVideoType);

    OwnedPyRef m = PyModule_Create(&videoLoaderModule);
    if (m.get() == nullptr)
        return nullptr;

    if (PyModule_AddObject(m.get(), "_VideoLoader", loaderType.get()) < 0) {
        return nullptr;
    }
    if (PyModule_AddObject(m.get(), "_Video", videoType.get()) < 0) {
        return nullptr;
    }

    return m.transfer();
}
