#include "isst_python.h"
#include "Python.h"
#undef HAVE_STAT
#undef HAVE_TM_ZONE
#include "isst.h"
#include "master.h"
#include "tienet.h"

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
static PyObject* isst_python_save(PyObject *self, PyObject* args);
static PyObject* isst_python_load(PyObject *self, PyObject* args);
static PyObject* isst_python_select(PyObject *self, PyObject* args);
static PyObject* isst_python_deselect(PyObject *self, PyObject* args);


static PyMethodDef ISST_Methods[] = {
    {"stdout", isst_python_stdout, METH_VARARGS, "redirected output."},
    {"commands", isst_python_commands, METH_VARARGS, "lists available commands."},
    {"get_camera_position", isst_python_get_camera_position, METH_VARARGS, "get camera position."},
    {"set_camera_position", isst_python_set_camera_position, METH_VARARGS, "set camera position."},
    {"get_camera_ae", isst_python_get_camera_ae, METH_VARARGS, "get camera azimuth and elevation."},
    {"set_camera_ae", isst_python_set_camera_ae, METH_VARARGS, "set camera azimuth and elevation."},
    {"get_spall_angle", isst_python_get_spall_angle, METH_VARARGS, "get spall angle."},
    {"set_spall_angle", isst_python_set_spall_angle, METH_VARARGS, "set spall angle."},
    {"save", isst_python_save, METH_VARARGS, "save shot."},
    {"load", isst_python_load, METH_VARARGS, "load shot."},
    {"select", isst_python_select, METH_VARARGS, "select geometry."},
    {"deselect", isst_python_deselect, METH_VARARGS, "deselect geometry."},
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

/* Save shot to shots.txt */
static PyObject* isst_python_save(PyObject *self, PyObject* args) {
  char *string;
  FILE *fh;

  if(PyArg_ParseTuple(args, "s", &string)) {
    strcat(isst_python_response, string);
    /* Append the data to the file shots.txt */
    fh = fopen("shots.txt", "a");

    if(!fh)
      return PyInt_FromLong(0);

    fprintf(fh, "label: %s\n", string);
    fprintf(fh, "camera_position: %f %f %f\n", isst_master_camera_position.v[0], isst_master_camera_position.v[1], isst_master_camera_position.v[2]);
    fprintf(fh, "camera_ae: %f %f\n", isst_master_camera_azimuth, isst_master_camera_elevation);
    fprintf(fh, "in_hit: %f %f %f\n", isst_master_in_hit.v[0], isst_master_in_hit.v[1], isst_master_in_hit.v[2]);
    fprintf(fh, "\n");

    fclose(fh);

    strcpy(isst_python_response, "shot saved.\n");
  }

  return PyInt_FromLong(0);
}


/* Load a Shot from shots.txt */
static PyObject* isst_python_load(PyObject *self, PyObject *args) {
  char *string, line[256];
  FILE *fh;

  if(PyArg_ParseTuple(args, "s", &string)) {
    /*
    * Given a label, try and locate the corresponding data from
    * shots.txt and load the values into memory.
    */
    fh = fopen("shots.txt", "r");

    if(!fh)
      return PyInt_FromLong(0);

    /* Search for matching label using value in string */
    while(!feof(fh)) {
      fgets(line, 256, fh);
      if(!strstr(line, "label:"))
        continue;

      if(!strstr(line, string))
        continue;

       /* Read in camera_position and camera_ae values */
       fscanf(fh, "camera_position: %f %f %f\n", &isst_master_camera_position.v[0], &isst_master_camera_position.v[1], &isst_master_camera_position.v[2]);
       fscanf(fh, "camera_ae: %f %f\n", &isst_master_camera_azimuth, &isst_master_camera_elevation);
       sprintf(line, "succesfully loaded: %s\n", string);
       strcpy(isst_python_response, line);
       return PyInt_FromLong(0);
    }

    sprintf(line, "cannot find: %s\n", string);
    strcpy(isst_python_response, line);
  }

  return PyInt_FromLong(0);
}


/* Select geometry if mesh name contains string */
static PyObject* isst_python_select(PyObject *self, PyObject *args) {
  char *string, mesg[256];
  uint16_t op;
  uint8_t c, t;

  if(PyArg_ParseTuple(args, "s", &string)) {
    op = ISST_OP_SELECT;
    memcpy(mesg, &op, 2);
    t = 1;
    memcpy(&mesg[2], &t, 1);
    c = strlen(string) + 1;
    memcpy(&mesg[3], &c, 1);
    memcpy(&mesg[4], string, c);
    tienet_master_broadcast(mesg, c + 4);
  }

  strcpy(isst_python_response, "done.\n");
  return PyInt_FromLong(0);
}


/* Deselect geometry if mesh name contains string */
static PyObject* isst_python_deselect(PyObject *self, PyObject *args) {
  char *string, mesg[256];
  uint16_t op;
  uint8_t c, t;

  if(PyArg_ParseTuple(args, "s", &string)) {
    op = ISST_OP_SELECT;
    memcpy(mesg, &op, 2);
    t = 0;
    memcpy(&mesg[2], &t, 1);
    c = strlen(string) + 1;
    memcpy(&mesg[3], &c, 1);
    memcpy(&mesg[4], string, c);
    tienet_master_broadcast(mesg, c + 4);
  }

  strcpy(isst_python_response, "done.\n");
  return PyInt_FromLong(0);
}

