#include "igvt_python.h"
#include "Python.h"


char *igvt_python_response;


void igvt_python_init(void);
void igvt_python_command(char *command);
static PyObject* igvt_python_stdout(PyObject *self, PyObject* args);


static PyMethodDef IGVT_Methods[] = {
    {"stdout",  igvt_python_stdout, METH_VARARGS,
     "test an external function call."},
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

  free(igvt_python_response);
}


void igvt_python_free() {
  Py_Finalize();
}


void igvt_python_command(char *command) {
  igvt_python_response[0] = 0;
  PyRun_SimpleString(command);
  strcpy(command, igvt_python_response);
}


static PyObject* igvt_python_stdout(PyObject *self, PyObject* args) {
  char *string;

  if(PyArg_ParseTuple(args, "s", &string))
    strcat(igvt_python_response, string);

  return PyInt_FromLong(42L);
}
