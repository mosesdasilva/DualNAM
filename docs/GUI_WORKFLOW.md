# DualNAM GUI workflow

DualNAM front-end work uses an SVG-first workflow.

The product owner designs the visual components in a vector tool and exports
approved components as SVGs. The plug-in code should place, transform, bind, and
animate those assets rather than manually redrawing the artwork in C++.

## Source of truth

- Visual design source of truth: product-owner-approved SVG exports.
- Runtime asset location: `NeuralAmpModeler/resources/img/`.
- Runtime behavior source of truth: DualNAM-owned iPlug2 controls and layout
  code in `NeuralAmpModeler/`.
- Visual approval source of truth: product owner manual inspection.

## What should be SVG assets

Use SVG assets for visual components such as:

- panels and background strips;
- knobs;
- switches;
- selector bars;
- meter tracks and meter segments;
- arrows, crosses, and other icons.

## What should stay in code

Use C++ for runtime behavior:

- absolute positioning and spacing;
- parameter binding;
- live value text;
- knob rotation;
- switch state;
- dynamic meter segment repetition;
- file-browser behavior;
- disabled/enabled state;
- source-level layout assertions.

## Practical rules

- Prefer replacing or editing an SVG asset when the visual design changes.
- Avoid re-creating approved artwork with C++ primitives unless SVG rendering
  cannot express the needed runtime behavior.
- Keep reusable behavior in project controls, for example labeled SVG knobs,
  SVG switches, selector bars, and dynamic SVG meters.
- Normalize exported SVG coordinates only when required for predictable scaling
  or cropping inside iPlug2, and mention that normalization in the completion
  report.
- Do not use builds, source assertions, or bundle validation as proof that the
  GUI looks right. Visual inspection belongs to the product owner.

## Current successful pattern

The current GUI uses:

- SVG panel and strip backgrounds;
- white/black knob SVGs rotated by parameter value;
- labeled SVG knob controls that draw fixed names and live values;
- SVG switch states;
- SVG selector bars with SVG icons;
- dynamic meters that draw a black SVG track and repeat a green SVG segment
  according to the live meter level.
