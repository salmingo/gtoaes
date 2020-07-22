bin_PROGRAMS=gtoaes
gtoaes_SOURCES=AnnexProtocol.cpp AsciiProtocol.cpp ATimeSpace.cpp daemon.cpp \
               GLog.cpp IOServiceKeep.cpp MessageQueue.cpp NTPClient.cpp ObservationSystem.cpp \
               tcpasio.cpp GeneralControl.cpp DBCurl.cpp gtoaes_hn.cpp

if DEBUG
  AM_CFLAGS = -g3 -O0 -Wall -DNDEBUG
  AM_CXXFLAGS = -g3 -O0 -Wall -DNDEBUG
else
  AM_CFLAGS = -O3 -Wall
  AM_CXXFLAGS = -O3 -Wall
endif

gtoaes_LDFLAGS = -L/usr/local/lib
BOOST_LIBS = -lboost_system-mt -lboost_chrono-mt -lboost_date_time-mt -lboost_filesystem-mt -lboost_thread-mt
gtoaes_LDADD = -lm -lpthread -lcurl ${BOOST_LIBS}
if LINUX
gtoaes_LDADD += -lrt
endif
