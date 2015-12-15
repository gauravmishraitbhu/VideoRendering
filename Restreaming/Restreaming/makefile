INCLUDES = -I/Users/apple/Vorator/casablanca/Release/include -I/usr/local/Cellar/ffmpeg/2.3/include -I/usr/local/Cellar/boost/1.56.0/include

LDFLAGS = -L/usr/local/Cellar/boost/1.56.0/lib -L/usr/local/Cellar/ffmpeg/2.3/lib -L/Users/apple/Vorator/casablanca/build.release/Binaries

LIBRARIES = -lavformat -lavcodec -lavutil -lpthread -lbz2 -liconv -lfdk-aac -lx264 -lopus -lswresample  -lboost_system -lboost_thread-mt -lboost_chrono -lcpprest 

CC = g++ -std=c++11

all:final

final:Api.o ImageSequence.o OverlayAnimation.o Utils.o
	$(CC) Api.o ImageSequence.o OverlayAnimation.o Utils.o $(LDFLAGS) $(LIBRARIES) -o final

ImageSequence.o:ImageSequence.cpp
	$(CC) -c $(INCLUDES) ImageSequence.cpp

OverlayAnimation.o:OverlayAnimation.cpp
	$(CC) -c $(INCLUDES) OverlayAnimation.cpp

Utils.o:Utils.cpp
	$(CC) -c $(INCLUDES) Utils.cpp

Api.o:Api.cpp
	$(CC) -c $(INCLUDES) Api.cpp

clean:
	rm *o
