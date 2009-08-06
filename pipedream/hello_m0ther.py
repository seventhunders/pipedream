#!/usr/bin/env python

#first, we should find pipeman
import logging
import sys, os
for p in sys.path:
    tryt = os.path.realpath(os.path.join(p,"pipeman_src/pipeman/bin/Debug/pipeman.exe"))
  #  print tryt
    if os.path.exists(tryt):
        pipeman_path = tryt
        break
if pipeman_path==None:
    raise Exception("Can't find pipeman...")

from environment import get_setting
def redirect(f,none):
    import logging
    i = 0
    while True:
        line = f.readline().strip()
        if line=="":
            i+=1
        if i==5:
            logging.debug( "REDIRECT DIED")
            break
        logging.warning( "REDIRECT:%s" % line)
def machine_readable_regex(id):
    import re
    compilestr = "(?<=<%s>).+?(?=</%s>)" % (id,id)
    print compilestr
    return re.compile(compilestr)
def bind_mother():
    
    args = ["m0ther",
            "--rsa=%s" % get_setting("m0ther-key"),
            "--identity=%s" % get_setting("identity")]
    from subprocess import Popen, PIPE
    r = machine_readable_regex("m0therbound")

    pipeman = Popen(["mono"] + [pipeman_path] + args,stderr=PIPE,stdout=PIPE)
    while True:
            #guido's not going to like this, but

        line = pipeman.stdout.readline()
        if line=="":
            logging.warning("bind_m0ther crashed, here's why:")
            import thread
            redirect(pipeman.stderr,None)
            redirect(pipeman.stdout,None)
        
            raise Exception("Hmm, that didn't work (pipeman died?)")
        logging.debug( line)
        result = r.search(line)
        if result!=None:
            port = int(result.group(0))
            print "Connected to %d" % port
            import thread
            thread.start_new_thread(redirect,(pipeman.stderr,None))
            thread.start_new_thread(redirect,(pipeman.stdout,None))
            return (pipeman,port)
            #print "result is none..."
            

def get_mother_request():
    (process,port) = bind_mother()
    import httplib
    conn = httplib.HTTPConnection("localhost",port=port)
    return (conn,process)
    
def api_url(apiurl,data,method):
    (request,kill) = get_mother_request()
    import urllib
    apiurl += "?" + urllib.urlencode(data)
    logging.critical("invoking method %s" % method)
    request.request(method=method,url=apiurl,headers={"host":"pipem0ther.appspot.com"})
    response = request.getresponse().read()
    kill.terminate()
    return response
def api_put(apiurl,data):
    return api_body(apiurl,data,"PUT")
def api_get(apiurl,data):
    return api_url(apiurl,data,"GET")
def api_post(apiurl,data):
    return api_body(apiurl,data,"POST")
def api_body(apiurl,data,method):
    (request,kill) = get_mother_request()
    import urllib
    realdata = urllib.urlencode(data)
    headers = {"Content-Type":"application/x-www-form-urlencoded","Host":"pipem0ther.appspot.com"}
    request.request(method,apiurl,realdata,headers)
    response = request.getresponse().read()
    kill.kill()
    return response

def areyouthere():
    print api_get("/api/areyouthere",{})