#define PY_SSIZE_T_CLEAN
#include <Python.h>
#include <numpy/arrayobject.h>

#include <typeindex>
#include <typeinfo>
#include <unordered_map>

#include "PyRef.h"
#include "videoloader.h"

using namespace huww;

static auto dlTensorCapsuleName = "dltensor";

static std::unordered_map<error_t, PyObject *> osExceptionMap{
    {ENOENT, PyExc_FileNotFoundError},
    {EISDIR, PyExc_IsADirectoryError},
};

static std::unordered_map<std::type_index, PyObject *> exceptionMap{
    {std::type_index(typeid(std::runtime_error)), PyExc_RuntimeError},
    {std::type_index(typeid(std::out_of_range)), PyExc_IndexError},
    {std::type_index(typeid(std::system_error)), PyExc_OSError},
};

static error_t getErrorCode(std::exception &e) {
    if (auto systemError = dynamic_cast<std::system_error *>(&e)) {
        auto &code = systemError->code();
        if (code.category() == std::system_category()) {
            return code.value();
        } else {
            return 0;
        }
    }
    if (auto avError = dynamic_cast<videoloader::AvError *>(&e)) {
        return AVUNERROR(avError->code());
    }
    return 0;
}

static void handleException(std::exception &e) {
    PyObject *pyException = nullptr;

    auto code = getErrorCode(e);
    if (code > 0) {
        try {
            pyException = osExceptionMap.at(code);
        } catch (std::out_of_range &) {
            /* Ignore */
        }
    }

    if (pyException == nullptr) {
        try {
            pyException = exceptionMap.at(std::type_index(typeid(e)));
        } catch (std::out_of_range &) {
            pyException = PyExc_RuntimeError;
        }
    }
    PyErr_SetString(pyException, e.what());
}

static PyObject *DLTensor_to_numpy(PyObject *unused, PyObject *_arg) {
    OwnedPyRef cap = BorrowedPyRef(_arg).own();
    auto p = PyCapsule_GetPointer(cap.get(), dlTensorCapsuleName);
    if (p == nullptr) {
        PyErr_SetString(PyExc_ValueError, "No compatible DLTensor found.");
        return nullptr;
    }
    auto dlManager = static_cast<DLManagedTensor *>(p);
    auto &dl = dlManager->dl_tensor;
    OwnedPyRef array = PyArray_New(&PyArray_Type, dl.ndim, dl.shape, NPY_UINT8,
                                   dl.strides, dl.data, 0, 0, nullptr);
    PyArray_SetBaseObject((PyArrayObject *)array.get(), cap.transfer());
    return array.transfer();
}

static PyMethodDef videoLoaderMethods[] = {
    {"dltensor_to_numpy", DLTensor_to_numpy, METH_O, nullptr},
    {nullptr},
};

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

static PyObject *PyVideo_numFrames(PyVideo *self, PyObject *args) {
    return PyLong_FromSize_t(self->video.numFrames());
}

static OwnedPyRef FractionClass;

static PyObject *PyVideo_averageFrameRate(PyVideo *self, PyObject *args) {
    auto frameRate = self->video.averageFrameRate();
    OwnedPyRef pyFrameRateArgs =
        Py_BuildValue("ii", frameRate.num, frameRate.den);
    if (!pyFrameRateArgs) {
        return nullptr;
    }
    OwnedPyRef pyFrameRate =
        PyObject_Call(FractionClass.get(), pyFrameRateArgs.get(), nullptr);
    return pyFrameRate.transfer();
}

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
                if (strcmp(PyCapsule_GetName(cap), dlTensorCapsuleName) != 0) {
                    return; // used.
                }
                auto p = PyCapsule_GetPointer(cap, dlTensorCapsuleName);
                auto dlTensor = static_cast<DLManagedTensor *>(p);
                dlTensor->deleter(dlTensor);
            });
    } catch (std::exception &e) {
        handleException(e);
        return nullptr;
    }
}

static PyMethodDef Video_methods[] = {
    {"sleep", (PyCFunction)PyVideo_sleep, METH_NOARGS, nullptr},
    {"is_sleeping", (PyCFunction)PyVideo_isSleeping, METH_NOARGS, nullptr},
    {"get_batch", (PyCFunction)PyVideo_getBatch, METH_O, nullptr},
    {"num_frames", (PyCFunction)PyVideo_numFrames, METH_NOARGS, nullptr},
    {"__len__", (PyCFunction)PyVideo_numFrames, METH_NOARGS, nullptr},
    {"average_frame_rate", (PyCFunction)PyVideo_averageFrameRate, METH_NOARGS,
     nullptr},
    {nullptr},
};

static PyTypeObject PyVideoType = {
    .ob_base = PyVarObject_HEAD_INIT(nullptr, 0) // clang-format off
    .tp_name = "videoloader._ext._Video", // clang-format on
    .tp_basicsize = sizeof(PyVideo),
    .tp_itemsize = 0,
    .tp_dealloc = (destructor)PyVideo_dealloc,
    .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
    .tp_methods = Video_methods,
};

struct PyVideoLoader {
    PyObject_HEAD;
    videoloader::VideoLoader videoLoader;
    OwnedPyRef videoType;
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

        auto videoType = (PyTypeObject *)self->videoType.get();
        OwnedPyRef pyVideo = videoType->tp_alloc(videoType, 0);
        if (pyVideo.get() == nullptr)
            return nullptr;
        new (&((PyVideo *)pyVideo.get())->video)
            videoloader::Video(std::move(video));
        return pyVideo.transfer();
    } catch (std::exception &e) {
        handleException(e);
        return nullptr;
    }
}

static PyObject *PyVideoLoader_new(PyTypeObject *type, PyObject *args,
                                   PyObject *kwds) {
    OwnedPyRef self = type->tp_alloc(type, 0);
    if (!self) {
        return nullptr;
    }
    auto &pyVideoLoader = *(PyVideoLoader *)self.get();
    new (&pyVideoLoader.videoLoader) videoloader::VideoLoader();
    new (&pyVideoLoader.videoType)
        OwnedPyRef(BorrowedPyRef((PyObject *)&PyVideoType).own());
    return self.transfer();
}

static void PyVideoLoader_dealloc(PyVideoLoader *v) {
    v->videoType.~OwnedPyRef();
    Py_TYPE(v)->tp_free((PyObject *)v);
}

static int PyVideoLoader_init(PyVideoLoader *self, PyObject *args,
                              PyObject *kwds) {
    static const char *kwlist[] = {"video_type", nullptr};
    PyObject *_video_type = nullptr;
    if (!PyArg_ParseTupleAndKeywords(args, kwds, "|O!", (char **)kwlist,
                                     &PyType_Type, &_video_type)) {
        return -1;
    }
    if (_video_type != nullptr) {
        if (!PyType_IsSubtype((PyTypeObject *)_video_type, &PyVideoType)) {
            PyErr_SetString(PyExc_TypeError,
                            "Expecting a sub-type of videoloader._ext._Video");
            return -1;
        }
        self->videoType = BorrowedPyRef(_video_type).own();
    }
    return 0;
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
    .tp_dealloc = (destructor)PyVideoLoader_dealloc,
    .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
    .tp_methods = VideoLoader_methods,
    .tp_init = (initproc)PyVideoLoader_init,
    .tp_new = PyVideoLoader_new,
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

    import_array(); // import numpy

    if (PyModule_AddObject(m.get(), "_VideoLoader", loaderType.get()) < 0) {
        return nullptr;
    }
    if (PyModule_AddObject(m.get(), "_Video", videoType.get()) < 0) {
        return nullptr;
    }

    OwnedPyRef fractionsModule = PyImport_ImportModule("fractions");
    if (!fractionsModule) {
        return nullptr;
    }
    FractionClass = PyObject_GetAttrString(fractionsModule.get(), "Fraction");
    if (!FractionClass) {
        return nullptr;
    }

    return m.transfer();
}
