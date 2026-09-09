#pragma once

// Slice 9 — minimal procedural SFX via SDL2 audio device.
// Mute-safe: init may fail (no hardware / headless); play() is then a no-op.
// Public game / AI / other engine APIs unchanged.
namespace Audio {

enum class Sfx : int {
  Fire = 0,
  Door,
  Hurt,
  Pickup,
  MonsterDeath,
  Win,
};

// Open SDL audio device and generate short beep buffers.
// Returns true if playback is available. On failure the module stays muted.
bool init();

void shutdown();

bool available();

// Queue a short generated SFX. No-op if muted / not initialized.
void play(Sfx s);

}  // namespace Audio
