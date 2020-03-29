# __all__ = []

# import pkgutil
# import inspect
# import imp

# blacklist = ('emu_plugin', 'emu_plugin_base')

# for loader, module_name, is_pkg in pkgutil.walk_packages(__path__):
#     if module_name in blacklist:
#         continue

#     # module = loader.find_module(name).load_module(name)
#     import_path = 'trex.emu.emu_plugins'
    
#     # Create the plugin class name
#     plugin_name = module_name[len('emu_plugin_'):]
#     # print('module: %s' % module_name)
#     # print('plugin_name: %s' % plugin_name)
#     plugins_cls_name = "%sPlugin" % plugin_name.upper()
#     # print("plugins_cls_name: %s" % plugins_cls_name)
    
#     try:
#         module = imp.load_source('%s.%s' % (import_path, module_name), '%s/%s.py' % (__path__[0], module_name))
#     except BaseException as e:
#         raise Exception('Exception during import of %s: %s' % (import_path, e))


#     for name, value in inspect.getmembers(module):
#         if name == plugins_cls_name:
#             globals()[name] = value
#             __all__.append(name)
