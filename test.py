#!/usr/bin/env python

import os
from pipedream.environment import get_setting
def spawn_pipeman(args):
    newargs = ["mono","--trace=pipette", "pipeman_src/pipeman/bin/Debug/pipeman.exe"]
    for arg in args:
        newargs.append(arg)
    from subprocess import Popen
    import sys
    Popen(["mono"] + newargs[1:],stdout=sys.stdout,stderr=sys.stderr)
    #os.spawnvp(os.P_NOWAIT,"mono",newargs)

os.system("killall mono")
#bring up gateway
gateway_key = get_setting("gateway-key")
args = [
    "gateway",
    "--rsa=%s" % gateway_key
]
spawn_pipeman(args)
"""from time import sleep
sleep(2)
mother_key = get_setting("m0ther-key")
identity = get_setting("identity")
args = [
    "m0ther",
    "--rsa=%s" % mother_key,
    "--identity=%s" % identity
]
spawn_pipeman(args)
from subprocess import Popen
Popen(["nano"])"""

#bring up a pipe to mother
#mother_key = get_setting("mother-key")
#os.system(pipeman + "m0ther --rsa=%s" % mother_key)