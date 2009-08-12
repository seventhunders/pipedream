#!/usr/bin/env python
import logging
logging.basicConfig(level=logging.DEBUG)

def usage():
    print "pipedream 0.1 Copyright 2009 DefyCensorship.com.  Released under MIT license."
    print "set"
    print "get"
    print "pipeman"
    print "makesvc"
    print "connect"
    print "slowsearch"
    print "selftest"
    print "piped"
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
elif cmd=="pipeman":
    from pipedream.hello_m0ther import pipeman_path
    argstr = ""
    for arg in sys.argv[2:]:
        argstr += "'" + arg + "' "
    import os
    os.system(pipeman_path + " " + argstr)
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
    from pipedream.hello_m0ther import areyouthere
    print areyouthere()
    
else:
    usage()
