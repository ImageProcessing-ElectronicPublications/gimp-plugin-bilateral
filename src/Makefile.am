## Process this file with automake to produce Makefile.in

bin_PROGRAMS = simple-bilateral

bindir = $(GIMP_LIBDIR)/plug-ins

simple_bilateral_SOURCES = \
	plugin-intl.h	\
	interface.c	\
	interface.h	\
	main.c		\
	main.h		\
	render.cpp	\
	render.h	\
	image.cpp	\
	image.h		\
	bilateral.cpp	\
	bilateral.h

AM_CPPFLAGS = \
	-DLOCALEDIR=\""$(LOCALEDIR)"\"		\
	-DDATADIR=\""$(DATADIR)"\"

INCLUDES =\
	-I$(top_srcdir)		\
	@GIMP_CFLAGS@		\
	-I$(includedir)

LDADD = $(GIMP_LIBS)

