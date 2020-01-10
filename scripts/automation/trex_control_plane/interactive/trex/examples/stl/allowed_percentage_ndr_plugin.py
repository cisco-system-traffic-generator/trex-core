import stl_path

class CPUAllowedPercentageNDRPlugin():
    def __init__(self):
        pass

    def pre_iteration(self, finding_max_rate, run_results=None, **kwargs):
        pass

    def post_iteration(self, finding_max_rate, run_results, **kwargs):
        cpu_percentage = run_results['cpu_util']
        allowed_percentage = kwargs['allowed_percentage']
        should_stop = True if cpu_percentage > allowed_percentage else False
        return should_stop


# dynamic load of python module
def register():
    return CPUAllowedPercentageNDRPlugin()