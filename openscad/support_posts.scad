include <BOSL2/std.scad>

module support_posts(post_intervals, support_diameter, support_height, retention_height, total_height, bounding_box, retention_diameter, support_adjustment, back_posts=true, anchor = CENTER, spin = 0, orient = UP) {
  module support_post(support_height, support_diameter, retention_height, retention_diameter, back_posts=true) {
    cylinder(d=support_diameter, h=support_height) {
      if(back_posts) {
        attach(TOP, BOT) {
          cylinder(d=retention_diameter, h=retention_height);
        }
      }
    }
  }

  attachable(anchor, spin, orient, size=bounding_box) {
    down(total_height / 2) {
      fwd(post_intervals.y / 2) {
        // Only (potentially) adjust the height of the support posts if there are only front posts (i.e. this is the top)
        if(!back_posts) {
          left(post_intervals.x / 2) support_post(support_height + support_adjustment, support_diameter, retention_height, retention_diameter, back_posts);
          right(post_intervals.x / 2) support_post(support_height + support_adjustment, support_diameter, retention_height, retention_diameter, back_posts);
        } else {
          left(post_intervals.x / 2) support_post(support_height, support_diameter, retention_height, retention_diameter, back_posts);
          right(post_intervals.x / 2) support_post(support_height, support_diameter, retention_height, retention_diameter, back_posts);
        }
      }

      if(back_posts) {
        back(post_intervals.y / 2) {
          left(post_intervals.x / 2) support_post(support_height, support_diameter, retention_height, retention_diameter);
          right(post_intervals.x / 2) support_post(support_height, support_diameter, retention_height, retention_diameter);
        }
      }
    }
    children();
  }
}
