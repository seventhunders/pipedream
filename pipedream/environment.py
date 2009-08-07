#!/usr/bin/env python
import os
setting_dir = os.path.expanduser("~/.pipedream")
def check_dir():
    if not os.path.exists(setting_dir):
        os.mkdir(setting_dir)
def get_setting(setting):
    check_dir()
    file = setting_dir + "/" + setting
    if not os.path.exists(file):
        return None
    f = open(file)
    t = f.read()
    f.close()
    return t.strip()
def set_setting(setting,value):
    check_dir()
    file = setting_dir + "/" + setting
    f = open(file,"w")
    f.write(value)
    f.close()
    
def get_arg(str):
    import sys
    for arg in sys.argv:
        if arg.startswith("--"+str+"="):
            return arg[len(str)+3:]
    return None
def expect_arg(str):
    a = get_arg(str)
    if a==None:
        raise Exception("Don't have argument %s; try with --%s=something" % (str,str))
    return a
def get_lan_addr():
    if get_setting("override-ip")!=None:
        return get_setting("override-ip")
    import socket
    return socket.gethostbyname(socket.gethostname())