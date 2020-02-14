Non Drop Rate Benchmark API
===========================

We will show a simple use of the API, for example one can implement a simple version of find_ndr in the following way:

.. code-block:: python


    def simple_find_ndr():

        first_run_results = self.__find_max_rate()
        self.update_opt_stats(first_run_results)

        drop_percent = self.results.stats['drop_rate_percentage']
        q_full_percent = self.results.stats['queue_full_percentage']

        if drop_percent > self.config.pdr:
            self.results.update(self.perf_run_interval(100, 0))
        elif q_full_percent >= self.config.q_full_resolution:
            self.results.update(self.perf_run_interval(100.00, 0.00))
        else:
            # ndr found at max rate

        self.calculate_ndr_points()



Another implementation of a simple find_ndr using the optimized binary search is:

.. code-block:: python

    def simple_optimized_find_ndr():

        first_run_results = self.__find_max_rate()
        self.update_opt_stats(first_run_results)

        drop_percent = self.results.stats['drop_rate_percentage']
        q_full_percent = self.results.stats['queue_full_percentage']

        if drop_percent > self.config.pdr:
            self.results.update(self.optimized_binary_search(drop_percent, self.config.pdr, 'drop_rate_percentage'))
        elif q_full_percent >= self.config.q_full_resolution:
            self.results.update(self.optimized_binary_search(q_full_percent, self.config.q_full_resolution, 'queue_full_percentage'))
        else:
            # ndr found at max rate

        self.calculate_ndr_points()


And lastly a version that would allow the plugin pre and post iteration:

.. code-block:: python


    def simple_plugin_find_ndr():

        self.plugin_pre_iteration(finding_max_rate=True, run_results=None, **self.config.tunables)
        first_run_results = self.__find_max_rate()
        self.update_opt_stats(first_run_results)
        plugin_stop = self.plugin_post_iteration(finding_max_rate=True, run_results=first_run_results, **self.config.tunables)
        if plugin_stop:
            self.calculate_ndr_points()
            return

        drop_percent = self.results.stats['drop_rate_percentage']
        q_full_percent = self.results.stats['queue_full_percentage']

        if drop_percent > self.config.pdr:
            #Inside perf_run we take care of plugins prior and post each iteration
            self.results.update(self.perf_run_interval(100, 0)) 
        elif q_full_percent >= self.config.q_full_resolution:
            #Inside perf_run we take care of plugins prior and post each iteration
            self.results.update(self.perf_run_interval(100.00, 0.00))
        else:
            # ndr found at max rate
            pass
            
        self.calculate_ndr_points()


Let's finally see a simple implementation of the optimized binary search algorithm:

.. code-block:: python

    def simple_optimized_bin_search(self, lost_percentage, lost_allowed_percentage, stat_type):

        current_run_stats = deepcopy(self.results.stats)

        max_rate = Rate(self.results.stats['max_rate_bps'])
        assumed_ndr_rate = Rate(max_rate.convert_percent_to_rate(100 - lost_percentage))
        upper_bound = min(max_rate.rate, assumed_ndr_rate.convert_percent_to_rate(100 + self.config.opt_binary_search_percentage))
        upper_bound_percentage_of_max_rate = max_rate.convert_rate_to_percent_of_max_rate(upper_bound)

        if not max_rate.is_close(upper_bound):
            # in case we are not close to the max rate, try with the upper bound of the assumed NDR
            current_run_stats['rate_p'] = upper_bound_percentage_of_max_rate
            current_run_stats['rate_tx_bps'] = upper_bound
            current_run_stats['iteration'] = "Upper bound of assumed NDR"
            current_run_stats.update(self.perf_run(upper_bound_percentage_of_max_rate))
            self.results.update(current_run_stats)
            self.update_opt_stats(current_run_stats)

            upper_bound_lost_percentage = current_run_stats[stat_type]
            if upper_bound_lost_percentage <= lost_allowed_percentage:
                return self.perf_run_interval(100, upper_bound_percentage_of_max_rate)

        # if you got here -> upper bound of assumed ndr has too many drops
        lower_bound = assumed_ndr_rate.convert_percent_to_rate(100 - self.config.opt_binary_search_percentage)
        lower_bound_percentage_of_max_rate = max_rate.convert_rate_to_percent_of_max_rate(lower_bound)

        current_run_stats['rate_p'] = lower_bound_percentage_of_max_rate
        current_run_stats['rate_tx_bps'] = lower_bound
        current_run_stats['iteration'] = "Lower bound of assumed NDR"

        current_run_stats.update(self.perf_run(lower_bound_percentage_of_max_rate))
        self.results.update(current_run_stats)
        self.update_opt_stats(current_run_stats)

        lower_bound_lost_percentage = current_run_stats[stat_type]
        if lower_bound_lost_percentage <= lost_allowed_percentage:
            return self.perf_run_interval(upper_bound_percentage_of_max_rate, lower_bound_percentage_of_max_rate)

        # if you got here -> lower bound of assumed ndr has too many drops
        else:
            return self.perf_run_interval(lower_bound_percentage_of_max_rate, 0)


Rate class
----------------------

.. autoclass:: trex.examples.stl.ndr_bench.Rate
    :members: 
    :inherited-members:
    :member-order: bysource


NdrBenchConfig class
-------------------------

.. autoclass:: trex.examples.stl.ndr_bench.NdrBenchConfig
    :members: 
    :inherited-members:
    :member-order: bysource

NdrBenchResults class
--------------------------

.. autoclass:: trex.examples.stl.ndr_bench.NdrBenchResults
    :members: 
    :inherited-members:
    :member-order: bysource

NdrBench class
----------------------

.. autoclass:: trex.examples.stl.ndr_bench.NdrBench
    :members: 
    :inherited-members:
    :member-order: bysource
