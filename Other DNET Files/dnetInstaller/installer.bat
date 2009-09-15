Git-1.6.4-preview20090730.exe
python-2.6.2.msi
mkdir C:\dnet
xcopy "zebedee-2.4.1A" "C:\dnet\zebedee-2.4.1A\"
cd C:\dnet\
path = %PATH%;"C:\Program Files\Git\bin\"
git clone git://github.com/seventhunders/pipedream.git
cd pipedream\
git checkout -b windows origin/windows
cd ../
xcopy "zebedee-2.4.1A" "C:\dnet\pipedream\zebedee-2.4.1A\"
rm zebedee-2.4.1A -rf
pause
