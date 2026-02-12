include <BOSL2/std.scad>

module cable_holes(cable_holes_cube, cable_diameter, hole_spacing, anchor = CENTER, spin = 0, orient = UP) {
  module holes() {
    diff() {
      cube(cable_holes_cube, center=true);
      tag("remove") left((cable_diameter / 2) + (hole_spacing / 2)) cyl(h=cable_holes_cube.y + 0.02, d=cable_diameter, orient=BACK);
      tag("remove") right((cable_diameter / 2) + (hole_spacing / 2)) cyl(h=cable_holes_cube.y + 0.02, d=cable_diameter, orient=BACK);
    }
  }

  attachable(anchor, spin, orient, size=[cable_holes_cube.x, cable_holes_cube.y, cable_holes_cube.z / 2]) {
    up(cable_holes_cube.z / 4) bottom_half() holes();

    children();
  }
}
