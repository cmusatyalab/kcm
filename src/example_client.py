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

import dbus
bus = dbus.SessionBus()
object = bus.get_object('edu.cmu.cs.kimberley.kcm', '/edu/cmu/cs/diamond/opendiamond/kcm')
print object.client()

