// ps2x2pico case
// © dwhweb 2026 by the terms of the MIT licence

include <BOSL2/std.scad>
include <bottom.scad>
include <top.scad>


// Variables for customiser

// Show the top part of the case
show_top = true;

// Show the bottom part of the case
show_bottom = true;

// Add a pinhole so the BOOTSEL button can be pressed, for programming
pinhole = true;

// Diameter of the pinhole
pinhole_diameter = 2;

// Height of the middle ring section of the case
ring_height = 5;

// Diameter of the cable holes
cable_diameter = 4;

// Amount to adjust the top support posts by
support_adjustment = 0;

/* [Hidden] */
$fn = $preview ? 50 : 200;

module ps2x2pico_case() {
  assert(ring_height >= cable_diameter, "The height of the ring must be greater than or equal to the diameter of the cable holes.");

  // Chamfered section variables
  wall_thickness = 3; // Wall thickness at top, also used for the rings 
  chamfer_top_dimensions = [30, 70];
  chamfer_width = 1; // Distance top tapers inward to bottom
  chamfer_bottom_dimensions = [chamfer_top_dimensions.x - (2 * chamfer_width), chamfer_top_dimensions.y - (2 * chamfer_width)];
  void_top = [chamfer_top_dimensions.x - (2 * wall_thickness), chamfer_top_dimensions.y - (2 * wall_thickness)]; // Hollow dimensions at top
  void_bottom = [chamfer_bottom_dimensions.x - (2 * wall_thickness), chamfer_bottom_dimensions.y - (2 * wall_thickness)]; // Hollow dimensions at bottom
  chamfer_height = 5; // Total height of chamfered section
  base_height = 2; // Base thickness

  // Outer ring variables
  outer_ring_dimensions = [chamfer_top_dimensions.x, chamfer_top_dimensions.y, ring_height];
  rounding = 2; // Also used for inner ring and chamfered section

  // Inner ring variables
  inner_ring_dimensions = [outer_ring_dimensions.x - wall_thickness, outer_ring_dimensions.y - wall_thickness, ring_height];

  // Usb socket variables
  usb_socket = [8, 3]; // USB socket xy
  front_scale_factor = 1.5; // Amount to scale the USB connector hole for the inward taper
  socket_cube = [usb_socket.x * front_scale_factor + 2, wall_thickness, ring_height]; // Total dimensions/bounding box of the USB socket section
  usb_socket_bottom = (ring_height - usb_socket.y) / 2; // Offset from bottom of rings to the bottom of the USB socket hole

  // Support posts variables
  post_intervals = [11.4, 47]; // From the Pi Pico datasheet, xy spacing between posts 
  support_diameter = 4; // Diameter of larger bottom section of the post
  pico_height = 1; // Height of the Pi Pico
  retention_height = pico_height; // Height of the smaller top section of the post
  retention_diameter = 2;
  support_height = chamfer_height - base_height + usb_socket_bottom - retention_height; // Height of the larger bottom section of the post, brings the USB socket in line with the hole
  total_post_height = support_height + retention_height; // Total height of a support post
  posts_bounding_box = [post_intervals.x + support_diameter, post_intervals.y + support_diameter, total_post_height]; // Bounding box of all the support posts, for attachable()
  shift_distance = (void_bottom.y / 2) - (posts_bounding_box.y / 2); // Distance to shift the posts forward such the they touch the edge of the bottom of the chamfered section

  // Cable holes variables
  hole_spacing = 1;
  cable_holes_cube = [(2 * cable_diameter) + hole_spacing + 2, wall_thickness, ring_height];// Total dimensions/bounding box of the cable holes section

  // Placement variables
  spacing = 5; // Spacing between top and bottom
  placement_offset = 0.5 * (chamfer_top_dimensions.x + spacing); // Actual distance OpenSCAD needs to move the parts

  // Case bottom
  if(show_bottom) {
    left(placement_offset) bottom(inner_ring_dimensions, rounding, wall_thickness, socket_cube, cable_holes_cube, usb_socket, front_scale_factor, ring_height, cable_diameter, 
        hole_spacing, chamfer_bottom_dimensions, chamfer_height, chamfer_top_dimensions, void_top, void_bottom, base_height, shift_distance, post_intervals, support_diameter, 
        support_height, retention_height, retention_diameter, total_post_height, posts_bounding_box, pico_height, support_adjustment);
  }

  // Case top
  if(show_top) {
    right(placement_offset) top(inner_ring_dimensions, outer_ring_dimensions, rounding, wall_thickness, socket_cube, cable_holes_cube, usb_socket, front_scale_factor, ring_height, 
        cable_diameter, hole_spacing, chamfer_bottom_dimensions, chamfer_height, chamfer_top_dimensions, void_top, void_bottom, base_height, shift_distance, post_intervals, 
        support_diameter, support_height, retention_height, retention_diameter, total_post_height, posts_bounding_box, pico_height, support_adjustment, pinhole_diameter, pinhole);
  }
}

ps2x2pico_case();
