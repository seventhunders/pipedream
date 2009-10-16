#!/usr/bin/env python

#test
def what_branch():
    from os import chdir, popen
    from sys import path
    chdir(path[0])
    gitBranch = popen("git branch").read()
    gitBranch = gitBranch[gitBranch.find('*') + 2:]
    gitBranch = gitBranch[:gitBranch.find('\n')]
    #print gitBranch
    return gitBranch
def whats_new():
    from sys import path
    from os import chdir, popen
    lines = popen("git log HEAD..origin/" + what_branch() + " --format='%s'").read()
    return lines.split("\n")
def get_local_version():
    from sys import path
    from os import chdir, popen
    chdir(path[0])
    local = popen("git ls-remote . HEAD").read()
    local = local.split("\n")[0]
    local = local[:local.find('\t')]
    return local
def needs_update():
    from sys import path
    from os import chdir, popen
    chdir(path[0])
    whatever = popen("git fetch origin").read()
    local = get_local_version()
    remote = popen("git ls-remote . origin/" + what_branch()).read()
    remote = remote.split("\n")[0]
    remote = remote[:remote.find('\t')]
    #print local, remote
    #if local == remote:
        #print local, remote
    return local!=remote
def do_update():
    import sys
    if sys.platform != "win32":
        from commands import getstatusoutput
        (status,output) = getstatusoutput("whoami")
        if output != "root" and not sys.platform=="win32":
            print "Not root.  Giving up..."
            return
        from os import chdir
        from sys import path
        chdir(path[0])
        (status,output) = getstatusoutput("git pull --rebase")
        if status != 0:
            raise Exception(output)
        from os import system
        status = system("./superinstall.py")
        if status != 0:
            raise Exception("That didn't work")
    else:
        import os
        gitPullStatus = os.system("git pull --rebase")
        if gitPullStatus != 0:
            raise Exception("git pull failed")
        if sys.platform != "win32":
            status = os.system("./superinstall.py")
        else:
            status = os.system("superinstall.py")
        if status != 0:
            raise Exception("That didn't work")