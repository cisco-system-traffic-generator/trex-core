
import sys
import time
import copy
from collections import deque, OrderedDict

from ..trex_types import RpcCmdData, RC, RC_OK, listify

from ...utils.common import calc_bps_L1, round_float
from ...utils.text_tables import Tableable
from ...utils.text_opts import format_text, format_threshold, format_num

#use to calculate diffs relative to the previous values
# for example, BW
def calculate_diff (samples):
    total = 0.0

    weight_step = 1.0 / sum(range(0, len(samples)))
    weight = weight_step

    for i in range(0, len(samples) - 1):
        current = samples[i] if samples[i] > 0 else 1
        next = samples[i + 1] if samples[i + 1] > 0 else 1

        s = 100 * ((float(next) / current) - 1.0)

        # block change by 100%
        total  += (min(s, 100) * weight)
        weight += weight_step

    return total


# calculate by absolute values and not relatives (useful for CPU usage in % and etc.)
def calculate_diff_raw (samples):
    total = 0.0

    weight_step = 1.0 / sum(range(0, len(samples)))
    weight = weight_step

    for i in range(0, len(samples) - 1):
        current = samples[i]
        next = samples[i + 1]

        total  += ( (next - current) * weight )
        weight += weight_step

    return total


class AbstractStats(Tableable):
    """ This is an abstract class to represent a stats object """

    def __init__(self, rpc_cmd, hlen = 47):
        self.reference_stats = {}
        self.latest_stats    = {}
        self.last_update_ts  = time.time()
        self.history         = deque(maxlen = hlen)

        # RpcCmdData
        self.rpc_cmd         = rpc_cmd

    ######## methods to be derived ##########

    # generate a dictionary of stats
    def to_dict (self):
        raise NotImplementedError()

    # a subclass can add fields (calculated) to a snapshot
    def _pre_update (self, snapshot):
        return snapshot


    ######## API ##########

    def reset(self):
        """
            async reset - coroutine
        """
        rc = yield self.rpc_cmd
        if not rc:
            yield rc
        else:
            self._reset_common(rc.data())
            yield RC_OK()


    def update(self):
        """
            async update the stats object - coroutine
        """
        rc = yield self.rpc_cmd
        if not rc:
            yield rc
        else:
            self._update_common(rc.data())
            yield RC_OK()
        

    
    def reset_sync (self, rpc):
        """
            reset the stats object
        """
        rc = rpc.transmit(self.rpc_cmd.method, self.rpc_cmd.params)
        if not rc:
            return rc

        self._reset_common(rc.data())

        return RC_OK()


    def update_sync (self, rpc):
        """
            update the stats object
        """
        rc = rpc.transmit(self.rpc_cmd.method, self.rpc_cmd.params)
        if not rc:
            return rc

        self._update_common(rc.data())


    def get(self, field, format=False, suffix="", opts = None):
        value = self._get(self.latest_stats, field)
        if value == None:
            return 'N/A'

        return value if not format else format_num(value, suffix = suffix, opts = opts)


    def get_rel(self, field, format=False, suffix=""):

        # must have some refrence
        #assert(self.reference_stats)

        ref_value = self._get(self.reference_stats, field)
        latest_value = self._get(self.latest_stats, field)

        # latest value is an aggregation - must contain the value
        if latest_value == None:
            return 'N/A'

        if ref_value == None:
            ref_value = 0

        value = latest_value - ref_value

        return value if not format else format_num(value, suffix)


    # get trend for a field
    def get_trend (self, field, use_raw = False, percision = 10.0):
        if field not in self.latest_stats:
            return 0

        # not enough history - no trend
        if len(self.history) < 5:
            return 0

        # absolute value is too low 0 considered noise
        if self.latest_stats[field] < percision:
            return 0
        
        field_samples = [sample[field] for sample in list(self.history)[-5:]]

        if use_raw:
            return calculate_diff_raw(field_samples)
        else:
            return calculate_diff(field_samples)


    def get_trend_gui (self, field, show_value = False, use_raw = False, up_color = 'red', down_color = 'green'):
        v = self.get_trend(field, use_raw)

        value = abs(v)

        # use arrows if utf-8 is supported
        if sys.__stdout__.encoding == 'UTF-8':
            arrow = u'\u25b2' if v > 0 else u'\u25bc'
        else:
            arrow = ''

        if sys.version_info < (3,0):
            arrow = arrow.encode('utf-8')

        color = up_color if v > 0 else down_color

        # change in 1% is not meaningful
        if value < 1:
            return ""

        elif value > 5:

            if show_value:
                return format_text("{0}{0}{0} {1:.2f}%".format(arrow,v), color)
            else:
                return format_text("{0}{0}{0}".format(arrow), color)

        elif value > 2:

            if show_value:
                return format_text("{0}{0} {1:.2f}%".format(arrow,v), color)
            else:
                return format_text("{0}{0}".format(arrow), color)

        else:
            if show_value:
                return format_text("{0} {1:.2f}%".format(arrow,v), color)
            else:
                return format_text("{0}".format(arrow), color)


    ##################                   ##################
    ################## private functions ##################
    ##################                   ##################

    def _reset_common (self, snapshot):
        """
            reset stats object
            create a ref state
        """

        # clear any previous data
        self.reference_stats = {}
        self.history.clear()

        # call the update hook if exists
        snapshot = self._pre_update(snapshot)

        # update the latest
        self.latest_stats = snapshot

        # copy latest to refrence
        self.reference_stats = copy.deepcopy(self.latest_stats)


    def _update_common (self, snapshot):
        """
            update stats object
        """

        # call the update hook if exists
        snapshot = self._pre_update(snapshot)

        # update the latest
        self.latest_stats = snapshot

        self.history.append(self.latest_stats)


    def _get (self, src, field, default = None):
        if isinstance(field, list):
            # deep
            value = src
            for level in field:
                if not level in value:
                    return default
                value = value[level]
        else:
            # flat
            if not field in src:
                return default
            value = src[field]

        return value



class StatsBatch(object):
    """
        static class for performing batch operations
        on stats objects
    """
    @staticmethod
    def reset (stats_list, rpc):

        # init the coroutine list
        co_list = [stat.reset() for stat in stats_list]

        return StatsBatch._run_until_completion(co_list, rpc)
        

    @staticmethod
    def update (stats_list, rpc):

        # init the coroutine list
        co_list = [stat.update() for stat in stats_list]

        return StatsBatch._run_until_completion(co_list, rpc)


    @staticmethod
    def _run_until_completion (co_list, rpc):

        # generate the RPC requests
        batch = [next(co) for co in co_list]

        # transmit the RPC batch
        rc_batch = rpc.transmit_batch(batch)

        # send to coroutines
        grc = RC()
        for co, rc in zip(co_list, rc_batch):
            try:
                grc.add(co.send(rc))
                next(co)
            except StopIteration:
                pass

        return grc

