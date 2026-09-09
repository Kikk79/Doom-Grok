#pragma once
#include "../camera/camera.hpp"
#include <cstdint>
#include <vector>
enum class Tile : uint8_t { Empty=0, Wall=1, DoorClosed=2, DoorOpen=3 };
struct EntitySpawn {
  enum class Kind : uint8_t { PlayerStart, Monster, Item, Door } kind;
  Vec2 pos;
  int type_id;
};
class Map {
public:
  bool load(const char* path);
  int width() const; int height() const;
  Tile tile_at(int tx, int ty) const; // OOB → Wall
  bool is_solid(int tx, int ty) const; // Wall + DoorClosed; DoorOpen is walkable
  bool world_to_tile(Vec2 p, int& tx, int& ty) const;
  const std::vector<EntitySpawn>& spawns() const;

  // Slice 3 — minimal runtime mutators (do not rewrite the map file).
  // set_tile: false if OOB; otherwise writes tile and returns true.
  bool set_tile(int tx, int ty, Tile t);
  // try_open_door: DoorClosed → DoorOpen only; else false (incl. OOB / wrong tile).
  bool try_open_door(int tx, int ty);
private:
  int width_ = 0;
  int height_ = 0;
  std::vector<Tile> tiles_;
  std::vector<EntitySpawn> spawns_;
};
