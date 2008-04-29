bin_PROGRAMS = kcm
bin_SCRIPTS = kcm_encrypt

noinst_PROGRAMS = example_client example_server

kcm_SOURCES = avahi.c avahi.h client.c common.c common.h kcm.c kcm.h \
              server.c kcm-glib.h
example_client_SOURCES = example_client.c
example_server_SOURCES = example_server.c common.c common.h

BUILT_SOURCES = kcm-dbus-glue.h kcm-dbus-app-glue.h

# We don't want to install this header
noinst_HEADERS = $(BUILT_SOURCES)

# Correctly clean the generated headers, but keep the xml description
CLEANFILES = $(BUILT_SOURCES)
EXTRA_DIST = kcm.xml

# Rule to generate glib D-Bus bindings
kcm-dbus-glue.h: kcm.xml
	dbus-binding-tool --mode=glib-server --prefix=kcm $<> $@

kcm-dbus-app-glue.h: kcm.xml
	dbus-binding-tool --mode=glib-client --prefix=kcm $<> $@

