__doc__= ''' This script runs fedex_python over each EXPRESS schema in the test/unitary_schemas folder'''

unitary_schemas_path = '../../../../test/unitary_schemas'
fedex_python_path = '../../../../cmake-build/bin/fedex_python'

import subprocess
import glob
import os

unitary_schemas = glob.glob(os.path.join(unitary_schemas_path,'*.exp'))

for unitary_schema in unitary_schemas:
    subprocess.call([fedex_python_path,unitary_schema])