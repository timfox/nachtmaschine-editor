# Nachtmaschine materials: Silhouette Parallax Occlusion Mapping (SPOM)

TrenchBroom edits brush textures and entity data; **surface materials** for the Nachtmaschine engine live in **`.mtr`** declaration files under your mod’s `materials/` tree (or equivalent). SPOM is a **per-material** keyword block on **PBR / `rmaomap`** surfaces.

## Requirements

- **PBR path:** Material stages use **`rmaomap`** (or merged PBR specular usage). SPOM runs in **`USE_PBR`** forward and IBL shaders.
- **Displacement:** Store height in the **RMAO / specular texture alpha**. **`0.5`** is neutral (no displacement). Values above/below that create raised/recessed detail.

## Keywords (`.mtr` global / material scope)

| Keyword | Meaning |
|--------|---------|
| **`spom`** | Strength **`0`–`1`**. **`0`** disables SPOM. |
| **`spomHeight`** | Parallax depth scale in tangent UV space (typical **`0.02`–`0.08`**; default in engine **`0.04`** if omitted when `spom` is on). |
| **`spomSteps`** | **Maximum** ray-march layers (**`4`–`48`**; default **`16`**). The shader uses **fewer steps when looking straight at the surface** and **more when grazing**, for quality vs cost. |
| **`spomSilhouette`** | UV-edge / grazing softness divisor (**`0.1`–`8`**; default **`1`**). Larger values soften the effect near UV seams and at glancing angles (reduces wrong extrusion). |

Expressions use the same rules as other material parms (constants, **`parm*`** registers, **`time`**, etc.).

## Example

```text
material textures/my/wall_pbr
{
    ...
    {
        blend diffusemap
        map textures/my/wall_d.tga
    }
    {
        blend bumpmap
        map textures/my/wall_local.tga
    }
    {
        blend specularmap
        map textures/my/wall_rmao.tga
    }

    spom 1
    spomHeight 0.045
    spomSteps 24
    spomSilhouette 1.2
}
```

## Engine reference

Implementation details (shader paths, uniform **`rpSpomParams`**, IBL vs interaction) are documented in the **engine** repository **`AGENTS.md`** (section *Silhouette Parallax Occlusion Mapping*).
