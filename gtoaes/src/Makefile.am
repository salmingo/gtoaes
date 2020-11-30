bin_PROGRAMS=gtoaes
gtoaes_SOURCES=daemon.cpp GLog.cpp Parameter.cpp NTPClient.cpp \
               AsioIOServiceKeep.cpp AsioTCP.cpp AsioUDP.cpp \
               ATimeSpace.cpp \
               KvProtocol.cpp NonkvProtocol.cpp \
               CurlBase.cpp DatabaseCurl.cpp MessageQueue.cpp \
               ObservationPlan.cpp ObservationSystem.cpp GeneralControl.cpp gtoaes.cpp

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
BOOST_LIBS = -lboost_system-mt -lboost_thread-mt -lboost_chrono-mt -lboost_date_time-mt -lboost_filesystem-mt
gtoaes_LDADD = -lm -lcurl ${BOOST_LIBS}
if LINUX
gtoaes_LDADD += -lrt -lpthread
endif
