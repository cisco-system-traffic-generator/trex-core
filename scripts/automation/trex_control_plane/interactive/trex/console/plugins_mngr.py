#/usr/bin/python
from ..utils import parsing_opts, text_tables
from ..utils.text_opts import bold
from ..common.trex_types import *
from ..common.trex_exceptions import TRexError, TRexConsoleNoAction, TRexConsoleError

from .plugins import ConsolePlugin
import glob
import imp
import argparse
import os
import traceback


class PluginsManager:
    def __init__(self, console):
        self.console = console
        self.client = console.client

        self.plugins_parser = parsing_opts.gen_parser(
                self.client,
                'plugins',
                'Show / load / use plugins',
                )

        self.plugins_subparsers = self.plugins_parser.add_subparsers(title = 'command', dest = 'plugins_command', metavar = '')
        self.plugins_subparsers.add_parser('show', help = 'Show / search for plugins')
        load_parser = self.plugins_subparsers.add_parser('load', help = 'Load (or implicitly reload) plugin by name')
        load_parser.add_arg_list(parsing_opts.PLUGIN_NAME)
        unload_parser = self.plugins_subparsers.add_parser('unload', help = 'Unload plugin by name')
        unload_parser.add_arg_list(parsing_opts.PLUGIN_NAME)
        self.loaded_plugins = {}


    def _get_plugins(self):
        plugins = {}
        cur_dir = os.path.dirname(__file__)
        for f in glob.glob(os.path.join(cur_dir, 'plugins', 'plugin_*.py')):
            module = os.path.basename(f)[:-3]
            plugin_name = module[7:]
            if plugin_name:
                plugins[plugin_name] = (f, module)
        return plugins


    def err(self, msg):
        raise Exception(msg)


    def print_err(self, msg):
        print(bold(msg))


    def _unload_plugin(self, names = None):
        try:
            if names:
                names = listify(names)
            else:
                names = list(self.loaded_plugins.keys())
            if not names:
                return
            self.client.logger.pre_cmd('Unloading plugin%s: %s' % ('s' if len(names) > 1 else '', ', '.join(names)))
            for name in names:
                if name not in self.loaded_plugins:
                    self.err('Plugin "%s" is not loaded' % name)

                if self.plugins_subparsers.has_parser(name):
                    self.plugins_subparsers.remove_parser(name)
                try:
                    self.loaded_plugins[name].plugin_unload()
                except Exception as e:
                    self.err('Error unloading plugin "%s": %s' % (name, e))
                del self.loaded_plugins[name]
        except BaseException as e:
            self.client.logger.post_cmd(False)
            self.print_err(e)
            return RC_ERR()
        self.client.logger.post_cmd(True)
        return RC_OK()


    def _load_plugin(self, name):
        plugin = self._get_plugins().get(name)
        if not plugin:
            self.err('Plugin with name "%s" not found.' % name)

        file_name, module_name = plugin
        import_path = 'trex.console.plugins.%s' % module_name

        try:
            m = imp.load_source(import_path, file_name)
        except BaseException as e:
            self.err('Exception during import of %s: %s' % (import_path, e))

        plugin_classes = {}
        for var_name, var in vars(m).items():
            try:
                if var is not ConsolePlugin and issubclass(var, ConsolePlugin):
                    plugin_classes[var_name] = var
            except:
                continue

        if not plugin_classes:
            self.err('Could not find plugins (check inheritance from ConsolePlugin) in file: %s' % file_name)
        if len(plugin_classes) > 1:
            self.err('There are several plugins withing namespace of file %s: %s' % (file_name, list(plugin_classes.keys())))

        try:
            plugin_class = list(plugin_classes.values())[0]
            plugin_class._args = {}
            plugin_class.trex_client = self.client
            plugin_obj = plugin_class()
        except BaseException as e:
            self.err('Could not initialize the plugin "%s", error: %s' % (name, e))

        try:
            plugin_obj.set_plugin_console(self.console)
            plugin_obj.plugin_load()
        except BaseException as e:
            self.err('Could not run "plugin_load()" of plugin "%s", error: %s' % (name, e))

        try:
            parser = parsing_opts.gen_parser(self.client, name, None)
            subparsers = parser.add_subparsers(title = 'commands', dest = 'subparser_command')

            for attr in dir(plugin_obj):
                if attr.startswith('do_'):
                    func = getattr(plugin_obj, attr)
                    if not (hasattr(func, '__code__') and hasattr(func, '__func__')): # check quacking
                        continue
                    descr = func.__doc__
                    if not descr:
                        self.err('Function %s does not have docstring' % attr)

                    arg_count = func.__code__.co_argcount
                    args = func.__code__.co_varnames[:arg_count]
                    if args[0] == 'self':
                        args = args[1:]
                    if not func.__code__.__doc__:
                        return

                    s = subparsers.add_parser(attr[3:], description = descr, help = descr)
                    for arg in args:
                        if arg not in plugin_obj._args:
                            self.err('Argument %s in function %s should be added by add_argument()' % (arg, attr))
                        s.add_argument(*plugin_obj._args[arg]['a'], **plugin_obj._args[arg]['k'])

            self.plugins_subparsers.add_parser(
                name,
                parents = [parser],
                help = plugin_obj.plugin_description(),
                add_help = False)
        except BaseException as e:
            import traceback
            traceback.print_exc()
            self.err('Could not create parser of plugin "%s", error: %s' % (name,e))

        self.loaded_plugins[name] = plugin_obj


    def do_plugins(self, line):
        '''Show / load / use plugins\n'''
        if not line:
            line = '-h'

        try:
            opts = self.plugins_parser.parse_args(line.split(), default_ports = [])
        except TRexConsoleNoAction:
            return
        except TRexConsoleError:
            return

        if not opts:
            return

        if opts.plugins_command == 'show':
            plugins = self._get_plugins()
            plugins.update(self.loaded_plugins)
            table = text_tables.Texttable(max_width = 200)
            table.set_cols_align(['c', 'c'])
            table.set_deco(15)
            table.header([bold('Plugin name'), bold('Loaded')])
            for plugin_name in sorted(plugins.keys()):
                table.add_row([plugin_name, 'Yes' if plugin_name in self.loaded_plugins else 'No'])
            print(table.draw())

        elif opts.plugins_command == 'load':
            plugin_name = opts.plugin_name
            if plugin_name in self.loaded_plugins:
                rc = self._unload_plugin([plugin_name])
                if not rc:
                    return

            try:
                reserved_names = ('show', 'load', 'unload')
                if plugin_name in reserved_names:
                    self.err('Error, names of plugins: "%s" are reserved.' % '", "'.join(reserved_names))
                self._load_plugin(plugin_name)
            except Exception as e:
                self.client.logger.pre_cmd('Loading plugin: %s' % plugin_name)
                self.client.logger.post_cmd(False)
                self.print_err(e)
            else:
                self.client.logger.pre_cmd('Loading plugin: %s' % plugin_name)
                self.client.logger.post_cmd(True)


        elif opts.plugins_command == 'unload':
            self._unload_plugin(opts.plugin_name)

        elif opts.plugins_command in self.loaded_plugins:
            plugin_name = opts.plugins_command
            plugin = self.loaded_plugins[plugin_name]
            if not opts.subparser_command:
                parser = self.plugins_subparsers.get_parser(plugin_name)
                assert parser, 'Internal error, could not find parser for plugin %s' % plugin_name
                parser.print_help()
                return
            func = getattr(plugin, 'do_%s' % (opts.subparser_command))
            del opts.plugins_command
            del opts.subparser_command

            try:
                func(**vars(opts))
            except TRexError as e:
                self.client.logger.debug(traceback.format_exc())
                self.print_err('%s\n' % e)
            except Exception as e:
                self.client.logger.error(traceback.format_exc())


    def complete_plugins(self, text, line, start_index, end_index):
        try:
            line_arr = line[:end_index].split()
            if len(line) > end_index and line[end_index] != ' ':
                return []
            assert line_arr[0] == 'plugins', line_arr
            parsed = 1 # skip word "plugins" in line
            parser_positionals = 0
            parser = self.plugins_subparsers
            while parsed <= len(line_arr):
                if isinstance(parser, argparse.ArgumentParser):
                    actions = parser._get_positional_actions()
                    if actions:
                        if isinstance(actions[0], argparse._SubParsersAction):
                            parser = actions[0]
                            continue
                        else:
                            if parser_positionals < len(actions):
                                parsed += 1
                                parser_positionals += 1
                                continue

                    actions_strings = []
                    for action in parser._get_optional_actions():
                        actions_strings.extend(action.option_strings)
                    cur_text = line_arr[-1]

                    if cur_text in actions_strings:
                        if text and text[-1] != ' ':
                            return ['%s ' % text]
                    elif cur_text.startswith('--'):
                        return ['%s ' % x[2:] for x in actions_strings if x.startswith(cur_text)]
                    elif cur_text.startswith('-'):
                        return ['%s ' % x[1:] for x in actions_strings if x.startswith(cur_text)]
                    else:
                        return ['%s ' % x for x in actions_strings if x.startswith(text)]

                elif isinstance(parser, argparse._SubParsersAction):
                    if parsed == len(line_arr): # "future" arg
                        return ['%s ' % x for x in parser._name_parser_map.keys()]
                    cur_text = line_arr[parsed]
                    if parsed == len(line_arr) - 1: # our last arg
                        if cur_text in parser._name_parser_map:
                            if text and text[-1] != ' ':
                                return ['%s ' % text]
                        else:
                            return ['%s ' % x for x in parser._name_parser_map.keys() if x.startswith(cur_text)]
                    elif cur_text not in parser._name_parser_map:
                        break
                    parser = parser._name_parser_map[cur_text]
                parsed += 1
        except Exception as e:
            import traceback
            traceback.print_exc()
            self.err('Exception in complete_plugins: %s' % e)
        return []


