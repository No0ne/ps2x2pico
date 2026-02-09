include <BOSL2/std.scad>

module chamfered_section(chamfer_top_dimensions, bottom_dimensions, wall_thickness, void_top, void_bottom, chamfer_height, base_height, rounding, pinhole_diameter, pinhole=false,
    anchor = CENTER, spin = 0, orient = UP) {
  attachable(anchor, spin, orient, size=point3d(bottom_dimensions, chamfer_height), size2=chamfer_top_dimensions) {
    diff() {
      prismoid(size1=bottom_dimensions, size2=chamfer_top_dimensions, h=chamfer_height, rounding=rounding, center=true)
        // Inner void
        align(BOTTOM, inside=true) {
          up(base_height) {
            prismoid(size1=void_bottom, size2=void_top, h=chamfer_height - base_height + 0.01, rounding=rounding, center=true) {
              // BOOTSEL pinhole
              if(pinhole) {
                align(BOTTOM + FRONT) {
                  up(0.01) back(12.25 + (0.5 * pinhole_diameter)) left(3.75) cyl(h=base_height + 0.02, d=pinhole_diameter); 
                }
              }
            }
          }
        }
    }

    children();
  }
}
