import astf_path


class ErrorLogger():
    def __init__(self, name, allowed_errors):
        self.iteration_counter = 0
        self.name = name
        self.client_allowed_errors = set(allowed_errors['client'])
        self.server_allowed_errors = set(allowed_errors['server'])
        self.iteration_to_mult_map = {}
        self.iteration_to_error_map = {}

    def increment_iteration_counter(self):
        self.iteration_counter += 1

    def log(self, errors):
        self.iteration_to_error_map[self.iteration_counter] = errors

    def log_multiplier(self, mult):
        self.iteration_to_mult_map[self.iteration_counter] = mult

    def should_stop(self):
        return self.iteration_counter == 5

    def invalid_errors(self, errors):
        client_errors = set(errors.get('client', []))
        server_errors = set(errors.get('server', []))
        return not (client_errors.issubset(self.client_allowed_errors)  and server_errors.issubset(self.server_allowed_errors))


class ErrorLoggingNDRPlugin():
    def __init__(self, **kwargs):
        allowed_errors = {'client': {u'tcps_conndrops': u'embryonic connections dropped'},
                          'server': {u'err_no_template': u"server can't match L7 template",
                                     u'err_no_syn': u'server first flow packet with no SYN'}
                         }
        self.logger = ErrorLogger(name="Plugin Demonstration for ASTF NDR", allowed_errors=allowed_errors)

    def pre_iteration(self, run_results=None, **kwargs):
        pass

    def post_iteration(self, run_results, **kwargs):
        if run_results['error_flag']:
            self.logger.log(run_results['errors'])
        self.logger.log_multiplier(run_results['mult'])
        self.logger.increment_iteration_counter()
        should_stop = self.logger.should_stop()
        invalid_errors = self.logger.invalid_errors(run_results['errors'])
        return should_stop, invalid_errors


# dynamic load of python module
def register():
    return ErrorLoggingNDRPlugin()
