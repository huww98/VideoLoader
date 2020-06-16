#define PY_SSIZE_T_CLEAN
#include <Python.h>
#include <numpy/arrayobject.h>

#include <optional>
#include <typeindex>
#include <typeinfo>
#include <unordered_map>

#include "pyref.h"
#include "video.h"

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

static int PyVideo_init(PyVideo *self, PyObject *args, PyObject *kwds) {
    std::string file_path_str;
    {
        static const char *kwlist[] = {"url", nullptr};
        PyBytesObject *_file_path_obj;
        if (!PyArg_ParseTupleAndKeywords(args, kwds, "O&", (char **)kwlist, PyUnicode_FSConverter,
                                         &_file_path_obj)) {
            return -1;
        }

        owned_pyref file_path_obj((PyObject *)_file_path_obj);
        auto file_path = PyBytes_AsString(file_path_obj.get());
        if (file_path == nullptr)
            return -1;
        file_path_str = file_path;
    }

    try {
        release_GIL_guard no_GIL;
        new (&self->video) videoloader::video(file_path_str);
        return 0;
    } catch (std::exception &e) {
        handle_exception(e);
        return -1;
    }
}

static void PyVideo_dealloc(PyVideo *v) {
    v->video.~video();
    Py_TYPE(v)->tp_free((PyObject *)v);
}

static PyObject *PyVideo_Sleep(PyVideo *self, PyObject *args) {
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
    {"sleep", (PyCFunction)PyVideo_Sleep, METH_NOARGS, nullptr},
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
    .tp_init = (initproc)PyVideo_init,
    .tp_new = PyType_GenericNew,
};

PyMODINIT_FUNC PyInit__ext(void) {
    if (PyType_Ready(&PyVideoType) < 0)
        return nullptr;
    borrowed_pyref videoType((PyObject *)&PyVideoType);

    owned_pyref m = PyModule_Create(&videoLoaderModule);
    if (m.get() == nullptr)
        return nullptr;

    import_array(); // import numpy
    videoloader::init();

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
