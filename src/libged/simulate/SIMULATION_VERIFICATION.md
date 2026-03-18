# M35 Truck Simulation Verification

This document describes how to verify that the M35 truck simulation behaves correctly when falling onto the terra terrain.

## Verification Requirements

1. **Pre-simulation**: Truck must be positioned above the terrain (will fall down)
2. **Post-simulation**: 
   - Truck should have landed on terrain
   - Wheels should be on/near terrain surface
   - No excessive penetration or overlap
   - Distance between truck and terrain should show proper contact

## Verification Tool

### Building the Tool

```bash
cd build
gcc -o verify_simulation /path/to/verify_sim_complete.c \
    -I include/brlcad -L lib -lged -lrt -lbu -lm
```

### Usage

The verification tool has three modes:

#### 1. Pre-Simulation Check

Verifies the initial setup before running simulation:

```bash
./verify_simulation presim test_truck_terrain.g componentcomponent terra.sterra.s
```

This checks:
- Truck bounding box position
- Terrain bounding box position  
- Vertical separation (truck bottom to terrain top)
- Confirms truck is above terrain and will fall

Expected output:
```
✓ PASS: Truck is above terrain and will fall
        Separation = XXX.XX mm
```

#### 2. Post-Simulation Check

Verifies the results after simulation has run:

```bash
./verify_simulation postsim test_truck_terrain.g componentcomponent terra.sterra.s
```

This checks:
- Final truck position
- Truck-terrain gap/penetration
- Landing quality assessment
- Detects excessive penetration or failure to land

Expected output:
```
✓ PASS: Truck landed with wheels on terrain
        Penetration = XX.XX mm (expected for wheel contact)
```

## Verification Criteria

### Pre-Simulation (PASS conditions)

- Truck bottom Z > Terrain top Z
- Separation should be positive (truck is above terrain)
- Reasonable separation distance (not astronomical)

### Post-Simulation (PASS conditions)

- Truck-terrain gap between -10mm and +10mm (wheels touching surface)
- Small penetration acceptable: -10mm to -20% of truck height
  - Represents wheel compression into terrain surface
  - Bullet's collision resolution may allow slight interpenetration
- Truck top must be above terrain top
- No excessive penetration (> 20% of truck height)

### Failure Modes

❌ **Pre-simulation failures:**
- Truck starting below terrain
- Truck and terrain overlapping significantly

❌ **Post-simulation failures:**
- Truck still far above terrain (didn't fall/land)
- Truck completely below terrain surface
- Excessive penetration (> 20% of truck height)

## Example Full Workflow

```bash
# 1. Check initial setup
./verify_simulation presim test_truck_terrain.g componentcomponent terra.sterra.s

# Output should show truck above terrain

# 2. Run simulation (if passed pre-check)
# This is done through your test program that calls ged_exec

# 3. Verify results
./verify_simulation postsim test_truck_terrain.g componentcomponent terra.sterra.s

# Output should show truck landed on terrain with minimal gap/penetration
```

## Interpreting Results

### Good Landing (Expected)

```
Truck-terrain gap: -15.50 mm
✓ PASS: Truck landed with wheels on terrain
        Penetration = 15.50 mm (expected for wheel contact)
```

The small negative gap (penetration) indicates wheels are in contact with terrain.
This is expected behavior in physics simulations.

### Failed to Land

```
Truck-terrain gap: 500.00 mm
❌ FAIL: Truck did NOT land on terrain!
         Still 500.00 mm above terrain surface
```

Indicates simulation didn't run long enough or truck didn't have downward velocity.

### Excessive Penetration

```
Truck-terrain gap: -800.00 mm
❌ FAIL: Truck appears to have excessive penetration
         Penetration = 800.00 mm (32.5% of truck height)
```

Indicates physics simulation may have failed or timestep was too large.

## ROI Feature Verification

The ROI (Region-of-Interest) proxy feature should be active for the terrain:
- Terrain has `simulate::mass=0` (static body)
- ROI proxy enabled by default for static bodies
- ROI should track truck as it falls
- Large terrain dimensions (80km+) handled without error

This is verified by successful simulation completion without dimension errors.

## Additional Debug Commands

### Check attributes:
```bash
mged -c database.g
attr show componentcomponent
attr show terra.sterra.s
```

### Check positions visually:
```bash
mged database.g
# In MGED:
B componentcomponent
B terra.sterra.s
```

### Get exact coordinates:
```bash
mged -c database.g
bb -q componentcomponent
bb -q terra.sterra.s
```

