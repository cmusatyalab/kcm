import dbus
bus = dbus.SessionBus()
object = bus.get_object('edu.cmu.cs.diamond.opendiamond.dcm', '/edu/cmu/cs/diamond/opendiamond/dcm')
print object.client()

