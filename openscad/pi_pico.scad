include <BOSL2/std.scad>

module pi_pico(pico_height, anchor = CENTER, spin = 0, orient = UP) {
  // From the Pi Pico datasheet
  pcb_dimensions = [21, 51, pico_height];
  hole_diameter = 2.1;
  hole_spacing = 11.4;

  attachable(anchor, spin, orient, size=pcb_dimensions) {
    diff() {
      // PCB
      tag("keep") color_this("green") cuboid(pcb_dimensions) {
          // Micro USB socket
          align(TOP, FRONT) {
            fwd(1.3) {
              color_this("gray") cuboid([8, 5.75, 3]);
            }
            // BOOTSEL switch
            back(10) right(3.75) color_this("white") cuboid([3.35, 4.5, 2]);
          }
          // Mounting holes
          up(0.1) {
            align(TOP, FRONT, inset=-(hole_diameter / 2), inside=true) {
              back(2) {
                left(hole_spacing / 2) cylinder(h=pcb_dimensions.z + 1, d=hole_diameter);
                right(hole_spacing / 2) cylinder(h=pcb_dimensions.z + 1, d=hole_diameter);
              }
            }
            align(TOP, BACK, inset=-(hole_diameter / 2), inside=true) {
              fwd(2) {
                left(hole_spacing / 2) cylinder(h=pcb_dimensions.z + 1, d=hole_diameter);
                right(hole_spacing / 2) cylinder(h=pcb_dimensions.z + 1, d=hole_diameter);
              }
            }
          }
        }
    }

    children();
  }
}
