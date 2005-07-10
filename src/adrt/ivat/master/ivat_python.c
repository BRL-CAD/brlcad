#include "ivat_python.h"
#include "Python.h"
#undef HAVE_STAT
#undef HAVE_TM_ZONE
#include "master.h"

char *ivat_python_response;


void ivat_python_init(void);
void ivat_python_command(char *command);
static PyObject* ivat_python_stdout(PyObject *self, PyObject* args);
static PyObject* ivat_python_get_camera_pos(PyObject *self, PyObject* args);
static PyObject* ivat_python_set_camera_pos(PyObject *self, PyObject* args);
static PyObject* ivat_python_get_azel(PyObject *self, PyObject* args);
static PyObject* ivat_python_set_azel(PyObject *self, PyObject* args);
static PyObject* ivat_python_get_spall_angle(PyObject *self, PyObject* args);
static PyObject* ivat_python_set_spall_angle(PyObject *self, PyObject* args);


static PyMethodDef IVAT_Methods[] = {
    {"stdout", ivat_python_stdout, METH_VARARGS, "redirected output."},
    {"get_camera_pos", ivat_python_get_camera_pos, METH_VARARGS, "get camera position."},
    {"set_camera_pos", ivat_python_set_camera_pos, METH_VARARGS, "set camera position."},
    {"get_azel", ivat_python_get_azel, METH_VARARGS, "get azimuth and elevation."},
    {"set_azel", ivat_python_set_azel, METH_VARARGS, "set azimuth and elevation."},
    {"get_spall_angle", ivat_python_get_spall_angle, METH_VARARGS, "get spall angle."},
    {"set_spall_angle", ivat_python_set_spall_angle, METH_VARARGS, "set spall angle."},
    {NULL, NULL, 0, NULL}
};


void ivat_python_init() {
  Py_Initialize();


  ivat_python_response = (char *)malloc(1024);

  PyImport_AddModule("adrt");
  Py_InitModule("adrt", IVAT_Methods);
  PyRun_SimpleString("import adrt");
  

  /* Redirect the output */
  PyRun_SimpleString("\
import sys\n\
import string\n\
class Redirect:\n\
    def __init__(self, stdout):\n\
        self.stdout = stdout\n\
    def write(self, s):\n\
        adrt.stdout(s)\n\
sys.stdout = Redirect(sys.stdout)\n\
sys.stderr = Redirect(sys.stderr)\n\
");

}


void ivat_python_free() {
  free(ivat_python_response);
  Py_Finalize();
}


void ivat_python_code(char *code) {
  ivat_python_response[0] = 0;

  PyRun_SimpleString(code);
  strcpy(code, ivat_python_response);
}


/* Called once for every line */
static PyObject* ivat_python_stdout(PyObject *self, PyObject* args) {
  char *string;

  if(PyArg_ParseTuple(args, "s", &string))
    strcat(ivat_python_response, string);

  return PyInt_FromLong(0);
}


/* Get camera position */
static PyObject* ivat_python_get_camera_pos(PyObject *self, PyObject* args) {
  return Py_BuildValue("fff", ivat_master_camera_pos.v[0], ivat_master_camera_pos.v[1], ivat_master_camera_pos.v[2]);
}


/* Set camera position */
static PyObject* ivat_python_set_camera_pos(PyObject *self, PyObject* args) {
  PyArg_ParseTuple(args, "fff", &ivat_master_camera_pos.v[0], &ivat_master_camera_pos.v[1], &ivat_master_camera_pos.v[2]);
  return PyInt_FromLong(0);
}


/* Get azimuth and elevation */
static PyObject* ivat_python_get_azel(PyObject *self, PyObject* args) {
  return Py_BuildValue("ff", ivat_master_azim, ivat_master_elev);
}


/* Set azimith and elevation */
static PyObject* ivat_python_set_azel(PyObject *self, PyObject* args) {
  PyArg_ParseTuple(args, "ff", &ivat_master_azim, &ivat_master_elev);
  return PyInt_FromLong(0);
}


/* Get spall angle */
static PyObject* ivat_python_get_spall_angle(PyObject *self, PyObject* args) {
  return Py_BuildValue("f", ivat_master_spall_angle);
}


/* Set spall angle */
static PyObject* ivat_python_set_spall_angle(PyObject *self, PyObject* args) {
  PyArg_ParseTuple(args, "f", &ivat_master_spall_angle);
  return PyInt_FromLong(0);
}

