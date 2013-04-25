CXXFLAGS =	-O0 -g -Wall -fmessage-length=0

OBJS =		RRMode.o master_thread.o worker_threads.o init_configure.o 

LIBS =

TARGET =	RRMode

$(TARGET):	$(OBJS)
	$(CXX) -o $(TARGET) $(OBJS) $(LIBS) -llog4cxx -lboost_system -lboost_filesystem -levent -Wl,-rpath,/usr/local/lib

all:	$(TARGET)

clean:
	rm -f $(OBJS) $(TARGET)
