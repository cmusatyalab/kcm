bin_PROGRAMS = dcm

noinst_PROGRAMS = example_client example_server

dcm_SOURCES = client.c common.c common.h dcm.c dcm.h dcm-avahi.c dcm-avahi.h \
              server.c dcm-glib.h
example_client_SOURCES = example_client.c
example_server_SOURCES = example_server.c common.c common.h

BUILT_SOURCES = dcm-dbus-glue.h dcm-dbus-app-glue.h

# We don't want to install this header
noinst_HEADERS = $(BUILT_SOURCES)

# Correctly clean the generated headers, but keep the xml description
CLEANFILES = $(BUILT_SOURCES)
EXTRA_DIST = dcm.xml

# Rule to generate glib D-Bus bindings
dcm-dbus-glue.h: dcm.xml
	dbus-binding-tool --mode=glib-server --prefix=dcm $<> $@

dcm-dbus-app-glue.h: dcm.xml
	dbus-binding-tool --mode=glib-client --prefix=dcm $<> $@

