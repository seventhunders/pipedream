Git-1.6.4-preview20090730.exe
python-2.6.2.msi
rmdir /S c:\dnet
mkdir C:\dnet
xcopy checkout.bat c:\dnet\
xcopy branch.bat c:\dnet\
cd C:\dnet\
PATH = %PATH%;C:\program files\git\cmd\;
cmd.exe /c c:\dnet\checkout.bat
cd pipedream
cmd.exe /c ..\branch.bat
superinstall.py
cd c:\program files\pipdream\
pipe.py setup
pipe.pt selftest
pipe.py chat
pause
