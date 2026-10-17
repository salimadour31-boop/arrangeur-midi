# Composants tiers / Third-party components

| Composant | Rôle | Licence |
|---|---|---|
| [DISTRHO Plugin Framework (DPF)](https://github.com/DISTRHO/DPF) — sous-module `dpf/` | cadre VST3 / VST2 / CLAP / LV2, interface | ISC |
| pugl (inclus dans DPF) | fenêtres | ISC |
| NanoVG (inclus dans DPF) | dessin vectoriel | zlib |
| stb_truetype / stb_image (inclus dans DPF via NanoVG) | polices / images | domaine public / MIT |
| DejaVu Sans (police intégrée à DPF) | texte de l'interface | licence Bitstream Vera / DejaVu (libre) |
| VST3 headers (`travesty`, réécriture indépendante incluse dans DPF) | format VST3 | ISC (DPF) |
| VST2 header (`xaymar-vst2`, réécriture « clean room » incluse dans DPF) | format VST2 | BSD (voir `dpf/distrho/src/xaymar-vst2/LICENSE`) |
| CLAP headers (inclus dans DPF) | format CLAP | MIT |
| LV2 headers (inclus dans DPF) | format LV2 | ISC |

## Khronos OpenGL headers (Windows build only)
`third_party/khronos/GL/glext.h` and `third_party/khronos/KHR/khrplatform.h` — © The Khronos Group Inc., MIT licence (see the file headers). Used unmodified because MSVC does not ship them.

Outil de test (non distribué) : [pluginval](https://github.com/Tracktion/pluginval) (GPLv3), téléchargé par la CI.

La logique musicale (`src/music/`), le plugin (`plugin/`), les tests et les outils sont originaux, sous licence MIT. Les gammes et progressions sont des faits musicaux du domaine commun ; aucune donnée ni interface d'un autre produit n'a été copiée.
