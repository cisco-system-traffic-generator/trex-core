"""Wizard tests from petrounias.org

http://www.petrounias.org/articles/2014/09/16/pickling-python-collections-with-non-built-in-type-keys-and-cycles/

Includes functionality to assist with adding compatibility to jsonpickle.

"""
from __future__ import absolute_import, division, unicode_literals
import unittest
import collections

from jsonpickle import encode, decode


class World(object):
    def __init__(self):
        self.wizards = []


class Wizard(object):
    def __init__(self, world, name):
        self.name = name
        self.spells = collections.OrderedDict()
        world.wizards.append(self)

    def __hash__(self):
        return hash('Wizard %s' % self.name)

    def __eq__(self, other):
        for (ka, va), (kb, vb) in zip(self.spells.items(), other.spells.items()):
            if ka.name != kb.name:
                print('Wizards differ: %s != %s' % (ka.name, kb.name))
                return False
            for sa, sb in zip(va, vb):
                if sa != sb:
                    print('Spells differ: %s != %s' % (sa.name, sb.name))
                    return False
        return self.name == other.name

    def __cmp__(self, other):
        for (ka, va), (kb, vb) in zip(self.spells.items(), other.spells.items()):
            cmp_name = cmp(ka.name, kb.name)  # noqa: F821
            if cmp_name != 0:
                print('Wizards cmp: %s != %s' % (ka.name, kb.name))
                return cmp_name
            for sa, sb in zip(va, vb):
                cmp_spell = cmp(sa, sb)  # noqa: F821
                if cmp_spell != 0:
                    print('Spells cmp: %s != %s' % (sa.name, sb.name))
                    return cmp_spell
        return cmp(self.name, other.name)  # noqa: F821


class Spell(object):
    def __init__(self, caster, target, name):
        self.caster = caster
        self.target = target
        self.name = name
        try:
            spells = caster.spells[target]
        except KeyError:
            spells = caster.spells[target] = []
        spells.append(self)

    def __hash__(self):
        return hash(
            'Spell %s by %s on %s' % (self.name, self.caster.name, self.target.name)
        )

    def __eq__(self, other):
        return (
            self.name == other.name
            and self.caster.name == other.caster.name
            and self.target.name == other.target.name
        )

    def __cmp__(self, other):
        return (
            cmp(self.name, other.name)  # noqa: F821
            or cmp(self.caster.name, other.caster.name)  # noqa: F821
            or cmp(self.target.name, other.target.name)  # noqa: F821
        )  # noqa: F821


def hashsum(items):
    return sum([hash(x) for x in items])


def compare_spells(a, b):
    for (ka, va), (kb, vb) in zip(a.items(), b.items()):
        if ka != kb:
            print('Keys differ: %s != %s' % (ka, kb))
            return False
    return True


class MagicTestCase(unittest.TestCase):
    def test_without_pickling(self):
        world = World()
        wizard_merlin = Wizard(world, 'Merlin')
        wizard_morgana = Wizard(world, 'Morgana')
        spell_a = Spell(wizard_merlin, wizard_morgana, 'magic-missile')
        spell_b = Spell(wizard_merlin, wizard_merlin, 'stone-skin')
        spell_c = Spell(wizard_morgana, wizard_merlin, 'geas')

        self.assertEqual(wizard_merlin.spells[wizard_morgana][0], spell_a)
        self.assertEqual(wizard_merlin.spells[wizard_merlin][0], spell_b)
        self.assertEqual(wizard_morgana.spells[wizard_merlin][0], spell_c)

        # Merlin has cast Magic Missile on Morgana, and Stone Skin on himself
        self.assertEqual(wizard_merlin.spells[wizard_morgana][0].name, 'magic-missile')
        self.assertEqual(wizard_merlin.spells[wizard_merlin][0].name, 'stone-skin')

        # Morgana has cast Geas on Merlin
        self.assertEqual(wizard_morgana.spells[wizard_merlin][0].name, 'geas')

        # Merlin's first target was Morgana
        merlin_spells = wizard_merlin.spells
        merlin_spells_keys = list(merlin_spells.keys())
        self.assertTrue(merlin_spells_keys[0] in wizard_merlin.spells)
        self.assertEqual(merlin_spells_keys[0], wizard_morgana)

        # Merlin's second target was himself
        self.assertTrue(merlin_spells_keys[1] in wizard_merlin.spells)
        self.assertEqual(merlin_spells_keys[1], wizard_merlin)

        # Morgana's first target was Merlin
        morgana_spells_keys = list(wizard_morgana.spells.keys())
        self.assertTrue(morgana_spells_keys[0] in wizard_morgana.spells)
        self.assertEqual(morgana_spells_keys[0], wizard_merlin)

        # Merlin's first spell cast with himself as target is in the dictionary,
        # first by looking up directly with Merlin's instance object...
        self.assertEqual(wizard_merlin, wizard_merlin.spells[wizard_merlin][0].target)

        # ...and then with the instance object directly from the dictionary keys
        self.assertEqual(wizard_merlin, merlin_spells[merlin_spells_keys[1]][0].target)

        # Ensure Merlin's object is unique...
        self.assertEqual(id(wizard_merlin), id(merlin_spells_keys[1]))

        # ...and consistently hashed
        self.assertEqual(hash(wizard_merlin), hash(merlin_spells_keys[1]))

    def test_with_pickling(self):
        world = World()
        wizard_merlin = Wizard(world, 'Merlin')
        wizard_morgana = Wizard(world, 'Morgana')
        wizard_morgana_prime = Wizard(world, 'Morgana')

        self.assertEqual(wizard_morgana.__dict__, wizard_morgana_prime.__dict__)

        spell_a = Spell(wizard_merlin, wizard_morgana, 'magic-missile')
        spell_b = Spell(wizard_merlin, wizard_merlin, 'stone-skin')
        spell_c = Spell(wizard_morgana, wizard_merlin, 'geas')

        self.assertEqual(wizard_merlin.spells[wizard_morgana][0], spell_a)
        self.assertEqual(wizard_merlin.spells[wizard_merlin][0], spell_b)
        self.assertEqual(wizard_morgana.spells[wizard_merlin][0], spell_c)
        flat_world = encode(world, keys=True)
        u_world = decode(flat_world, keys=True)
        u_wizard_merlin = u_world.wizards[0]
        u_wizard_morgana = u_world.wizards[1]

        morgana_spells_encoded = encode(wizard_morgana.spells, keys=True)
        morgana_spells_decoded = decode(morgana_spells_encoded, keys=True)

        self.assertEqual(wizard_morgana.spells, morgana_spells_decoded)

        morgana_encoded = encode(wizard_morgana, keys=True)
        morgana_decoded = decode(morgana_encoded, keys=True)

        self.assertEqual(wizard_morgana, morgana_decoded)
        self.assertEqual(hash(wizard_morgana), hash(morgana_decoded))
        self.assertEqual(wizard_morgana.spells, morgana_decoded.spells)

        # Merlin has cast Magic Missile on Morgana, and Stone Skin on himself
        merlin_spells = u_wizard_merlin.spells
        self.assertEqual(merlin_spells[u_wizard_morgana][0].name, 'magic-missile')
        self.assertEqual(merlin_spells[u_wizard_merlin][0].name, 'stone-skin')

        # Morgana has cast Geas on Merlin
        self.assertEqual(u_wizard_morgana.spells[u_wizard_merlin][0].name, 'geas')

        # Merlin's first target was Morgana
        merlin_spells_keys = list(u_wizard_merlin.spells.keys())
        self.assertTrue(merlin_spells_keys[0] in u_wizard_merlin.spells)
        self.assertEqual(merlin_spells_keys[0], u_wizard_morgana)

        # Merlin's second target was himself
        self.assertTrue(merlin_spells_keys[1] in u_wizard_merlin.spells)
        self.assertEqual(merlin_spells_keys[1], u_wizard_merlin)

        # Morgana's first target was Merlin
        morgana_spells_keys = list(u_wizard_morgana.spells.keys())
        self.assertTrue(morgana_spells_keys[0] in u_wizard_morgana.spells)
        self.assertEqual(morgana_spells_keys[0], u_wizard_merlin)

        # Merlin's first spell cast with himself as target is in the dict.
        # First try the lookup with Merlin's instance object
        self.assertEqual(u_wizard_merlin, merlin_spells[u_wizard_merlin][0].target)
        # Next try the lookup with the object from the dictionary keys.
        self.assertEqual(
            u_wizard_merlin, merlin_spells[merlin_spells_keys[1]][0].target
        )

        # Ensure Merlin's object is unique and consistently hashed.
        self.assertEqual(id(u_wizard_merlin), id(merlin_spells_keys[1]))
        self.assertEqual(hash(u_wizard_merlin), hash(merlin_spells_keys[1]))


if __name__ == '__main__':
    unittest.main()
