import dbus
bus = dbus.SessionBus()
object = bus.get_object('edu.cmu.cs.kimberley.kcm', '/edu/cmu/cs/diamond/opendiamond/kcm')
print object.client()

