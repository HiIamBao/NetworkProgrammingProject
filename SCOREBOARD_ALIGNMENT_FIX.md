# Scoreboard Text Alignment Fix

## Problem
The scoreboard text was not properly aligned with its semi-transparent background box. The issue was caused by mixing percentage-based positioning for the background with percentage-to-pixel converted positioning for the text, leading to misalignment at different window sizes.

## Root Cause
```cpp
// BEFORE: Mixed positioning approach
float xPosPerc = 0.08f;
float xPixel = xPosPerc * renderer.windowW;  // Convert to pixels for text

auto bgBox = Ui::Box()
    .xLeftPerc(xPosPerc - 0.015f)  // Background uses percentages
    .yTopPerc(yPosPerc - 0.015f)
    .xDimensionPercentage(bgWidth)
    .yDimensionPercentage(bgHeight);

// Text uses converted pixel position
renderer.renderText(glm::vec2(xPixel, yPixel), ...);
```

The background box and text were using different coordinate systems, causing alignment issues.

## Solution
Changed both the background box and text rendering to use **fully pixel-based positioning** for perfect alignment:

```cpp
// AFTER: Consistent pixel-based positioning
float xPixel = 100.0f;  // 100 pixels from left edge
float yPixel = 60.0f;   // 60 pixels from top edge
float lineHeightPixel = 35.0f;  // 35 pixels between lines
float paddingPixel = 15.0f;  // Padding around background

// Background uses pixel methods
auto bgBox = Ui::Box()
    .xLeft(xPixel - paddingPixel)
    .yTop(yPixel - paddingPixel)
    .xDimensionPixels(bgWidthPixel)  // Fixed: was .xDimension()
    .yDimensionPixels(bgHeightPixel); // Fixed: was .yDimension()

// Text uses same pixel positions
renderer.renderText(glm::vec2(xPixel, yPixel), ...);
```

## Changes Made
1. **Removed percentage-based positioning** for background box
2. **Used `xLeft()` and `yTop()`** instead of `xLeftPerc()` and `yTopPerc()`
3. **Used `xDimensionPixels()` and `yDimensionPixels()`** for background size (corrected method names)
4. **Consistent pixel positioning** for all text elements
5. **Fixed padding**: Applied same pixel padding to background edges

## Benefits
- ✅ **Perfect alignment** between background and text at any window size
- ✅ **Predictable positioning** with fixed pixel offsets
- ✅ **No clipping** of text outside background
- ✅ **Consistent spacing** between scoreboard lines
- ✅ **Better visual polish** with proper background padding

## File Modified
- `src/gameLayer/client.cpp` - Scoreboard rendering in `clientFunction()`

## Visual Improvements
- Scoreboard positioned 100px from left, 60px from top
- 35px line height for comfortable reading
- 15px padding around background for visual breathing room
- Dark semi-transparent background (0.75 alpha) for better contrast
- All text perfectly aligned within background box

## Testing
Rebuild and run the game to see the improved scoreboard alignment:
```bash
./clean_and_build.sh
./build/multyPlayer
```

The scoreboard should now display with perfect text alignment inside its background box, with no clipping or misalignment issues.
