# Auto Deco

Geode mod for Geometry Dash 2.2081. Decorates a layout in one click: pick a style, press Apply.

## What it does

- Finds every 1x1 solid block on the 30-unit grid and replaces it with the right **autotile piece**
  (inner, edge, corner, inner-corner notch, cap, pillar, single) from the chosen style, rotated to fit.
- Recolors hazards (spikes, saws) into the style's hazard channel. Hitboxes are never changed.
- Sprinkles background decoration (seeded per level, so re-applying gives the same result).
- Sets the level's BG / ground / line colours and the style's colour channels.
- Everything it adds is tagged with one free group ID and the original objects are backed up,
  so **Undo deco** restores the exact original layout, even after restarting the game.

Portals, orbs, pads, triggers and anything not on the grid are left alone.

## Using it

In the editor, press the **Deco** button (top-left, next to undo/redo).

- **Selection only** — only restyle selected objects (edges still take unselected neighbours into account).
- **Background deco** — toggle scatter decoration.
- **Recolor level** — toggle palette changes.
- **Restyle hazards** — toggle hazard recolour/remap.
- **Undo deco** — remove everything the mod added and restore the original layout.
- **Save selection as style** — build a small sample by hand (a filled rectangle of your themed blocks
  with a few single blocks / pillars, some spikes, a few background deco objects), select it, and the mod
  learns which object ID plays which autotile role, plus your colours, and writes a style JSON you can edit.
- Folder icon opens your styles folder (`geode/config/brittj.auto-deco/styles`). Reload icon rescans it.

## Style format

```json
{
  "name": "Ocean",
  "description": "…",
  "palette": { "bg": "#0b1a3a", "ground": "#0d2250", "line": "#8fd3ff", "1": "#4fa8ff", "2": "#9fe8ff", "3": "#1f3f7a" },
  "blocks": {
    "inner": 5, "edge": 2, "corner": 3, "innerCorner": 4, "cap": 6, "pillar": 7, "single": 83,
    "baseColor": 1, "detailColor": 1
  },
  "hazards": { "recolor": true, "baseColor": 2, "detailColor": 2, "remap": { "8": 8 } },
  "scatter": [
    { "id": 144, "density": 0.01, "scale": [0.5, 1.2], "randomRotation": false, "zLayer": -3, "zOrder": 0, "color": 3, "clearance": 1 }
  ]
}
```

- Block pieces follow the orientation of the 1.0 grid family: `edge` has its outline on top, `corner`
  on top+left, `cap` is open at the bottom, `pillar` is vertical. A piece can be `{ "id": 123, "rotation": 90 }`
  if your family's reference piece faces another way.
- Missing roles fall back to simpler ones (no `cap` → `corner` → `edge` → `inner`).
- `palette` keys: `bg`, `ground`, `ground2`, `line`, `3dl`, `object`, or a channel number.
- `scatter.zLayer`: B5=-5, B4=-3, B3=-1, B2=1, B1=3, T1=5, T2=7, T3=9.
- A palette entry can be an object: `{ "color": "#4fa8ff", "blending": true, "opacity": 0.6 }` (for glow channels).
- `edgeLayers`: objects placed along every exposed block edge, e.g. outlines and glow:
  `{ "name": "outline", "edge": 1234, "corner": 1235, "innerCorner": 1236, "rotation": 0, "offsetX": 0, "offsetY": 0, "scale": 1, "zLayer": 3, "zOrder": 1, "color": 2 }`.
  `edge` is drawn on the top edge, `corner` at the top-left outer corner, `innerCorner` in a top-left concave corner; the mod rotates them.
- `overlays`: objects stacked on every block cell (`innerOnly: true` for cells with no exposed side):
  `{ "id": 1234, "innerOnly": true, "zLayer": 0, "zOrder": 1, "color": 5 }`.
- `hazardOverlays`: objects placed on every hazard: `{ "id": 1012, "scale": 1.5, "offsetX": 15, "offsetY": 15, "rotation": 90, "zLayer": 1, "color": 4 }`.
  Four `1012` quarter-gradients at rotations 0/90/180/270 with offset (15,15) make a radial glow disc.
- Edge layers accept `scaleX` / `scaleY` for non-uniform scale (e.g. `468` with `scaleY: 3` is a thick outline).

### Bundled styles

| Style | Look |
|---|---|
| White Glow | black silhouettes, thick white outline, white glow, nested-square interiors |
| Monochrome Core | dark-grey fill, thin white outline, grid interiors, light beams, no glow |
| Hellfire | black silhouettes, orange outline + heavy glow, embers, lava ground |
| Classic Dark / Classic Grid / Ocean | 1.0 grid block families |

Useful object IDs (from the game's own sprite table): `211` solid square, `468` 30x2 line, `69` outline square,
`1011` 30x30 gradient (bright at bottom), `1012` corner gradient (bright at bottom-left), `503`/`504` 20px versions,
`1764` small dot, `917` small square, `1521`/`1019` light beams, `54` star, `1004` thin line.
- The **IDs** button writes `object-table.tsv` (id, sprite frame, size) to the config folder, for authoring styles by hand.

## Building

Standard Geode project (SDK 5.11.2). CI builds Windows + Android64 on push.
