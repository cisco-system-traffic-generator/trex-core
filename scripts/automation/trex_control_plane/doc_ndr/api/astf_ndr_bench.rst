Non Drop Rate Benchmark API for ASTF
====================================

Comparing to STL, the ASTF version of the benchmarker is much simpler. Since the script receives a lower and upper bound by the user, there is no need to run a first iteration with the maximal possible traffic.
Note that also, ASTF doesn't have the ability to do this. Hence, the actual find_ndr function in ASTF is:

.. code-block:: python


    def find_ndr(self):
        self.perf_run_interval(high_bound=100, low_bound=0)



Using the MultiplierDomain class, we can convert multipliers to percents and vice versa and actually work with percents. A simple implementation of the main logic behind the binary search would be like this:

.. code-block:: python

    def perf_run_interval(self, high_bound, low_bound):

        current_run_results = ASTFNdrBenchResults(self.config)
        current_run_stats = self.results.stats
        current_run_stats['iteration'] = 0

        while current_run_stats['iteration'] <= self.config.max_iterations:
            current_run_stats['mult_p'] = float((high_bound + low_bound)) / 2.00
            current_run_stats['mult'] = self.mult_domain.convert_percent_to_mult(current_run_stats['mult_p'])
            current_run_stats.update(self.perf_run(current_run_stats['mult']))

            error_flag = current_run_stats['error_flag']
            q_full_percentage = current_run_stats['queue_full_percentage']
            valid_latency = current_run_stats['valid_latency']

            current_run_stats['mult_difference'] = abs(current_run_stats['mult_p'] - self.opt_run_stats['mult_p'])
            current_run_results.update(current_run_stats)

            if q_full_percentage <= self.config.q_full_resolution and valid_latency and not error_flag:
                if current_run_stats['mult_p'] > self.opt_run_stats['mult_p']:
                    self.opt_run_stats.update(current_run_stats)
                    if current_run_stats['mult_difference'] <= self.config.allowed_error:
                        break
                    low_bound = current_run_stats['mult_p']
                    current_run_stats['iteration'] += 1
                    if self.max_iterations_reached(current_run_stats, high_bound, low_bound):
                        break
                    else:
                        continue
                else:
                    break
            else:
                if current_run_stats['mult_difference'] <= self.config.allowed_error:
                    break
            high_bound = current_run_stats['mult_p']
            current_run_stats['iteration'] += 1
            if self.max_iterations_reached(current_run_stats, high_bound, low_bound):
                break

        self.opt_run_stats['iteration'] = current_run_stats['iteration']
        self.opt_run_stats['total_iterations'] = current_run_stats['total_iterations']
        self.opt_run_stats['rate_difference'] = 0
        return self.opt_run_stats


A version of the previous function that would allow the plugin pre and post iteration would include the following changes:

.. code-block:: python


    def perf_run_interval(self, high_bound, low_bound):

        while current_run_stats['iteration'] <= self.config.max_iterations:
            current_run_stats['mult_p'] = float((high_bound + low_bound)) / 2.00
            current_run_stats['mult'] = self.mult_domain.convert_percent_to_mult(current_run_stats['mult_p'])

            if plugin_enabled:
                self.plugin_pre_iteration(run_results=current_run_stats, **self.config.tunables)

            current_run_stats.update(self.perf_run(current_run_stats['mult']))

            if plugin_enabled:
                plugin_stop = self.plugin_post_iteration(run_results=current_run_stats, **self.config.tunables)

            current_run_results.update(current_run_stats)

            if plugin_stop:
                if self.config.verbose:
                     current_run_results.print_state("Plugin decided to stop after the iteration!", high_bound, low_bound)
                self.update_opt_stats(current_run_stats)
                break


MultiplierDomain
----------------------

.. autoclass:: trex.examples.astf.ndr_bench.MultiplierDomain
    :members: 
    :inherited-members:
    :member-order: bysource


ASTFNdrBenchConfig class
-------------------------

.. autoclass:: trex.examples.astf.ndr_bench.ASTFNdrBenchConfig
    :members: 
    :inherited-members:
    :member-order: bysource

ASTFNdrBenchResults class
--------------------------

.. autoclass:: trex.examples.astf.ndr_bench.ASTFNdrBenchResults
    :members: 
    :inherited-members:
    :member-order: bysource

ASTFNdrBench class
----------------------

.. autoclass:: trex.examples.astf.ndr_bench.ASTFNdrBench
    :members: 
    :inherited-members:
    :member-order: bysource
