#include "isst_python.h"
#include "Python.h"
#undef HAVE_STAT
#undef HAVE_TM_ZONE
#include "master.h"

char *isst_python_response;


void isst_python_init(void);
void isst_python_command(char *command);
static PyObject* isst_python_stdout(PyObject *self, PyObject* args);
static PyObject* isst_python_commands(PyObject *self, PyObject* args);
static PyObject* isst_python_get_camera_position(PyObject *self, PyObject* args);
static PyObject* isst_python_set_camera_position(PyObject *self, PyObject* args);
static PyObject* isst_python_get_origin_ae(PyObject *self, PyObject* args);
static PyObject* isst_python_set_origin_ae(PyObject *self, PyObject* args);
static PyObject* isst_python_get_camera_ae(PyObject *self, PyObject* args);
static PyObject* isst_python_set_camera_ae(PyObject *self, PyObject* args);
static PyObject* isst_python_get_spall_angle(PyObject *self, PyObject* args);
static PyObject* isst_python_set_spall_angle(PyObject *self, PyObject* args);
static PyObject* isst_python_dump(PyObject *self, PyObject* args);


static PyMethodDef ISST_Methods[] = {
    {"stdout", isst_python_stdout, METH_VARARGS, "redirected output."},
    {"commands", isst_python_commands, METH_VARARGS, "lists available commands."},
    {"get_camera_position", isst_python_get_camera_position, METH_VARARGS, "get camera position."},
    {"set_camera_position", isst_python_set_camera_position, METH_VARARGS, "set camera position."},
    {"get_camera_ae", isst_python_get_camera_ae, METH_VARARGS, "get camera azimuth and elevation."},
    {"set_camera_ae", isst_python_set_camera_ae, METH_VARARGS, "set camera azimuth and elevation."},
    {"get_spall_angle", isst_python_get_spall_angle, METH_VARARGS, "get spall angle."},
    {"set_spall_angle", isst_python_set_spall_angle, METH_VARARGS, "set spall angle."},
    {"dump", isst_python_dump, METH_VARARGS, "dump all."},
    {NULL, NULL, 0, NULL}
};


void isst_python_init() {
  Py_Initialize();


  isst_python_response = (char *)malloc(1024);

  PyImport_AddModule("adrt");
  Py_InitModule("adrt", ISST_Methods);
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


void isst_python_free() {
  free(isst_python_response);
  Py_Finalize();
}


void isst_python_code(char *code) {
  isst_python_response[0] = 0;

  PyRun_SimpleString(code);
  strcpy(code, isst_python_response);
}


/* Called once for every line */
static PyObject* isst_python_stdout(PyObject *self, PyObject* args) {
  char *string;

  if(PyArg_ParseTuple(args, "s", &string))
    strcat(isst_python_response, string);

  return PyInt_FromLong(0);
}


/* Get camera position */
static PyObject* isst_python_commands(PyObject *self, PyObject* args) {
  return Py_BuildValue("available commands:\n");
}


/* Get camera position */
static PyObject* isst_python_get_camera_position(PyObject *self, PyObject* args) {
  return Py_BuildValue("fff", isst_master_camera_position.v[0], isst_master_camera_position.v[1], isst_master_camera_position.v[2]);
}


/* Set camera position */
static PyObject* isst_python_set_camera_position(PyObject *self, PyObject* args) {
  PyArg_ParseTuple(args, "fff", &isst_master_camera_position.v[0], &isst_master_camera_position.v[1], &isst_master_camera_position.v[2]);
  return PyInt_FromLong(0);
}


/* Get camera azimuth and elevation */
static PyObject* isst_python_get_camera_ae(PyObject *self, PyObject* args) {
  return Py_BuildValue("ff", isst_master_camera_azimuth, isst_master_camera_elevation);
}


/* Set camera azimith and elevation */
static PyObject* isst_python_set_camera_ae(PyObject *self, PyObject* args) {
  PyArg_ParseTuple(args, "ff", &isst_master_camera_azimuth, &isst_master_camera_elevation);
  return PyInt_FromLong(0);
}


/* Get spall angle */
static PyObject* isst_python_get_spall_angle(PyObject *self, PyObject* args) {
  return Py_BuildValue("f", isst_master_spall_angle);
}


/* Set spall angle */
static PyObject* isst_python_set_spall_angle(PyObject *self, PyObject* args) {
  PyArg_ParseTuple(args, "f", &isst_master_spall_angle);
  return PyInt_FromLong(0);
}

/* Dump all data */
static PyObject* isst_python_dump(PyObject *self, PyObject* args) {
  char *string;
  FILE *fh;

  if(PyArg_ParseTuple(args, "s", &string)) {
    strcat(isst_python_response, string);
    /* Append the data to the file dump.txt */
    fh = fopen("dump.txt", "a");

    if(!fh)
      return PyInt_FromLong(0);

    fprintf(fh, "label: %s\n", string);
    fprintf(fh, "========================\n");
    fprintf(fh, "camera_position: %f %f %f\n", isst_master_camera_position.v[0], isst_master_camera_position.v[1], isst_master_camera_position.v[2]);
    fprintf(fh, "camera_ae: %f %f\n", isst_master_camera_azimuth, isst_master_camera_elevation);
    fprintf(fh, "\n");

    fclose(fh);
  }

  return PyInt_FromLong(0);
}
