# Bullet Shooting Direction Bug Fix

## Problem

When the player moved left or right on the map, bullets would shoot in the wrong direction and appear "stuck" in one direction. The bullet direction didn't follow the mouse cursor correctly after the player moved.

## Root Cause

The bullet shooting code was calculating the direction using **screen-space coordinates** without accounting for the **camera position** in world-space.

### Original Buggy Code:
```cpp
auto mousePos = platform::getRelMousePosition();
auto screenCenter = glm::vec2(renderer.windowW, renderer.windowH) / 2.f;

auto delta = glm::vec2(mousePos) - screenCenter;
```

### The Issue:
1. `mousePos` is in **screen-space** (relative to the window, e.g., 0-1920 pixels)
2. `screenCenter` is just the middle of the screen
3. When the camera moves (following the player), the **world-space** positions change
4. But the calculation stays in screen-space, causing bullets to shoot in wrong directions

### Visual Example:
```
Player at position (100, 100) - screen center:
  Mouse at screen (800, 400) → Shoots RIGHT ✓

Player moves to position (500, 100) - camera follows:
  Mouse STILL at screen (800, 400)
  BUT world position changed!
  Old code: Still calculates RIGHT (WRONG!) ✗
  Should shoot: Towards actual mouse world position
```

## Solution

Convert the mouse position from **screen-space to world-space** using the camera's `convertPoint` method before calculating the bullet direction.

### Fixed Code:
```cpp
// Convert mouse position from screen-space to world-space
auto mousePos = platform::getRelMousePosition();

// Convert mouse from screen space to world space using camera
glm::vec2 mouseWorldPos = renderer.currentCamera.convertPoint(mousePos, renderer.windowW, renderer.windowH);

// Get player center in world coordinates  
glm::vec2 playerCenter = (player.pos + player.dimensions / 2.f) * worldMagnification;

// Calculate direction from player to mouse in world space
auto delta = mouseWorldPos - playerCenter;

float magnitude = glm::length(delta);
if (magnitude == 0)
{
    b.direction = {1,0};
}
else
{
    b.direction = delta / magnitude;
}
```

### How It Works:

1. **Get Mouse Screen Position**: `platform::getRelMousePosition()` - e.g., (800, 400) pixels
2. **Convert to World Space**: `camera.convertPoint()` - accounts for camera position, zoom, rotation
3. **Get Player World Position**: `(player.pos + dimensions/2) * worldMagnification`
4. **Calculate Direction**: `mouseWorldPos - playerCenter` gives the correct vector
5. **Normalize**: Divide by magnitude to get unit direction vector

## Technical Details

### Camera Coordinate Transformation

The camera applies transformations in this order:
1. **Position Offset**: Camera follows player, moving the viewport
2. **Zoom**: Can magnify or shrink the view
3. **Rotation**: Can rotate the world view

The `convertPoint` method reverses these transformations:
```
Screen Coordinates → Apply inverse camera matrix → World Coordinates
```

### Why This Matters

In a 2D game with a moving camera:
- **Screen space**: Fixed window coordinates (0,0 to windowW, windowH)
- **World space**: Game world coordinates (can be much larger, camera shows a portion)
- **Camera**: Acts as a "window" into the world

When calculating directions from player to mouse, both must be in the **same coordinate system** (world-space).

## Testing

To verify the fix works:

1. **Start Game**: Run `./build/multyPlayer`
2. **Move Player**: Use WASD to move around the map
3. **Shoot**: Click mouse in different directions
4. **Verify**: Bullets should always shoot toward the mouse cursor, regardless of player position

### Test Cases:

✅ **Player at (0, 0)**: Bullets shoot correctly toward mouse  
✅ **Player at (500, 0)**: Bullets still shoot toward mouse  
✅ **Player moving left**: Bullets shoot toward mouse while moving  
✅ **Player moving right**: Bullets shoot toward mouse while moving  
✅ **Player at top of map**: Bullets shoot toward mouse  
✅ **Player at bottom of map**: Bullets shoot toward mouse  

## Files Modified

- `/src/gameLayer/client.cpp` - Fixed bullet direction calculation in `clientFunction()`
  - Lines ~456-476: Bullet shooting mouse direction code

## Related Systems

### Camera System
- Location: `thirdparty/gl2d/include/gl2d/gl2d.h`
- Method: `Camera::convertPoint(const glm::vec2 &p, float windowW, float windowH)`
- Purpose: Converts screen coordinates to world coordinates

### Player Movement
- Location: `src/gameLayer/client.cpp`
- Camera follows player: `renderer.currentCamera.follow(player.pos * worldMagnification, ...)`

### Controller Support
- The fix only affects mouse shooting
- Controller right stick shooting already worked correctly (uses world-space direction directly)

## Benefits

✅ Bullets now shoot correctly toward mouse cursor at all times  
✅ Works regardless of player position on map  
✅ Consistent with how controller shooting works  
✅ More intuitive and playable game experience  

## Additional Notes

### World Magnification
The `worldMagnification` constant scales world coordinates for rendering. It's applied to both:
- Player position when converting to world coords
- Mouse position when converting from screen coords

This ensures both are in the same scale/coordinate system.

### Battery Shooting
The battery power-up (rapid-fire circular pattern) was NOT affected by this bug because it:
- Doesn't use mouse position
- Calculates direction using rotation angles
- Already works in world-space

## Prevention

To avoid similar bugs in the future:
- Always consider which coordinate system you're working in
- Use camera's `convertPoint()` when mixing screen and world coordinates
- Test gameplay after camera movement is implemented
- Document coordinate system expectations in code comments
