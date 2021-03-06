import re
from . import types as ppp_types


class Robot:
  """
  Describe a robot.

  Attributes:
    devices -- a list of devices (Device instances)

  """

  def __init__(self, devices):
    """Create a new robot description.
    Check device and message consistency.
    Auto-attribute IDs to message without ID.
    """
    # assign robot to devices
    for dev in devices:
      if dev.robot is not None:
        raise AttributeError("device %r already assigned to a robot" % dev.name)
      dev.robot = self
    self.devices = devices

    # check for duplicate names (case insensitive)
    dev_names = set()  # device names, lowercase
    for dev in devices:
      name = dev.name.lower()
      if name in dev_names:
        raise ValueError("device name %r already used" % name)
      else:
        dev_names.add(name)

    # check for duplicate ROIDs
    dev_roids = {}  # devices, indexed by ROID
    for dev in devices:
      roid = dev.roid
      if roid in dev_roids:
        raise ValueError("ROID %u set on %r already used by %r"
            % (roid, dev.name, dev_roids[roid].name))
      else:
        dev_roids[roid] = dev

    # check for duplicate message IDs or names (case insensitive)
    messages = {}  # messages, indexed by ID
    msgs_noid = []  # messages without ID
    msgs_names = set()
    for dev in devices:
      for msg in dev.messages:
        mid = msg.mid
        if mid is None:
          msgs_noid.append(msg)
        elif mid in messages:
          raise ValueError("message ID %u set on %r already used by %r"
              % (mid, msg.name, messages[mid].name))
        else:
          messages[mid] = msg
        name = msg.name.lower()
        if name in msgs_names:
          raise ValueError("message name %r already used" % name)
        else:
          msgs_names.add(name)

    # auto-attribute message IDs
    mid = 0
    for msg in msgs_noid:
      while mid in messages:
        mid += 1
        if mid > 0xff:
          raise OverflowError("too many messages")
      msg.mid = mid
      messages[mid] = msg


  def messages(self):
    """Return all messages."""
    return [ msg
        for dev in self.devices
        for msg in dev.messages
        ]

  def commands(self):
    """Return all commands."""
    return [ msg
        for dev in self.devices
        for msg in dev.commands()
        ]

  def telemetries(self):
    """Return all telemetries."""
    return [ msg
        for dev in self.devices
        for msg in dev.telemetries()
        ]


class Device:
  """
  Describe robot device (part of robot system).

  Attributes:
    name -- device's name
    roid -- device's ROID (and I2C address)
    messages -- list of messsages
    robot -- device's robot (set by the robot itself)

  """
  def __init__(self, name, roid, messages):
    if not re.match('^[a-zA-Z][a-zA-Z0-9_]*$', name):
      raise ValueError("invalid device name: %r" % name)
    if not 0x08 <= roid < 0x78:
      raise ValueError("invalid ROID: %r" % roid)
    self.name = name
    self.roid = int(roid)
    self.messages = messages
    self.robot = None

    # assign device to messages
    for msg in self.messages:
      if msg.device is not None:
        raise AttributeError("message %r already assigned to a device" % msg.name)
      msg.device = self

  def commands(self):
    """Return device commands."""
    return [ msg for msg in self.messages if isinstance(msg, Command) ]

  def telemetries(self):
    """Return device telemetries."""
    return [ msg for msg in self.messages if isinstance(msg, Telemetry) ]



class Message:
  """
  Transmitted message, either from or to a device.

  Attributes:
    name -- message name (lowercase, with underscores)
    mid -- message ID (automatically assigned if None)
    desc -- message description, or None
    device -- message's device (set by the device itself)

  """

  def __init__(self, name, mid=None, desc=None):
    if not re.match('^[a-z][a-z0-9_]*$', name):
      raise ValueError("invalid message name: %r" % name)
    self.name = name
    self.mid = mid
    self.desc = desc
    self.device = None

  @classmethod
  def _convert_type(cls, t):
    """Helper method to check and convert a parameter type."""
    if isinstance(t, basestring):
      t = ppp_types.types[t]
    if not isinstance(t, ppp_types._Type):
      raise TypeError("invalid data type")
    return t

  @classmethod
  def _check_name(cls, name):
    """Helper method to check a parameter name."""
    if not re.match('^[a-zA-Z][a-zA-Z0-9_]*$', name):
      raise ValueError("invalid parameter name: %r" % name)


class Telemetry(Message):
  """
  Telemetry message, from device to exterior.

  Attributes:
    params -- payload parameters, as a list of fields

  """

  def __init__(self, name, params, **kw):
    Message.__init__(self, name, **kw)
    names = set()  # defined parameter names
    self.params = []
    for n, t in params:
      self._check_name(n)
      if n in names:
        raise ValueError("parameter %r already defined" % n)
      names.add(n)
      self.params.append((n, self._convert_type(t)))


class Command(Message):
  """
  Command message, from exterior to device.

  Attributes:
    iparams -- input parameters (arguments)
    oparams -- output parameters (returned values)

  """

  def __init__(self, name, iparams, oparams, **kw):
    Message.__init__(self, name, **kw)
    names = set()  # defined parameter names (shared by in/out params)

    self.iparams = []
    for n, t in iparams:
      self._check_name(n)
      if n in names:
        raise ValueError("parameter %r already defined" % n)
      names.add(n)
      self.iparams.append((n, self._convert_type(t)))

    self.oparams = []
    for n, t in oparams:
      self._check_name(n)
      if n in names:
        raise ValueError("parameter %r already defined" % n)
      names.add(n)
      self.oparams.append((n, self._convert_type(t)))


