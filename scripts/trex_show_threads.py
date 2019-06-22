#!/usr/bin/env python
# -*- coding: utf-8 -*-

import os

def read_task_stats (task_path):

    # files
    status = task_path + "/status"
    stat  = task_path + "/stat"

    stats_dict = {}
    for line in open(status, 'r').readlines():
        name, value = line.split(':', 1)
        stats_dict[name.strip().lower()] = value.strip()

    stat_data = open(stat, 'r').readline().split()

    stats_dict['last_sched_cpu'] = stat_data[-14]
    stats_dict['priority'] = stat_data[17] if stat_data[43] == '0' else 'RT'
    
    return stats_dict


def show_threads (pid):
    process_dir = "/proc/{0}/task".format(pid)
    task_paths = ["{0}/{1}".format(process_dir, task) for task in os.listdir(process_dir)]

    
    header = [ 'Task Name', 'PID', 'Priority', 'Allowed CPU', 'Last Sched CPU', 'Asked Ctx Switch', 'Forced Ctx Switch']
    for x in header:
        print('{:^20}'.format(x)),
    print("")

    tasks = []
    for task_path in task_paths:
        task = read_task_stats(task_path)
        tasks.append(task)

    tasks = sorted(tasks, key = lambda x: x['cpus_allowed_list'])
    for task in tasks:
        # name
        print("{:<20}".format(task['name'])),
        print("{:^20}".format(task['pid'])),
        print("{:^20}".format(task['priority'])),
        print("{:^20}".format(task['cpus_allowed_list'])),
        print("{:^20}".format(task['last_sched_cpu'])),
        print("{:^20}".format(task['voluntary_ctxt_switches'])),
        print("{:^20}".format(task['nonvoluntary_ctxt_switches'])),
        print("")

def isnum (x):
    try:
        int(x)
        return True
    except ValueError:
        return False


def find_trex_pid ():
    procs = [x for x in os.listdir('/proc/') if isnum(x)]
    for proc in procs:
        cmd = open('/proc/{0}/{1}'.format(proc, 'comm')).readline()
        if cmd.startswith('_t-rex-64'):
            return proc

    return None

def main ():
    trex_pid = find_trex_pid()
    if trex_pid is None:
        print("Unable to find Trex PID")
        exit(1)

    print("\nTrex PID on {0}\n".format(trex_pid))
    show_threads(trex_pid)



if __name__ == '__main__':
     main()

