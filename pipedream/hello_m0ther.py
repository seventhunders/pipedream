#!/usr/bin/env python
mother_bind_port=1337
known_gateways = [("69.197.162.44",1234)]
#first, we should find pipeman
import logging
import sys, os
zebedee_path = None
for p in sys.path:
    tryt = os.path.realpath(os.path.join(p,"zebedee_src/zebedee-2.4.1A/zebedee"))
  #  print tryt
    if os.path.exists(tryt):
        zebedee_path = tryt
        break
if zebedee_path==None:
    raise Exception("Can't find zebedee...")

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
            f.close()
            break
        logging.warning( "REDIRECT:%s" % line)


    
def try_gateway(ip,port):
    import os, signal
    logging.info("Trying %s %d" % (ip,port))
    from environment import get_setting, set_setting
    try:
        lastpid = int(get_setting("last-mother-pid"))
        os.kill(lastpid,signal.SIGKILL)
    except:
        logging.warning("Couldn't shutdown last mother pid.")
    from subprocess import Popen
    args = [
        zebedee_path,
        "-T %d" % port,
        '-x',
        "checkidfile %s" % os.path.expanduser("~/.pipedream/m0ther-key"),
        '-o',
        '/tmp/pipe-m0ther.log',
        ip,
        "1337:pipem0ther.appspot.com:80"
    ]
    print " ".join(args)
    motherpid = Popen(args)
    motherpid.wait()
    #get the real motherpid, because zebedee detached
    #on osx it's just motherpid++, but that's not guaranteed
    #for posix.  Of course, on Windows the entire exercise is fail, good luck
    f = open("/tmp/pipe-m0ther.log")
    result = f.read()
    f.close()
    #grab the last line
    lines = result.split("\n")
    last = lines[len(lines)-2]
    print "last is %s" % last
    import re
    pidregex = re.compile("(?<=zebedee\()\d+")
    print last
    rmotherpid = pidregex.search(last).group(0)
    print "Real motherpid is %s" % rmotherpid
    set_setting("last-mother-pid",rmotherpid)
    try:
        return api_url("/api/areyouthere",{},"GET",ensure=False)=="YES"
    except:
        print sys.exc_info()
        print "That gateway wouldn't work"
        from os import kill
        from signal import SIGKILL
        try:
            pass
            kill(int(rmotherpid), SIGKILL)
        except:
            print "couldn't kill runaway zebedee process"
    return False
def ensure_m0thers_there():
    try:
        if api_url("/api/areyouthere",{},"GET",ensure=False)=="YES": return True
    except Exception as ex:
        logging.info("Unable to connect right off the bat: %s." % ex)
    for (ip,port) in known_gateways:
        if try_gateway(ip,port): return True
    raise Exception("Couldn't find m0ther")

def api_get(apiurl,data):
    return api_url(apiurl,data,"GET")
def get_mother_request(ensure=True):
    if ensure:
        ensure_m0thers_there()
    import httplib
    return httplib.HTTPConnection(host="localhost",port=mother_bind_port)
def api_url(apiurl,data,method,ensure=True):
    request = get_mother_request(ensure)
    import urllib
    apiurl += "?" + urllib.urlencode(data)
    logging.critical("invoking method %s" % method)
    request.request(method=method,url=apiurl,headers={"host":"pipem0ther.appspot.com"})
    response = request.getresponse().read()
    request.close()
    return response
def api_post(apiurl,data):
    return api_body(apiurl,data,"POST")
def api_body(apiurl,data,method):
    request = get_mother_request()
    import urllib
    realdata = urllib.urlencode(data)
    headers = {"Content-Type":"application/x-www-form-urlencoded","Host":"pipem0ther.appspot.com"}
    request.request(method,apiurl,realdata,headers)
    response = request.getresponse().read()
    request.close()
    return response


from BaseHTTPServer import BaseHTTPRequestHandler
class TransparentMother(BaseHTTPRequestHandler):
    def do_GET(self):
        if self.path=="/favico.ico":
            self.send_response(404)
        else:
            self.send_response(200)
        self.send_header('Content-type','text/html')
        self.end_headers()
        import urlparse
        from cgi import parse_qs
        #o = urlparse.urlparse(self.path)
        #print o
        print self.path
        pathparts = self.path.split("?")
        if len(pathparts)>=2:
            data = parse_qs(self.path.split("?")[1])
            real_data = {}
            for key in data:
                real_data[key]=(data[key][0])
            data = real_data
        else:
            data = {}
        print data
        print "about tu query"
        self.wfile.write(api_get(pathparts[0],data))
        print "done"
        #print self.path
    def do_POST(self):
        self.send_response(200)
        self.send_header('Content-type','text/html')
        self.end_headers()
        length = int(self.headers.getheader('content-length'))
        import cgi
            
        pdict = cgi.parse_qs(self.rfile.read(length))
        real_data = {}
        for key in pdict:
            real_data[key]=(pdict[key][0])
        pdict = real_data
        print pdict
        import urlparse
        self.wfile.write(api_post(self.path,pdict))
def transparent_mother():
    from BaseHTTPServer import HTTPServer
    server = HTTPServer(('127.0.0.1',3547),TransparentMother)
    server.serve_forever()
    
def bind_chat():
    #transparent_mother()
    import urllib2
    from environment import get_setting
    
    identity = get_setting("identity")
    if api_get("/chat/nickname",{"identity":identity})=="NOTSET":
        print "You don't have a nickname set.  Choose one below.  This is PERMANENT and cannot be changed"
        nickname = raw_input("Nickname: ")
        print api_post("/chat/nickname",data={"identity":identity,"nickname":nickname})
    openurl = "http://localhost:3547/chat/?identity=%s" % identity
    import thread
    print "Right, try connecting to %s" % openurl
    from environment import super_open
    super_open(openurl)
    transparent_mother()
    
"""def machine_readable_regex(id):
    import re
    compilestr = "(?<=<%s>).+?(?=</%s>)" % (id,id)
    print compilestr
    return re.compile(compilestr)
def bind_chat():
    #transparent_mother()
    import urllib2
    from environment import get_setting
    
    identity = get_setting("identity")
    if api_get("/chat/nickname",{"identity":identity})=="NOTSET":
        print "You don't have a nickname set.  Choose one below.  This is PERMANENT and cannot be changed"
        nickname = raw_input("Nickname: ")
        print api_post("/chat/nickname",data={"identity":identity,"nickname":nickname})
    openurl = "http://localhost:3547/chat/?identity=%s" % identity
    import thread
    print "Right, try connecting to %s" % openurl
    from environment import super_open
    super_open(openurl)
    transparent_mother()
        
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
    request.close()
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
    request.close()
    return response

def areyouthere():
    print api_get("/api/areyouthere",{})
    
from BaseHTTPServer import BaseHTTPRequestHandler
class TransparentMother(BaseHTTPRequestHandler):
    def do_GET(self):
        if self.path=="/favico.ico":
            self.send_response(404)
        else:
            self.send_response(200)
        self.send_header('Content-type','text/html')
        self.end_headers()
        import urlparse
        from cgi import parse_qs
        #o = urlparse.urlparse(self.path)
        #print o
        print self.path
        pathparts = self.path.split("?")
        if len(pathparts)>=2:
            data = parse_qs(self.path.split("?")[1])
            real_data = {}
            for key in data:
                real_data[key]=(data[key][0])
            data = real_data
        else:
            data = {}
        print data
        print "about tu query"
        self.wfile.write(api_get(pathparts[0],data))
        print "done"
        #print self.path
    def do_POST(self):
        self.send_response(200)
        self.send_header('Content-type','text/html')
        self.end_headers()
        length = int(self.headers.getheader('content-length'))
        import cgi
            
        pdict = cgi.parse_qs(self.rfile.read(length))
        real_data = {}
        for key in pdict:
            real_data[key]=(pdict[key][0])
        pdict = real_data
        print pdict
        import urlparse
        self.wfile.write(api_post(self.path,pdict))
def transparent_mother():
    from BaseHTTPServer import HTTPServer
    server = HTTPServer(('127.0.0.1',3547),TransparentMother)
    server.serve_forever()"""