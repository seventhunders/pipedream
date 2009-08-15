#!/usr/bin/env python
import logging
logging.basicConfig(level=logging.DEBUG)
from pipedream.updates import needs_update, whats_new
if needs_update():
    print "Your pipedream version is out of date.  Please update ASAP.  Just run pipe selfupdate!"
    print "You're missing out on great stuff like " + ", ".join(whats_new())+"."
def usage():
    print "pipedream 0.1 Copyright 2009 DefyCensorship.com.  Released under MIT license."
    from pipedream.updates import what_branch, get_local_version
    print "You're running the '%s' version, commit %s" % (what_branch(),get_local_version())
    print "set"
    print "get"
    print "zebedee"
    print "makesvc"
    print "connect"
    print "slowsearch"
    print "selftest"
    print "piped"
    print "chat"
    print "selfupdate"
    print "rungateway"
    exit(1)
import sys
if len(sys.argv)==1:
    usage()
cmd = sys.argv[1]

if cmd=="set":
    c = "".join(sys.argv[2:])
    c = c.split("=")
    value = "=".join(c[1:])
    from pipedream.environment import set_setting
    set_setting(c[0],value)
elif cmd=="get":
    from pipedream.environment import get_setting
    print get_setting(sys.argv[2])
elif cmd=="rungateway":
    from pipedream.hello_m0ther import zebedee_path
    from pipedream.environment import expect_arg, get_setting
    import os
    config_file = """privatekey %s""" % (get_setting("gateway-key"))
    execute = zebedee_path + " -s -o /tmp/gateway.log -T " + expect_arg("port") + " -x 'privatekey %s' 0.0.0.0/0:80 localhost" % get_setting("gateway-key")
    os.system(execute)
    print execute
elif cmd=="zebedee":
    from pipedream.hello_m0ther import zebedee_path
    argstr = ""
    for arg in sys.argv[2:]:
        argstr += "'" + arg + "' "
    import os
    os.system(zebedee_path + " " + argstr)
elif cmd=="makesvc":
    from pipedream.environment import expect_arg, get_setting
    shortname = expect_arg("shortname")
    protocol = expect_arg("protocol")
    description = expect_arg("description")
    identity = get_setting("identity")
    from pipedream.hello_m0ther import api_post
    print api_post("/api/moresvc",{"shortname":shortname,"protocol":protocol,"description":description,"identity":identity})
elif cmd=="chat":
    from pipedream.hello_m0ther import bind_chat
    
    bind_chat()
elif cmd=="piped":
    from pipedream.piped import piped
    piped(sys.argv[2:])
    #areyouthere()
elif cmd=="connect":
    from pipedream.client import connect_to
    connect_to(sys.argv[2])
elif cmd=="slowsearch":
    from pipedream.search import slowsearch
    
    items =  eval(slowsearch())
    format = "%10s %10s %10s %32s"
    print format % ("shortname","protocol","description","key")
    for item in items:
        print format % (item["shortname"],item["protocol"],item["description"],item["key"])
    #print items
elif cmd=="selftest":
    from pipedream.hello_m0ther import ensure_m0thers_there
    print ensure_m0thers_there()
elif cmd=="selfupdate":
    from pipedream.updates import do_update
    do_update()
    
else:
    usage()
