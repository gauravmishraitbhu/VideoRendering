INCLUDES =  -I/usr/local/Cellar/ffmpeg/2.8/include  -I/Library/Java/JavaVirtualMachines/jdk1.7.0_65.jdk/Contents/Home/include -I/Library/Java/JavaVirtualMachines/jdk1.7.0_65.jdk/Contents/Home/include/darwin


LDFLAGS =   -L/usr/local/Cellar/ffmpeg/2.8/lib



LIBRARIES = -lavformat -lavcodec -lswscale -lswresample -lavutil -lpthread  -lx264


CC = g++ -std=c++11 -g -O0 

all:clean final

final:Api.o ImageSequence.o OverlayAnimation.o Utils.o ImageFrame.o RenderJob.o
	$(CC)  -o final.so Api.o ImageSequence.o OverlayAnimation.o Utils.o  ImageFrame.o RenderJob.o $(LDFLAGS) $(LIBRARIES) -shared



ImageFrame.o:ImageFrame.cpp
	$(CC) -c $(INCLUDES) ImageFrame.cpp

ImageSequence.o:ImageSequence.cpp
	$(CC) -c $(INCLUDES) ImageSequence.cpp

OverlayAnimation.o:OverlayAnimation.cpp
	$(CC) -c $(INCLUDES) OverlayAnimation.cpp

Utils.o:Utils.cpp
	$(CC) -c $(INCLUDES) Utils.cpp

Api.o:Api.cpp
	$(CC) -c $(INCLUDES) Api.cpp
RenderJob.o : RenderJob.cpp
	$(CC) -c $(INCLUDES) RenderJob.cpp

clean:
	rm -rf Api.o ImageSequence.o OverlayAnimation.o Utils.o GlobalData.o ImageFrame.o RenderJob.o
