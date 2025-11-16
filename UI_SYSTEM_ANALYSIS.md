# UI System Analysis - Multiplayer Game Project

## Overview
This project uses a **custom C++ UI system** with multiple layers:
1. **Low-level rendering**: gl2d library (OpenGL 2D renderer)
2. **Layout system**: Custom `Ui::Box` for positioning
3. **ImGui**: For debug/editor interfaces (not used for game UI)

---

## 1. Core UI Library: `Ui::Box` (Custom Layout System)

**Location**: `include/gameLayer/Ui.h` and `src/gameLayer/Ui.cpp`

### Purpose
A fluent API for positioning and sizing UI elements using a **method-chaining pattern**.

### Key Features

#### A. Positioning Methods
```cpp
Box box;
box.xLeft(10)           // 10 pixels from left edge
   .yTop(20)            // 20 pixels from top edge
   .xCenter(5)          // Centered horizontally with 5px offset
   .yCenter()           // Centered vertically
   .xRight(10)          // 10 pixels from right edge
   .yBottom(10)         // 10 pixels from bottom edge
```

#### B. Percentage-based Positioning
```cpp
box.xLeftPerc(0.5f)    // 50% from left (half screen width)
   .yTopPerc(0.02f)    // 2% from top
```

#### C. Sizing Methods
```cpp
box.xDimensionPixels(100)         // 100 pixels wide
   .yDimensionPixels(50)          // 50 pixels tall
   .xDimensionPercentage(0.5f)    // 50% of screen width
   .yDimensionPercentage(0.3f)    // 30% of screen height
```

#### D. Aspect Ratio
```cpp
box.xDimensionPercentage(0.04f)   // Set width to 4% of screen
   .yAspectRatio(1.0f)            // Height = width (square)
```

### Internal State
- **XcenterState**: `-1` (left), `0` (none), `1` (center), `2` (right)
- **YcenterState**: Same for vertical
- **dimensionsState**: `0` (pixels), `1` (x-dominant aspect), `2` (y-dominant aspect)
- **dimensions**: `glm::ivec4 {x, y, width, height}` (in pixels after calculation)

### Example Usage (from client.cpp)
```cpp
// Health icon at top-right with aspect ratio
auto crossPos = Ui::Box()
    .xLeftPerc(0.95)          // 95% from left (near right edge)
    .yTopPerc(0.02)           // 2% from top
    .xDimensionPercentage(0.04)  // 4% of screen width
    .yAspectRatio(1.f);       // Square (height = width)

renderer.renderRectangle(crossPos, color, {}, 0.f, textures.cross);
```

---

## 2. Rendering Library: gl2d (OpenGL 2D)

**Location**: `thirdparty/gl2d/include/gl2d/gl2d.h`

### Main Components

#### A. Renderer2D Class
Central rendering engine that handles:
- **Sprite batching** (up to 16k triangles per batch)
- **Camera transformations**
- **Text rendering** (using custom fonts)
- **Texture management**

#### B. Key Methods

##### renderRectangle()
```cpp
renderer.renderRectangle(
    Rect transforms,           // Position & size (can use Ui::Box)
    Color4f color,             // RGBA color (0-1 range)
    glm::vec2 origin,          // Rotation origin
    float rotation,            // Rotation in radians
    Texture texture,           // Optional texture
    glm::vec4 textureCoords    // UV coords (optional)
);
```

**Example**:
```cpp
// Render semi-transparent black background
auto bgBox = Ui::Box().xLeft(100).yTop(50)
    .xDimensionPixels(300).yDimensionPixels(200);
renderer.renderRectangle(bgBox, {0.1f, 0.1f, 0.15f, 0.95f});
```

##### renderText()
```cpp
renderer.renderText(
    glm::vec2 position,     // Bottom-left corner of text
    const char* text,       // Text string
    Font font,              // Font texture
    Color4f color,          // Text color
    float size,             // Scale factor (1.0 = normal)
    float spacing,          // Letter spacing (pixels * size)
    float line_space,       // Line height (pixels * size)
    bool showInCenter       // Center the text?
);
```

**Example**:
```cpp
// Render yellow text at top-center
renderer.renderText(
    glm::vec2(0.38f * renderer.windowW, 50.0f),  // Position
    "Wave 5",                                     // Text
    textures.font,                                // Font
    glm::vec4(1.0f, 1.0f, 0.2f, 1.0f),          // Yellow
    0.9f,                                         // Size scale
    4.0f,                                         // Letter spacing
    3.0f,                                         // Line spacing
    false                                         // Don't center
);
```

#### C. Camera System
```cpp
struct Camera {
    glm::vec2 position;     // World position
    float zoom;             // Zoom level (1.0 = normal)
    float aspectRatio;      // Width / height
    
    glm::vec2 convertPoint(glm::vec2 screenPos, int w, int h);
    // Converts screen coordinates to world coordinates
};

renderer.setCamera(camera);  // Apply camera for world-space rendering
renderer.resetCameraAndShader();  // Reset to screen-space UI
```

---

## 3. UI Rendering Patterns in This Project

### Pattern 1: Percentage-based Positioning (Responsive)
Used for elements that should scale with screen size:
```cpp
// Wave info (center-top)
glm::vec2 wavePos = glm::vec2(0.38f * renderer.windowW, 50.0f);
renderer.renderText(wavePos, "Wave 5", ...);
```

### Pattern 2: Pixel-based Positioning (Absolute)
Used for fixed-size UI elements:
```cpp
// Money display (top-right, 80px from top)
glm::vec2 moneyPos = glm::vec2(renderer.windowW - 250.0f, 80.0f);
renderer.renderText(moneyPos, "Money: $500", ...);
```

### Pattern 3: Ui::Box with Percentages (Best for icons/buttons)
```cpp
// Health heart icon (top-right)
auto heartPos = Ui::Box()
    .xLeftPerc(0.95)                  // 95% from left
    .yTopPerc(0.02)                   // 2% from top
    .xDimensionPercentage(0.04)       // 4% of screen width
    .yAspectRatio(1.f);               // Square

renderer.renderRectangle(heartPos, {1,1,1,1}, {}, 0, textures.cross);
```

### Pattern 4: Compound Positioning (Offsets)
```cpp
// Multiple hearts in a row
float xLeft = 0.95;
float xAdvance = 0.015;  // Space between hearts

for (int i = 0; i < maxLife; i++) {
    auto heartPos = Ui::Box()
        .xLeftPerc(xLeft)
        .yTopPerc(0.02)
        .xDimensionPercentage(0.04)
        .yAspectRatio(1.f);
    
    renderer.renderRectangle(heartPos, color, {}, 0, textures.cross);
    xLeft -= xAdvance;  // Move left for next heart
}
```

---

## 4. Color System

### glm::vec4 Color Format (RGBA)
- **Range**: 0.0 to 1.0 for each channel
- **Format**: `{red, green, blue, alpha}`

### Common Colors in Project
```cpp
glm::vec4(1.0f, 1.0f, 1.0f, 1.0f)   // White (opaque)
glm::vec4(0.0f, 0.0f, 0.0f, 0.8f)   // Black (semi-transparent)
glm::vec4(1.0f, 0.9f, 0.2f, 1.0f)   // Gold/Yellow
glm::vec4(0.2f, 1.0f, 0.2f, 1.0f)   // Green
glm::vec4(1.0f, 0.3f, 0.3f, 1.0f)   // Red
glm::vec4(0.2f, 0.8f, 1.0f, 1.0f)   // Cyan
```

---

## 5. Complete Example: Shop UI

From `client.cpp` (lines 1360-1490):

```cpp
// Semi-transparent overlay
auto overlayPos = Ui::Box()
    .xLeftPerc(0.0)
    .yTopPerc(0.0)
    .xDimensionPercentage(1.0)
    .yDimensionPercentage(1.0);
renderer.renderRectangle(overlayPos, {0.0f, 0.0f, 0.0f, 0.8f});

// Shop window background (pixel-based)
float shopX = 0.15f * renderer.windowW;
float shopY = 0.1f * renderer.windowH;
float shopW = 0.7f * renderer.windowW;
float shopH = 0.8f * renderer.windowH;

auto shopBox = Ui::Box()
    .xLeft(shopX)
    .yTop(shopY)
    .xDimensionPixels(shopW)
    .yDimensionPixels(shopH);
renderer.renderRectangle(shopBox, {0.1f, 0.1f, 0.15f, 0.95f});

// Shop title (centered)
glm::vec2 titlePos = glm::vec2(shopX + shopW * 0.38f, shopY + 20.0f);
renderer.renderText(titlePos, "SHOP", textures.font, 
    glm::vec4(1.0f, 0.9f, 0.2f, 1.0f), 1.2f, 4.f, 3.f, false);

// Money display
char moneyText[64];
snprintf(moneyText, sizeof(moneyText), "Money: $%d", playerMoney);
glm::vec2 moneyPos = glm::vec2(shopX + shopW * 0.35f, shopY + 70.0f);
renderer.renderText(moneyPos, moneyText, textures.font, 
    glm::vec4(0.2f, 1.0f, 0.2f, 1.0f), 0.9f, 4.f, 3.f, false);

// Selection highlight
if (i == selectedIndex) {
    auto highlightBox = Ui::Box()
        .xLeft(itemX - 10.0f)
        .yTop(itemY - 5.0f)
        .xDimensionPixels(shopW - 80.0f)
        .yDimensionPixels(75.0f);
    renderer.renderRectangle(highlightBox, {0.3f, 0.3f, 0.5f, 0.5f});
}
```

---

## 6. Best Practices from This Project

### ✅ DO:
1. **Use percentages for responsive elements**
   - Health icons: `xLeftPerc(0.95)`
   - Centered text: `0.38f * renderer.windowW`

2. **Use pixels for fixed offsets**
   - Padding: `yTop(80.0f)` for consistent spacing
   - Text alignment: `renderer.windowW - 250.0f`

3. **Combine both for complex layouts**
   ```cpp
   float shopX = 0.15f * renderer.windowW;  // Responsive position
   float itemY = shopY + 80.0f;             // Fixed offset
   ```

4. **Use alpha for overlays**
   ```cpp
   renderer.renderRectangle(overlayPos, {0.0f, 0.0f, 0.0f, 0.8f});
   // 0.8 alpha = semi-transparent
   ```

5. **Maintain aspect ratio for icons**
   ```cpp
   box.xDimensionPercentage(0.04).yAspectRatio(1.f);  // Square
   ```

### ❌ DON'T:
1. **Don't mix coordinate systems without conversion**
   - Text uses screen-space pixels
   - Game objects use world-space coordinates
   - Use `camera.convertPoint()` to convert

2. **Don't hardcode screen dimensions**
   - ❌ `glm::vec2(1920/2, 1080/2)`
   - ✅ `glm::vec2(renderer.windowW * 0.5f, renderer.windowH * 0.5f)`

3. **Don't forget to update window metrics**
   ```cpp
   renderer.updateWindowMetrics(w, h);  // Call every frame
   ```

---

## 7. Coordinate Systems

### Screen-Space (UI)
- **Origin**: Top-left corner `(0, 0)`
- **Range**: `(0, 0)` to `(windowW, windowH)`
- **Used for**: UI elements, text, HUD

### World-Space (Game)
- **Origin**: Game world origin
- **Range**: Determined by game map/level
- **Used for**: Players, bullets, enemies
- **Conversion**: `camera.convertPoint(screenPos, w, h)`

### Example: Mouse to World Conversion
```cpp
auto mousePos = platform::getRelMousePosition();  // Screen-space
glm::vec2 mouseWorldPos = renderer.currentCamera.convertPoint(
    mousePos, renderer.windowW, renderer.windowH
);  // World-space

// Calculate bullet direction
glm::vec2 playerCenter = (player.pos + player.dimensions / 2.f) * worldMagnification;
glm::vec2 direction = glm::normalize(mouseWorldPos - playerCenter);
```

---

## 8. Summary

**UI System Architecture:**
```
User Input → Ui::Box (Layout) → Renderer2D (OpenGL) → Screen
                ↓
        Position Calculations
    (percentage/pixels/aspect)
```

**Key Takeaways:**
1. **Ui::Box**: Fluent API for positioning with chaining methods
2. **gl2d Renderer2D**: Low-level OpenGL rendering with batching
3. **Hybrid approach**: Mix percentages (responsive) + pixels (consistent spacing)
4. **Color format**: `glm::vec4 {r, g, b, a}` with 0-1 range
5. **Two coordinate systems**: Screen-space (UI) vs World-space (game)

This system is **lightweight**, **flexible**, and **performant** for 2D games!
