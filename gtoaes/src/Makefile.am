bin_PROGRAMS=gtoaes
gtoaes_SOURCES=daemon.cpp GLog.cpp Parameter.cpp NTPClient.cpp \
               AsioIOServiceKeep.cpp AsioTCP.cpp AsioUDP.cpp \
               ATimeSpace.cpp \
               KvProtocol.cpp NonkvProtocol.cpp \
               CurlBase.cpp DatabaseCurl.cpp \
               MessageQueue.cpp ObservationPlan.cpp ObservationSystem.cpp GeneralControl.cpp \
               gtoaes.cpp

if DEBUG
  AM_CFLAGS = -g3 -O0 -Wall -DNDEBUG
  AM_CXXFLAGS = -g3 -O0 -Wall -DNDEBUG
else
  AM_CFLAGS = -O3 -Wall
  AM_CXXFLAGS = -O3 -Wall
endif

if GWAC
  AM_CFLAGS += -DGWAC
  AM_CXXFLAGS += -DGWAC
endif

gtoaes_LDFLAGS = -L/usr/local/lib
gtoaes_LDADD = -lm -lcurl
if LINUX
BOOST_LIBS = -lboost_system-mt-x64 -lboost_thread-mt-x64 -lboost_chrono-mt-x64 -lboost_date_time-mt-x64 -lboost_filesystem-mt-x64
gtoaes_LDADD += -lrt -lpthread
endif
if OSX
BOOST_LIBS = -lboost_system-mt -lboost_thread-mt -lboost_chrono-mt -lboost_date_time-mt -lboost_filesystem-mt
endif
gtoaes_LDADD += ${BOOST_LIBS}
