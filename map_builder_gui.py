#!/usr/bin/env python3
"""
Interactive Map Builder for NetworkProgrammingProject
Creates .bin game map files with a visual grid editor.
"""

import tkinter as tk
from tkinter import messagebox, filedialog
import struct
import os

# Constants
DEFAULT_WIDTH = 50
DEFAULT_HEIGHT = 50
CELL_SIZE = 15  # pixels per grid cell
PADDING = 20

# Tile types
EMPTY = 0  # non-collidable (default)
WALL = 1   # collidable

# Collision map matching C++ tiles::collisionMap
# This string maps tile IDs (0-79) to collision: '-' = non-collidable, 'X' = collidable
COLLISION_MAP = (
    "-X------"  # Tiles 0-7
    "-----X--"  # Tiles 8-15
    "-X-XXX--"  # Tiles 16-23
    "-X-XXX--"  # Tiles 24-31
    "-XXXXX-X"  # Tiles 32-39
    "XXXXXXXX"  # Tiles 40-47
    "------XX"  # Tiles 48-55
    "--------"  # Tiles 56-63
    "--------"  # Tiles 64-71
    "--XX--XX"  # Tiles 72-79
)

def is_tile_collidable(tile_id):
    """Check if a tile ID is collidable based on collision map"""
    if 0 <= tile_id < len(COLLISION_MAP):
        return COLLISION_MAP[tile_id] == 'X'
    return False

# Colors
COLOR_EMPTY = "#E8E8E8"      # Light gray for non-collidable
COLOR_WALL = "#2C3E50"       # Dark blue for collidable
COLOR_GRID = "#CCCCCC"       # Grid lines
COLOR_HOVER = "#3498DB"      # Hover highlight


class MapBuilder:
    def __init__(self, root):
        self.root = root
        self.root.title("Map Builder - NetworkProgrammingProject")
        
        # Map data
        self.width = DEFAULT_WIDTH
        self.height = DEFAULT_HEIGHT
        self.map_data = [EMPTY] * (self.width * self.height)
        self.current_brush = WALL  # Start with wall brush
        
        # UI setup
        self.setup_ui()
        
        # Try to load existing map data on startup
        self.load_map_on_startup()
        
        self.draw_grid()
        
    def setup_ui(self):
        # Top control panel
        control_frame = tk.Frame(self.root)
        control_frame.pack(side=tk.TOP, fill=tk.X, padx=5, pady=5)
        
        # Brush selection
        tk.Label(control_frame, text="Brush:").pack(side=tk.LEFT, padx=5)
        
        self.brush_var = tk.IntVar(value=WALL)
        tk.Radiobutton(control_frame, text="Wall (Collidable)", 
                      variable=self.brush_var, value=WALL,
                      command=self.change_brush).pack(side=tk.LEFT)
        tk.Radiobutton(control_frame, text="Empty (Non-Collidable)", 
                      variable=self.brush_var, value=EMPTY,
                      command=self.change_brush).pack(side=tk.LEFT)
        
        # Tools
        tk.Button(control_frame, text="Clear All", 
                 command=self.clear_map).pack(side=tk.LEFT, padx=10)
        tk.Button(control_frame, text="Fill Border", 
                 command=self.fill_border).pack(side=tk.LEFT)
        tk.Button(control_frame, text="Save Map", 
                 command=self.save_map).pack(side=tk.LEFT, padx=10)
        tk.Button(control_frame, text="Load Map", 
                 command=self.load_map).pack(side=tk.LEFT)
        tk.Button(control_frame, text="New Map", 
                 command=self.new_map).pack(side=tk.LEFT, padx=10)
        
        # Info label
        self.info_label = tk.Label(control_frame, text=f"Size: {self.width}×{self.height}")
        self.info_label.pack(side=tk.RIGHT, padx=5)
        
        # Canvas frame with scrollbars
        canvas_frame = tk.Frame(self.root)
        canvas_frame.pack(fill=tk.BOTH, expand=True)
        
        # Create scrollbars
        v_scrollbar = tk.Scrollbar(canvas_frame, orient=tk.VERTICAL)
        v_scrollbar.pack(side=tk.RIGHT, fill=tk.Y)
        
        h_scrollbar = tk.Scrollbar(canvas_frame, orient=tk.HORIZONTAL)
        h_scrollbar.pack(side=tk.BOTTOM, fill=tk.X)
        
        # Create canvas
        canvas_width = min(800, self.width * CELL_SIZE + PADDING * 2)
        canvas_height = min(600, self.height * CELL_SIZE + PADDING * 2)
        
        self.canvas = tk.Canvas(canvas_frame, 
                               width=canvas_width,
                               height=canvas_height,
                               bg="white",
                               scrollregion=(0, 0, 
                                           self.width * CELL_SIZE + PADDING * 2,
                                           self.height * CELL_SIZE + PADDING * 2),
                               yscrollcommand=v_scrollbar.set,
                               xscrollcommand=h_scrollbar.set)
        self.canvas.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)
        
        v_scrollbar.config(command=self.canvas.yview)
        h_scrollbar.config(command=self.canvas.xview)
        
        # Bind mouse events
        self.canvas.bind("<Button-1>", self.on_click)
        self.canvas.bind("<B1-Motion>", self.on_drag)
        self.canvas.bind("<Motion>", self.on_hover)
        
        # Status bar
        status_frame = tk.Frame(self.root)
        status_frame.pack(side=tk.BOTTOM, fill=tk.X)
        
        self.status_label = tk.Label(status_frame, text="Ready", anchor=tk.W)
        self.status_label.pack(fill=tk.X, padx=5, pady=2)
        
    def draw_grid(self):
        """Draw the entire grid"""
        self.canvas.delete("all")
        
        # Draw cells
        for y in range(self.height):
            for x in range(self.width):
                self.draw_cell(x, y)
        
        # Draw grid lines
        for x in range(self.width + 1):
            x_pos = PADDING + x * CELL_SIZE
            self.canvas.create_line(x_pos, PADDING, 
                                   x_pos, PADDING + self.height * CELL_SIZE,
                                   fill=COLOR_GRID, tags="grid")
        
        for y in range(self.height + 1):
            y_pos = PADDING + y * CELL_SIZE
            self.canvas.create_line(PADDING, y_pos,
                                   PADDING + self.width * CELL_SIZE, y_pos,
                                   fill=COLOR_GRID, tags="grid")
    
    def draw_cell(self, x, y):
        """Draw a single cell"""
        idx = y * self.width + x
        if idx >= len(self.map_data):
            return
            
        tile_type = self.map_data[idx]
        # Check if tile is collidable using collision map (for loading existing maps)
        # or if it's explicitly set to WALL (for new maps)
        is_collidable = (tile_type == WALL) or is_tile_collidable(tile_type)
        color = COLOR_WALL if is_collidable else COLOR_EMPTY
        
        x1 = PADDING + x * CELL_SIZE + 1
        y1 = PADDING + y * CELL_SIZE + 1
        x2 = x1 + CELL_SIZE - 1
        y2 = y1 + CELL_SIZE - 1
        
        self.canvas.create_rectangle(x1, y1, x2, y2, 
                                     fill=color, outline="",
                                     tags=f"cell_{x}_{y}")
    
    def get_cell_coords(self, event):
        """Convert canvas coordinates to grid coordinates"""
        canvas_x = self.canvas.canvasx(event.x)
        canvas_y = self.canvas.canvasy(event.y)
        
        x = int((canvas_x - PADDING) / CELL_SIZE)
        y = int((canvas_y - PADDING) / CELL_SIZE)
        
        if 0 <= x < self.width and 0 <= y < self.height:
            return x, y
        return None, None
    
    def on_click(self, event):
        """Handle mouse click"""
        x, y = self.get_cell_coords(event)
        if x is not None:
            self.set_cell(x, y, self.current_brush)
    
    def on_drag(self, event):
        """Handle mouse drag"""
        x, y = self.get_cell_coords(event)
        if x is not None:
            self.set_cell(x, y, self.current_brush)
    
    def on_hover(self, event):
        """Update status on hover"""
        x, y = self.get_cell_coords(event)
        if x is not None:
            idx = y * self.width + x
            tile_type = self.map_data[idx]
            is_collidable = (tile_type == WALL) or is_tile_collidable(tile_type)
            tile_name = "Wall (Collidable)" if is_collidable else "Empty (Non-Collidable)"
            tile_info = f"Tile ID: {tile_type}" if tile_type != WALL and tile_type != EMPTY else ""
            self.status_label.config(text=f"Cell ({x}, {y}) - Current: {tile_name} {tile_info}".strip())
        else:
            self.status_label.config(text="Ready")
    
    def set_cell(self, x, y, tile_type):
        """Set a cell to a specific tile type"""
        idx = y * self.width + x
        if self.map_data[idx] != tile_type:
            self.map_data[idx] = tile_type
            self.draw_cell(x, y)
    
    def change_brush(self):
        """Change the current brush type"""
        self.current_brush = self.brush_var.get()
    
    def clear_map(self):
        """Clear the entire map"""
        if messagebox.askyesno("Clear Map", "Clear entire map?"):
            self.map_data = [EMPTY] * (self.width * self.height)
            self.draw_grid()
            self.status_label.config(text="Map cleared")
    
    def fill_border(self):
        """Fill the border with walls"""
        for y in range(self.height):
            for x in range(self.width):
                if x == 0 or y == 0 or x == self.width - 1 or y == self.height - 1:
                    idx = y * self.width + x
                    self.map_data[idx] = WALL
        self.draw_grid()
        self.status_label.config(text="Border filled")
    
    def save_map(self):
        """Save map to binary file"""
        default_path = os.path.join(os.path.dirname(__file__), 
                                   "resources", "mapData2.bin")
        
        filepath = filedialog.asksaveasfilename(
            defaultextension=".bin",
            initialfile=default_path,
            filetypes=[("Binary Map Files", "*.bin"), ("All Files", "*.*")],
            title="Save Map"
        )
        
        if filepath:
            try:
                os.makedirs(os.path.dirname(filepath), exist_ok=True)
                
                with open(filepath, 'wb') as f:
                    # Write width and height as unsigned bytes
                    f.write(struct.pack('B', self.width))
                    f.write(struct.pack('B', self.height))
                    # Write tile data
                    f.write(bytes(self.map_data))
                
                size = os.path.getsize(filepath)
                wall_count = sum(1 for t in self.map_data if (t == WALL) or is_tile_collidable(t))
                self.status_label.config(
                    text=f"Saved to {filepath} ({size} bytes, {wall_count} collidable tiles)"
                )
                messagebox.showinfo("Success", f"Map saved successfully!\n{filepath}")
            except Exception as e:
                messagebox.showerror("Error", f"Failed to save map:\n{str(e)}")
    
    def load_map_file(self, filepath, show_error=True):
        """Load map from binary file (internal method)"""
        if not filepath or not os.path.exists(filepath):
            return False
        
        try:
            with open(filepath, 'rb') as f:
                # Read width and height
                width = struct.unpack('B', f.read(1))[0]
                height = struct.unpack('B', f.read(1))[0]
                # Read tile data
                data = list(f.read(width * height))
            
            # Update map
            self.width = width
            self.height = height
            self.map_data = data
            
            # Update canvas size
            self.canvas.config(scrollregion=(0, 0,
                                            self.width * CELL_SIZE + PADDING * 2,
                                            self.height * CELL_SIZE + PADDING * 2))
            
            # Redraw
            self.info_label.config(text=f"Size: {self.width}×{self.height}")
            self.draw_grid()
            
            # Count collidable tiles (both WALL=1 and other collidable tile IDs)
            wall_count = sum(1 for t in self.map_data if (t == WALL) or is_tile_collidable(t))
            self.status_label.config(
                text=f"Loaded {filepath} ({wall_count} collidable tiles)"
            )
            return True
        except Exception as e:
            if show_error:
                messagebox.showerror("Error", f"Failed to load map:\n{str(e)}")
            return False
    
    def load_map_on_startup(self):
        """Automatically load mapData2.bin on startup if it exists"""
        default_path = os.path.join(os.path.dirname(__file__), 
                                   "resources", "mapData2.bin")
        
        if os.path.exists(default_path):
            self.load_map_file(default_path, show_error=False)
    
    def load_map(self):
        """Load map from binary file (with file dialog)"""
        default_path = os.path.join(os.path.dirname(__file__), 
                                   "resources", "mapData2.bin")
        
        filepath = filedialog.askopenfilename(
            defaultextension=".bin",
            initialfile=default_path,
            filetypes=[("Binary Map Files", "*.bin"), ("All Files", "*.*")],
            title="Load Map"
        )
        
        if filepath:
            self.load_map_file(filepath, show_error=True)
    
    def new_map(self):
        """Create a new map with custom dimensions"""
        dialog = tk.Toplevel(self.root)
        dialog.title("New Map")
        dialog.geometry("300x150")
        dialog.resizable(False, False)
        
        tk.Label(dialog, text="Width:").grid(row=0, column=0, padx=10, pady=10, sticky=tk.W)
        width_entry = tk.Entry(dialog)
        width_entry.insert(0, str(self.width))
        width_entry.grid(row=0, column=1, padx=10, pady=10)
        
        tk.Label(dialog, text="Height:").grid(row=1, column=0, padx=10, pady=10, sticky=tk.W)
        height_entry = tk.Entry(dialog)
        height_entry.insert(0, str(self.height))
        height_entry.grid(row=1, column=1, padx=10, pady=10)
        
        def create():
            try:
                w = int(width_entry.get())
                h = int(height_entry.get())
                
                if w < 1 or w > 255 or h < 1 or h > 255:
                    messagebox.showerror("Error", "Dimensions must be between 1 and 255")
                    return
                
                self.width = w
                self.height = h
                self.map_data = [EMPTY] * (self.width * self.height)
                
                # Update canvas size
                self.canvas.config(scrollregion=(0, 0,
                                                self.width * CELL_SIZE + PADDING * 2,
                                                self.height * CELL_SIZE + PADDING * 2))
                
                self.info_label.config(text=f"Size: {self.width}×{self.height}")
                self.draw_grid()
                self.status_label.config(text=f"Created new {w}×{h} map")
                dialog.destroy()
            except ValueError:
                messagebox.showerror("Error", "Please enter valid numbers")
        
        button_frame = tk.Frame(dialog)
        button_frame.grid(row=2, column=0, columnspan=2, pady=20)
        
        tk.Button(button_frame, text="Create", command=create, width=10).pack(side=tk.LEFT, padx=5)
        tk.Button(button_frame, text="Cancel", command=dialog.destroy, width=10).pack(side=tk.LEFT, padx=5)


def main():
    root = tk.Tk()
    app = MapBuilder(root)
    root.mainloop()


if __name__ == "__main__":
    main()
