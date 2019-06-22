import functional_general_test
from trex_scenario import CTRexScenario
import os

def is_ignored(name, ignores):
    for ignore in ignores:
        if ignore in name:
            return True
    return False

class CSyntax_Test(functional_general_test.CGeneralFunctional_Test):
    def test_python_tabs(self):
        ignores = [
            'c_dumbpreproc.py', # part of waf, uses only tabs
            'external_libs',
            '.waf',
            ]

        files_with_tabs = []
        scripts = CTRexScenario.scripts_path
        if os.path.islink(os.path.join(scripts, 'bp-sim-64')):
            path = os.path.abspath(os.path.join(scripts, os.path.pardir))
        else:
            path = os.path.abspath(scripts)

        for path, _, files in os.walk(path):
            if is_ignored(path, ignores):
                continue
            for file in files:
                if is_ignored(file, ignores):
                    continue
                if file.endswith('.py'):
                    fullpath = os.path.join(path, file)
                    with open(fullpath) as f:
                        if '\t' in f.read():
                            files_with_tabs.append(fullpath)

        if files_with_tabs:
            raise Exception('Found Python files with tabs:\n%s' % '\n'.join(files_with_tabs))
