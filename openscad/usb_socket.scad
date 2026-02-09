include <BOSL2/std.scad>

module usb_socket(usb_socket, socket_cube, front_scale_factor, wall_thickness, ring_height, show_top, anchor = CENTER, spin = 0, orient = UP) {
  bottom_width = 5;
  top_section_height = 1.5;
  bottom_section_height = usb_socket.y - top_section_height;
  bottom_inset = (usb_socket.x - bottom_width) / 2;

  module full_socket() {
    difference() {
      // Socket cube
      cube(socket_cube, center=true);

      // Socket void
      back(wall_thickness / 2 + 0.01) xrot(90) linear_extrude(h=wall_thickness + 0.02, scale=front_scale_factor) {
            left(usb_socket.x / 2) fwd(usb_socket.y / 2) round2d(0.5) polygon(
                    [
                      [0, usb_socket.y],
                      [usb_socket.x, usb_socket.y],
                      [usb_socket.x, top_section_height],
                      [usb_socket.x - bottom_inset, 0],
                      [bottom_inset, 0],
                      [0, bottom_section_height],
                    ]
                  );
          }
    }
  }

  attachable(anchor, spin, orient, size=[socket_cube.x, socket_cube.y, (socket_cube.z / 2)]) {
    up(ring_height / 4) {
      // Show the top half or bottom dependent on show_top
      if (show_top) {
        zflip() top_half() full_socket();
      } else {
        bottom_half() full_socket();
      }
    } 

    children();
  }
}
