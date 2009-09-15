rm cryptlib/bin/Debug/*
rm logger/bin/Debug/*
rm pipette/bin/Debug/*
rm pipeman/bin/Debug/*

cd logger
xbuild
cd ../cryptlib
xbuild
cd ../pipette
xbuild
cd ../pipeman
xbuild
