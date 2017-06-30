#!/usr/bin/env python

from  __future__ import  print_function
from releaseCommon import Version

v = Version()
v.incrementMinorVersion()
v.updateVersionFile()
v.updateReadmeFile()
v.updateConanFile()
v.updateConanTestFile()
v.updateTravisFile()

print( "Updated Version.hpp, README and Conan to v{0}".format( v.getVersionString() ) )
