#-------------------------------------------------
# This builds a static library for building windows apps so we
# don't have to distribute and keep track of dlls.

TARGET = son64
TEMPLATE = lib

win32 {

CONFIG -= debug_and_release
CONFIG += warn_off
CONFIG += c++17
DEFINES += S64_NOTDLL
DEFINES -= WIN32
DEFINES += WIN64
DEFINES -= UNICODE
DEFINES -= _UNICODE

QT -= gui

# these get installed in /opt/mxe 
# to install,
# Make -f Makefile_s64_static_winlib.qt install
headers.path = /opt/mxe/usr/x86_64-w64-mingw32.static/include
headers.files += machine.h \
   s3264.h \
   sonintl.h \
   son.h \
   s64.h \
   s32priv.h \
   sonpriv.h

INSTALLS += headers

SOURCES += s3264.cpp \
   s32priv.cpp \
   s64blkmgr.cpp \
   s64chan.cpp \
   s64dblk.cpp \
   s64event.cpp \
   s64filt.cpp \
   s64head.cpp \
   s64mark.cpp \
   s64ss.cpp \
   s64st.cpp \
   s64wave.cpp \
   s64xmark.cpp \
   son64.cpp

HEADERS += s3264.h \
   s32priv.h \
   s64chan.h \
   s64circ.h \
   s64dblk.h \
   s64doc.h \
   s64filt.h \
   s64.h \
   s64iter.h \
   s64priv.h \
   s64range.h \
   s64ss.h \
   s64st.h

QMAKE_CXXFLAGS += -I/opt/mxe/usr/include -static
QMAKE_LFLAGS += -static
LIBS += -lwinpthread
CONFIG -= debug
OBJECTS_DIR=winobj
DESTDIR=/opt/mxe/usr/x86_64-w64-mingw32.static/lib
MAKEFILE=Makefile_s64_static_winlib.qt

}
