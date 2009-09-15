Git-1.6.4-preview20090730.exe
python-2.6.2.msi
rmdir /S c:\dnet
mkdir C:\dnet
xcopy checkout.bat c:\dnet\
cd C:\dnet\
cmd.exe /c c:\dnet\checkout.bat
cd pipedream
git checkout -b windows origin/windows
superinstall.py
pause
