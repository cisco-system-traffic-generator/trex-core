from inspect import signature


def check_class_methods_signatures(cls, signatures):
    """Check that class 'cls' has all methods in signatures and each method has the correct signature.
    Returns (success, message) where success is True if the test passes, if the test fails, is False and message is the exception string to raise.

    Args:
        cls: the class
        signatures: a dict { method name (string) -> method signature (string)}
            e.g. {"myMethod": "(self, arg1, arg2=None)",}
    """

    for method_name, expected_sig in signatures.items():
        try:
            method = getattr(cls, method_name)
            if not callable(method):
                return False, "{}'s {} should be callable".format(cls.__name__, method_name)
            sig = str(signature(method))
        except AttributeError:
            return False, "{} should have method {}".format(cls.__name__, method_name)
    return True, None