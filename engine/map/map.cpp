#include "map.hpp"
#include <cctype>
#include <fstream>
#include <sstream>
#include <string>

bool Map::load(const char* path) {
  std::ifstream in(path);
  if (!in) return false;

  width_ = 0;
  height_ = 0;
  tiles_.clear();
  spawns_.clear();

  auto is_grid_line = [](const std::string& s) {
    bool any = false;
    for (char c : s) {
      if (c == ' ' || c == '\t') continue;
      // '#' is Wall in the grid; do not treat grid rows as comments
      if (c != '.' && c != '#' && c != 'D' && c != 'O') return false;
      any = true;
    }
    return any;
  };

  std::vector<std::string> grid_rows;
  std::string line;
  while (std::getline(in, line)) {
    // Trim trailing CR
    if (!line.empty() && line.back() == '\r') line.pop_back();
    if (line.empty()) continue;

    std::string code;
    if (is_grid_line(line)) {
      // Grid rows may start with '#' (wall); skip comment stripping
      code = line;
    } else {
      // Strip comments: full-line # or trailing (metadata / notes only)
      auto hash = line.find('#');
      code = (hash == 0) ? std::string()
           : (hash == std::string::npos ? line : line.substr(0, hash));
    }

    // Trim
    auto l = code.find_first_not_of(" \t");
    if (l == std::string::npos) continue;
    code = code.substr(l);
    auto r = code.find_last_not_of(" \t");
    code = code.substr(0, r + 1);

    std::istringstream iss(code);
    std::string tok;
    if (!(iss >> tok)) continue;

    if (tok == "width") {
      iss >> width_;
    } else if (tok == "height") {
      iss >> height_;
    } else if (tok.size() == 1 && (tok[0] == 'P' || tok[0] == 'M' || tok[0] == 'I' || tok[0] == 'E')) {
      float x = 0, y = 0;
      int type_id = 0;
      iss >> x >> y >> type_id;
      EntitySpawn s;
      s.pos = {x, y};
      s.type_id = type_id;
      switch (tok[0]) {
        case 'P': s.kind = EntitySpawn::Kind::PlayerStart; break;
        case 'M': s.kind = EntitySpawn::Kind::Monster; break;
        case 'I': s.kind = EntitySpawn::Kind::Item; break;
        case 'E': s.kind = EntitySpawn::Kind::Door; break;
        default: break;
      }
      spawns_.push_back(s);
    } else if (is_grid_line(code)) {
      std::string row;
      for (char c : code) {
        if (c == ' ' || c == '\t') continue;
        row.push_back(c);
      }
      if (!row.empty()) grid_rows.push_back(row);
    }
  }

  if (width_ <= 0 || height_ <= 0) return false;
  tiles_.assign(static_cast<size_t>(width_ * height_), Tile::Empty);

  for (int y = 0; y < height_ && y < static_cast<int>(grid_rows.size()); ++y) {
    const std::string& row = grid_rows[static_cast<size_t>(y)];
    for (int x = 0; x < width_ && x < static_cast<int>(row.size()); ++x) {
      Tile t = Tile::Empty;
      switch (row[static_cast<size_t>(x)]) {
        case '#': t = Tile::Wall; break;
        case 'D': t = Tile::DoorClosed; break;
        case 'O': t = Tile::DoorOpen; break;
        default: t = Tile::Empty; break;
      }
      tiles_[static_cast<size_t>(y * width_ + x)] = t;
    }
  }
  return true;
}

int Map::width() const { return width_; }
int Map::height() const { return height_; }

Tile Map::tile_at(int tx, int ty) const {
  if (tx < 0 || ty < 0 || tx >= width_ || ty >= height_) return Tile::Wall;
  return tiles_[static_cast<size_t>(ty * width_ + tx)];
}

bool Map::is_solid(int tx, int ty) const {
  const Tile t = tile_at(tx, ty);
  return t == Tile::Wall || t == Tile::DoorClosed;
}

bool Map::world_to_tile(Vec2 p, int& tx, int& ty) const {
  tx = static_cast<int>(p.x);
  ty = static_cast<int>(p.y);
  if (p.x < 0.0f || p.y < 0.0f) return false;
  return tx >= 0 && ty >= 0 && tx < width_ && ty < height_;
}

const std::vector<EntitySpawn>& Map::spawns() const { return spawns_; }

bool Map::try_set_tile(int tx, int ty, Tile t) {
  if (tx < 0 || ty < 0 || tx >= width_ || ty >= height_) return false;
  tiles_[static_cast<size_t>(ty * width_ + tx)] = t;
  return true;
}
