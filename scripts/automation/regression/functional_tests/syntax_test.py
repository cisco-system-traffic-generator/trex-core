import functional_general_test
from trex import CTRexScenario
import os
import subprocess

class CSyntax_Test(functional_general_test.CGeneralFunctional_Test):
    def test_python_tabs(self):
        ignores = [
            'c_dumbpreproc.py', # part of waf, uses only tabs
            'external_libs',
            '.waf',
            ]

        path = os.path.abspath(os.path.join(CTRexScenario.scripts_path, os.path.pardir))
        cmd = 'find %s -name \*.py' % path
        for ignore in ignores:
            cmd += ' | grep -v %s' % ignore
        cmd += " | xargs -i grep '\\t' -PHn {} || :"
        out = subprocess.check_output(cmd, shell = True)
        if out:
            print('')
            print(out)
            raise Exception('Found Python files with tabs')

