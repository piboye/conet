cd thirdparty/

echo "get gtest"
if [[ ! -d gtest ]]
then
    svn checkout http://googletest.googlecode.com/svn/trunk/ gtest
#else 
    #(cd gtest; svn update)
fi
cp GTEST_BUILD gtest/BUILD

echo "get gmock"
if [[ ! -d gmock ]]
then
    svn checkout http://googlemock.googlecode.com/svn/trunk/ gmock
#else
    #(cd gmock; svn update)
fi

cp GMOCK_BUILD gmock/BUILD

echo "get glog"
if [[ ! -d glog ]]
then
    svn checkout http://google-glog.googlecode.com/svn/trunk/  glog
    (cd glog; ./configure)
#else
    #(cd glog; svn update)
fi


#echo "get gflags"
#if [[ ! -d gflags ]]
#then
#    git clone https://code.google.com/p/gflags/
#    (cd gflags; )
##else
#    #(cd glog; svn update)
#fi
