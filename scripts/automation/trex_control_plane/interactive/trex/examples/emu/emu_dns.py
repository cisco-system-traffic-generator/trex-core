#!/usr/bin/python

import emu_path
from trex.emu.api import *
from pprint import pprint


LINE_LENGTH = 80
SIMPLE_DNS_PROFILE = os.path.join(emu_path.EMU_PROFILES_PATH, "simple_dns.py")


c = EMUClient(server='localhost',
              sync_port=4510,
              verbose_level= "error",
              logger=None,
              sync_timeout=None)

ns_key = EMUNamespaceKey(vport = 0)
c_key  = EMUClientKey(ns_key, Mac("00:00:00:70:00:03").V()) # Dns Server


# connect and load profile
c.connect()
c.load_profile(profile = SIMPLE_DNS_PROFILE, tunables = ['--clients', '2'])

print("*" * LINE_LENGTH)

# print tables of namespaces and clients
print("Namespaces and Clients:\n")
c.print_all_ns_clients(max_ns_show = 1, max_c_show = 10)

print("*" * LINE_LENGTH)

# Get domains
print("Getting registered domains from Dns Name Server:\n")
domains = c.dns.get_domains(c_key)
pprint(domains)

print("*" * LINE_LENGTH)

# See Entries that the profile had defined for Cisco.com
print("Getting Domain Entries for domain cisco.com:\n")
res = c.dns.get_domain_entries(c_key, "cisco.com")
pprint(res)

print("*" * LINE_LENGTH)

# Remove one entry that exists and attempt to removing one that doesn't exist!
print("Will attempt removing the following entries: (only one exists)\n")
entriesToRemove = [
    {'answer': '72.163.4.161', 'class': 'IN', 'type': 'A'}, # existing
    {'answer': '72.163.4.165', 'class': 'IN', 'type': 'A'}  # non existent
]
pprint(entriesToRemove)
c.dns.remove_domain_entries(c_key, "cisco.com", entriesToRemove)

print("*" * LINE_LENGTH)

# Verify that the entry was removed successfully
print("Getting Domain Entries for domain cisco.com after entries were removed:\n")
resAfterRemove = c.dns.get_domain_entries(c_key, "cisco.com")
pprint(resAfterRemove)

print("*" * LINE_LENGTH)

if len(res) == len(resAfterRemove) + 1: 
    print("\nEntries removed successfully from cisco.com!!!\n")
else:
    raise TRexError("Entries were not removed successfully!")

print("*" * LINE_LENGTH)

# Add one entry that exists and attempt adding twice a new entry.
print("Will attempt adding the following entries to domain cisco.com:\n")
entriesToAdd = [
    {'answer': '72.163.4.185', 'class': 'IN', 'type': 'A'}, # existing
    {'answer': 'desc=new, exp=add_twice', 'class': 'IN', 'type': 'TXT'}, # non existent
    {'answer': 'desc=new, exp=add_twice', 'class': 'IN', 'type': 'TXT'} # same as previous
]
pprint(entriesToAdd)
c.dns.add_domain_entries(c_key, "cisco.com", entriesToAdd)

print("*" * LINE_LENGTH)

# Verify that the entry was added successfully
print("Getting Domain Entries for domain cisco.com after entries were added:\n")
resAfterAdd = c.dns.get_domain_entries(c_key, "cisco.com")
pprint(resAfterAdd)

print("*" * LINE_LENGTH)

if len(resAfterRemove) == len(resAfterAdd) - 1: 
    print("\nEntries added successfully for cisco.com!!!\n")
else:
    raise TRexError("Entries were not added successfully!")

print("*" * LINE_LENGTH)

print("Adding entries for a domain that doesn't exist in the database, github.com:\n")
entriesToAdd = [
    {'answer': '140.82.121.4', 'class': 'IN', 'type': 'A'},
    {'answer': 'desc=github.com', 'class': 'IN', 'type': 'TXT'}
]
pprint(entriesToAdd)
c.dns.add_domain_entries(c_key, "github.com", entriesToAdd)

print("*" * LINE_LENGTH)

# Verify that the entry was added successfully
print("Getting Domain Entries for domain github.com after entries were added:\n")
resAfterAdd = c.dns.get_domain_entries(c_key, "github.com")
pprint(resAfterAdd)

print("*" * LINE_LENGTH)

if resAfterAdd == entriesToAdd:
    print("\nEntries added successfully for github.com!!!\n")
else:
    raise TRexError("Entries were not added successfully!")

print("*" * LINE_LENGTH)


# Get domains
print("Getting registered domains from Dns Name Server:\n")
domains = c.dns.get_domains(c_key)
pprint(domains)

print("*" * LINE_LENGTH)
