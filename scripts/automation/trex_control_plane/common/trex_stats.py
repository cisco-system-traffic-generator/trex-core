#!/router/bin/python
import copy

class CTRexStatsManager(object):

    def __init__(self, *args):
        for stat_type in args:
            # register stat handler for each stats type
            setattr(self, stat_type, CTRexStatsManager.CSingleStatsHandler())

    def __getitem__(self, item):
        stats_obj = getattr(self,item)
        if stats_obj:
            return stats_obj.get_stats()
        else:
            return None

    class CSingleStatsHandler(object):

        def __init__(self):
            self._stats = {}

        def update(self, obj_id, stats_obj):
            assert isinstance(stats_obj, CTRexStats)
            self._stats[obj_id] = stats_obj

        def get_stats(self, obj_id=None):
            if obj_id:
                return copy.copy(self._stats.pop(obj_id))
            else:
                return copy.copy(self._stats)


class CTRexStats(object):
    def __init__(self, **kwargs):
        for k, v in kwargs.items():
            setattr(self, k, v)


class CGlobalStats(CTRexStats):
    def __init__(self, **kwargs):
        super(CGlobalStats, self).__init__(kwargs)
        pass


class CPortStats(CTRexStats):
    def __init__(self, **kwargs):
        super(CPortStats, self).__init__(kwargs)
        pass


class CStreamStats(CTRexStats):
    def __init__(self, **kwargs):
        super(CStreamStats, self).__init__(kwargs)
        pass


if __name__ == "__main__":
    pass
