#!/usr/bin/env python

from hello_m0ther import redirect
def connect_to(svcname,permission=None):
    from environment import get_setting, get_lan_addr, random_port
    from hello_m0ther import api_post, zebedee_path
    id = get_setting("identity")
    data = {"identity":id, "service":svcname,
            "connecting_from_uri":"tcp://" + get_lan_addr()}
    #we don't track "from" ports, although we probably should
    if permission != None:
        data["permisson"] = permission
    result = api_post("/api/connect",data)
    print result
    (key,otp,uri) = result.split("\n")
    import re
    tcphost = re.compile("(?<=tcp://).+?(?=/)")
    rport = re.compile("\d+(?=/\d+$)")
    lport = re.compile("(?<=/)\d+$")
    host = tcphost.search(uri).group(0)
    control_port = rport.search(uri).group(0)
    remote_port =  lport.search(uri).group(0)

    print host,control_port,remote_port
    from subprocess import Popen, PIPE
    connect_to = random_port()
    args = [
        "-T","%s" % control_port,
        "-x", "sharedkey %s" % otp,
        "%d:%s:%s" % (connect_to,host,remote_port)
    ]
    print "otp is %s" % otp
    pipeman = Popen([zebedee_path] + args,stdout=PIPE,stderr=PIPE)
    import thread
    thread.start_new_thread(redirect,(pipeman.stdout,None))
    thread.start_new_thread(redirect,(pipeman.stderr,None))
    print "Try connecting to localhost:%d" % connect_to
    return (connect_to,pipeman)
    
    