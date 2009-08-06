#!/usr/bin/env python

from hello_m0ther import redirect
def connect_to(svcname,permission=None):
    from environment import get_setting, get_lan_addr
    from hello_m0ther import api_post, pipeman_path, machine_readable_regex
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
    tcpport = re.compile("(?<=/)\d+$")
    host = tcphost.search(uri).group(0)
    port = tcpport.search(uri).group(0)
    print host,port
    from subprocess import Popen, PIPE
    args = ["connectsvc",
        "--rsa=%s" % get_setting("m0ther-key"),
        "--identity=%s" % id,
        "--otp=%s" % otp,
        "--otp-key=%s" % key,
        "--remote-hostname=%s" % host,
        "--remote-port=%s" % port
    ]
    pipeman = Popen(["mono"] + [pipeman_path] + args,stdout=PIPE,stderr=PIPE)
    r = machine_readable_regex("connectbound")
    while True:
            #guido's not going to like this, but

        line = pipeman.stdout.readline()
        print line,
        result = r.search(line)
        if result!=None:
            port = int(result.group(0))
            print "Try connecting to localhost:%d" % port
            break
    import thread
    thread.start_new_thread(redirect,(pipeman.stdout,None))
    thread.start_new_thread(redirect,(pipeman.stderr,None))

    return (port,pipeman)
    
    