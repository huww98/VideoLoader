#define PY_SSIZE_T_CLEAN
#include <Python.h>
#include <numpy/arrayobject.h>

#include <optional>
#include <typeindex>
#include <typeinfo>
#include <unordered_map>

#include "PyRef.h"
#include "videoloader.h"

using namespace huww;

static auto dltensor_capsule_name = "dltensor";

static std::unordered_map<error_t, PyObject *> system_error_map{
    {ENOENT, PyExc_FileNotFoundError},
    {EISDIR, PyExc_IsADirectoryError},
};

static std::unordered_map<std::type_index, PyObject *> exception_map{
    {std::type_index(typeid(std::runtime_error)), PyExc_RuntimeError},
    {std::type_index(typeid(std::out_of_range)), PyExc_IndexError},
    {std::type_index(typeid(std::system_error)), PyExc_OSError},
};

static error_t get_error_code(std::exception &e) {
    if (auto err = dynamic_cast<std::system_error *>(&e)) {
        auto &code = err->code();
        if (code.category() == std::system_category()) {
            return code.value();
        } else {
            return 0;
        }
    }
    if (auto err = dynamic_cast<videoloader::av_error *>(&e)) {
        return AVUNERROR(err->code());
    }
    return 0;
}

static void handle_exception(std::exception &e) {
    PyObject *py_exception = nullptr;

    auto code = get_error_code(e);
    if (code > 0) {
        try {
            py_exception = system_error_map.at(code);
        } catch (std::out_of_range &) {
            /* Ignore */
        }
    }

    if (py_exception == nullptr) {
        try {
            py_exception = exception_map.at(std::type_index(typeid(e)));
        } catch (std::out_of_range &) {
            py_exception = PyExc_RuntimeError;
        }
    }
    PyErr_SetString(py_exception, e.what());
}

class release_GIL_guard {
  private:
    PyThreadState *_save;

  public:
    release_GIL_guard() noexcept : _save(PyEval_SaveThread()) {}
    ~release_GIL_guard() noexcept { PyEval_RestoreThread(_save); }
};

static PyObject *DLTensor_to_numpy(PyObject *unused, PyObject *_arg) {
    owned_pyref cap = borrowed_pyref(_arg).own();
    auto p = PyCapsule_GetPointer(cap.get(), dltensor_capsule_name);
    if (p == nullptr) {
        PyErr_SetString(PyExc_ValueError, "No compatible DLTensor found.");
        return nullptr;
    }
    auto dlpack = static_cast<DLManagedTensor *>(p);
    auto &dl = dlpack->dl_tensor;
    owned_pyref array = PyArray_New(&PyArray_Type, dl.ndim, dl.shape, NPY_UINT8, dl.strides,
                                    dl.data, 0, 0, nullptr);
    PyArray_SetBaseObject((PyArrayObject *)array.get(), cap.transfer());
    return array.transfer();
}

static PyMethodDef videoLoader_methods[] = {
    {"dltensor_to_numpy", DLTensor_to_numpy, METH_O, nullptr},
    {nullptr},
};

static struct PyModuleDef videoLoaderModule = {
    .m_base = PyModuleDef_HEAD_INIT,
    .m_name = "videoloader._ext",
    .m_doc = nullptr,
    .m_size = -1, /* size of per-interpreter state of the module,
                     or -1 if the module keeps state in global variables. */
    .m_methods = videoLoader_methods,
};

struct PyVideo {
    PyObject_HEAD;
    videoloader::video video;
};

static void PyVideo_dealloc(PyVideo *v) {
    v->video.~video();
    Py_TYPE(v)->tp_free((PyObject *)v);
}

static PyObject *PyVideo_sleep(PyVideo *self, PyObject *args) {
    try {
        self->video.sleep();
    } catch (std::exception &e) {
        PyErr_SetString(PyExc_RuntimeError, e.what());
        return nullptr;
    }
    Py_RETURN_NONE;
}

static PyObject *PyVideo_IsSleeping(PyVideo *self, PyObject *args) {
    try {
        return self->video.is_sleeping() ? Py_True : Py_False;
    } catch (std::exception &e) {
        PyErr_SetString(PyExc_RuntimeError, e.what());
        return nullptr;
    }
}

static PyObject *PyVideo_NumFrames(PyVideo *self, PyObject *args) {
    return PyLong_FromSize_t(self->video.num_frames());
}

static owned_pyref FractionClass;

static PyObject *PyVideo_AverageFrameRate(PyVideo *self, PyObject *args) {
    auto frameRate = self->video.average_frame_rate();
    owned_pyref pyFrameRateArgs = Py_BuildValue("ii", frameRate.num, frameRate.den);
    if (!pyFrameRateArgs) {
        return nullptr;
    }
    owned_pyref pyFrameRate = PyObject_Call(FractionClass.get(), pyFrameRateArgs.get(), nullptr);
    return pyFrameRate.transfer();
}

static PyObject *PyVideo_GetBatch(PyVideo *self, PyObject *args) {
    owned_pyref iterator = PyObject_GetIter(args);
    if (iterator.get() == nullptr) {
        return nullptr;
    }
    std::vector<size_t> indices;
    while (true) {
        owned_pyref item = PyIter_Next(iterator.get());
        if (PyErr_Occurred()) {
            return nullptr;
        }
        if (item.get() == nullptr) {
            break;
        }
        auto idx = PyLong_AsSize_t(item.get());
        if (PyErr_Occurred()) {
            return nullptr;
        }
        indices.push_back(idx);
    }

    try {
        videoloader::video_dlpack::ptr dlPack;
        {
            release_GIL_guard no_GIL;
            dlPack = self->video.get_batch(indices);
        }
        return PyCapsule_New(dlPack.release(), dltensor_capsule_name, [](PyObject *cap) {
            if (strcmp(PyCapsule_GetName(cap), dltensor_capsule_name) != 0) {
                return; // used.
            }
            auto p = PyCapsule_GetPointer(cap, dltensor_capsule_name);
            auto dlpack = static_cast<DLManagedTensor *>(p);
            dlpack->deleter(dlpack);
        });
    } catch (std::exception &e) {
        handle_exception(e);
        return nullptr;
    }
}

static PyMethodDef Video_methods[] = {
    {"sleep", (PyCFunction)PyVideo_sleep, METH_NOARGS, nullptr},
    {"is_sleeping", (PyCFunction)PyVideo_IsSleeping, METH_NOARGS, nullptr},
    {"get_batch", (PyCFunction)PyVideo_GetBatch, METH_O, nullptr},
    {"num_frames", (PyCFunction)PyVideo_NumFrames, METH_NOARGS, nullptr},
    {"__len__", (PyCFunction)PyVideo_NumFrames, METH_NOARGS, nullptr},
    {"average_frame_rate", (PyCFunction)PyVideo_AverageFrameRate, METH_NOARGS, nullptr},
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
    videoloader::video_loader videoLoader;
    owned_pyref videoType;
};

static PyObject *VideoLoader_AddVideoFile(PyVideoLoader *self, PyObject *args) {
    std::string file_path_str;
    {
        PyBytesObject *_file_path_obj;
        if (!PyArg_ParseTuple(args, "O&", PyUnicode_FSConverter, &_file_path_obj))
            return nullptr;

        owned_pyref file_path_obj((PyObject *)_file_path_obj);
        auto file_path = PyBytes_AsString(file_path_obj.get());
        if (file_path == nullptr)
            return nullptr;
        file_path_str = file_path;
    }

    try {
        std::optional<videoloader::video> video;
        {
            release_GIL_guard no_GIL;
            video = self->videoLoader.add_video_file(file_path_str);
        }

        auto videoType = (PyTypeObject *)self->videoType.get();
        owned_pyref pyVideo = videoType->tp_alloc(videoType, 0);
        if (pyVideo.get() == nullptr)
            return nullptr;
        new (&((PyVideo *)pyVideo.get())->video) videoloader::video(std::move(*video));
        return pyVideo.transfer();
    } catch (std::exception &e) {
        handle_exception(e);
        return nullptr;
    }
}

static PyObject *PyVideoLoader_new(PyTypeObject *type, PyObject *args, PyObject *kwds) {
    owned_pyref self = type->tp_alloc(type, 0);
    if (!self) {
        return nullptr;
    }
    auto &pyVideoLoader = *(PyVideoLoader *)self.get();
    new (&pyVideoLoader.videoLoader) videoloader::video_loader();
    new (&pyVideoLoader.videoType) owned_pyref(borrowed_pyref((PyObject *)&PyVideoType).own());
    return self.transfer();
}

static void PyVideoLoader_dealloc(PyVideoLoader *v) {
    v->videoType.~owned_pyref();
    Py_TYPE(v)->tp_free((PyObject *)v);
}

static int PyVideoLoader_init(PyVideoLoader *self, PyObject *args, PyObject *kwds) {
    static const char *kwlist[] = {"video_type", nullptr};
    PyObject *_video_type = nullptr;
    if (!PyArg_ParseTupleAndKeywords(args, kwds, "|O!", (char **)kwlist, &PyType_Type,
                                     &_video_type)) {
        return -1;
    }
    if (_video_type != nullptr) {
        if (!PyType_IsSubtype((PyTypeObject *)_video_type, &PyVideoType)) {
            PyErr_SetString(PyExc_TypeError, "Expecting a sub-type of videoloader._ext._Video");
            return -1;
        }
        self->videoType = borrowed_pyref(_video_type).own();
    }
    return 0;
}

static PyMethodDef VideoLoader_methods[] = {
    {"add_video_file", (PyCFunction)VideoLoader_AddVideoFile, METH_VARARGS, nullptr},
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
    borrowed_pyref loaderType((PyObject *)&PyVideoLoaderType);

    if (PyType_Ready(&PyVideoType) < 0)
        return nullptr;
    borrowed_pyref videoType((PyObject *)&PyVideoType);

    owned_pyref m = PyModule_Create(&videoLoaderModule);
    if (m.get() == nullptr)
        return nullptr;

    import_array(); // import numpy
    videoloader::init();

    if (PyModule_AddObject(m.get(), "_VideoLoader", loaderType.get()) < 0) {
        return nullptr;
    }
    if (PyModule_AddObject(m.get(), "_Video", videoType.get()) < 0) {
        return nullptr;
    }

    owned_pyref fractionsModule = PyImport_ImportModule("fractions");
    if (!fractionsModule) {
        return nullptr;
    }
    FractionClass = PyObject_GetAttrString(fractionsModule.get(), "Fraction");
    if (!FractionClass) {
        return nullptr;
    }

    return m.transfer();
}
