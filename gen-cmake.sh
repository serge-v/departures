SRCDIR=$HOME/src/xtree/departures

mkdir -p ~/b/departuresx
cd ~/b/departuresx
cmake -G Xcode -DCMAKE_TOOLCHAIN_FILE=$SRCDIR/macports.cmake $SRCDIR

mkdir -p ~/b/departuresb
cd ~/b/departuresb
cmake -DCMAKE_TOOLCHAIN_FILE=$SRCDIR/macports.cmake $SRCDIR
