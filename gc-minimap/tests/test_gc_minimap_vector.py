"""Regression checks for diagonal piers, sea gaps and contour holes."""
import importlib.util
from pathlib import Path
import unittest
import numpy as np

spec = importlib.util.spec_from_file_location(
    'gc_vector', Path(__file__).resolve().parents[1]/'tools'/'gc-minimap-vector.py')
tracer = importlib.util.module_from_spec(spec)
spec.loader.exec_module(tracer)


class VectorTopology(unittest.TestCase):
    def registered(self, mask):
        yy, xx = np.mgrid[:mask.shape[0], :mask.shape[1]]
        centers = np.column_stack((xx.ravel()+.5, yy.ravel()+.5))
        actual = np.zeros(mask.size, dtype=bool)
        loops = tracer.contours(mask)
        for loop in loops:
            _, points = tracer.rounded_path(loop)
            actual ^= tracer.points_in_poly(centers, points)
        np.testing.assert_array_equal(actual.reshape(mask.shape), mask)
        return len(loops)

    def test_diagonal_pier_stays_connected(self):
        self.assertEqual(self.registered(np.eye(7, dtype=bool)), 1)

    def test_actual_sea_gap_stays_open(self):
        mask = np.zeros((5,5), dtype=bool)
        mask[1,1] = mask[3,3] = True
        self.assertEqual(self.registered(mask), 2)

    def test_lagoon_hole_stays_transparent(self):
        mask = np.ones((7,7), dtype=bool)
        mask[2:5,2:5] = False
        self.assertEqual(self.registered(mask), 2)


if __name__ == '__main__':
    unittest.main()
