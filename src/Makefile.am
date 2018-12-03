bin_PROGRAMS=gtoaes
gtoaes_SOURCES=ATimeSpace.cpp daemon.cpp GLog.cpp NTPClient.cpp IOServiceKeep.cpp tcpasio.cpp \
               AsciiProtocol.cpp MountProtocol.cpp DataTransfer.cpp MessageQueue.cpp \
               ObservationSystem.cpp ObservationSystemGWAC.cpp ObservationSystemNormal.cpp \
               GeneralControl.cpp \
               gtoaes.cpp

gtoaes_LDFLAGS = -L/usr/local/lib
BOOST_LIBS = -lboost_system-mt -lboost_thread-mt -lboost_chrono-mt  -lboost_date_time-mt -lboost_filesystem-mt
gtoaes_LDADD = -lm -lpthread -lcurl ${BOOST_LIBS}
