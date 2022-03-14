#!/usr/bin/env bash
echo "get thirdparty"
if [[ ! -d thirdparty ]]
then
git clone https://github.com/piboye/thirdparty thirdparty
fi

echo "get blade"
which blade
if (( $? != 0 ))
then
    git clone --branch 1.1.3 https://github.com/chen3feng/typhoon-blade.git blade/
    (cd blade; ./install)
    . ~/bin/bladefunctions
    pip install scons==3.1.1
fi

cp -r tools/* ~/bin/

exit


echo "get gtest"
if [[ ! -d gtest ]]
then
    svn checkout http://googletest.googlecode.com/svn/trunk/ gtest
    (cd gtest; cmake -f CMakeLists.txt)
#else 
    #(cd gtest; svn update)
fi
cp GTEST_BUILD gtest/BUILD
cp -fr gtest/include/gtest include/

echo "get gmock"
if [[ ! -d gmock ]]
then
    svn checkout http://googlemock.googlecode.com/svn/trunk/ gmock
    (cd gmock; cmake -f CMakeLists.txt; make)
#else
    #(cd gmock; svn update)
fi
cp GMOCK_BUILD gmock/BUILD
cp -fr gmock/include/gmock include/
cp -fr gmock/gtest/include/gtest include/

echo "get glog"
if [[ ! -d glog ]]
then
    svn checkout http://google-glog.googlecode.com/svn/trunk/  glog
    (cd glog; ./configure)
#else
    #(cd glog; svn update)
fi
cp GLOG_BUILD glog/BUILD
cp -r glog/src/glog include/


echo "get gflags"
if [[ ! -d gflags ]]
then
    git clone https://code.google.com/p/gflags/
    (cd gflags; cmake -f CMakeLists.txt)
#else
    #(cd glog; svn update)
fi
cp GFLAGS_BUILD gflags/BUILD
cp -r gflags/include/gflags include/

