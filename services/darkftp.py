#!/usr/bin/env python
import sys,os
if __name__ == "__main__":
    #this is a clever hack to treat "services" as if it's in the parent branch
    #for the purposes of loading modules, etc.
    sys.path.append(os.path.join(sys.path[0],"../"))
from pipedream.environment import random_port
def passive_th(s,none):
    
    print "Sup!  PASV"
    file = clientsocket.makefile("rb")
    s = file.readline()
    print "pasv got %s" % s
cache_services = {}
def client_thread(clientsocket,none):
    file = clientsocket.makefile("rb")
    clientsocket.send("220 Hello there, darkftp here\n")
    my_chdir = "/"
    uses_passive_mode=False
    passive_port=None
    pasv_socket = None
    data_socket = clientsocket
    global cache_services
    #cache_services = {}
    while True:
        cmd = file.readline()
        print "Got %s" % cmd
        if cmd.startswith("USER"):
            clientsocket.send("331 I want a password\r\n")
        elif cmd.startswith("PASS"):
            clientsocket.send("230 Thanks for that\r\n")
        elif cmd.startswith("FEAT"):
            clientsocket.send("500 I don't know that command\r\n")
        elif cmd.startswith("PWD"):
            clientsocket.send('257 "%s"\r\n' % my_chdir)
        elif cmd.startswith("CWD"):
            my_chdir = cmd[4:].strip()
            clientsocket.send("250 OK\r\n")
        elif cmd.startswith("SYST"):
            clientsocket.send("215 UNIX Type: I\r\n") #WTF!  FIREFOX WON'T CONNECT WITHOUT UNIX HERE, I HATE YOU MOZILLA
            #print "Oh hey there",file.read()
        elif cmd.startswith("STAT"):
            clientsocket.send("211 Don't know what you think this is...\r\n")
        elif cmd.startswith("NOOP"):
            clientsocket.send("200 Hope that was what you wanted \r\n")
        elif cmd.startswith("TYPE I"):
            clientsocket.send("200 Binary flag is great!\r\n")
        elif cmd.startswith("SIZE"):
            clientsocket.send("500 SCREW YOU FIREFOX\r\n")
        elif cmd.startswith("MDTM"):
            clientsocket.send("500 SCREW YOU FIREFOX\r\n")
        elif cmd.startswith("RETR"):
            what = cmd[5:].strip()
            import os
            if my_chdir=="/": #firefox just RETRs random crap
                clientsocket.send("500 I hate you firefox\r\n")
            else:
                # figure out what service
                clientsocket.send ("125 Here's that file you wanted\r\n")
                print cache_services
                service_key = cache_services[my_chdir.split("/")[1].strip()]
                from pipedream.client import connect_to
                retrieve_path = os.path.join(my_chdir,what)
                print "I should really retrieve %s" % retrieve_path
                (port,kill) = connect_to(service_key)
                s = socket.socket(socket.AF_INET,socket.SOCK_STREAM)
                s.connect(('127.0.0.1',port))
                r = s.makefile("rb")
                pathsegments = retrieve_path.split("/")
                for pathitem in pathsegments:
                    if pathitem=="": continue #maybe it starts with /
                    elif pathitem==my_chdir.split("/")[1].strip(): continue #move past the svc part of the pat
                    if pathsegments[len(pathsegments)-1]==pathitem:
                        print "Sending get %s" % pathitem
                        s.send("get %s\n" % pathitem)
                        go_rounds=0
                        while True:
                            go_rounds += 1
                            print "reading size"
                            size = int(r.readline())
                            print "read size"
                            if size != -1:
                                buf = ""
                                while len(buf) != size:
                                    print "Going to read %d bytes" % size
                                    buf += r.read(size)
                                    print "#%d Read %d bytes" % (go_rounds,len(buf))
                                data_socket.send(buf) #why is this hanging?
                            else:
                                print "Done with this madness"
                                break
                        print "Killing runaway processes"
                        kill.terminate()
                        print "Shutting down socket"
                        from socket import SHUT_RDWR
                        data_socket.shutdown(SHUT_RDWR)
                        print "Closing socket"
                        data_socket.close()
                        print "Socket closed"
                        clientsocket.send("226 There's the file you wanted\r\n")
                        
                    else:
                        print "chdiring to %s" % pathitem
                        s.send("chdir %s\n" % pathitem)
                        t = r.readline()
                        if t != "0":
                            raise Exception ("Read %s" % t )
                        
                    
                #s.send("GET %s" % )
        elif cmd.startswith("PORT"):
            clientsocket.shutdown(0)
            clientsocket.close()
            #clientsocket.send("500 I don't support active mode\r\n")
        elif cmd.startswith("LIST"):
            dirs = []
            files = []
            if my_chdir=="/":
                from darkdc import find_all_darkdc
                s = find_all_darkdc()
                for darkdc in s:
                    dirs.append((darkdc["shortname"],1337))
                    cache_services[darkdc["shortname"]] = darkdc["key"]
            else:
                
                from time import sleep
             #   sleep(5)
            
                from pipedream.client import connect_to
                if not my_chdir.startswith("/"): #prepend with a /, makes things more standardsey
                    my_chdir = "/"+my_chdir 
                service_key = cache_services[my_chdir.split("/")[1].strip()]
                print "I should connect to %s" % service_key
                (port,kill) = connect_to(service_key) #why does this line break everything?
                s = socket.socket(socket.AF_INET,socket.SOCK_STREAM)
                s.connect(('127.0.0.1',port))
                r = s.makefile("rb")
                cd_dirs = my_chdir.split("/")[2:]
                for dir in cd_dirs:
                    print "chdirng to %s" % dir.strip()
                    s.send("chdir %s" % dir.strip())
                    r.readline()
                    r.readline()
                    print "done"
                s.send("ls\n")
                sitems = r.readline()
                try:
                    items = int(sitems)
                except:
                    logging.error("I expected to get an integer, but instead I got %s" % sitems)
                lines = []
                for i in range(0,items):
                    line = r.readline().strip()
                    print line
                    lines.append(line)
                s.shutdown(0)
                s.close()
                kill.kill()

                from pipedream.argparse import parse
                for line in lines:
                    q = parse(line)
                    print q
                    name = q[0]
                    type = q[1]
                    size = int(q[2])
                    if type=="d":
                        pass
                        dirs.append((name,size))
                    elif type=="f":
                        pass
                        files.append((name,size))
            #dirs.append(("testdir",1024))
            clientsocket.send("150 have some files\r\n")
            #if data_socket == None:
            #    print "accepting new connection"
            #    data_socket = pasv_socket.accept()

            file_type = "-rw-r--r--"
            dir_type = "drwxr-xr-x"

            for (name,size) in dirs:
                str = "%s 1 coreyross letu %13d Jan  8  1986 %s\r\n" % (dir_type,size,name)
               # str = "rw-r--r-- 1 owner group          1383 Apr 10  1997 ip.c\015\012"# % file_type
                data_socket.send(str)
                print "sent this %s" % str
            for (name,size) in files:
                str = "%s 1 coreyross letu %13d Jan  8  1986 %s\r\n" % (file_type,size,name)
                data_socket.send(str)
                print "send this %s" % str

            clientsocket.send("226 there you go\r\n")
            data_socket.shutdown(0)
            data_socket.close()
            data_socket = None
            print "that was it"
        elif cmd.startswith("PASV"):
            uses_passive_mode=True
            passive_port = random_port()
            import socket
            print passive_port
            pasv_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            pasv_socket.bind(('127.0.0.1',passive_port))
            pasv_socket.listen(3)
            from math import floor
            p1 = int(floor(passive_port/256))
            p2 = passive_port % 256
            print "Random port %d %d %d" % (passive_port,p1,p2)
            clientsocket.send("227 Entering passive mode (127,0,0,1,%d,%d)\r\n" % (p1,p2))
            print "listening..."
            (data_socket,throwaway) = pasv_socket.accept()
            pasv_socket.close()
            print "done"
        else:
            print "Don't know %s" % cmd
            clientsocket.shutdown(0)
            clientsocket.close()
        
    pass
import socket
s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
port = random_port()
s.bind(('127.0.0.1',port))
s.listen(5)
print "Bound on %d" % port

while True:
    (clientsocket,address) = s.accept()
    import thread
    thread.start_new_thread(client_thread,(clientsocket,None))

