import sys, os

cur_dir = os.path.dirname(__file__)


try: # example is being run as "python -m trex.examples.emu.<example>"
    import trex.emu.api
except: # run as standalone script "python <example>"
    trex_path = os.path.join(cur_dir, os.pardir, os.pardir, os.pardir)
    sys.path.insert(0, os.path.abspath(trex_path))


try: # our usual repo
    scripts_path = os.path.abspath(os.path.join(cur_dir, os.pardir, os.pardir, os.pardir, os.pardir, os.pardir, os.pardir))
    EMU_PROFILES_PATH = os.path.join(scripts_path, 'emu')
    EXT_LIBS_PATH = os.path.join(scripts_path, 'external_libs')
    assert os.path.isdir(EMU_PROFILES_PATH)
    assert os.path.isdir(EXT_LIBS_PATH)
except: # in client package
    EMU_PROFILES_PATH = os.path.abspath(os.path.join(cur_dir, os.pardir, os.pardir, os.pardir, 'profiles', 'emu'))
    EXT_LIBS_PATH = os.path.join(cur_dir, os.pardir, os.pardir, os.pardir, os.pardir, 'external_libs') # client package path

assert os.path.isdir(EMU_PROFILES_PATH), 'Could not determine EMU profiles path'
assert os.path.isdir(EXT_LIBS_PATH), 'Could not determine external_libs path'

