CC = gcc
CPP = g++

CFLAGS = -O2 \
	-DG_DISABLE_DEPRECATED 	 	\
	-DGDK_DISABLE_DEPRECATED 	\
	-DGDK_PIXBUF_DISABLE_DEPRECATED \
	-DGTK_DISABLE_DEPRECATED \
	

LDFLAGS = 
#OBJECTS = sample.o stdafx.o color.o
OBJECTS = sample.o color.o verifier.o eyesDetector.o faceDetector.o tracker.o utils.o usermanager.o detector.o 
TARGET = faceauth

#INCLUDES = `pkg-config glib-2.0 gtk+-2.0 libgnome-2.0 libgnomeui-2.0 dbus-1 dbus-glib-1 gthread-2.0 --cflags`
#LIBS= `pkg-config glib-2.0 gtk+-2.0 libgnome-2.0 libgnomeui-2.0 dbus-1 dbus-glib-1 gthread-2.0 --libs`
#INCLUDES = `pkg-config glib-2.0 gtk+-2.0 gthread-2.0 --cflags`
#LIBS= `pkg-config glib-2.0 gtk+-2.0 gthread-2.0 --libs`
INCLUDES = `pkg-config gtk+-2.0 --cflags`
LIBS= `pkg-config gtk+-2.0 --libs` -lopencv_core -lopencv_imgproc -lopencv_objdetect

.SUFFIXES: .c .cpp

.c.o:
	$(CPP) -c $(CFLAGS) $(INCLUDES) -o $@ $< 
.cpp.o:
	$(CPP) -c $(CFLAGS) $(INCLUDES) -o $@ $<
	
all: $(OBJECTS)
#	$(CPP) $(LDFLAGS) $(LIBS) $(OBJECTS) -L./ -lcameraapi -o $(TARGET)
	$(CPP) $(LDFLAGS) $(LIBS) $(OBJECTS) -o $(TARGET)

clean: 
	rm -f *.o $(TARGET)

#stdafx.o: stdafx.cpp
sample.o: sample.cpp
verifier.o: verifier.cpp
eyesDetector.o: eyesDetector.cpp
faceDetector.o: faceDetector.cpp
tracker.o: tracker.cpp
usermanager.o: usermanager.cpp

	


