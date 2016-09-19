WoodSlipper 2000
================

Simple program to insert various size rectangles optimally into a bigger rectangular area.
The program outputs a G-code to be used with a Morbidelli CNC-machine.
Also features a simple OpenGL visualization during the process.

This program was created to help my brother at work. It's designed to be located at
the Windows desktop, and to be launched by dragging a .set file over it.


### .set file: (all measurements in millimeters)

 * WOOD_SIZE is the size of the wood in machine.
 * MILL_RADIUS determines the drill bit radius (distance between blocks)
 * SLOWNESS controls the algorithm, more sloweness = better placement
 * BLOCK specifies one rectangle which will be placed on the wood.


