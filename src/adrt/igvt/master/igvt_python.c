#include "igvt_python.h"
#include "Python.h"
#undef HAVE_STAT
#undef HAVE_TM_ZONE
#include "master.h"

char *igvt_python_response;


void igvt_python_init(void);
void igvt_python_command(char *command);
static PyObject* igvt_python_stdout(PyObject *self, PyObject* args);
static PyObject* igvt_python_get_camera_pos(PyObject *self, PyObject* args);
static PyObject* igvt_python_set_camera_pos(PyObject *self, PyObject* args);
static PyObject* igvt_python_get_azel(PyObject *self, PyObject* args);
static PyObject* igvt_python_set_azel(PyObject *self, PyObject* args);
static PyObject* igvt_python_get_spawl_angle(PyObject *self, PyObject* args);
static PyObject* igvt_python_set_spawl_angle(PyObject *self, PyObject* args);


static PyMethodDef IGVT_Methods[] = {
    {"stdout", igvt_python_stdout, METH_VARARGS, "redirected output."},
    {"get_camera_pos", igvt_python_get_camera_pos, METH_VARARGS, "get camera position."},
    {"set_camera_pos", igvt_python_set_camera_pos, METH_VARARGS, "set camera position."},
    {"get_azel", igvt_python_get_azel, METH_VARARGS, "get azimuth and elevation."},
    {"set_azel", igvt_python_set_azel, METH_VARARGS, "set azimuth and elevation."},
    {"get_spawl_angle", igvt_python_get_spawl_angle, METH_VARARGS, "get spawl angle."},
    {"set_spawl_angle", igvt_python_set_spawl_angle, METH_VARARGS, "set spawl angle."},
    {NULL, NULL, 0, NULL}
};


void igvt_python_init() {
  Py_Initialize();


  igvt_python_response = (char *)malloc(1024);

  PyImport_AddModule("adrt");
  Py_InitModule("adrt", IGVT_Methods);
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


void igvt_python_free() {
  free(igvt_python_response);
  Py_Finalize();
}


void igvt_python_code(char *code) {
  igvt_python_response[0] = 0;
  PyRun_SimpleString(code);
  strcpy(code, igvt_python_response);
}


/* Called once for every line */
static PyObject* igvt_python_stdout(PyObject *self, PyObject* args) {
  char *string;

  if(PyArg_ParseTuple(args, "s", &string))
    strcat(igvt_python_response, string);

  return PyInt_FromLong(0);
}


/* Get camera position */
static PyObject* igvt_python_get_camera_pos(PyObject *self, PyObject* args) {
  return Py_BuildValue("fff", igvt_master_camera_pos.v[0], igvt_master_camera_pos.v[1], igvt_master_camera_pos.v[2]);
}


/* Set camera position */
static PyObject* igvt_python_set_camera_pos(PyObject *self, PyObject* args) {
  PyArg_ParseTuple(args, "fff", &igvt_master_camera_pos.v[0], &igvt_master_camera_pos.v[1], &igvt_master_camera_pos.v[2]);
  return PyInt_FromLong(0);
}


/* Get azimuth and elevation */
static PyObject* igvt_python_get_azel(PyObject *self, PyObject* args) {
  return Py_BuildValue("ff", igvt_master_azim, igvt_master_elev);
}


/* Set azimith and elevation */
static PyObject* igvt_python_set_azel(PyObject *self, PyObject* args) {
  PyArg_ParseTuple(args, "ff", &igvt_master_azim, &igvt_master_elev);
  return PyInt_FromLong(0);
}


/* Get spawl angle */
static PyObject* igvt_python_get_spawl_angle(PyObject *self, PyObject* args) {
  return Py_BuildValue("f", igvt_master_spawl_angle);
}


/* Set spawl angle */
static PyObject* igvt_python_set_spawl_angle(PyObject *self, PyObject* args) {
  PyArg_ParseTuple(args, "f", &igvt_master_spawl_angle);
  return PyInt_FromLong(0);
}

