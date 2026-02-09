include <BOSL2/std.scad>

module case_ring(dimensions, rounding, wall_thickness, socket_cube, cable_holes_cube, anchor=CENTER, spin=0, orient=UP) {
  void_dimensions = [dimensions.x - wall_thickness, dimensions.y - wall_thickness, dimensions.z + 1];

  attachable(anchor, spin, orient, size=dimensions) {
    diff() {
      cuboid(dimensions, rounding=rounding, edges=[FRONT + LEFT, FRONT + RIGHT, BACK + LEFT, BACK + RIGHT]) {
        tag("remove") align(FRONT, inside=true, shiftout=0.01) cube([socket_cube.x, wall_thickness, dimensions.z + 0.02], center=true); // USB socket gap
        tag("remove") align(BACK, inside=true, shiftout=0.01) cube([cable_holes_cube.x, wall_thickness, dimensions.z + 0.02], center=true); // Cable holes gap
        tag("remove") down(0.1) cuboid(void_dimensions, rounding=rounding, edges=[FRONT + LEFT, FRONT + RIGHT, BACK + LEFT, BACK + RIGHT]); // Centre void
      }
    }

    children();
  }
}

module snap_tube(tube_length, diameter, left_side=false, anchor=CENTER, spin=0, orient=UP) {
  bounding_box = [0.5 * diameter, tube_length, diameter]; // For attachable()
  shift_distance = 0.25 * diameter; // X axis correction for attachable()

  attachable(anchor, spin, orient, size=bounding_box) {
    if(left_side) {
      right(shift_distance) left_half() cyl(d=diameter, h=tube_length, orient=FWD);
    } else {
      left(shift_distance) right_half() cyl(d=diameter, h=tube_length, orient=FWD);
    }

    children();
  }
}

module inner_ring(dimensions, rounding, wall_thickness, socket_cube, cable_holes_cube) {
  tube_length = 0.75 * dimensions.y;
  tube_diameter = 1;

  case_ring(dimensions, rounding, wall_thickness, socket_cube, cable_holes_cube) {
    align(LEFT) snap_tube(tube_length, tube_diameter, true);
    align(RIGHT) snap_tube(tube_length, tube_diameter);
  }
}

module outer_ring(outer_ring_dimensions, rounding, wall_thickness, socket_cube, cable_holes_cube, inner_ring_dimensions) {
  difference() {
    case_ring(outer_ring_dimensions, rounding, wall_thickness, socket_cube, cable_holes_cube);
    inner_ring(inner_ring_dimensions, rounding, wall_thickness, socket_cube, cable_holes_cube);
  }
}

