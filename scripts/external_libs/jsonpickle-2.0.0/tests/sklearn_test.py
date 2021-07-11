from __future__ import absolute_import, division, unicode_literals

import pytest

try:
    import numpy as np
    from sklearn.tree import DecisionTreeClassifier
except ImportError:
    pytest.skip('sklearn is not available', allow_module_level=True)

import jsonpickle
import jsonpickle.ext.numpy


@pytest.fixture(scope='module', autouse=True)
def numpy_extension():
    """Initialize the numpy extension for this test module"""
    jsonpickle.ext.numpy.register_handlers()
    yield  # control to the test function.
    jsonpickle.ext.numpy.unregister_handlers()


def test_decision_tree():
    #  Create data.
    np.random.seed(13)
    x_values = np.random.randint(low=0, high=10, size=12)
    x = x_values.reshape(4, 3)
    y_values = np.random.randint(low=0, high=2, size=4)
    y = y_values.reshape(-1, 1)

    # train model
    classifier = DecisionTreeClassifier(max_depth=1)
    classifier.fit(x, y)

    # freeze and thaw
    pickler = jsonpickle.pickler.Pickler()
    unpickler = jsonpickle.unpickler.Unpickler()
    actual = unpickler.restore(pickler.flatten(classifier))

    assert isinstance(actual, classifier.__class__)
    if hasattr(classifier, 'tree_'):
        assert isinstance(actual.tree_, classifier.tree_.__class__)

    # predict from thawed
    array_values = np.array([1, 2, 3])
    array = array_values.reshape(1, -1)
    prediction = actual.predict(array)
    assert prediction[0] == 1

    assert actual.max_depth == classifier.max_depth
    assert actual.score(x, y) == classifier.score(x, y)
    if hasattr(classifier, 'get_depth'):
        assert actual.get_depth() == classifier.get_depth()


if __name__ == '__main__':
    pytest.main([__file__])
