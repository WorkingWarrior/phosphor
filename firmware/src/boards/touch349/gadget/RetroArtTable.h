// Pictures per scene and theme, with where their effects go (RetroScene.cpp says what each number means). Measured on
// the previews in design/retro-proto/out; redo them when a picture changes.
// clang-format off
// scene              theme          picture               lcd    p                                                   c
{ RetroArt::Snow,     theme::AMIGA,  &art::art_snow_amiga, false, { 160, 120, 178, 139, 150, 0, 104, 56, 110, 25, 1 }, { rgb(255, 204, 119), rgb(255, 238, 187), rgb(204, 204, 238), rgb(119, 119, 187), rgb(238, 238, 255), rgb(153, 153, 221) } },
{ RetroArt::Snow,     theme::C64,    &art::art_snow_c64,   false, { 176, 130, 200, 138, 150, 0, 120, 80, 80, 0, 1 },   { rgb(184, 199, 111), rgb(154, 103, 89), rgb(149, 149, 149), rgb(108, 108, 108), rgb(255, 255, 255), rgb(149, 149, 149) } },
{ RetroArt::Snow,     theme::ATARI,  &art::art_snow_atari, false, { 144, 128, 160, 134, 140, 1, 112, 92, 45, 0, 2 },   { rgb(252, 188, 116), rgb(236, 140, 60), rgb(170, 170, 170), rgb(110, 110, 110), rgb(236, 236, 236), rgb(236, 236, 236) } },
{ RetroArt::Snow,     theme::LCD,    &art::art_snow_lcd,   true,  { 0, 0, 0, 0, 0, 0, 132, 56 },                       { 0 } },

{ RetroArt::Fire,     theme::AMIGA,  &art::art_fire_amiga, false, { 255, 60, 345, 122, 0, 0, 160, 20, 440, 172 },      { rgb(255, 221, 119), rgb(255, 136, 34), rgb(187, 51, 17), rgb(255, 170, 85) } },
{ RetroArt::Fire,     theme::C64,    &art::art_fire_c64,   false, { 205, 82, 285, 122, 200, 30 },                      { rgb(184, 199, 111), rgb(154, 103, 89), rgb(104, 55, 43) } },
{ RetroArt::Fire,     theme::ATARI,  &art::art_fire_atari, false, { 195, 80, 318, 118 },                               { rgb(252, 224, 140), rgb(252, 144, 72), rgb(200, 72, 24) } },
{ RetroArt::Fire,     theme::LCD,    &art::art_fire_lcd,   true,  { 215, 75, 315, 112 },                               { 0 } },

{ RetroArt::Sea,      theme::AMIGA,  &art::art_sea_amiga,  false, { 178, 30, 95, 150, 150, 168 },                      { rgb(255, 238, 170), rgb(221, 221, 255) } },
{ RetroArt::Sea,      theme::C64,    &art::art_sea_c64,    false, { 150, 36, 90, 140, 140, 165 },                      { rgb(184, 199, 111), rgb(255, 255, 255) } },
{ RetroArt::Sea,      theme::ATARI,  &art::art_sea_atari,  false, { 172, 57, 108, 148, 148, 165 },                     { rgb(252, 224, 140), rgb(236, 236, 236) } },
{ RetroArt::Sea,      theme::LCD,    &art::art_sea_lcd,    true,  { 180, 44, 118, 150 },                               { 0 } },

{ RetroArt::Aquarium, theme::AMIGA,  &art::art_tank_amiga, false, { 0, 10 },                                           { rgb(119, 187, 238) } },
{ RetroArt::Aquarium, theme::C64,    &art::art_tank_c64,   false, { 0, 8 },                                            { 0 } },
{ RetroArt::Aquarium, theme::ATARI,  &art::art_tank_atari, false, { 0, 8 },                                            { 0 } },
{ RetroArt::Aquarium, theme::LCD,    &art::art_tank_lcd,   true,  { 0, 10 },                                           { 0 } },
// clang-format on
