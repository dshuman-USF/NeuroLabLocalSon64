
WINOBJ=winobj

#DEBUG_OR_NOT = -ggdb3
DEBUG_OR_NOT = -O2

ACLOCAL_AMFLAGS = -I m4
AM_CPPFLAGS = $(DEBUG_OR_NOT) -std=c++17 -DS64_NOTDLL -DGNUC -Wall

first: all
all: libson64.la libson64.a

lib_LTLIBRARIES = libson64.la 

EXTRA_DIST = debian s64_static_winlib.pro HOW_TO_BUILD_WINDOWS_DLL \
             son.c sondll.c sonex.c


# these get installed in /usr/local/include
include_HEADERS = \
   machine.h \
   s3264.h \
   sonintl.h \
   son.h \
   s64.h \
   s32priv.h \
   sonpriv.h


libson64_la_SOURCES = \
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
		son64.cpp \
      s3264.cpp \
      machine.h \
      s3264.h \
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
      s64st.h \
      s64witer.h \
      sonex.h \
      son.h \
      sonintl.h \
      sonpriv.h

BUILT_SOURCES =  Makefile_s64_static_winlib.qt

CLEANFILES = ${BUILT_SOURCES} \
				  $(WINOBJ)/*.o

winlib:
	@mkdir -p $(WINOBJ)

# build the makefile
Makefile_s64_static_winlib.qt: s64_static_winlib.pro Makefile.am configure.ac
 ifdef MXE_QMAKE
	x86_64-w64-mingw32.static-qmake-qt5 s64_static_winlib.pro -nocache -nomoc -r -spec win32-g++ 'DEFINES+=VERSION=\\\"$(VERSION)\\\"'
 else
	@echo MXE environment not installed, windows Makefile not built
 endif

# build the static windows lib
libson64.a: Makefile_s64_static_winlib.qt
 ifdef MXE_QMAKE
	make -f Makefile_s64_static_winlib.qt
 else
	@echo MXE environment not installed, windows library not built
 endif


checkins = $(libson64_la_SOURCES) configure.ac Makefile.am s64_static_winlib.pro

checkin_release:
	git add $(checkins) && git commit -m "Release files for version $(VERSION)"

checkpoint:
	git add $(checkins) && git commit -q -m "checkpoint files for version $(VERSION)"

checkpoint_withcomment:
	git add $(checkins) && git commit -q


# typical first-time autotools setup run from src dir:
# aclocal
# autoconf -i
# touch AUTHORS NEWS README ChangeLog
# automake --add-missing --copy
#you may need to run run "sudo ldconfig" after "sudo make install"


deb:
	@echo 'Making debian packages'
	make distdir &&\
	cd $(distdir) &&\
	dpkg-buildpackage -Zgzip -uc -us && \
	cd .. && \
	rm -rf $(distdir)

