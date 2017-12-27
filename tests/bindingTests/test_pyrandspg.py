import unittest
import pyrandspg

class TestPyrandspg(unittest.TestCase):

    def setUp(self):
        self.Na = pyrandspg.AtomStruct(11, 0.1, 0.2, 0.3)
        self.Cl = pyrandspg.AtomStruct(17, 0.6, 0.7, 0.8)
        self.lattice = pyrandspg.LatticeStruct(2.0, 3.0, 4.0, 60, 70, 80)
        self.crystal = pyrandspg.Crystal(self.lattice, [self.Na, self.Cl])

    def test_AtomStruct(self):

        self.assertEqual(self.Na.atomicNum, 11)
        self.assertEqual(self.Na.x, 0.1)
        self.assertEqual(self.Na.y, 0.2)
        self.assertEqual(self.Na.z, 0.3)

    def test_LatticeStruct(self):

        self.assertEqual(self.lattice.a, 2.0)
        self.assertEqual(self.lattice.b, 3.0)
        self.assertEqual(self.lattice.c, 4.0)
        self.assertEqual(self.lattice.alpha, 60.0)
        self.assertEqual(self.lattice.beta, 70.0)
        self.assertEqual(self.lattice.gamma, 80.0)
