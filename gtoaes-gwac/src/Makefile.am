bin_PROGRAMS = gtoaes
gtoaes_SOURCES = GLog.cpp daemon.cpp ioservice_keep.cpp tcp_asio.cpp \
                 msgque_base.cpp NTPClient.cpp ATimeSpace.cpp asciiproto.cpp mountproto.cpp \
                 ObservationSystem.cpp \
                 GeneralControl.cpp \
                 gtoaes.cpp

gtoaes_LDFLAGS = -L/usr/local/lib
BOOST_LIBS = -lboost_system-mt -lboost_thread-mt -lboost_date_time-mt -lboost_chrono-mt
gtoaes_LDADD = ${BOOST_LIBS} -lrt -lm -lpthread
