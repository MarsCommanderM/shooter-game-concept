#!/usr/bin/env python3
"""Authors one StandardPBR .material per Rin FBX material (rin_m_<part>.material) next to STW_ENEMY_01_RIN.fbx.

The engine's Rin.fbx references textures (..\\Mocap\\Rigs\\Rin\\Textures\\*.tif) that ship neither with the O3DE
MotionMatching gem nor with this repository, so the Asset Processor generates all 16 materials without textures and the
character renders flat white. These constant-colour materials (linear baseColor, no external assets) replace them; the
names match the FBX materials so the scene builder picks them up. Deterministic: rerunning produces identical files.
"""
import json
from pathlib import Path

OUT = Path(__file__).parents[2] / "stw-o3de/Project/Assets/Enemies/STW_ENEMY_01_RIN"
SKIN = (0.34, 0.20, 0.15)
HAIR = (0.030, 0.019, 0.013)
# name: (linear baseColor rgb, metallic, roughness)
MATERIALS = {
    "rin_m_face": (SKIN, 0.0, 0.52),
    "rin_m_hands": (SKIN, 0.0, 0.55),
    "rin_m_fuzz": ((0.36, 0.22, 0.17), 0.0, 0.70),
    "rin_m_mouth": ((0.26, 0.08, 0.07), 0.0, 0.40),
    "rin_m_tearduct": ((0.48, 0.24, 0.21), 0.0, 0.35),
    "rin_m_eyeballs": ((0.72, 0.71, 0.68), 0.0, 0.12),
    "rin_m_eyewetness": ((0.90, 0.90, 0.90), 0.0, 0.04),
    "rin_m_eyecover": ((0.90, 0.90, 0.90), 0.0, 0.05),
    "rin_m_eyebrow": (HAIR, 0.0, 0.60),
    "rin_m_lashes": (HAIR, 0.0, 0.60),
    "rin_m_haircap": (HAIR, 0.0, 0.50),
    "rin_m_haircards": (HAIR, 0.0, 0.50),
    "rin_m_hairplanes": (HAIR, 0.0, 0.50),
    "rin_m_cloth": ((0.045, 0.065, 0.075), 0.0, 0.86),
    "rin_m_leather": ((0.060, 0.034, 0.020), 0.0, 0.55),
    "rin_m_armor": ((0.085, 0.095, 0.105), 0.70, 0.36),
    "rin_m_props": ((0.100, 0.090, 0.070), 0.40, 0.50),
}


def main():
    for name, (rgb, metallic, roughness) in sorted(MATERIALS.items()):
        material = {
            "materialType": "@gemroot:Atom_Feature_Common@/Assets/Materials/Types/StandardPBR.materialtype",
            "materialTypeVersion": 5,
            "propertyValues": {
                "baseColor.color": [rgb[0], rgb[1], rgb[2], 1.0],
                "baseColor.factor": 1.0,
                "metallic.factor": metallic,
                "roughness.factor": roughness,
                "specularF0.factor": 0.5,
            },
        }
        (OUT / (name + ".material")).write_text(json.dumps(material, indent=4) + "\n", encoding="utf-8")
    print("RIN_MATERIALS=%d DIR=%s" % (len(MATERIALS), OUT))


if __name__ == "__main__":
    main()
