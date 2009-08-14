#!/usr/bin/env python
import logging
#logging.basicConfig(level=logging.DEBUG)
import sys,os
if __name__ == "__main__":
    #this is a clever hack to treat "services" as if it's in the parent branch
    #for the purposes of loading modules, etc.
    sys.path.append(os.path.join(sys.path[0],"../"))

from pipedream.environment import expect_arg, get_arg

def cli_mode(host,port):
    logging.info("Connecting to %s %d" % (host,port))
    import socket
    s = socket.socket(socket.AF_INET,socket.SOCK_STREAM)
    s.connect((host,port))
    file = s.makefile("rb")
    while True:
        cmd = raw_input()
        s.send(cmd + "\n")
        if cmd.startswith("get"):
            do_get(cmd[4:],file)
        else:
            lines = int(file.readline())
            for line in range(0,lines):
                print file.readline(),
def find_all_darkdc():
    from pipedream.search import slowsearch
    result = eval(slowsearch())
    result = filter(lambda x: x["protocol"]=="darkdc",result)
    return result
def search_svc(key,param):
    from pipedream.client import connect_to
    (port,kill) = connect_to(key)
    import socket
    s = socket.socket(socket.AF_INET,socket.SOCK_STREAM)
    s.settimeout(5)
    s.connect(("localhost",port))
    s.send("search %s\n" % param)
    file = s.makefile("rb")
    try:
        l = file.readline()
    except Exception as ex:
        print "Connecting to this %s failed " % key,ex
        return []
    if l=="": return []#
    lines = int(l)
    result = []
    for i in range(0,lines):
        result.append(file.readline().strip())
    s.shutdown(0)
    s.close()
    kill.kill()
    return result
    
def do_get(filename,file):
    writefile = open(filename,"wb")
    while True:
        size = file.readline()
        data = ""
        print "reading %s" % size
        while len(data) < int(size):
            data += file.read(int(size))
        
        writefile.write(data)
        if not data:
            writefile.close()
            import sys
            sys.exit(0)
def search_for(something,walk_dir):
    index = index_stuff(walk_dir)
    import os
    searchparts = something.split(" ")
    result = []
    for q in index:
        (path,item) = q
        s = item.replace("."," ").replace("_"," ").replace(","," ")
        parts = s.lower().split(" ")
        found = True
        for sp in searchparts:
            if sp.lower() not in parts:
                found = False
                break
        if found: result.append(os.path.join(path,item))
    return result
def index_stuff(walk_dir):
    logging.info("re-indexing [hang on]...")
    import os
    result = []
    for root, dirs, files in os.walk(walk_dir):
        for file in files:
            result.append((os.path.relpath(root,walk_dir),file))
    return result
        
    
def daemon_mode():
    import logging
    
    logging.basicConfig(level=logging.DEBUG)
    logging.info("darkdc starting up...")
    
    import os
    global daemon_dir
    daemon_dir = expect_arg("share-folder")
    import socket
    import thread
    s = socket.socket(socket.AF_INET,socket.SOCK_STREAM)
    s.bind(('localhost',int(expect_arg("bindport"))))
    s.listen(5)
    while True:
        (clientsocket, address) = s.accept()
        logging.info("Accepted connection on port")
        thread.start_new_thread(client_thread,(clientsocket,None))
def super_check(where,daemon_dir,my_chdir,clientsocket):
    import os
    real_path = super_path(where,my_chdir)
    logging.debug(real_path)
    if os.path.commonprefix([daemon_dir,real_path])!=daemon_dir:
                clientsocket.send("1\n")
                clientsocket.send("nicetry\n")
                return False
    elif not os.path.exists(real_path):
                clientsocket.send("1\n")
                clientsocket.send("notfound\n")
                return False
    else:
            return True
def super_path(where,my_chdir):
    
    import os
    return os.path.realpath(os.path.join(my_chdir,where))
        
def client_thread(clientsocket,nothing):
    readfile = clientsocket.makefile("rb")
    import os
    import sys
    global daemon_dir
    my_chdir = daemon_dir
    while True:
        cmd = readfile.readline().strip()
        if cmd.startswith("quit"):
            readfile.close()
            clientsocket.shutdown(0)
            clientsocket.close()
            break
        elif cmd.startswith("ls"):
            dirs = []
            files = []
            for base in os.listdir(my_chdir):
                name = os.path.join(my_chdir,base)
                if os.path.isdir(name):
                    dirs.append(base)
                else:
                    files.append(base)
            clientsocket.send(str(len(dirs)+len(files))+"\n")
            for dir in dirs:
                clientsocket.send('"%s" d %d\n' % (dir,os.path.getsize(os.path.join(my_chdir,dir))))
            for file in files:
                clientsocket.send('"%s" f %d\n' % (file,os.path.getsize(os.path.join(my_chdir,file))))
        elif cmd.startswith("chdir"):
            where=cmd[6:]
            if super_check(where,daemon_dir,my_chdir,clientsocket):
                my_chdir = super_path(where,my_chdir)
            clientsocket.send("0\n")
        elif cmd.startswith("get"):
            where = cmd[4:]
            if super_check(where,daemon_dir,my_chdir,clientsocket):
                file = open(super_path(where,my_chdir),"rb")
                writefile = clientsocket.makefile("wb")
                
                while True:
                    data = file.read(512)
                    if len(data)==0:
                        writefile.write("-1\n")
                        pass
                    else:
                        print "writing %s" % str(len(data))
                        writefile.write(str(len(data)) + "\n")
                        writefile.write(data)

                        #clientsocket.shutdown(0)
                        #clientsocket.close()
        elif cmd.startswith("search"):
            what = cmd[7:]
            result = search_for(what,my_chdir)
            clientsocket.send(str(len(result))+"\n")
            for item in result:
                clientsocket.send(item + "\n")
                        
                   
                        
                    
        else:
            clientsocket.send("0\n")
                
            
def usage():
    print "daemon"
    print "client"
    print "slowsearch"
if __name__=="__main__":        
    import sys
    if sys.argv[1]=="daemon":
        daemon_mode()
    elif sys.argv[1]=="client":
        cli_mode("localhost",int(get_arg("port")))
    elif sys.argv[1]=="slowsearch":
        searchquery = " ".join(sys.argv[2:])
        svcs = find_all_darkdc()
        master_result = []
        for svc in svcs:
            key = svc["key"]
            results = search_svc(key,searchquery)
            for result in results:
                master_result.append(key + ":::" + result)
        for file in master_result:
            print file
        
    else:
        usage()