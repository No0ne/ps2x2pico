include <BOSL2/std.scad>
include <chamfered_section.scad>
include <usb_socket.scad>
include <cable_holes.scad>
include <rings.scad>
include <support_posts.scad>

module top(inner_ring_dimensions, outer_ring_dimensions, rounding, wall_thickness, socket_cube, cable_holes_cube, usb_socket, front_scale_factor, ring_height, cable_diameter, 
    hole_spacing, chamfer_bottom_dimensions, chamfer_height, chamfer_top_dimensions, void_top, void_bottom, base_height, shift_distance, post_intervals, support_diameter, 
    support_height, retention_height, retention_diameter, total_post_height, posts_bounding_box, pico_height, support_adjustment, pinhole_diameter, pinhole, anchor = CENTER, spin = 0, orient = UP) {

  attachable(anchor, spin, orient, size=point3d(chamfer_bottom_dimensions, chamfer_height), size2=chamfer_top_dimensions) {
    // Chamfered section
    chamfered_section(chamfer_top_dimensions, chamfer_bottom_dimensions, wall_thickness, void_top, void_bottom, chamfer_height, base_height, rounding, pinhole_diameter, pinhole) {
      align(TOP) outer_ring(outer_ring_dimensions, rounding, wall_thickness, socket_cube, cable_holes_cube, inner_ring_dimensions); // Outer ring
      align(TOP, FRONT) usb_socket(usb_socket, socket_cube, front_scale_factor, wall_thickness, ring_height, true); // Usb socket, top half
      align(TOP, BACK) cable_holes(cable_holes_cube, cable_diameter, hole_spacing); // Cable holes

      align(BOTTOM, inside=true) {
        up(base_height) {
          // Support posts
          fwd(shift_distance) support_posts(post_intervals, support_diameter, support_height, retention_height, total_post_height, posts_bounding_box, retention_diameter, support_adjustment, false);
        }
      }
    }

    children();
  }
}
