lib_LTLIBRARIES = relativeCaptureBonus.la

relativeCaptureBonus_la_SOURCES = relativeCaptureBonus.cpp
relativeCaptureBonus_la_CPPFLAGS= -I$(top_srcdir)/include -I$(top_srcdir)/plugins/plugin_utils
relativeCaptureBonus_la_LDFLAGS = -module -avoid-version -shared
relativeCaptureBonus_la_LIBADD = $(top_builddir)/plugins/plugin_utils/libplugin_utils.la

AM_CPPFLAGS = $(CONF_CPPFLAGS)
AM_CFLAGS = $(CONF_CFLAGS)
AM_CXXFLAGS = $(CONF_CXXFLAGS)

EXTRA_DIST = \
	LICENSE.md \
	README.md \
	relativeCaptureBonus.def \
	relativeCaptureBonus.sln \
	relativeCaptureBonus.vcxproj \
	relativeCaptureBonus.vcxproj.filters

MAINTAINERCLEANFILES =	\
	Makefile.in
