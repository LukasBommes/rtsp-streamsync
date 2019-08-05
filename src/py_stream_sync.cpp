#define PY_SSIZE_T_CLEAN
#include <Python.h>
#include <numpy/arrayobject.h>

#include "stream_sync.hpp"

typedef struct {
    PyObject_HEAD
    StreamSynchronizer stream_synchronizer;
} StreamSynchronizerObject;


static int
StreamSynchronizer_init(StreamSynchronizerObject *self, PyObject *args, PyObject *kwargs)
{
    static char *kwlist[] = {"cams_list",
                             "max_initial_stream_offset",
                             "max_read_errors",
                             "frame_packet_buffer_maxsize",
                             NULL};

    // list of camera dictionaries passed as argument
    PyObject *cams_list = NULL;

    // optional arguments
    double max_initial_stream_offset = 30.0;
    int max_read_errors = 3.0;
    int frame_packet_buffer_maxsize = 1;

    std::vector<const char*> cams; // vector of camera connection urls

    // parse camera list argument
    if(!PyArg_ParseTupleAndKeywords(args, kwargs, "O!|$dii", kwlist,
        &PyList_Type, &cams_list, &max_initial_stream_offset,
        &max_read_errors, &frame_packet_buffer_maxsize))
        return -1;

    int num_cams = PyList_Size(cams_list);

    if(num_cams < 0)
        return -1;  // not a list

    for(int i = 0; i < num_cams; i++) {
        PyObject *cam_dict = PyList_GetItem(cams_list, i);
        PyObject *cam_source = PyDict_GetItemString(cam_dict, "source");
        if(!cam_source)
            return -1;  // key "source" not found
        PyObject* cam_source_utf8_str = PyUnicode_AsUTF8String(cam_source);
        char* cam_source_str = PyBytes_AsString(cam_source_utf8_str);
        cams.push_back(cam_source_str);
    }

    self->stream_synchronizer.init(cams, max_initial_stream_offset,
        max_read_errors, frame_packet_buffer_maxsize);

    return 0;
}


static void
StreamSynchronizer_dealloc(StreamSynchronizerObject *self)
{
    self->stream_synchronizer.~StreamSynchronizer();  // might be called automatically
    Py_TYPE(self)->tp_free((PyObject *) self);
}


static PyObject *
StreamSynchronizer_get_frame_packet(StreamSynchronizerObject *self, PyObject *Py_UNUSED(ignored))
{
    PyObject *frame_packet_dict = PyDict_New(); // output dictionary containing the frame packet
    if(!frame_packet_dict)
        Py_RETURN_NONE;

    SSFramePacket frame_packet = self->stream_synchronizer.get_frame_packet();

    // convert frame_packet into python dictionary
    for (std::size_t cap_id = 0; cap_id < frame_packet.size(); cap_id++) {

        int ret;

        PyObject *frame_data_dict = PyDict_New(); // dictionary containing data of one frame
        if(!frame_data_dict)
            Py_RETURN_NONE;

        // insert frame status into frame data dict
        if (frame_packet[cap_id]->frame_status == FRAME_OKAY) {
            PyObject *frame_status = PyUnicode_FromString("FRAME_OKAY");
        }
        else if (frame_packet[cap_id]->frame_status == FRAME_DROPPED) {
            PyObject *frame_status = PyUnicode_FromString("FRAME_DROPPED");
        }
        else if (frame_packet[cap_id]->frame_status == FRAME_READ_ERROR) {
            PyObject *frame_status = PyUnicode_FromString("FRAME_READ_ERROR");
        }
        else if (frame_packet[cap_id]->frame_status == CAP_BROKEN) {
            PyObject *frame_status = PyUnicode_FromString("CAP_BROKEN");
        }
        ret = PyDict_SetItemString(frame_data_dict, "frame_status", frame_status);
        if(!frame_status || ret < 0)
            Py_RETURN_NONE;
        Py_XDECREF(frame_status);

        // check if frame is valid, otherwise insert NONE into fields
        if(frame_packet[cap_id]->frame_status != FRAME_OKAY) {
            if(PyDict_SetItemString(frame_data_dict, "timestamp", Py_None) < 0)
                Py_RETURN_NONE;

            if(PyDict_SetItemString(frame_data_dict, "frame_type", Py_None) < 0)
                Py_RETURN_NONE;

            if(PyDict_SetItemString(frame_data_dict, "frame", Py_None) < 0)
                Py_RETURN_NONE;

            if(PyDict_SetItemString(frame_data_dict, "motion_vector", Py_None) < 0)
                Py_RETURN_NONE;
        }
        // if frame is valid insert actual data into the frame data dict
        else {
            PyObject *timestamp = PyFloat_FromDouble(frame_packet[cap_id]->timestamp);
            ret = PyDict_SetItemString(frame_data_dict, "timestamp", timestamp);
            if(!timestamp || ret < 0)
                Py_RETURN_NONE;
            Py_XDECREF(timestamp);

            PyObject *frame_type = PyUnicode_FromString(frame_packet[cap_id]->frame_type);
            ret = PyDict_SetItemString(frame_data_dict, "frame_type", frame_type);
            if(!frame_type || ret < 0)
                Py_RETURN_NONE;
            Py_XDECREF(frame_type);

            // convert frame buffer into numpy array
            npy_intp dims_frame[3] = {(npy_intp)(frame_packet[cap_id]->height), (npy_intp)(frame_packet[cap_id]->width), 3};
            PyObject *np_frame_nd = PyArray_SimpleNewFromData(3, dims_frame, NPY_UINT8, frame_packet[cap_id]->frame);
            PyArray_ENABLEFLAGS((PyArrayObject*)np_frame_nd, NPY_ARRAY_OWNDATA); // tell numpy it has to free the data

            // convert motion vector buffer into numpy array
            npy_intp dims_mvs[2] = {(npy_intp)frame_packet[cap_id]->num_mvs, 10};
            PyObject *motion_vectors_nd = PyArray_SimpleNewFromData(2, dims_mvs, MVS_DTYPE_NP, frame_packet[cap_id]->motion_vectors);
            PyArray_ENABLEFLAGS((PyArrayObject*)motion_vectors_nd, NPY_ARRAY_OWNDATA);

            // insert items into python dictionary
            if(PyDict_SetItemString(frame_data_dict, "frame", np_frame_nd) < 0)
                Py_RETURN_NONE;
            Py_XDECREF(np_frame_nd);

            if(PyDict_SetItemString(frame_data_dict, "motion_vector", motion_vectors_nd) < 0)
                Py_RETURN_NONE;
            Py_XDECREF(motion_vectors_nd);
        }

        // insert the frame data into the frame_packet_dict with cap_id as key
        PyObject* key = PyLong_FromLong((long)cap_id);
        if(PyDict_SetItem(frame_packet_dict, key, frame_data_dict) < 0)
            Py_RETURN_NONE;
        Py_XDECREF(frame_data_dict);
    }

    return frame_packet_dict;
}


static PyMethodDef StreamSynchronizer_methods[] = {
    {"get_frame_packet", (PyCFunction) StreamSynchronizer_get_frame_packet, METH_NOARGS, "Get the next set of synchronized frames from each stream"},
    {NULL}  // Sentinel
};


static PyTypeObject StreamSynchronizerType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "h264extract.StreamSynchronizer",
    .tp_basicsize = sizeof(StreamSynchronizerObject),
    .tp_itemsize = 0,
    .tp_dealloc = (destructor) StreamSynchronizer_dealloc,
    .tp_print = NULL,
    .tp_getattr = NULL,
    .tp_setattr = NULL,
    .tp_as_async = NULL,
    .tp_repr = NULL,
    .tp_as_number = NULL,
    .tp_as_sequence = NULL,
    .tp_as_mapping = NULL,
    .tp_hash = NULL,
    .tp_call = NULL,
    .tp_str = NULL,
    .tp_getattro = NULL,
    .tp_setattro = NULL,
    .tp_as_buffer = NULL,
    .tp_flags = Py_TPFLAGS_DEFAULT,
    .tp_doc = "Stream Synchronizer Object",
    .tp_traverse = NULL,
    .tp_clear = NULL,
    .tp_richcompare = NULL,
    .tp_weaklistoffset = 0,
    .tp_iter = NULL,
    .tp_iternext = NULL,
    .tp_methods = StreamSynchronizer_methods,
    .tp_members = NULL,
    .tp_getset = NULL,
    .tp_base = NULL,
    .tp_dict = NULL,
    .tp_descr_get = NULL,
    .tp_descr_set = NULL,
    .tp_dictoffset = 0,
    .tp_init = (initproc) StreamSynchronizer_init,
    .tp_alloc = NULL,
    .tp_new = PyType_GenericNew,
    .tp_free = NULL,
    .tp_is_gc = NULL,
    .tp_bases = NULL,
    .tp_mro = NULL,
    .tp_cache = NULL,
    .tp_subclasses = NULL,
    .tp_weaklist = NULL,
    .tp_del = NULL,
    .tp_version_tag = 0,
    .tp_finalize  = NULL,
};


static PyModuleDef streamsyncmodule = {
    PyModuleDef_HEAD_INIT,
    .m_name = "stream_sync",
    .m_doc = "Synchronizes multiple video streams based on the NTP timestamp of each frame.",
    .m_size = -1,
};


PyMODINIT_FUNC
PyInit_stream_sync(void)
{
    import_array();

    PyObject *m;
    if (PyType_Ready(&StreamSynchronizerType) < 0)
        return NULL;

    m = PyModule_Create(&streamsyncmodule);
    if (m == NULL)
        return NULL;

    Py_INCREF(&StreamSynchronizerType);
    PyModule_AddObject(m, "StreamSynchronizer", (PyObject *) &StreamSynchronizerType);
    return m;
}
