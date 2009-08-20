#!/usr/bin/env python
import logging
running_svcs = {}
serversocket = None
from hello_m0ther import redirect
class Service:
    pipeman_proc = None
    other_proc = None
    
def piped(args):
    if args[0]=="daemon-mode":
        import logging.handlers
        handler = logging.handlers.RotatingFileHandler("/tmp/piped.log",maxBytes=1024*1024*20,backupCount=4)
        handler.setLevel(logging.DEBUG)
        logging.getLogger().addHandler(handler)
        logging.getLogger().setLevel(logging.DEBUG)
        logging.debug("Hey there")
        #logging.basicConfig(level=logging.DEBUG)
        daemon_mode()
    elif args[0]=="up":
        import socket
        s = socket.socket(socket.AF_INET,socket.SOCK_STREAM)
        s.connect(("localhost",2575))
        s.send("up %s\n" % args[1])
        print s.recv(1024)
        s.shutdown(0)
        s.close()
    elif args[0]=="down":
        import socket
        s = socket.socket(socket.AF_INET,socket.SOCK_STREAM)
        s.connect(("localhost",2575))
        s.send("down %s\n" % args[1])
        print s.recv(1024)
        s.shutdown(0)
        s.close()
    elif args[0]=="status":
        import socket
        s = socket.socket(socket.AF_INET,socket.SOCK_STREAM)
        try:
            s.connect(("localhost",2575))
            s.send("some-random-command-that-wont-work\n")
            f = s.makefile("rb")
            if f.readline().strip()!="UNKNOWN":
                raise Exception( "not sure what's going on...")
            print "Running OK"
        except Exception as ex:
            if str(ex)=="[Errno 61] Connection refused":
                print "Not running"
            else: raise ex
    elif args[0]=="start":
        from subprocess import Popen
        import sys, os
        #print sys.argv[1]
        pid = os.fork()
        if pid > 0:
            sys.exit(0)
        os.setsid()
        os.umask(0)
        #fork again
        pid = os.fork()
        if pid > 0:
            sys.exit(0)
        os.execvp(os.path.join(sys.path[0],"pipe.py"),("pipe.py",) + tuple(["piped","daemon-mode"]))
    elif args[0]=="stop":
        import socket
        s = socket.socket(socket.AF_INET,socket.SOCK_STREAM)
        s.connect(("localhost",2575))
        s.send("shutdown\n")
        s.shutdown(0)
        s.close()
        
            
        
    else:
        raise Exception("piped: no idea what you're talking about")
        
def config():
    import ConfigParser, os
    config = ConfigParser.ConfigParser()
    config.read(['piped.conf',os.path.expanduser("~/.pipedream/piped.conf")])
    return config
def shutdown_daemon():
    for s in running_svcs.keys():
        logging.info( "bringing down %s" % s)
        s = running_svcs[s]

        if s.other_proc != None:
            s.other_proc.kill()
        s.pipeman_proc.kill()
        logging.debug("done")

    import os
    os._exit(0)
    logging.critical("Why am I still here?")
def receive_signal(signum,stack):
    import signal
    if signum==signal.SIGINT:
        #serversocket.shutdown(0)
        serversocket.close()
        shutdown_daemon()
    else:
        raise Exception("not sure what to do with this signal")
    
def daemon_mode():
    logging.info("piped starting up")
    import os, sys
    os.chdir(sys.path[0])
    import socket
    import signal
    global serversocket
    signal.signal(signal.SIGINT,receive_signal)
    info = config()
    for section in info.sections():
        if maybe_config(section,"startup")=="yes":
            up_svc(section)
    serversocket = socket.socket(socket.AF_INET,socket.SOCK_STREAM)
    serversocket.bind(('localhost',2575))
    serversocket.listen(5)
    logging.info("Bound to port 2575")
    import thread
    while True:
        (clientsocket, address) = serversocket.accept()
        logging.info("Accepted connection on control port")
        thread.start_new_thread(client_thread,(clientsocket,None))

        
def client_thread(clientsocket,nothing):
    readfile = clientsocket.makefile("rb")
    cmd = readfile.readline().strip()
    readfile.close()
    args = cmd.split(" ")
    if args[0]=="up":
        svc = args[1]
        n = get_name_for_svc(svc)
        if n==None:
            clientsocket.send("NOSUCHSERVICE\n")
        else:
            logging.info("Bringing up %s" % n)
            up_svc(n)
            clientsocket.send("OK\n")
            clientsocket.close() #huh?
    elif args[0]=="down":
        svc = args[1]
        n = get_name_for_svc(svc)
        if n==None:
            clientsocket.send("NOSUCHSERVICE\n")
        else:
            logging.info("Bringing down %s" % n)
            down_svc(n)
            clientsocket.send("OK\n")
    elif args[0]=="shutdown":
        logging.info("Got request to shut down...")
        shutdown_daemon()
    else:
        clientsocket.send("UNKNOWN\n")
        logging.debug("Didn't know command %s" % cmd[0])
        
    clientsocket.shutdown(0)
    clientsocket.close()
        
def get_name_for_svc(name):
    info = config()
    if name in info.sections():
        return name
    for section in info.sections():
        n = info.get(section,"polite-name")
        if n==name:
            return section
    return None


def down_svc(name):
    #todo: let m0ther know
    logging.info("Bringing down %s" % name)
    global running_svcs
    svc = running_svcs[name]
    if svc.other_proc!=None:
        svc.other_proc.kill()
    svc.pipeman_proc.kill()
    del running_svcs[name]
def maybe_config(section,option):
    info = config()
    if info.has_option(section,option):
        return info.get(section,option)
    return None
def up_svc(name):
    global running_svcs
    from hello_m0ther import api_post, zebedee_path
    from subprocess import Popen, PIPE
    from environment import get_setting, get_lan_addr
    import sys
    from environment import random_port
    control_port = random_port()
    import os
    path_to_pipe = sys.path[0].split("/")
    path_to_pipe = "/".join(path_to_pipe[0:len(path_to_pipe)])
    logging.info("and the path is: " + path_to_pipe)

    info = config()
    
    extern = maybe_config(name,"extern")
    launch = maybe_config(name,"launch")
    svc = Service()
    if extern != None:
        (rhost,rport) = extern.split(" ")
    elif launch != None:
        from argparse import parse
        cmd = parse(launch)
        svc.other_proc=Popen(cmd,stdout=PIPE,stderr=PIPE)
        import thread
        thread.start_new_thread(redirect,(svc.other_proc.stderr,None))
        thread.start_new_thread(redirect,(svc.other_proc.stdout,None))
        rhost = "localhost"
        rport = info.get(name,"port")
        
    else:
        raise Exception("Can't understand this service type.  Try extern?")
    from commands import getstatusoutput
    (status,output) = getstatusoutput("which python")
        
    args = ["-d", #don't detach
            "-s",
            "-T","%d" % control_port,
            "-v","3",
            "-x","sharedkeygencommand \"%s\"" % (output + " " + path_to_pipe + "/acceptotk.py " + name),
            "%s:%s" % (rhost,rport)
            ]
    pipeman = Popen([zebedee_path] + args,stdout=PIPE,stderr=PIPE)
    thread.start_new_thread(redirect,(pipeman.stderr,None))
    thread.start_new_thread(redirect,(pipeman.stdout,None))
    
            #print "result is none..."
    
    
    data = {"visibility":info.get(name,"visibility"),
            "online":"YES",
            "identity":get_setting("identity"),
            "official_uri":"tcp://%s/%d/%s" % (get_lan_addr(),control_port,rport),
            "service":name}
    api_post("/api/service",data)
    
    svc.pipeman_proc = pipeman
    running_svcs[name]=svc
    
