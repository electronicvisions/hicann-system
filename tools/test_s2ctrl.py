import unittest
from time import sleep
from s2ctrl import *

class TestS2Ctrl(unittest.TestCase):

    def setUp(self):
        self.ip = "192.168.123.123"
        #import pyhalbe
        #self.ip = pyhalbe.Coordinate.IPv4.from_string("192.168.123.123")
        self.port = 1700
        self.mylock = HMFLock(self.ip, self.port)

    def test_lockunlock(self):
        self.mylock.lock()
        self.mylock.lock()
        self.assertTrue(self.mylock.owner())

    def test_lockunlock(self):
        self.mylock.lock()
        self.assertTrue(self.mylock.owner())
        self.mylock.unlock()
        self.assertFalse(self.mylock.owner())

    def test_locklockunlock(self):
        self.mylock.lock()
        self.assertTrue(self.mylock.owner())
        self.mylock.lock()
        self.assertTrue(self.mylock.owner())
        self.mylock.unlock()
        self.assertFalse(self.mylock.owner())

    def test_lockunlockunlock(self):
        self.mylock.lock()
        self.assertTrue(self.mylock.owner())
        self.mylock.unlock()
        self.assertFalse(self.mylock.owner())
        self.mylock.unlock()
        self.assertFalse(self.mylock.owner())

    def test_unlocklockunlock(self):
        self.mylock.unlock()
        self.assertFalse(self.mylock.owner())
        self.mylock.lock()
        self.assertTrue(self.mylock.owner())
        self.mylock.unlock()
        self.assertFalse(self.mylock.owner())

    @unittest.skip("ugly stdout output")
    def test_lockstatusunlockstatus(self):
        self.mylock.lock()
        self.mylock.print_status()
        self.assertTrue(self.mylock.owner())
        self.mylock.unlock()
        self.assertFalse(self.mylock.owner())
        self.mylock.print_status()

if __name__ == '__main__':
    unittest.main()
