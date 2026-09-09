#pragma once
struct Vec2 { float x, y; };
struct Camera {
  Vec2 pos;
  Vec2 dir;
  Vec2 plane;
  void set_pose(Vec2 p, Vec2 d);
};
