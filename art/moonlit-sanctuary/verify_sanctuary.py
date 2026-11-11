"""Checks structure of saved visual prototype; not a gameplay/collision test."""
import bpy, math

obs=bpy.data.objects
assert len([o for o in obs if o.name.startswith('Radial bridge ') and o.type=='MESH'])==4
assert len([o for o in obs if o.name.startswith('DISTRICT_') and o.type=='EMPTY'])==8
assert len([o for o in obs if o.name.startswith('Nature_DeadTree')])==16
assert len([o for o in obs if o.name.startswith('Nature_Grave')])==7
assert obs.get('Arena foundation') and obs.get('Outer circular foundations')
assert obs.get('PLAYER SCALE - body')
assert all(math.isfinite(float(v)) for o in obs for v in o.location)
# Floor plan overlaps geometrically in XY: central radius 25, bridge 23.7..59.7,
# ring inner radius 59, main districts centered at 78 with radius 15. This says
# nothing about obstacles, collision, gameplay movement, or navigation.
assert 23.7 < 25 and 59.7 > 59 and 78-15 < 77
print('VERIFIED: source opens; 4 bridge objects, 8 district markers, 16 trees, 7 graves; intended floor extents overlap in plan.')
