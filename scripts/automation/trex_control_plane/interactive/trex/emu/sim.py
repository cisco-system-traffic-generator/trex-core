import argparse
import os
import subprocess
from collections import namedtuple

from ..utils.text_opts import red, yellow
from .trex_emu_client import EMUClient


TREX_EMU_PATH = os.path.join("trex_emu", "trex-emu")


def validate_file(filename):
    """Validate if the filename provided by the user actually exists.

    Args:
        filename (str): Filename as provided by users.

    Raises:
        argparse.ArgumentTypeError: The file doesn't exists.

    Returns:
        str: Path to Emu profile which actually is the file provided by the user if it is valid.
    """
    if not os.path.isfile(filename):
        print(red("The file {} does not exist".format(filename)))
        print(red("Please provide a valid Emu profile."))
        raise argparse.ArgumentTypeError("The file {} does not exist".format(filename))
    return filename


def load_profile(emu_profile, duration):
    """Create an EMUClient which will connect to the server. This client will be used to load the user provided
    profile to the Emu server. It will also instruct the server to shutdown after duration seconds.

    Args:
        emu_profile (str): Path to the Emu profile.
        duration (int): Duration of the simulation.

    Raises:
        Can raise exceptions when communicating with the Emu server.
    """
    c = EMUClient() # create an Emu client.
    print(yellow("Connecting to the Emu server"))
    c.connect() # connect to the Emu server.
    print(yellow("Loading your profile"))
    c.load_profile(emu_profile) # load profile to the Emu server.
    print(yellow("Profile loaded successfully"))
    print(yellow("Setting server to shut in {} sec".format(duration)))
    c.shutdown(duration) # shutdown the server in duration seconds.
    c.disconnect() # disconnect.


def start_emu(emu_cmd_args):
    """Start Emu server with the user provided arguments.

    Args:
        emu_cmd_args (list): CLI arguments for Emu.
                             For example:  ['--monitor', '--monitor-file', 'emu_out.pcap', '--verbose']
    """
    print(yellow("Starting Emu server"))
    # For simulations, we always use dummy veth.
    cmd = ["./{exe}".format(exe=TREX_EMU_PATH)] + ["--dummy-veth"] + emu_cmd_args

    # If not verbose redirect the Emu stdout to a pipe.
    stdout, stderr = (None, None) if "--verbose" in emu_cmd_args else (subprocess.PIPE, subprocess.PIPE)

    # use Popen since we need a non blocking process.
    proc = subprocess.Popen(cmd, stdout=stdout, stderr=stderr)
    return proc


def compute_emu_args(args):
    """
    Convert arguments as provided by the user to the simulator to arguments 
    for the Emu server.

    Args:
        args (dict): Arguments as parsed from Argparse.

    Returns:
        list: A list with arguments for the Emu command.
              For example:  ['--monitor', '--monitor-file', 'emu_out.pcap', '--verbose']
    """
    EmuArgument = namedtuple("EmuArgument", ["argument_name", "is_flag", "req_flag"])
    emu_args_map = {
        "output": EmuArgument(argument_name="--monitor-pcap", is_flag=False, req_flag="--monitor"),
        "json": EmuArgument(argument_name="--capture-json", is_flag=False, req_flag="--capture"),
        "verbose": EmuArgument(argument_name="--verbose", is_flag=True, req_flag=None)
    }
    emu_cmd_args = []
    for arg, emu_arg in emu_args_map.items():
        if args[arg] is None:
            # None params shouldn't be forwarded.
            continue
        if emu_arg.is_flag:
            # This is a flag, it doesn't need a value.
            if args[arg] is True:
                # It needs to be set only if it evaluates to true.
                emu_cmd_args.append(emu_arg.argument_name)
        else:
            # This is not a flag, it needs a value too.
            if emu_arg.req_flag:
                # a flag is required for this argument
                emu_cmd_args.append(emu_arg.req_flag)
            emu_cmd_args.extend([emu_arg.argument_name, str(args[arg])])
    return emu_cmd_args


def get_args(argv=None):
    """
    Get arguments for Emu simulator.

    Args:
        argv (list, optional): List of strings to parse. The default is taken from sys.argv. Defaults to None.

    Returns:
        dict: Dictionary with arguments as keys and the user chosen options as values.
    """
    emu_details = (subprocess.check_output(["{} -V".format(TREX_EMU_PATH)], shell=True)).decode("utf-8")
    sep = "-" * 80
    desc = sep + emu_details + sep
    parser = argparse.ArgumentParser(description=desc, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("-f", "--file", type=validate_file, default="emu/simple_emu.py", help="Emu profile to simulate. (default: %(default)s)")
    parser.add_argument("-o", "--output", type=str, default="emu_out.pcap", help="output file for Emu simulation. (default: %(default)s)")
    parser.add_argument("-d", "--duration", type=int, default=10, help="duration of simulation in seconds. (default: %(default)s)")
    parser.add_argument("-j", "--json", type=str, default=None, help="JSON file to capture traffic, RPC, counters etc. for debug purposes. (default: %(default)s)")
    parser.add_argument("-v", "--verbose", action="store_true", help="run Emu in verbose mode")
    args = parser.parse_args(argv)
    return vars(args)


def emu_sim():
    """
    Emu simulator logic.
    """
    args = get_args()
    emu_cmd_args = compute_emu_args(args)
    proc = start_emu(emu_cmd_args)
    duration = args["duration"]
    EXTRA_TIME_TO_FINISH = 2 # extra 2 seconds
    max_duration = duration + EXTRA_TIME_TO_FINISH

    try:
        load_profile(emu_profile=args["file"], duration=duration)
    except Exception as e:
        print(red("The following exception occurred when loading the profile:"))
        print(e)
        print(red("Terminating Emu..."))
        proc.terminate() # terminate the process gracefully by sending SIGTERM
        return

    try:
        print(yellow("Waiting for the Emu simulation to finish..."))
        proc.communicate(timeout=max_duration)
    except subprocess.TimeoutExpired:
        print(red("The Emu process didn't finish in time. Killing it"))
        proc.kill() # kill the process using SIGKILL
        proc.communicate()
    if proc.returncode == 0:
        print(yellow("The Emu simulation completed successfully with a return code of 0"))
    else:
        print(red("The Emu simulation failed with a return code of {}".format(proc.returncode)))


if __name__ == "__main__":
    emu_sim()
