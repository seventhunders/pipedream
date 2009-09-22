#!/usr/bin/env python

MONO_VERSION="2.4.2.2"


def please_dont_fail(cmd):
    import commands
    (status,output)=commands.getstatusoutput(cmd)
    if status != 0:
        print output
        raise Exception("That didn't work")

def warn(why):
    print why
    print "Are you sure you want to continue?"
    import sys
    if (raw_input("Y/N: ")!="Y"): sys.exit(1)
def make(str):
    import os
    print "Building %s..." % str,
    os.chdir(str)
    import commands
    if os.path.exists("configure"):
	please_dont_fail("./configure")
    (status,output)=commands.getstatusoutput("make")
    if status != 0:
        print output
        raise Exception("That didn't work")
    print "OK"
    os.chdir("../")
def build_zebedee():
    print "Right, let's build zebedee"
    import os
    os.chdir("zebedee_src")
    make("bzip2-1.0.3")
    make("blowfish-0.9.5a")
    make("zlib-1.2.3")
    os.chdir("zebedee-2.4.1A")
    print "Building zebedee...",
    import commands
    import sys
    if sys.platform=="darwin":
        what_os = "macosx"
    elif sys.platform=="linux2":
        what_os = "linux"
    elif sys.platform=="cygwin":
	what_os = "win32"
    else:
        raise Exception("Can't make zebedee on platform %s" % sys.platform)
    #snow leopard needs some extra lines on the make config
    from commands import getstatusoutput
    (status,output) = getstatusoutput("uname -r")
    if "10." in output:
        please_dont_fail("make OS=%s POD2MAN=/usr/bin/pod2man POD2HTML=/usr/bin/pod2html" % what_os)
    else:
        please_dont_fail("make OS=%s" % what_os)
    os.chdir("../")
    os.chdir("../")
    print "Well, that was swell"
def install_path():
    import os
    """
    prefix = "/opt/local/bin"
    folder = "pipedream"
    path = os.path.join(prefix,folder)
    print "Installing to %s (probably requires root)" % path
  
    import commands
    if os.getcwd()!=path:
        print os.getcwd(),path
        status = os.system("cp -R . %s" % (path + "/"))
        #(status,output) = commands.getstatusoutput()
        if status != 0:
            raise Exception("That didn't work.")
    print "Special fix just for malcom...",
    #os.system("cp %s/pipe.py %s/pipe" % (path,path))
    if not os.path.exists("/opt/local/bin/pipe"):
        os.system("ln -s %s/pipe.py /opt/local/bin/pipe" % path)
    os.system("chmod -R 755 %s" % path)
    os.system("chmod -R 777 %s/.git" % path)
    os.system("chmod 755 /opt/local/bin/pipe")
    print "Got your root password! (j/k)"
    os.system("chmod +x %s/pipe.py" % path)
    import sys
    """
#Check for system type. If not running Windows, it runs the unix install, otherwise it runs windows installer.

    if sys.platform!="win32":
	prefix = "/opt/local/bin"
	folder = "pipedream"
	path = os.path.join(prefix,folder)
	print "Installing to %s (probably requires root)" % path
        if not os.path.exists(path):
            pathparts = path.split("/")
            for i in range(2,len(pathparts)+1):
                dir = "/" + "/".join(pathparts[1:i])
                print "Making directory %s" % dir
                try:
                    os.mkdir(dir)
                except:
                    pass
	import commands
	if os.getcwd()!=path:
	    print os.getcwd(),path
	    status = os.system("cp -R . %s" % (path + "/"))
	    #(status,output) = commands.getstatusoutput()
	    if status != 0:
		raise Exception("That didn't work.")
	print "Special fix just for malcom...",
	#os.system("cp %s/pipe.py %s/pipe" % (path,path))
	if not os.path.exists("/opt/local/bin/pipe"):
	    os.system("ln -s %s/pipe.py /opt/local/bin/pipe" % path)
	os.system("chmod -R 755 %s" % path)
	os.system("chmod -R 777 %s/.git" % path)
	os.system("chmod 755 /opt/local/bin/pipe")
	print "Got your root password! (j/k)"
	os.system("chmod +x %s/pipe.py" % path)
    else:
	path = "C:\\Program Files\\pipedream"
	if not os.path.exists("C:\\Program Files\\pipedream"):
	    print "No path exists, making new path."
	    os.mkdir(path)
	else:
	    print "Directory Already Exists"
	if os.getcwd() != path:
	    cwd = os.getcwd()
	    print cwd
	    status = os.system('xcopy "%s" "%s" /E' % (cwd, path))
	    if status != 0:
		raise Exception("Files were not copied.")
		
def require_python():
    import sys
    print "Checking your python install...",
    if sys.version_info < (2,6,0):
        raise Exception("You need Python 2.6 to continue.  2.5 isn't good enough; sorry")
    if sys.version_info < (2,6,2):
        warn("You should really be using Python 2.6.2...")
    print "OK"
def require_mono():
    print "Checking to see if you have mono installed...",
    import commands
    if commands.getoutput("which mono")=="":
        raise Exception("Can't find mono!  You need to install mono first...")
    print "yes"
    print "Checking to see if we can find xbuild...",
    if commands.getoutput("which xbuild")=="":
        raise Exception("Can't find xbuild.  This typically ships with mono...")
    print "yes"
    import re
    v = re.compile("(?<=Mono JIT compiler version ).+(?= \(tarball)")
    monoversion = commands.getoutput("mono --version")
    result = v.search(monoversion)
    if result==None:
        warn("Something wrong with your mono version.")
    else:
        v = result.group(0)
        if v < MONO_VERSION:
            warn("Your mono version is %s, but you should really be using %s" % (v,MONO_VERSION))
print "Woohoo, you're going to install pipedream!"

def upgrade_launchd():
    import os
    print "Is piped running?",
    import commands
    (status,output) = commands.getstatusoutput("pipe piped status")
    if output=="Not running":
        print "No"
    if output=="Running OK":
        print "Yes"
        print "Shutting down piped...",
        please_dont_fail("pipe piped stop")
        print "done"
    print "uninstalling piped...",
    os.system("launchctl unload ~/Library/LaunchAgents/com.defycensorship.pipedream.piped")
    print "done"
    print "Installing piped...",
    (status,output) = commands.getstatusoutput("whoami")
    if output=="root":
        print
        real_user = raw_input("Besides root, what's your day-to-day user name? ")
    else:
        real_user=None
        
    please_dont_fail("cp com.defycensorship.pipedream.piped.plist ~/Library/LaunchAgents/")
    if real_user != None:
        please_dont_fail("chown %s:staff ~/Library/LaunchAgents/com.defycensorship.pipedream.piped.plist" % real_user)
    please_dont_fail("chmod 600 ~/Library/LaunchAgents/com.defycensorship.pipedream.piped.plist")
    please_dont_fail("su %s -c 'launchctl load ~/Library/LaunchAgents/com.defycensorship.pipedream.piped.plist'" % real_user)
    please_dont_fail("su -l %s -c 'launchctl start com.defycensorship.pipedream.piped'" % real_user)
    #please_dont_fail("")
    print "done"
def install_init():
	import os
	os.system("rm -f /etc/init.d/pipe")
	os.system("cp ./scripts/pipe /etc/init.d/pipe")
	os.system("chmod 744 /etc/init.d/pipe")
	print "I install it (the /etc/init.d/pipe file should be there)"
def upgrade_piped():
    import sys
    if sys.platform=="darwin":
        print "You're running mac, assuming you want to use launchd..."
        upgrade_launchd()
    elif sys.platform=="linux2":
        import os
        if os.path.exists("/etc/init.d"):
            print "Looks like init to me..."
            install_init()
    else:
        print "I don't know how to install piped on dubious platforms like '%s'" % sys.platform
        print "Perhaps you can tell me how?"
        print "In the meantime, make either 'pipe piped start' (preferred) or 'pipe piped daemon-mode' a startup item on your system."
        print "Also, it would be a good idea to start one of those right now..."

import sys

#require_mono()
require_python()
if sys.platform!="win32":
    build_zebedee()
install_path()
upgrade_piped()


