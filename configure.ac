#
#  Kimberley Display Control Manager
#
#  Copyright (c) 2007-2008 Carnegie Mellon University
#  All rights reserved.
#
#  Kimberley is free software: you can redistribute it and/or modify
#  it under the terms of version 2 of the GNU General Public License
#  as published by the Free Software Foundation.
#
#  Kimberley is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with Kimberley. If not, see <http://www.gnu.org/licenses/>.
#

AC_INIT([kcm], [1.0], [awolbach@cmu.edu])
AC_PREREQ([2.59])
AC_CONFIG_SRCDIR([config.h.in])
AM_INIT_AUTOMAKE([foreign 1.8])
AC_CONFIG_HEADERS([config.h])

AC_PROG_CC
AM_PROG_LEX

AC_DISABLE_STATIC
AC_PROG_LIBTOOL


# Check for recent pkg-config which supports Requires.private
# (snippet taken from cairo configure.in)
PKG_PROG_PKG_CONFIG()
if test "x$PKG_CONFIG" = x; then
        AC_MSG_ERROR([pkg-config >= $PKGCONFIG_REQUIRED required but not found (http://pkgconfig.freedesktop.org/)])
fi

case `$PKG_CONFIG --version` in
[0.?|0.?.?|0.1[0-7]|0.1[0-7].?]) PKGCONFIG_REQUIRES="Requires"; ;;
*) PKGCONFIG_REQUIRES="Requires.private"; ;;
esac

AC_SUBST(PKGCONFIG_REQUIRES)


# glib
PKG_CHECK_MODULES(GLIB2, [glib-2.0])
AC_SUBST(GLIB2_CFLAGS)
AC_SUBST(GLIB2_LIBS)

# D-Bus
PKG_CHECK_MODULES(DBUS, [dbus-1])
AC_SUBST(DBUS_CFLAGS)
AC_SUBST(DBUS_LIBS)

# D-Bus glib bindings
PKG_CHECK_MODULES(DBUS_GLIB, [dbus-glib-1])
AC_SUBST(DBUS_GLIB_CFLAGS)
AC_SUBST(DBUS_GLIB_LIBS)

# Avahi client 
PKG_CHECK_MODULES(AVAHI, [avahi-glib avahi-client])
AC_SUBST(AVAHI_CFLAGS)
AC_SUBST(AVAHI_LIBS)

# libs
AC_SEARCH_LIBS([pthread_create],
	[pthread],, AC_MSG_FAILURE([cannot find pthread_create function]))
AC_SEARCH_LIBS([OpenSSL_add_all_digests],
    [ssl],, AC_MSG_FAILURE([cannot find ssl function]))
AC_SEARCH_LIBS([dbus_bus_get],
	[dbus-1],, AC_MSG_FAILURE([cannot find dbus_bus_get function]))
AC_SEARCH_LIBS([dbus_g_bus_get],
	[dbus-glib-1],, AC_MSG_FAILURE([cannot find dbus_g_bus_get function]))
AC_SEARCH_LIBS([avahi_strerror],
	[avahi-common],, AC_MSG_FAILURE([cannot find avahi_strerror function]))
AC_SEARCH_LIBS([avahi_client_new],
	[avahi-client],, AC_MSG_FAILURE([cannot find avahi_client_new function]))
AC_SEARCH_LIBS([avahi_glib_poll_get],
	[avahi-glib],, AC_MSG_FAILURE([cannot find avahi_glib_poll_get function]))

DBUS_SERVICES_DIR="`pkg-config --variable=session_bus_services_dir dbus-1`"
AC_SUBST(DBUS_SERVICES_DIR)
AC_DEFINE_UNQUOTED(DBUS_SERVICES_DIR, "$DBUS_SERVICES_DIR", [Where services dir for DBUS is])

AC_SUBST(AM_CFLAGS, ['-Wall -Werror-implicit-function-declaration $(GLIB2_CFLAGS) $(DBUS_CFLAGS) $(DBUS_GLIB_CFLAGS) $(AVAHI_CFLAGS)'])


# done
AC_CONFIG_FILES([
Makefile
data/Makefile
src/Makefile
])
AC_OUTPUT
